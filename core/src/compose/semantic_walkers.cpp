module;

module insight.canon;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;
import insight.canon.compose;

namespace insight
{
namespace
{

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
    // refs: ADR-17.D1, F-SRC-insight-canon:test_semantic_walkers.cpp
    [[nodiscard]] constexpr bool loc_is_path(char chr) noexcept
    {
        return loc_is_word(chr) || chr == '/' || chr == '\\' || chr == '.' || chr == '-' ||
               chr == '@' || chr == '+' || chr == '~' || chr == '*';
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

    // post: the end offset one past the extension, or npos.
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

    // post: the end offset one past the extension, or npos.
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
                continue;
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

    // post: the end offset one past a word-boundary-terminated suffix, or npos.
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

    // post: the first family in declared order that claims the token wins; npos when none does.
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

// refs: ADR-17.D1
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
        {
            std::size_t begin{end};
            while (begin > 0 && loc_is_path(tok[begin - 1U]))
                --begin;
            return loc_slice(tok, begin, end - begin);
        }
    }
    return {};
}

namespace tokenization
{

    // pre: `composed` is a view already resolved for the stream; no dialect gate is tested here.
    // post: a matching shape row wins over every prefix row, being a claim on the whole content;
    // among rows of one kind the longest declared bytes win.
    // invariant: two shape rows of one length both matching a line keep the first in composed
    // order, which is canonical, so the answer never depends on declaration or link order.
    // refs: ADR-17.D1, ADR-17.D4, ADR-22.D6, DN-134.D9
    StructuralRole classify(NormalizedContent normalized,
                            const insight::semantic::ComposedSemantics& composed) noexcept
    {
        const std::string_view content{normalized.bytes()};
        StructuralRole best{StructuralRole::None};
        bool best_is_shape{false};
        std::size_t best_len{0};
        for (const insight::semantic::StructuralRoleRow& row : composed.roles())
        {
            const bool shape{row.match == insight::semantic::RoleMatchKind::Shape};
            if (best_is_shape && !shape)
                continue;
            const bool longer{(shape && !best_is_shape) || row.prefix.size() > best_len};
            if (!longer)
                continue;
            if (shape ? insight::semantic::shape_matches(row.prefix, content)
                      : content.starts_with(row.prefix))
            {
                best = row.role;
                best_is_shape = shape;
                best_len = row.prefix.size();
            }
        }
        return best;
    }

    namespace
    {
        // post: a digit run, one ':', then the payload — which ENDS at the first CR and drops a
        // trailing option group.
        // post: nullopt on any shape failure, an empty payload included.
        // note: the CR TERMINATES the payload; trimming it welds header prose into the name.
        // refs: ADR-17.D6, F-SRC-insight-canon:canon.api.cppm:normalize
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
                if (const std::size_t group{remainder.rfind('[')}; group != std::string_view::npos)
                    remainder = std::string_view{remainder.data(), group};
            if (remainder.empty())
                return std::nullopt;
            return remainder;
        }

        // pre: the caller has matched `row.prefix`, so `row.prefix.size() <= content.size()`.
        // post: nullopt when the extractor's own shape requirement fails, so the ROW does not
        // match.
        // note: the in-place trims replace substr, whose throw path would escape this noexcept.
        [[nodiscard]] std::optional<std::string_view>
        extract_payload(std::string_view content,
                        const insight::semantic::IntentMarkerRow& row) noexcept
        {
            std::string_view remainder{content};
            remainder.remove_prefix(row.prefix.size());
            switch (row.extract)
            {
            case insight::semantic::PayloadExtract::None:
                return std::string_view{};
            case insight::semantic::PayloadExtract::RemainderAfterPrefix:
                return remainder;
            case insight::semantic::PayloadExtract::RemainderToClosingParen:
                // note: a line-final ')' is REQUIRED; it is dropped, nested parens kept.
                // refs: STU-6
                if (remainder.size() < 2U || remainder.back() != ')')
                    return std::nullopt;
                remainder.remove_suffix(1U);
                return remainder;
            case insight::semantic::PayloadExtract::NumericFieldThenRemainder:
                return skip_numeric_field(remainder);
            }
            return std::nullopt;
        }

        // post: the payload's version under the row's declared coordinate; empty when the row
        // declares none or the payload is not the declared shape.
        [[nodiscard]] std::string_view
        declared_version(std::string_view payload,
                         const insight::semantic::IntentMarkerRow& row) noexcept
        {
            switch (row.version.shape)
            {
            case insight::semantic::VersionPayloadShape::None:
                return {};
            case insight::semantic::VersionPayloadShape::OneToken:
                return one_token_version_of(payload, row.version.introducer);
            }
            return {};
        }

        // post: true when an entry equals the payload or is its leading space-delimited token.
        // refs: ADR-17.D6, STU-6
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

    // pre: `composed` is a view already resolved for the stream; no dialect gate is tested here.
    // post: the longest VALID match wins — a row whose extractor fails or whose payload is
    // excluded falls through.
    // refs: ADR-17.D1, ADR-17.D4, ADR-22.D6
    IntentMarker recognize(NormalizedContent normalized,
                           const insight::semantic::ComposedSemantics& composed) noexcept
    {
        const std::string_view content{normalized.bytes()};
        const insight::semantic::IntentMarkerRow* best{nullptr};
        std::string_view best_payload;
        for (const insight::semantic::IntentMarkerRow& row : composed.markers())
        {
            if (row.role != insight::semantic::MarkerRole::Names ||
                !content.starts_with(row.prefix) ||
                (best != nullptr && row.prefix.size() <= best->prefix.size()))
                continue;
            const std::optional<std::string_view> payload{extract_payload(content, row)};
            if (!payload || payload_excluded(*payload, row))
                continue;
            best = &row;
            best_payload = *payload;
        }
        if (best == nullptr)
            return {};
        // note: the payload is the extractor's capture verbatim; the class is derived downstream.
        // refs: ADR-18.D5
        const std::string_view version{declared_version(best_payload, *best)};
        return {.kind = best->kind,
                .name = best_payload,
                .discriminant = discriminant_of(best_payload, version),
                .child_order = best->child_order,
                .version = version};
    }

    // pre: `composed` is a view already resolved for the stream; no dialect gate is tested here.
    // post: the longest opening row's kind; an opening row has no extractor, so its prefix alone
    // decides the match.
    // refs: DN-89.D33
    IntentMarkerKind recognize_opener(NormalizedContent normalized,
                                      const insight::semantic::ComposedSemantics& composed) noexcept
    {
        const std::string_view content{normalized.bytes()};
        const insight::semantic::IntentMarkerRow* best{nullptr};
        for (const insight::semantic::IntentMarkerRow& row : composed.markers())
            if (row.role == insight::semantic::MarkerRole::Opens &&
                content.starts_with(row.prefix) &&
                (best == nullptr || row.prefix.size() > best->prefix.size()))
                best = &row;
        return best == nullptr ? IntentMarkerKind::None : best->kind;
    }

    // pre: `composed` is a view already resolved for the stream; no dialect gate is tested here.
    // post: a closing row has no extractor, so its prefix alone decides the match.
    // refs: DN-89.D40
    bool recognize_closer(NormalizedContent content,
                          const insight::semantic::ComposedSemantics& composed) noexcept
    {
        const std::string_view bytes{content.bytes()};
        return std::ranges::any_of(composed.markers(),
                                   [bytes](const insight::semantic::IntentMarkerRow& row) noexcept
                                   {
                                       return row.role == insight::semantic::MarkerRole::Closes &&
                                              bytes.starts_with(row.prefix);
                                   });
    }

} // namespace tokenization
} // namespace insight
