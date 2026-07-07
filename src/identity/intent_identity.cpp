module insight.canon.api;
import insight.canon.internal;

// intent_identity.cpp — the canon-owned INTENT identity (intent_identity_model.md §5/§5.1).
//
// `canonicalize_intent` is the templating discipline of the stateless value masker
// (canon.detail.mask, D-TID-1/2) REAPPLIED to identifiers — the closure-as-identity-constructor
// make-or-break (§5.1 detail 1). It is a DISTINCT rule set from the value masker, by design:
//   - the value masker keeps structure to DISTINGUISH   ( `yarn (1/10)` → `yarn (1/<*>` )
//   - identity canonicalization COLLAPSES to ALIGN      ( `yarn (1/10)` → `yarn (M)`     )
// so that matrix legs / shards / version-parameterized jobs of ONE intent map to ONE class and
// pair across homologous runs; the instance discriminant (§5.3 multiplicity) is the ordinal the
// aligner adds downstream, NEVER a fingerprint-similarity merge here (II-2: alignment must never
// eat the signal). Gate G1 (studies/004) measured this exact mask on the real GH-Actions corpus:
// matrix jobs align 0%→100% (ESLint v6/v7, byte-identical steps) once the version/tuple tokens are
// masked; the residual is genuine step-count churn, not a canonicalization miss.
//
// The frozen rule set (kIntentRegistryVersion "intent-gha-1", the GitHub-Actions dialect tier of
// the §5.2 registry). Applied left-to-right in ONE pass, at word boundaries (\w = [A-Za-z0-9_]),
// each rule masking the maximal token it claims:
//   R1 dotted-version   v?\d+(\.\d+)+   → vX   (`1.2.3`, `v1.2.3`)   — before R3 so it is not fragmented
//   R2 v-version         v\d+           → vX   (`v6`, `v7`)
//   R3 multi-digit       \d{2,}         → N    (`Node 18`, `worker-42`) — shard/index/build numbers
//   R4 paren group       (...)          → (M)  (`(1/10)`, `(Shard 1/5)`, `(ubuntu-latest)`) — first `)` (non-greedy)
// A single bare digit (no `v`, no dot) is KEPT — collapsing it would over-merge (`Shard 1` vs
// `Shard 2` are distinct WHERE, separated as instances, not fused). ASCII-only classification (no
// locale isalnum — the [[determinism-musts]] locale hazard); pure function of the name's bytes.
//
// `intent_id_of` = template_id_of(canonicalize_intent(name)): the 16-byte SHA-256 structural key
// (II-1). Identity IS "the hash under the registry version", so it is co-located here, exactly as
// template identity is co-located with kCanonicalizationVersion (template_id.cpp).

namespace insight
{
namespace
{
constexpr std::string_view kVersionMask{"vX"};
constexpr std::string_view kDigitMask{"N"};
constexpr std::string_view kParenMask{"(M)"};
constexpr std::size_t kMinMaskedDigits{2}; // R3: \d{2,} — a single digit is kept

// ASCII \w = [A-Za-z0-9_]. Manual (never <cctype> isalnum — locale-dependent, a determinism
// hazard [[determinism-musts]]); a word boundary is a transition word↔non-word.
[[nodiscard]] constexpr bool is_word(char chr) noexcept
{
    return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') ||
           (chr >= '0' && chr <= '9') || chr == '_';
}

[[nodiscard]] constexpr bool is_digit(char chr) noexcept
{
    return chr >= '0' && chr <= '9';
}

// Trailing word boundary at `pos` (end-of-string, or the next char is non-word). The masks R1–R3
// are anchored `\b…\b`: a numeric run glued to trailing letters (`v6x`, `42px`, `1.2.3rc`) is NOT
// a version/index token and stays literal (the regex would find no closing \b and not match).
[[nodiscard]] constexpr bool boundary_after(std::string_view str, std::size_t pos) noexcept
{
    return pos >= str.size() || !is_word(str[pos]);
}

// Try R1/R2/R3 at a leading word boundary `i`. Returns the mask + the end offset when one claims
// the token; std::nullopt otherwise (the char is emitted literally by the caller). `i` is known to
// be a word boundary and str[i] is 'v' or a digit.
struct NumericClaim
{
    std::string_view mask;
    std::size_t end;
};

[[nodiscard]] std::optional<NumericClaim> claim_numeric(std::string_view str, std::size_t i) noexcept
{
    std::size_t pos{i};
    const bool has_v{str[pos] == 'v'};
    if (has_v)
        ++pos;
    const std::size_t digits_start{pos};
    while (pos < str.size() && is_digit(str[pos]))
        ++pos;
    if (pos == digits_start) // 'v' not followed by a digit → not a version (e.g. "verbose")
        return std::nullopt;

    // Dotted continuation: (\.\d+)+ — a '.' followed by ≥1 digit, repeated.
    std::size_t dot_end{pos};
    std::size_t dot_groups{0};
    while (dot_end + 1 < str.size() && str[dot_end] == '.' && is_digit(str[dot_end + 1]))
    {
        ++dot_end; // consume '.'
        while (dot_end < str.size() && is_digit(str[dot_end]))
            ++dot_end;
        ++dot_groups;
    }

    if (dot_groups > 0 && boundary_after(str, dot_end)) // R1: v?\d+(\.\d+)+
        return NumericClaim{.mask = kVersionMask, .end = dot_end};
    if (has_v && boundary_after(str, pos)) // R2: v\d+
        return NumericClaim{.mask = kVersionMask, .end = pos};
    if (!has_v && (pos - digits_start) >= kMinMaskedDigits && boundary_after(str, pos)) // R3: \d{2,}
        return NumericClaim{.mask = kDigitMask, .end = pos};
    return std::nullopt;
}
} // namespace

std::string canonicalize_intent(std::string_view name)
{
    // Trim (the marker payload is extracted verbatim after the banner prefix; G1 strips).
    while (!name.empty() && (name.front() == ' ' || name.front() == '\t'))
        name.remove_prefix(1);
    while (!name.empty() && (name.back() == ' ' || name.back() == '\t'))
        name.remove_suffix(1);

    std::string out;
    out.reserve(name.size());
    std::size_t i{0};
    bool prev_is_word{false}; // start-of-string is a boundary
    while (i < name.size())
    {
        const char chr{name[i]};

        // R4: a paren group → (M). Non-greedy: to the FIRST ')'. Unbalanced '(' stays literal.
        if (chr == '(')
        {
            const std::size_t close{name.find(')', i + 1)};
            if (close != std::string_view::npos)
            {
                out.append(kParenMask);
                i = close + 1;
                prev_is_word = false; // ')' is non-word
                continue;
            }
        }

        // R1/R2/R3 fire only at a leading word boundary, on a 'v' or digit anchor.
        if (!prev_is_word && (chr == 'v' || is_digit(chr)))
        {
            if (const std::optional<NumericClaim> claim{claim_numeric(name, i)}; claim)
            {
                out.append(claim->mask);
                i = claim->end;
                prev_is_word = true; // last emitted char ('X'/'N') is a word char
                continue;
            }
        }

        out.push_back(chr);
        prev_is_word = is_word(chr);
        ++i;
    }
    return out;
}

TemplateId intent_id_of(std::string_view name)
{
    return template_id_of(canonicalize_intent(name));
}

} // namespace insight
