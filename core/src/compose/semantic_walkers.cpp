module;

module insight.canon;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;     // the composed row types (StructuralRoleRow / IntentMarkerRow / LocationRow)
import insight.canon.compose; // ComposedSemantics

// semantic_walkers.cpp — the dialect-recognition ALGORITHMS over the composed vocabulary (ADR 0024
// §3). Canon owns these algorithms; the composed rows (from the semantic packages) are the DATA.
// Homed as a facade impl unit (module insight.canon) because they consume ComposedSemantics, which
// imports api — so they cannot live in api. Ported byte-for-byte from the pre-split hardcoded
// StructuralRoleRegistry::classify / IntentMarkerRegistry::recognize / recognize_location so the
// composed pipeline is byte-identical (G-SP-1).

namespace insight
{
namespace
{
// A row's format gate matches the line's routed format when the gate is kAnyFormat (fire on any
// format — the pre-split ungated behavior) or equals the concrete format (II-6 dialect gating). NOT
// gates_intersect: a line whose routed format is Unknown must NOT trigger a concretely-gated dialect
// row (the pre-split `format != GitHubActions → {}` guard).
[[nodiscard]] constexpr bool gate_matches(LogFormat row_gate, LogFormat line_format) noexcept
{
    return row_gate == insight::semantic::kAnyFormat || row_gate == line_format;
}

// ── Location matching (ported from location_recognizer.cpp) — the three closed families ──
[[nodiscard]] constexpr bool loc_is_lower(char chr) noexcept { return chr >= 'a' && chr <= 'z'; }
[[nodiscard]] constexpr bool loc_is_space(char chr) noexcept { return chr == ' ' || chr == '\t'; }
[[nodiscard]] constexpr bool loc_is_word(char chr) noexcept
{
    return loc_is_lower(chr) || (chr >= 'A' && chr <= 'Z') || (chr >= '0' && chr <= '9') || chr == '_';
}
[[nodiscard]] constexpr std::string_view loc_slice(std::string_view str, std::size_t pos,
                                                  std::size_t count) noexcept
{
    return std::string_view{str.data() + pos, count};
}
[[nodiscard]] bool ext_in(std::string_view ext, std::span<const std::string_view> set) noexcept
{
    return std::ranges::any_of(set, [ext](std::string_view kind) noexcept { return ext == kind; });
}

// `.test.<ext>` / `.spec.<ext>` with ext ∈ row.extensions (no dot). Returns end offset one past the
// extension, or npos.
[[nodiscard]] std::size_t match_test_spec(std::string_view tok, const insight::semantic::LocationRow& row) noexcept
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
// row.prefixes OR ends_with any row.suffixes. Returns end offset one past the extension, or npos.
[[nodiscard]] std::size_t match_prefix_ext(std::string_view tok, const insight::semantic::LocationRow& row) noexcept
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
        const bool prefix_hit{std::ranges::any_of(
            row.prefixes, [base](std::string_view pre) noexcept { return base.starts_with(pre); })};
        const bool suffix_hit{std::ranges::any_of(
            row.suffixes, [base](std::string_view suf) noexcept { return base.ends_with(suf); })};
        if (prefix_hit || suffix_hit)
            return end;
    }
    return std::string_view::npos;
}

// go / ruby: any full-file suffix in row.suffixes, word-boundary-terminated. Returns end offset, or npos.
[[nodiscard]] std::size_t match_suffix_set(std::string_view tok, const insight::semantic::LocationRow& row) noexcept
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

// Walk the composed location rows in canonical order for ONE token; the first family that claims a
// test-file wins (declared order == the pre-split family order 1:TestSpec, 2:pytest, 3:suffix).
[[nodiscard]] std::size_t test_file_end(std::string_view tok,
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

std::string_view recognize_location(std::string_view content,
                                    const insight::semantic::ComposedSemantics& composed) noexcept
{
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

StructuralRole classify(std::string_view content, LogFormat format,
                        const insight::semantic::ComposedSemantics& composed) noexcept
{
    // Longest-match: the row with the longest matching prefix wins (deterministic, declaration-order-
    // free). No nesting among today's rows → equivalent to the pre-split fixed-order chain.
    StructuralRole best{StructuralRole::None};
    std::size_t best_len{0};
    for (const insight::semantic::StructuralRoleRow& row : composed.roles())
        if (gate_matches(row.format_gate, format) && content.starts_with(row.prefix) &&
            row.prefix.size() > best_len)
        {
            best = row.role;
            best_len = row.prefix.size();
        }
    return best;
}

IntentMarker recognize(std::string_view content, LogFormat format,
                       const insight::semantic::ComposedSemantics& composed) noexcept
{
    const insight::semantic::IntentMarkerRow* best{nullptr};
    for (const insight::semantic::IntentMarkerRow& row : composed.markers())
        if (gate_matches(row.format_gate, format) && content.starts_with(row.prefix) &&
            (best == nullptr || row.prefix.size() > best->prefix.size()))
            best = &row;
    if (best == nullptr)
        return {};
    // RemainderAfterPrefix: the payload is the content after the matched prefix, verbatim; the class
    // (canonicalize_intent) is derived downstream, the discriminant here (the ADR 0023 raw coordinate).
    const std::string_view payload{content.substr(best->prefix.size())};
    return {.kind = best->kind,
            .name = payload,
            .discriminant = discriminant_of(payload),
            .child_order = best->child_order};
}

} // namespace tokenization
} // namespace insight
