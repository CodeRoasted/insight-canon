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
    // refs: ADR-20.D12, DN-134.D13
    // invariant: ONE definition, because the class and the discriminant are complements —
    // different trim sets would disagree about where a name starts.
    // invariant: no CR: canon removes a line's ending at its doors, so a CR in a name is content.
    [[nodiscard]] constexpr bool is_intent_trim_byte(char byte) noexcept
    {
        return byte == ' ' || byte == '\t';
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

// refs: F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate, STU-4
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

// refs: ADR-18.D1
TemplateId intent_id_of(std::string_view name)
{
    return template_id_of(canonicalize_intent(name));
}

std::string canonicalize_intent(const tokenization::IntentMarker& marker)
{
    const std::string_view name{trimmed_intent_name(marker.name)};
    if (marker.version.empty() || marker.version.size() > name.size())
        return canonicalize_intent(name);
    // invariant: the head keeps its introducer and takes the three rules as any name does; the
    // version is masked whole, whatever its bytes are.
    std::string out{canonicalize_intent(name.substr(0, name.size() - marker.version.size()))};
    out.append(kVersionMask);
    return out;
}

std::string_view one_token_version_of(std::string_view name, std::string_view introducer) noexcept
{
    name = trimmed_intent_name(name);
    if (introducer.empty())
        return {};
    for (const char byte : name)
        if (is_intent_trim_byte(byte))
            return {};
    const std::size_t found{name.rfind(introducer)};
    if (found == std::string_view::npos)
        return {};
    name.remove_prefix(found + introducer.size());
    return name;
}

std::string_view discriminant_of(std::string_view name, std::string_view version) noexcept
{
    name = trimmed_intent_name(name);
    if (version.empty() || version.size() > name.size())
        return discriminant_of(name);
    std::string_view head{name};
    head.remove_suffix(version.size());
    const std::string_view head_spans{discriminant_of(head)};
    // invariant: the envelope opens at the head's first masked span when it has one, else at the
    // version, and always closes at the end of the name.
    const std::size_t first{head_spans.empty()
                                ? head.size()
                                : static_cast<std::size_t>(head_spans.data() - name.data())};
    name.remove_prefix(first);
    return name;
}

std::string_view discriminant_of(std::string_view name) noexcept
{
    // refs: ADR-18, ADR-18.D1, ADR-16.D13
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

std::string declared_discriminant_of(std::string_view name, std::string_view version,
                                     std::span<const tokenization::DeclaredRun> runs)
{
    name = trimmed_intent_name(name);
    const std::string_view envelope{discriminant_of(name, version)};
    if (runs.empty() || envelope.empty())
        return std::string{envelope};
    // invariant: the envelope is a view into the trimmed name, so its offsets are the name's.
    const auto first{static_cast<std::size_t>(envelope.data() - name.data())};
    const std::size_t last{first + envelope.size()};
    const std::size_t tail{
        version.empty() || version.size() > name.size() ? last : name.size() - version.size()};
    std::vector<tokenization::ClaimedRun> claims;
    tokenization::claim_declared_runs(name, runs, claims);
    std::string out;
    out.reserve(envelope.size());
    std::size_t copied{first};
    for (const tokenization::ClaimedRun& claim : claims)
    {
        if (claim.first < first || claim.last > tail)
            continue;
        out.append(name.substr(copied, claim.first - copied));
        out.append(tokenization::kMaskWildcard);
        copied = claim.last;
    }
    out.append(name.substr(copied, last - copied));
    return out;
}

} // namespace insight

namespace insight::tokenization
{
namespace
{
    [[nodiscard]] constexpr bool is_alpha(char chr) noexcept
    {
        return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z');
    }

    // post: the position after the digit run a marker of `run` opens at `pos`, `claims` holding it
    // when it is the run's value; `pos + 1` when no marker opens a digit run there.
    // pre: `pos` is the form's start or follows a byte that is neither a letter nor a digit.
    [[nodiscard]] std::size_t claim_at(std::string_view form, std::size_t pos,
                                       const DeclaredRun& run, std::vector<ClaimedRun>& claims)
    {
        for (const std::string_view marker : run.markers)
        {
            if (!form.substr(pos).starts_with(marker))
                continue;
            const std::size_t first_digit{pos + marker.size()};
            std::size_t end{first_digit};
            while (end < form.size() && is_digit(form[end]))
                ++end;
            if (end == first_digit || (end < form.size() && is_alpha(form[end])))
                continue;
            if (form.substr(first_digit, end - first_digit) == run.value)
                claims.push_back({.first = first_digit, .last = end});
            return end;
        }
        return pos + 1;
    }
} // namespace

void claim_declared_runs(std::string_view form, std::span<const DeclaredRun> runs,
                         std::vector<ClaimedRun>& claims)
{
    claims.clear();
    for (const DeclaredRun& run : runs)
    {
        // note: the whole cost of a form naming no declared value is this one search per run.
        if (form.find(run.value) == std::string_view::npos)
            continue;
        std::size_t pos{0};
        while (pos < form.size())
            pos = pos == 0 || (!is_alpha(form[pos - 1]) && !is_digit(form[pos - 1]))
                      ? claim_at(form, pos, run, claims)
                      : pos + 1;
    }
    std::ranges::sort(claims, {}, &ClaimedRun::first);
    const auto [dup_first, dup_last]{std::ranges::unique(claims)};
    claims.erase(dup_first, dup_last);
}

} // namespace insight::tokenization
