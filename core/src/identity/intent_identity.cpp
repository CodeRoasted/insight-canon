module insight.canon.api;
import insight.canon.internal;

// refs: BIB:intent_identity, ADR-25.D6, ADR-17, ADR-18
// invariant: geometry TREE, axis species POPULATION: matrix legs and shards are exchangeable
// siblings, so the algebra is alignment, never compose.
// note: the appearance ordinal refused below is a forbidden identity key, not that axis species.
namespace insight
{
namespace
{
    constexpr std::string_view kVersionMask{"vX"};
    constexpr std::string_view kDigitMask{"N"};
    constexpr std::string_view kParenMask{"(M)"};
    constexpr std::size_t kMinMaskedDigits{2};

    // invariant: ASCII only — a locale-dependent classifier is a determinism hazard.
    [[nodiscard]] constexpr bool is_word(char chr) noexcept
    {
        return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') ||
               (chr >= '0' && chr <= '9') || chr == '_';
    }

    [[nodiscard]] constexpr bool is_digit(char chr) noexcept
    {
        return chr >= '0' && chr <= '9';
    }

    // invariant: R1-R3 are anchored at both ends, so a numeric run glued to trailing letters (v6x,
    // 42px) is not a version token and stays literal.
    [[nodiscard]] constexpr bool boundary_after(std::string_view str, std::size_t pos) noexcept
    {
        return pos >= str.size() || !is_word(str[pos]);
    }

    struct NumericClaim
    {
        std::string_view mask;
        std::size_t end;
    };

    // post: the mask and end offset when a rule claims the token at `start`, nullopt when none
    // does.
    [[nodiscard]] std::optional<NumericClaim> claim_numeric(std::string_view str,
                                                            std::size_t start) noexcept
    {
        std::size_t pos{start};
        const bool has_v{str[pos] == 'v'};
        if (has_v)
            ++pos;
        const std::size_t digits_start{pos};
        while (pos < str.size() && is_digit(str[pos]))
            ++pos;
        if (pos == digits_start)
            return std::nullopt;

        std::size_t dot_end{pos};
        std::size_t dot_groups{0};
        while (dot_end + 1 < str.size() && str[dot_end] == '.' && is_digit(str[dot_end + 1]))
        {
            ++dot_end;
            while (dot_end < str.size() && is_digit(str[dot_end]))
                ++dot_end;
            ++dot_groups;
        }

        if (dot_groups > 0 && boundary_after(str, dot_end))
            return NumericClaim{.mask = kVersionMask, .end = dot_end};
        if (has_v && boundary_after(str, pos))
            return NumericClaim{.mask = kVersionMask, .end = pos};
        if (!has_v && (pos - digits_start) >= kMinMaskedDigits && boundary_after(str, pos))
            return NumericClaim{.mask = kDigitMask, .end = pos};
        return std::nullopt;
    }
    // refs: DN-38.D1
    // invariant: ONE definition, because the class and the discriminant are complements —
    // different trim sets would disagree about where a name starts.
    // note: CR is a materialization artifact — a Windows runner emits CRLF into banners.
    [[nodiscard]] constexpr bool is_intent_trim_byte(char byte) noexcept
    {
        return byte == ' ' || byte == '\t' || byte == '\r';
    }
} // namespace

std::string_view trimmed_intent_name(std::string_view name) noexcept
{
    while (!name.empty() && is_intent_trim_byte(name.front()))
        name.remove_prefix(1);
    while (!name.empty() && is_intent_trim_byte(name.back()))
        name.remove_suffix(1);
    return name;
}

// refs: SRC-D-TID-1, SRC-D-TID-2, SRC-II-2, STU-4
// invariant: a DISTINCT rule set from the value masker: the masker keeps structure to distinguish,
// identity canonicalization collapses it to align.
// invariant: the rules run left-to-right in one pass at word boundaries, R1 before R3 so a dotted
// version is not fragmented.
// note: a single bare digit is KEPT — collapsing it would over-merge two WHEREs.
std::string canonicalize_intent(std::string_view name)
{
    name = trimmed_intent_name(name);

    std::string out;
    out.reserve(name.size());
    std::size_t idx{0};
    bool prev_is_word{false};
    while (idx < name.size())
    {
        const char chr{name[idx]};

        if (chr == '(')
        {
            const std::size_t close{name.find(')', idx + 1)};
            if (close != std::string_view::npos)
            {
                out.append(kParenMask);
                idx = close + 1;
                prev_is_word = false;
                continue;
            }
        }

        if (!prev_is_word && (chr == 'v' || is_digit(chr)))
        {
            if (const std::optional<NumericClaim> claim{claim_numeric(name, idx)}; claim)
            {
                out.append(claim->mask);
                idx = claim->end;
                prev_is_word = true;
                continue;
            }
        }

        out.push_back(chr);
        prev_is_word = is_word(chr);
        ++idx;
    }
    return out;
}

// refs: SRC-II-1
TemplateId intent_id_of(std::string_view name)
{
    return template_id_of(canonicalize_intent(name));
}

std::string_view discriminant_of(std::string_view name) noexcept
{
    // refs: ADR-18, SRC-II-9, DN-38.D3
    // post: the envelope of the masked spans, first span's start to last span's end, class material
    // between them included; empty when no span is claimed.
    // invariant: (class, envelope) separates two names whose spans occupy the same class positions;
    // it is NOT injective over arbitrary strings.
    // note: a contiguous view, not a span list: a join would need a separator.
    name = trimmed_intent_name(name);

    std::size_t first{std::string_view::npos};
    std::size_t last{0};
    std::size_t idx{0};
    bool prev_is_word{false};
    while (idx < name.size())
    {
        const char chr{name[idx]};
        if (chr == '(')
        {
            if (const std::size_t close{name.find(')', idx + 1)}; close != std::string_view::npos)
            {
                if (first == std::string_view::npos)
                    first = idx;
                last = close + 1;
                idx = close + 1;
                prev_is_word = false;
                continue;
            }
        }
        if (!prev_is_word && (chr == 'v' || is_digit(chr)))
        {
            if (const std::optional<NumericClaim> claim{claim_numeric(name, idx)}; claim)
            {
                if (first == std::string_view::npos)
                    first = idx;
                last = claim->end;
                idx = claim->end;
                prev_is_word = true;
                continue;
            }
        }
        prev_is_word = is_word(chr);
        ++idx;
    }
    if (first == std::string_view::npos)
        return {};
    return std::string_view{name.data() + first, last - first};
}

} // namespace insight
