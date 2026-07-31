module;

module insight.canon;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi; // the composed row types (StructuralRoleRow / IntentMarkerRow / LocationRow)
import insight.canon.compose; // ComposedSemantics

// semantic_walkers.cpp — the dialect-recognition ALGORITHMS over the composed vocabulary (ADR-17
// §3). Canon owns these algorithms; the composed rows (from the semantic packages) are the DATA.
// Homed as a facade impl unit (module insight.canon) because they consume ComposedSemantics, which
// imports api — so they cannot live in api. Ported byte-for-byte from the pre-split hardcoded
// StructuralRoleRegistry::classify / IntentMarkerRegistry::recognize / recognize_location so the
// composed pipeline is byte-identical (G-SP-1).

namespace insight
{
namespace
{
    // NO row-gate predicate here any more (ADR-22). The dialect gate is evaluated ONCE,
    // at `resolve_stream`, and filtered into the view these walkers receive — so a row that is in
    // the table is a row that fires, and there is nothing left to test per line. What this removed
    // is not a compare: it is a DETERMINISM hazard. The old gate argument was
    // `LogParser::routed_format()`, the per-line detector winner under a sticky-strategy fast path,
    // so which DECLARED rows fired was a function of content.

    // ── Location matching (ported from location_recognizer.cpp) — the three closed families ──
    [[nodiscard]] constexpr bool loc_is_lower(char chr) noexcept
    {
        return chr >= 'a' && chr <= 'z';
    }
    [[nodiscard]] constexpr bool loc_is_space(char chr) noexcept
    {
        return chr == ' ' || chr == '\t';
    }
    [[nodiscard]] constexpr bool loc_is_word(char chr) noexcept
    {
        return loc_is_lower(chr) || (chr >= 'A' && chr <= 'Z') || (chr >= '0' && chr <= '9') ||
               chr == '_';
    }
    [[nodiscard]] constexpr std::string_view loc_slice(std::string_view str, std::size_t pos,
                                                       std::size_t count) noexcept
    {
        return std::string_view{str.data() + pos, count};
    }
    [[nodiscard]] bool ext_in(std::string_view ext, std::span<const std::string_view> set) noexcept
    {
        return std::ranges::any_of(set,
                                   [ext](std::string_view kind) noexcept { return ext == kind; });
    }

    // `.test.<ext>` / `.spec.<ext>` with ext ∈ row.extensions (no dot). Returns end offset one past
    // the extension, or npos.
    [[nodiscard]] std::size_t match_test_spec(std::string_view tok,
                                              const insight::semantic::LocationRow& row) noexcept
    {
        for (const std::string_view infix : row.infixes)
        {
            const std::size_t pos{tok.find(infix)};
            if (pos == std::string_view::npos)
                continue;
            const std::size_t ext_start{pos + infix.size()};
            std::size_t ext_end{ext_start};
            while (ext_end < tok.size() && loc_is_lower(tok[ext_end]))
                ++ext_end;
            if (ext_in(loc_slice(tok, ext_start, ext_end - ext_start), row.extensions))
                return ext_end;
        }
        return std::string_view::npos;
    }

    // pytest bare module: an extension (row.extensions, e.g. `.py`) whose basename starts_with any
    // row.prefixes OR ends_with any row.suffixes. Returns end offset one past the extension, or
    // npos.
    [[nodiscard]] std::size_t match_prefix_ext(std::string_view tok,
                                               const insight::semantic::LocationRow& row) noexcept
    {
        for (const std::string_view ext : row.extensions)
        {
            const std::size_t pos{tok.find(ext)};
            if (pos == std::string_view::npos)
                continue;
            const std::size_t end{pos + ext.size()};
            if (end != tok.size() && loc_is_word(tok[end]))
                continue; // the extension is glued to more word chars → not a file end
            const std::size_t slash{tok.rfind('/', pos)};
            const std::size_t base_start{slash == std::string_view::npos ? 0 : slash + 1};
            const std::string_view base{loc_slice(tok, base_start, pos - base_start)};
            const bool prefix_hit{std::ranges::any_of(row.prefixes,
                                                      [base](std::string_view pre) noexcept
                                                      { return base.starts_with(pre); })};
            const bool suffix_hit{std::ranges::any_of(row.suffixes,
                                                      [base](std::string_view suf) noexcept
                                                      { return base.ends_with(suf); })};
            if (prefix_hit || suffix_hit)
                return end;
        }
        return std::string_view::npos;
    }

    // go / ruby: any full-file suffix in row.suffixes, word-boundary-terminated. Returns end
    // offset, or npos.
    [[nodiscard]] std::size_t match_suffix_set(std::string_view tok,
                                               const insight::semantic::LocationRow& row) noexcept
    {
        for (const std::string_view suffix : row.suffixes)
        {
            const std::size_t pos{tok.find(suffix)};
            if (pos == std::string_view::npos)
                continue;
            const std::size_t end{pos + suffix.size()};
            if (end == tok.size() || !loc_is_word(tok[end]))
                return end;
        }
        return std::string_view::npos;
    }

    // Walk the composed location rows in canonical order for ONE token; the first family that
    // claims a test-file wins (declared order == the pre-split family order 1:TestSpec, 2:pytest,
    // 3:suffix).
    [[nodiscard]] std::size_t
    test_file_end(std::string_view tok,
                  std::span<const insight::semantic::LocationRow> rows) noexcept
    {
        for (const insight::semantic::LocationRow& row : rows)
        {
            std::size_t end{std::string_view::npos};
            switch (row.kind)
            {
            case insight::semantic::LocationMatchKind::TestSpecExtension:
                end = match_test_spec(tok, row);
                break;
            case insight::semantic::LocationMatchKind::PrefixAndExtension:
                end = match_prefix_ext(tok, row);
                break;
            case insight::semantic::LocationMatchKind::SuffixSet:
                end = match_suffix_set(tok, row);
                break;
            }
            if (end != std::string_view::npos)
                return end;
        }
        return std::string_view::npos;
    }
} // namespace

std::string_view recognize_location(insight::tokenization::NormalizedContent normalized,
                                    const insight::semantic::ComposedSemantics& composed) noexcept
{
    const std::string_view content{normalized.bytes()};
    const std::span<const insight::semantic::LocationRow> rows{composed.locations()};
    std::size_t cursor{0};
    const std::size_t len{content.size()};
    while (cursor < len)
    {
        while (cursor < len && loc_is_space(content[cursor]))
            ++cursor;
        if (cursor >= len)
            break;
        const std::size_t start{cursor};
        while (cursor < len && !loc_is_space(content[cursor]))
            ++cursor;
        const std::string_view tok{loc_slice(content, start, cursor - start)};
        if (const std::size_t end{test_file_end(tok, rows)}; end != std::string_view::npos)
            return loc_slice(tok, 0, end);
    }
    return {};
}

namespace tokenization
{

    StructuralRole classify(NormalizedContent normalized,
                            const insight::semantic::ComposedSemantics& composed) noexcept
    {
        const std::string_view content{normalized.bytes()};
        // Longest-match: the row with the longest matching prefix wins (deterministic,
        // declaration-order- free). No nesting among today's rows → equivalent to the pre-split
        // fixed-order chain.
        StructuralRole best{StructuralRole::None};
        std::size_t best_len{0};
        for (const insight::semantic::StructuralRoleRow& row : composed.roles())
            if (content.starts_with(row.prefix) && row.prefix.size() > best_len)
            {
                best = row.role;
                best_len = row.prefix.size();
            }
        return best;
    }

    namespace
    {
        // The NumericFieldThenRemainder grammar (grammar-5, ADR-17), over the content past a
        // matched prefix: a non-empty run of ASCII digits, a single ':', then the payload — which
        // ENDS AT THE FIRST '\r', and from which a trailing `[…]` option group is then dropped.
        // nullopt on any shape failure, including an empty payload: a declined row is the whole
        // point, since the malformed producer marker (`section_start:%s:name`) must not be
        // mis-parsed into a section named after an unexpanded shell expression.
        //
        // THE CR IS A TERMINATOR, NOT A TRAILING BYTE TO TRIM, and the distinction is the whole
        // measurement. GitLab closes a marker with `\r\x1b[0K` (CR + erase-line) and MAY then
        // continue the SAME line with the section's human-readable header: `…:build_tools_section
        // \r\x1b[0KTools build`. Canon's SRC-D-TID-11 ingest strip removes the escape and leaves
        // the CR, so a rule that merely trimmed a trailing CR would yield the payload
        // `build_tools_section\rTools build`. Measured on the 482 stamped traces of
        // marker_corpus_v1: trimming gives 56 distinct names, 36 of them carrying an embedded CR
        // and arbitrary human prose; terminating at the CR gives 46, every one inside the
        // producer's declared `[A-Za-z0-9_.-]+` charset. Recognition count is 3193 either way — the
        // difference is invisible in a recall number and lands entirely in the NAME, which
        // `compare_skeletons` keys on raw (ADR-18). Those 36 sections would only ever align
        // against a run whose header prose is byte-identical.
        //
        // The option group is taken as the LAST '[' of a ']'-terminated payload, not the first '['
        // anywhere: a name containing a bracket must not silently lose its tail. The observed name
        // charset carries no bracket at all, so this is a guard, not a live case.
        [[nodiscard]] constexpr std::optional<std::string_view>
        skip_numeric_field(std::string_view remainder) noexcept
        {
            std::size_t digits{0};
            while (digits < remainder.size() && remainder[digits] >= '0' &&
                   remainder[digits] <= '9')
                ++digits;
            if (digits == 0 || digits >= remainder.size() || remainder[digits] != ':')
                return std::nullopt;
            remainder.remove_prefix(digits + 1U);
            if (const std::size_t terminator{remainder.find('\r')};
                terminator != std::string_view::npos)
                remainder = std::string_view{remainder.data(), terminator};
            if (!remainder.empty() && remainder.back() == ']')
                if (const std::size_t group{remainder.rfind('[')};
                    group != std::string_view::npos)
                    remainder = std::string_view{remainder.data(), group};
            if (remainder.empty())
                return std::nullopt;
            return remainder;
        }

        // Extract a row's payload from the content past its matched prefix (the closed extractor
        // algorithms, grammar-2). nullopt = the extractor's own shape requirement failed ⇒ the ROW
        // does not match at all (RemainderToClosingParen without a line-final ')' — an un-named
        // `[Pipeline] {` wrapper).
        [[nodiscard]] std::optional<std::string_view>
        extract_payload(std::string_view content,
                        const insight::semantic::IntentMarkerRow& row) noexcept
        {
            // The caller (`recognize`) only reaches here after `content.starts_with(row.prefix)`,
            // so `prefix.size() <= content.size()` — `remove_prefix`/`remove_suffix` are the
            // noexcept in-place equivalents of the `substr` calls whose out-of-range `throw` path
            // the analyzer cannot rule out inter-procedurally (bugprone-exception-escape).
            std::string_view remainder{content};
            remainder.remove_prefix(row.prefix.size());
            switch (row.extract)
            {
            case insight::semantic::PayloadExtract::None:
                return std::string_view{};
            case insight::semantic::PayloadExtract::RemainderAfterPrefix:
                return remainder;
            case insight::semantic::PayloadExtract::RemainderToClosingParen:
                // The content after the prefix up to a REQUIRED line-final ')' (studies/006 STAGE
                // form
                // `^\{ \((.+)\)$`): non-empty payload, single trailing delimiter dropped, nested
                // parens kept.
                if (remainder.size() < 2U || remainder.back() != ')')
                    return std::nullopt;
                remainder.remove_suffix(1U); // drop the required line-final ')'
                return remainder;
            case insight::semantic::PayloadExtract::NumericFieldThenRemainder:
                return skip_numeric_field(remainder);
            }
            return std::nullopt;
        }

        // grammar-2 payload exclusion (ADR-17 / studies/006): an entry excludes when it equals
        // the payload or is its leading space-delimited token — `stage` excludes `stage` and `node`
        // excludes `node {`, while `stages` stays a step (word boundary). Matches the spike's
        // first-token/whole-body semantics.
        [[nodiscard]] bool payload_excluded(std::string_view payload,
                                            const insight::semantic::IntentMarkerRow& row) noexcept
        {
            return std::ranges::any_of(row.payload_excludes,
                                       [payload](std::string_view entry) noexcept
                                       {
                                           return payload.starts_with(entry) &&
                                                  (payload.size() == entry.size() ||
                                                   payload[entry.size()] == ' ');
                                       });
        }
    } // namespace

    IntentMarker recognize(NormalizedContent normalized,
                           const insight::semantic::ComposedSemantics& composed) noexcept
    {
        const std::string_view content{normalized.bytes()};
        const insight::semantic::IntentMarkerRow* best{nullptr};
        std::string_view best_payload;
        for (const insight::semantic::IntentMarkerRow& row : composed.markers())
        {
            if (!content.starts_with(row.prefix) ||
                (best != nullptr && row.prefix.size() <= best->prefix.size()))
                continue;
            // A row matches only when its extractor's shape holds AND the payload is not excluded
            // (grammar-2) — a failed row falls through so a shorter row may still claim the line
            // (longest VALID match wins, deterministic).
            const std::optional<std::string_view> payload{extract_payload(content, row)};
            if (!payload || payload_excluded(*payload, row))
                continue;
            best = &row;
            best_payload = *payload;
        }
        if (best == nullptr)
            return {};
        // The payload is the extractor's capture, verbatim; the class (canonicalize_intent) is
        // derived downstream, the discriminant here (the ADR-18 raw coordinate).
        return {.kind = best->kind,
                .name = best_payload,
                .discriminant = discriminant_of(best_payload),
                .child_order = best->child_order};
    }

} // namespace tokenization
} // namespace insight
