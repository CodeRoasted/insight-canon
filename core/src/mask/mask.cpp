module insight.canon.detail.mask;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// invariant: the sole identity source - a pure function of a line's own whitespace-delimited
// tokens, each classified by its OWN class.
// refs: ADR-6.D8, ADR-16.D5, SRC-D-TID-1, SRC-D-TID-2, SRC-D-TID-3
// refs: SRC-D-TID-11, SRC-D-TID-12, SRC-D-TID-13, SRC-D-TID-14
// note: the composite-normalizer contracts are declared beside the exported masker they govern.
namespace insight::tokenization
{

namespace
{

    constexpr std::string_view kWildcard{"<*>"};

    constexpr unsigned kDecimalBase{10U};
    constexpr unsigned kAsciiCaseMask{32U};
    constexpr unsigned kHexLetterCount{6U};

    // invariant: pure, byte-only classifiers over a single token - zero regular-expression engine.
    // refs: ADR-16.D5
    [[nodiscard]] constexpr bool is_hex_char(char chr) noexcept
    {
        return (static_cast<unsigned>(chr) - '0' < kDecimalBase) ||
               (static_cast<unsigned>(static_cast<unsigned char>(chr) | kAsciiCaseMask) - 'a' <
                kHexLetterCount);
    }

    // post: consumes one to three decimal digits at `pos`; false when no digit is there.
    [[nodiscard]] constexpr bool consume_ipv4_octet(std::string_view str, std::size_t& pos) noexcept
    {
        if (pos >= str.size() || static_cast<unsigned>(str[pos]) - '0' >= kDecimalBase)
            return false;
        ++pos;
        if (pos < str.size() && static_cast<unsigned>(str[pos]) - '0' < kDecimalBase)
            ++pos;
        if (pos < str.size() && static_cast<unsigned>(str[pos]) - '0' < kDecimalBase)
            ++pos;
        return true;
    }

    // invariant: disjoint from the wrapper-pair closers - together the two sets are what the
    // address rule tolerates after the address.
    [[nodiscard]] constexpr bool is_trailing_punct(char chr) noexcept
    {
        return chr == ',' || chr == ';' || chr == ':' || chr == '.';
    }

    // invariant: held at 2 so a repair widens WHICH bytes are tolerated, never HOW MANY - a longer
    // punctuation run is a different token, not a wrapped address.
    constexpr std::size_t kMaxIpv4TrailBytes{2};

    // invariant: a STRICT SUPERSET of the retired grammar - every string that one accepted, this
    // one accepts, so no token that masked before can stop masking.
    [[nodiscard]] constexpr bool is_ipv4_token(std::string_view str) noexcept
    {
        if (str.empty())
            return false;
        std::size_t pos{0};
        if (is_wrapper_open(str[0]))
            ++pos;
        for (int oct{0}; oct < 4; ++oct)
        {
            if (!consume_ipv4_octet(str, pos))
                return false;
            if (oct < 3)
            {
                if (pos >= str.size() || str[pos] != '.')
                    return false;
                ++pos;
            }
        }
        if (pos < str.size() && str[pos] == ':')
        {
            ++pos;
            while (pos < str.size() && static_cast<unsigned>(str[pos]) - '0' < kDecimalBase)
                ++pos;
        }
        for (std::size_t taken{0}; taken < kMaxIpv4TrailBytes && pos < str.size() &&
                                   (is_wrapper_close(str[pos]) || is_trailing_punct(str[pos]));
             ++taken)
            ++pos;
        return pos == str.size();
    }

    [[nodiscard]] inline bool is_all_digits(std::string_view str) noexcept
    {
        if (str.empty())
            return false;
        return std::ranges::all_of(str, [](const char chr)
                                   { return static_cast<unsigned>(chr) - '0' < kDecimalBase; });
    }

    // invariant: an integer is KEPT only when it follows a status keyword AND is short, so an exit
    // code or an HTTP status stays distinct while a bare count stays masked.
    // refs: SRC-D-TID-14
    constexpr std::size_t kMaxStatusDigits{3};

    [[nodiscard]] inline bool equals_ascii_lower(std::string_view tok,
                                                 std::string_view lower) noexcept
    {
        if (tok.size() != lower.size())
            return false;
        for (std::size_t pos{0}; pos < tok.size(); ++pos)
        {
            char chr{tok[pos]};
            if (chr >= 'A' && chr <= 'Z')
                chr = static_cast<char>(chr - 'A' + 'a');
            if (chr != lower[pos])
                return false;
        }
        return true;
    }

    // invariant: a TABLE and not a chain, because a chain cannot be enumerated and a fifth keyword
    // would join the rule set unwitnessed.
    inline constexpr std::array<std::string_view, 4> kStatusKeywords{
        {std::string_view{"code"}, std::string_view{"status"}, std::string_view{"exit"},
         std::string_view{"signal"}}};

    [[nodiscard]] inline bool is_status_keyword(std::string_view tok) noexcept
    {
        return std::ranges::any_of(kStatusKeywords, [tok](const std::string_view keyword)
                                   { return equals_ascii_lower(tok, keyword); });
    }

    // invariant: the ROOT is the decidable thing - no length or alphabet rule separates an
    // ephemeral path component from content.
    // invariant: ONE catalog, consulted as a per-segment predicate from every call site, so adding
    // a root extends them all with no second edit.
    // note: masking only and no semantics, so it is canon CORE and never a dialect package.
    // refs: ADR-16.D2, ADR-17.D4, SRC-D-MSK-4
    enum class RootAnchor : std::uint8_t
    {
        // note: the root's first component is the first component after a leading separator.
        TokenStart,
        // note: the root matches at ANY component boundary - a mid-path root.
        Floating,
    };
    enum class RootScope : std::uint8_t
    {
        // note: everything under the root is ephemeral - a namespace of ephemeral trees.
        Subtree,
        // note: exactly the ONE component under the root is ephemeral; the tail resumes normally.
        Instance,
    };

    struct EphemeralRoot
    {
        std::span<const std::string_view> segments;
        RootAnchor anchor;
        RootScope scope;
    };

    // invariant: anchor and scope are EXPLICIT, never inferred - a mis-spelled root that silently
    // floated would over-mask, and over-masking destroys signal irrecoverably.
    // refs: ADR-9, ADR-16.D2
    inline constexpr std::array<std::string_view, 1> kRootTmp{{std::string_view{"tmp"}}};
    inline constexpr std::array<std::string_view, 2> kRootVarTmp{
        {std::string_view{"var"}, std::string_view{"tmp"}}};
    inline constexpr std::array<std::string_view, 2> kRootVarFolders{
        {std::string_view{"var"}, std::string_view{"folders"}}};
    inline constexpr std::array<std::string_view, 3> kRootConan{
        {std::string_view{".conan2"}, std::string_view{"p"}, std::string_view{"b"}}};
    inline constexpr std::array<std::string_view, 2> kRootNixStore{
        {std::string_view{"nix"}, std::string_view{"store"}}};

    // invariant: a root whose component is the build CONFIGURATION is deliberately absent - it is
    // stable per config, carries no hash, and masking it would destroy signal to fix nothing.
    inline constexpr std::array<EphemeralRoot, 5> kEphemeralRoots{{
        {.segments = kRootTmp, .anchor = RootAnchor::TokenStart, .scope = RootScope::Subtree},
        {.segments = kRootVarTmp, .anchor = RootAnchor::TokenStart, .scope = RootScope::Subtree},
        {.segments = kRootVarFolders,
         .anchor = RootAnchor::TokenStart,
         .scope = RootScope::Subtree},
        {.segments = kRootConan, .anchor = RootAnchor::Floating, .scope = RootScope::Instance},
        {.segments = kRootNixStore, .anchor = RootAnchor::TokenStart, .scope = RootScope::Instance},
    }};

    // invariant: the longest declared root, so this is the look-back window the matcher needs.
    inline constexpr std::size_t kMaxRootSegments{3U};

    // post: one component of a segment walk - its core text, the separator byte immediately before
    // it, and whether it is the first component after a leading separator.
    struct PathComponent
    {
        std::string_view text;
        char sep_before{'\0'};
        bool at_token_start{false};
    };

    // pre: `window` ends at the CURRENT component and holds at most kMaxRootSegments of them.
    // post: the scope of the longest declared root ENDING at the current component, else nullopt -
    // longest wins, so the answer is order-independent.
    // refs: SRC-D-MSK-4
    [[nodiscard]] inline std::optional<RootScope>
    root_scope_ending_at(std::span<const PathComponent> window) noexcept
    {
        if (window.empty())
            return std::nullopt;
        std::optional<RootScope> best{};
        std::size_t best_len{0};
        for (const EphemeralRoot& root : kEphemeralRoots)
        {
            const std::size_t klen{root.segments.size()};
            // assert: the current component must be the root's LAST segment, which rejects almost
            // every component in about one length compare.
            if (klen > window.size() || klen <= best_len ||
                window.back().text != root.segments[klen - 1U])
                continue;
            const std::size_t first{window.size() - klen};
            bool matched{true};
            for (std::size_t seg{0}; seg + 1U < klen; ++seg)
                if (window[first + seg].text != root.segments[seg])
                {
                    matched = false;
                    break;
                }
            if (!matched)
                continue;
            // assert: consecutive AND separator-joined - every separator from the root's FIRST
            // component onward must be the path separator, so a colon coincidence never matches.
            // refs: ADR-16.D2
            for (std::size_t idx{first}; idx < window.size(); ++idx)
                if (window[idx].sep_before != '/')
                {
                    matched = false;
                    break;
                }
            if (!matched)
                continue;
            if (root.anchor == RootAnchor::TokenStart && !window[first].at_token_start)
                continue;
            best = root.scope;
            best_len = klen;
        }
        return best;
    }

    // post: true, with `out` filled, only when at least one segment masked AND a letter-leading
    // anchor exists; false otherwise, leaving the dispatch to fall through.
    // invariant: pure, byte-only and single-token, so the normal form is bit-identical across
    // standard libraries.
    // refs: SRC-D-MSK-1, SRC-D-MSK-4
    // note: one pass does the walk, the classification and the root masking; a split fragments it.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    [[nodiscard]] inline bool normalize_diagnostic_composite(std::string_view tok, std::string& out)
    {
        // assert: the trigger is a `:` immediately followed by a digit, which subsumes
        // source-location.
        bool has_colon_digit{false};
        for (std::size_t pos{0}; pos + 1 < tok.size(); ++pos)
            if (tok[pos] == ':' && is_digit(tok[pos + 1]))
            {
                has_colon_digit = true;
                break;
            }
        if (!has_colon_digit)
            return false;

        out.clear();
        bool masked{false};
        bool has_letter_anchor{false};
        std::string_view prev_core{};
        std::size_t seg_start{0};
        // assert: the same catalog as the standalone rule; the component after a declared root is a
        // per-run instance and masks whatever its leading byte, overriding the letter-leading KEEP.
        // assert: scope is CLAMPED to Instance here, so a file-and-line tail is never masked.
        // refs: SRC-D-MSK-4
        bool mask_next{false};
        std::array<PathComponent, kMaxRootSegments> window{};
        std::size_t window_len{0};
        const auto push_component{[&](std::string_view text, char sep_before, bool at_token_start)
                                  {
                                      const PathComponent comp{.text = text,
                                                               .sep_before = sep_before,
                                                               .at_token_start = at_token_start};
                                      if (window_len < kMaxRootSegments)
                                      {
                                          window[window_len++] = comp;
                                          return;
                                      }
                                      for (std::size_t idx{1}; idx < kMaxRootSegments; ++idx)
                                          window[idx - 1U] = window[idx];
                                      window[kMaxRootSegments - 1U] = comp;
                                  }};
        const auto root_ends_here{[&]
                                  {
                                      return root_scope_ending_at(std::span<const PathComponent>{
                                                                      window.data(), window_len})
                                          .has_value();
                                  }};
        const auto flush_segment{
            [&](std::size_t end)
            {
                const std::string_view seg{tok.substr(seg_start, end - seg_start)};
                std::size_t lead{0};
                while (lead < seg.size() && !is_digit(seg[lead]) && !is_alpha(seg[lead]))
                    ++lead;
                std::size_t trail{seg.size()};
                while (trail > lead && !is_digit(seg[trail - 1]) && !is_alpha(seg[trail - 1]))
                    --trail;
                const std::string_view core{seg.substr(lead, trail - lead)};
                const char sep_before{seg_start == 0 ? '\0' : tok[seg_start - 1U]};
                const bool at_token_start{seg_start == 1U && !tok.empty() && tok.front() == '/'};

                if (core.empty())
                {
                    out.append(seg);
                    prev_core = {};
                    // assert: an empty or punctuation-only component is never a root segment - it
                    // neither ends a root nor consumes the pending instance.
                    return;
                }

                if (mask_next)
                {
                    // assert: this component is the per-run instance directly under a declared
                    // root.
                    out.append(seg.substr(0, lead));
                    out.append(kWildcard);
                    out.append(seg.substr(trail));
                    masked = true;
                    prev_core = core;
                    push_component(seg, sep_before, at_token_start);
                    mask_next = root_ends_here();
                    return;
                }

                if (is_alpha(core.front()))
                {
                    out.append(seg);
                    has_letter_anchor = true;
                }
                else if (is_status_keyword(prev_core) && is_all_digits(core) &&
                         core.size() <= kMaxStatusDigits)
                {
                    out.append(seg);
                }
                else
                {
                    out.append(seg.substr(0, lead));
                    out.append(kWildcard);
                    out.append(seg.substr(trail));
                    masked = true;
                }
                prev_core = core;
                push_component(seg, sep_before, at_token_start);
                mask_next = root_ends_here();
            }};
        for (std::size_t pos{0}; pos < tok.size(); ++pos)
            if (tok[pos] == ':' || tok[pos] == '/')
            {
                flush_segment(pos);
                out.push_back(tok[pos]);
                seg_start = pos + 1;
            }
        flush_segment(tok.size());
        return masked && has_letter_anchor;
    }

    // pre: reached only for a token the diagnostic composite did not claim.
    // post: honours the DECLARED scope - a subtree root collapses the whole remainder, an instance
    // root masks the one component under it and KEEPS the tail.
    // invariant: segment-anchored rather than prefix-matched, which is what lets a mid-path
    // floating root match; it is not a general absolute-path masker.
    // refs: SRC-D-MSK-2, SRC-D-MSK-4
    [[nodiscard]] inline bool normalize_ephemeral_root(std::string_view tok, std::string& out)
    {
        std::array<PathComponent, kMaxRootSegments> window{};
        std::size_t window_len{0};
        const auto push{[&](const PathComponent& comp)
                        {
                            if (window_len < kMaxRootSegments)
                            {
                                window[window_len++] = comp;
                                return;
                            }
                            for (std::size_t idx{1}; idx < kMaxRootSegments; ++idx)
                                window[idx - 1U] = window[idx];
                            window[kMaxRootSegments - 1U] = comp;
                        }};

        std::size_t comp_start{0};
        std::size_t root_end{std::string_view::npos};
        RootScope scope{RootScope::Subtree};
        for (std::size_t pos{0}; pos <= tok.size(); ++pos)
            if (pos == tok.size() || tok[pos] == '/')
            {
                const std::string_view comp{tok.substr(comp_start, pos - comp_start)};
                const char sep_before{comp_start == 0 ? '\0' : tok[comp_start - 1U]};
                const bool at_token_start{comp_start == 1U && !tok.empty() && tok.front() == '/'};
                push(PathComponent{
                    .text = comp, .sep_before = sep_before, .at_token_start = at_token_start});
                if (const std::optional<RootScope> hit{root_scope_ending_at(
                        std::span<const PathComponent>{window.data(), window_len})};
                    hit.has_value())
                {
                    root_end = pos;
                    scope = *hit;
                    break;
                }
                comp_start = pos + 1U;
            }

        if (root_end == std::string_view::npos)
            return false;

        // assert: a non-empty instance component directly under the root is required, so a bare
        // root and a doubled separator are not collapsed.
        if (root_end >= tok.size() || tok[root_end] != '/')
            return false;
        const std::size_t inst_start{root_end + 1U};
        if (inst_start >= tok.size() || tok[inst_start] == '/')
            return false;

        out.clear();
        out.append(tok.substr(0, root_end));
        out.push_back('/');
        out.append(kWildcard);
        if (scope == RootScope::Instance)
        {
            std::size_t inst_end{inst_start};
            while (inst_end < tok.size() && tok[inst_end] != '/')
                ++inst_end;
            out.append(tok.substr(inst_end));
        }
        return true;
    }

    // post: keeps the name and masks the numeric version, so a version bump is not a new template.
    // pre: a separator whose suffix is a numeric version run, then punctuation only - an alphabetic
    // suffix is a path segment and is declined.
    // refs: SRC-D-TID-12
    [[nodiscard]] inline bool normalize_versioned_ref(std::string_view tok, std::string& out)
    {
        const std::size_t slash{tok.rfind('/')};
        if (slash == std::string_view::npos || slash + 1 >= tok.size())
            return false;
        if (!is_digit(tok[slash + 1]))
            return false;

        std::size_t cursor{slash + 1};
        bool saw_digit{false};
        while (cursor < tok.size() && (is_digit(tok[cursor]) || tok[cursor] == '.'))
        {
            saw_digit = saw_digit || is_digit(tok[cursor]);
            ++cursor;
        }
        if (!saw_digit)
            return false;
        for (std::size_t pos{cursor}; pos < tok.size(); ++pos)
        {
            const char chr{tok[pos]};
            if (is_digit(chr) || (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z'))
                return false;
        }

        out.clear();
        out.append(tok.substr(0, slash + 1));
        out.append("<*>");
        out.append(tok.substr(cursor));
        return true;
    }

    // post: the whole token is `[`, one COMPLETE RFC3339 full datetime, `]`, and it masks to a
    // bracketed wildcard; every other interior and any trailing punctuation is declined.
    // invariant: the byte grammar has ONE owner and is never spelled twice here.
    // refs: SRC-D-MSK-5, ADR-23.D1
    // note: the output-class collision with the bracketed-index normal form is named and accepted.
    [[nodiscard]] inline bool normalize_bracket_timestamp(std::string_view tok, std::string& out)
    {
        if (tok.size() < 3U || tok.front() != '[' || tok.back() != ']')
            return false;
        const std::size_t interior{insight::utils::rfc3339_datetime_length(tok, 1U)};
        if (interior == 0 || 1U + interior + 1U != tok.size())
            return false;
        out.clear();
        out.push_back('[');
        out.append(kWildcard);
        out.push_back(']');
        return true;
    }

    // post: normalizes the bracketed digit run and KEEPS a short alphabetic class prefix inside the
    // bracket - keep the stable class marker, mask the varying index.
    // refs: SRC-D-TID-13b
    [[nodiscard]] inline bool normalize_bracket_index(std::string_view tok, std::string& out)
    {
        const std::size_t open{tok.find('[')};
        if (open == std::string_view::npos || open + 1 >= tok.size())
            return false;
        std::size_t cursor{open + 1};
        const std::size_t prefix_begin{cursor};
        while (cursor < tok.size() && is_alpha(tok[cursor]))
            ++cursor;
        const std::string_view prefix{tok.substr(prefix_begin, cursor - prefix_begin)};
        bool saw_digit{false};
        while (cursor < tok.size() && is_digit(tok[cursor]))
        {
            saw_digit = true;
            ++cursor;
        }
        if (!saw_digit || cursor >= tok.size() || tok[cursor] != ']')
            return false;

        out.clear();
        out.append(tok.substr(0, open + 1));
        out.append(prefix);
        out.append("<*>");
        out.append(tok.substr(cursor));
        return true;
    }

    // invariant: per-line masking is the SOLE generalizer, so these classify the high-cardinality
    // SYNTACTIC token classes the fixed masks miss.
    // post: true for a digit-leading token after an optional sign - a number, measurement, version
    // or timestamp, and intrinsically high-cardinality.
    // invariant: subsumes the all-digit mask and every separator, decimal, unit-suffixed and
    // versioned numeric in ONE rule, with no unit lexicon.
    // refs: ADR-16.D5, SRC-D-TID-12, SRC-D-TID-13, SRC-D-TID-14
    [[nodiscard]] inline bool is_digit_leading(std::string_view tok) noexcept
    {
        std::size_t pos{0};
        if (pos < tok.size() && (tok[pos] == '+' || tok[pos] == '-'))
            ++pos;
        return pos < tok.size() && is_digit(tok[pos]);
    }

    // invariant: ONE declaration of the hex-run floor, read by the standalone check and by the
    // embedded-identity scanner, so the two maskers cannot disagree about the same token.
    constexpr std::size_t kMinHashLen{16};

    // post: true for a standalone UUID or a hex-only run at or above the floor.
    // invariant: the floor is what keeps a short hex-looking word literal.
    // refs: SRC-D-TID-12
    [[nodiscard]] inline bool is_uuid_or_long_hash(std::string_view tok) noexcept
    {
        constexpr std::size_t kUuidLen{36};
        static constexpr std::size_t kUuidDash1{8}, kUuidDash2{13}, kUuidDash3{18}, kUuidDash4{23};
        if (tok.size() == kUuidLen && tok[kUuidDash1] == '-' && tok[kUuidDash2] == '-' &&
            tok[kUuidDash3] == '-' && tok[kUuidDash4] == '-')
        {
            for (std::size_t pos{0}; pos < kUuidLen; ++pos)
                if (pos != kUuidDash1 && pos != kUuidDash2 && pos != kUuidDash3 &&
                    pos != kUuidDash4 && !is_hex_char(tok[pos]))
                    return false;
            return true;
        }
        if (tok.size() >= kMinHashLen)
            return std::ranges::all_of(tok, [](char chr) { return is_hex_char(chr); });
        return false;
    }

    // post: keeps the counter marker and masks the index; the digit run must reach end or
    // punctuation, so a marker followed by a word is not a counter.
    // refs: SRC-D-TID-13
    [[nodiscard]] inline bool normalize_hash_counter(std::string_view tok, std::string& out)
    {
        if (tok.size() < 2U || tok[0] != '#' || !is_digit(tok[1]))
            return false;
        std::size_t cursor{1};
        while (cursor < tok.size() && is_digit(tok[cursor]))
            ++cursor;
        for (std::size_t pos{cursor}; pos < tok.size(); ++pos)
            if (is_digit(tok[pos]) || is_alpha(tok[pos]))
                return false;
        out.clear();
        out.append("#<*>");
        out.append(tok.substr(cursor));
        return true;
    }

    // invariant: FROZEN, DECLARED byte sequences - byte-exact, with no Unicode property lookup,
    // which is what keeps the decision identical across standard libraries.
    // invariant: adding a marker here extends BOTH touch points, so there is one source of truth.
    // refs: SRC-D-TID-9, SRC-D-TID-22
    inline constexpr std::array<std::string_view, 1> kCurrencyMarkers{std::string_view{"$"}};

    // post: the length in BYTES of the declared marker prefixing `tok`, 0 when there is none.
    // pre: at least one byte must follow the marker, so a lone marker is not one.
    [[nodiscard]] inline std::size_t marker_prefix_len(std::string_view tok) noexcept
    {
        for (const std::string_view marker : kCurrencyMarkers)
            if (tok.size() > marker.size() && tok.starts_with(marker))
                return marker.size();
        return 0;
    }

    // post: keeps the marker and masks the amount; the core is digits plus one optional fraction, a
    // trailing alphanumeric rejects, and trailing punctuation is kept.
    // invariant: a DECIDABLE numeric - no low-cardinality keyword has the shape marker-then-digits,
    // so it joins the digit-leading numerics the first-byte test misses on a leading marker.
    // refs: SRC-D-TID-12, SRC-D-TID-22
    [[nodiscard]] inline bool normalize_marker_number(std::string_view tok, std::string& out)
    {
        const std::size_t marker{marker_prefix_len(tok)};
        if (marker == 0 || !is_digit(tok[marker]))
            return false;
        std::size_t cursor{marker + 1};
        while (cursor < tok.size() && is_digit(tok[cursor]))
            ++cursor;
        if (cursor < tok.size() && tok[cursor] == '.')
        {
            const std::size_t frac{cursor + 1};
            cursor = frac;
            while (cursor < tok.size() && is_digit(tok[cursor]))
                ++cursor;
            if (cursor == frac)
                return false;
        }
        for (std::size_t pos{cursor}; pos < tok.size(); ++pos)
            if (is_digit(tok[pos]) || is_alpha(tok[pos]))
                return false;
        out.clear();
        out.append(tok.substr(0, marker));
        out.append(kWildcard);
        out.append(tok.substr(cursor));
        return true;
    }

    // note: hoisted out of their one caller: nested, they carried its complexity past the limit.
    constexpr std::size_t kUuidLen{36};
    constexpr std::array<std::size_t, 4> kUuidDashes{8, 13, 18, 23};

    // invariant: the floor is declared once, above the standalone hex check, and read here.
    // post: true when a UUID starts exactly at `pos`.
    [[nodiscard]] inline bool uuid_at(std::string_view tok, std::size_t pos) noexcept
    {
        if (pos + kUuidLen > tok.size())
            return false;
        for (std::size_t off{0}; off < kUuidLen; ++off)
        {
            const bool is_dash{off == kUuidDashes[0] || off == kUuidDashes[1] ||
                               off == kUuidDashes[2] || off == kUuidDashes[3]};
            if (is_dash ? (tok[pos + off] != '-') : !is_hex_char(tok[pos + off]))
                return false;
        }
        return true;
    }

    // invariant: file-local and PRIVATE on purpose - the shared RFC3339 grammar requires colons in
    // the time, so admitting this colon-free profile would mean WIDENING it.
    // invariant: 18 bytes with no options and no variable-length part; each anchor is load-bearing,
    // and the mandatory terminator is what keeps the 17-byte zoneless form out.
    // note: it earns a public seat the day a second consumer outside this unit needs it.
    // refs: F-SRC-insight-canon:canon.api.cppm:rfc3339_datetime_length
    constexpr std::size_t kInstantLen{18};
    constexpr std::size_t kInstantDash1{4};
    constexpr std::size_t kInstantDash2{7};
    constexpr std::size_t kInstantTimeMark{10};
    constexpr std::size_t kInstantZulu{17};

    // invariant: delimiter-gated on BOTH sides - the byte before and the byte after the match, when
    // each exists, must be non-alphanumeric, so the fixed width is a whole token and not a window.
    [[nodiscard]] inline bool compact_instant_at(std::string_view tok, std::size_t pos) noexcept
    {
        if (pos + kInstantLen > tok.size())
            return false;
        if (pos > 0 && (is_digit(tok[pos - 1U]) || is_alpha(tok[pos - 1U])))
            return false;
        const std::size_t after{pos + kInstantLen};
        if (after < tok.size() && (is_digit(tok[after]) || is_alpha(tok[after])))
            return false;
        if (tok[pos + kInstantDash1] != '-' || tok[pos + kInstantDash2] != '-' ||
            tok[pos + kInstantTimeMark] != 'T' || tok[pos + kInstantZulu] != 'Z')
            return false;
        for (std::size_t off{0}; off < kInstantLen; ++off)
        {
            const bool literal{off == kInstantDash1 || off == kInstantDash2 ||
                               off == kInstantTimeMark || off == kInstantZulu};
            if (!literal && !is_digit(tok[pos + off]))
                return false;
        }
        return true;
    }

    // post: masks a UUID, a long hex run or a compact UTC instant EMBEDDED in a larger token,
    // keeping the surrounding structure.
    // invariant: a CLOSED grammar pinned by literal bytes at fixed offsets, every member of whose
    // acceptance set is an instance value by the encoding's own semantics.
    // refs: SRC-D-TID-12, SRC-D-TID-14
    // invariant: the reasoning does not extend to an embedded long-DIGIT-run arm - a stable name
    // and an ephemeral id are the same shape there, and no parameter separates them.
    [[nodiscard]] inline bool normalize_embedded_identity(std::string_view tok, std::string& out)
    {
        out.clear();
        bool masked{false};
        std::size_t pos{0};
        // assert: the three arms are DISJOINT BY CONSTRUCTION and their order is a COST choice,
        // never a precedence claim - the fixed-length probes run first because they are cheaper.
        // refs: F-SRC-insight-canon:test_stateless_template.cpp:EmbeddedIdentityArmsAreDisjoint
        while (pos < tok.size())
        {
            if (uuid_at(tok, pos))
            {
                out.append(kWildcard);
                pos += kUuidLen;
                masked = true;
                continue;
            }
            if (compact_instant_at(tok, pos))
            {
                out.append(kWildcard);
                pos += kInstantLen;
                masked = true;
                continue;
            }
            if (is_hex_char(tok[pos]) && (pos == 0 || !is_hex_char(tok[pos - 1])))
            {
                std::size_t end{pos};
                while (end < tok.size() && is_hex_char(tok[end]))
                    ++end;
                if (end - pos >= kMinHashLen)
                {
                    out.append(kWildcard);
                    pos = end;
                    masked = true;
                    continue;
                }
            }
            out.push_back(tok[pos]);
            ++pos;
        }
        return masked;
    }

    // post: keeps the key and masks a digit-leading value; a status value and a value WORD are both
    // excluded, so a green-to-red flip stays distinct and a varying word stays literal.
    // refs: SRC-D-TID-5, SRC-D-TID-14, SRC-D-TID-17
    [[nodiscard]] inline bool normalize_kv_value(std::string_view tok, std::string& out)
    {
        const std::size_t eq_pos{tok.find('=')};
        if (eq_pos == 0 || eq_pos == std::string_view::npos || eq_pos + 1 >= tok.size())
            return false;
        const std::string_view key{tok.substr(0, eq_pos)};
        const std::string_view raw_value{tok.substr(eq_pos + 1)};
        // assert: a declared currency marker is stripped off the value before the digit-leading
        // gate, so the key AND the marker are kept while the amount masks.
        // refs: SRC-D-TID-22
        const std::size_t marker{marker_prefix_len(raw_value)};
        const std::string_view value{raw_value.substr(marker)};
        if (!is_digit_leading(value))
            return false;
        // assert: the status-value KEEP in its key-value form, on the same keyword-and-size gate as
        // the space-separated carve-out.
        if (is_status_keyword(key) && is_all_digits(value) && value.size() <= kMaxStatusDigits)
            return false;
        out.clear();
        out.append(key);
        out.push_back('=');
        out.append(raw_value.substr(0, marker));
        out.append(kWildcard);
        return true;
    }

    // invariant: the array ORDER IS THE PRECEDENCE - tried top to bottom, the first rule that
    // claims the token wins.
    // invariant: this catalog DEFINES the composite layer of the generation the canonicalization
    // version names, so adding, reordering or removing a rule REQUIRES a version bump.
    // assert: the catalog's SHAPE is one limb of that obligation and not all of it - widening an
    // existing rule in place leaves this array byte-identical and owes the same bump.
    // refs: SRC-D-TID-12, SRC-D-TID-16
    struct CompositeRule
    {
        std::string_view name;
        bool (*normalize)(std::string_view tok, std::string& out);
    };
    // invariant: the two bracket rules are ADJACENT, most specific first, and non-overlapping
    // today, so future drift between them has a rule to violate loudly.
    // refs: SRC-D-MSK-1, SRC-D-MSK-2, SRC-D-MSK-5
    // refs: SRC-D-TID-12, SRC-D-TID-13, SRC-D-TID-13b, SRC-D-TID-17, SRC-D-TID-22
    constexpr std::array<CompositeRule, 9U> kCompositeRules{{
        {.name = "diagnostic_composite", .normalize = normalize_diagnostic_composite},
        {.name = "ephemeral_root", .normalize = normalize_ephemeral_root},
        {.name = "versioned_ref", .normalize = normalize_versioned_ref},
        {.name = "bracket_timestamp", .normalize = normalize_bracket_timestamp},
        {.name = "bracket_index", .normalize = normalize_bracket_index},
        {.name = "hash_counter", .normalize = normalize_hash_counter},
        {.name = "marker_number", .normalize = normalize_marker_number},
        {.name = "embedded_identity", .normalize = normalize_embedded_identity},
        {.name = "kv_value", .normalize = normalize_kv_value},
    }};

    // invariant: THE composite step, in ONE place - the dispatcher and the coverage gate's
    // discriminator both call it, so the gate cannot answer about a loop the masker no longer runs.
    // pre: `shape` is passed in because the dispatcher already computed it; recomputing here would
    // put a second byte walk of every token back on the hot path.
    // note: the pre-gate is part of the step: a token with no separator never reaches the catalog.
    [[nodiscard]] inline bool try_composite(std::string_view tok, const TokenShape& shape,
                                            std::string& out, std::string_view* claimed_by)
    {
        if (marker_prefix_len(tok) == 0 && !shape.has_separator)
            return false;
        for (const CompositeRule& rule : kCompositeRules)
            if (rule.normalize(tok, out))
            {
                if (claimed_by != nullptr)
                    *claimed_by = rule.name;
                return true;
            }
        return false;
    }

    template <typename Cb>
        requires std::invocable<Cb, std::string_view>
    inline void for_each_token(std::string_view content, Cb callback)
    {
        const char* const base{content.data()};
        std::size_t cursor{0};
        const std::size_t len{content.size()};
        while (cursor < len)
        {
            while (cursor < len && base[cursor] == ' ')
                ++cursor;
            if (cursor >= len)
                break;
            const std::size_t start{cursor};
            // note: memchr scans to the next space with boundaries bit-identical to the byte loop.
            const void* const space{std::memchr(base + cursor, ' ', len - cursor)};
            cursor = space != nullptr
                         ? static_cast<std::size_t>(static_cast<const char*>(space) - base)
                         : len;
            callback(content.substr(start, cursor - start));
        }
    }

} // namespace

// post: the joined per-token canonical forms; a masked position contributes a param, a kept or
// normalized position does not.
// invariant: a function of the content bytes only - no float, no map iteration, no state - so it is
// bit-identical across standard libraries and independent of order and stream.
// refs: ADR-16.D5, SRC-D-TID-1, SRC-D-TID-2, SRC-D-TID-9
StatelessTemplate stateless_template(std::string_view content, ArenaAllocator& out_arena,
                                     const MaskConfig& config)
{
    std::string tmpl;
    tmpl.reserve(content.size() + kWildcard.size());
    std::vector<std::string_view> params;
    std::string composite;
    std::string_view prev{};
    bool first{true};

    // assert: the declared per-token classification in TOTAL precedence - the KEEP carve-outs win
    // first, then the masks.
    // refs: SRC-D-TID-12
    for_each_token(content,
                   [&](std::string_view tok)
                   {
                       if (!first)
                           tmpl.push_back(' ');
                       first = false;
                       // note: one byte pass yields the shape facts, replacing three scans.
                       const TokenShape shape{tok};
                       const auto mask{[&]
                                       {
                                           tmpl.append(kWildcard);
                                           params.push_back(tok);
                                       }};

                       // assert: the status-value KEEP, so an exit code stays distinct from its
                       // neighbour.
                       if (shape.all_digits && tok.size() <= kMaxStatusDigits &&
                           is_status_keyword(prev))
                       {
                           tmpl.append(tok);
                           prev = tok;
                           return;
                       }
                       // assert: the declared rule set AND its precedence are the catalog, tried in
                       // array order with the first claim winning.
                       if (try_composite(tok, shape, composite, nullptr))
                       {
                           tmpl.append(composite);
                           prev = tok;
                           return;
                       }
                       // assert: a hexadecimal-prefixed token needs no arm: it starts with a digit,
                       // so the digit-leading test carries it.
                       // refs: DN-27
                       if (shape.empty || is_uuid_or_long_hash(tok) ||
                           (config.mask_ip_addresses && is_ipv4_token(tok)) || shape.digit_leading)
                       {
                           mask();
                           prev = tok;
                           return;
                       }
                       tmpl.append(tok);
                       prev = tok;
                   });

    const std::string_view tmpl_view{out_arena.store_string(tmpl)};
    std::span<const std::string_view> params_span{};
    if (!params.empty())
    {
        auto* buf{static_cast<std::string_view*>(out_arena.allocate(
            params.size() * sizeof(std::string_view), alignof(std::string_view)))};
        const auto buf_span{std::span<std::string_view>{buf, params.size()}};
        std::ranges::copy(params, buf_span.begin());
        params_span = buf_span;
    }
    return {.template_str = tmpl_view, .params = params_span};
}

// invariant: every table here is DERIVED from the catalog the masker itself reads, never restated
// beside it - a parallel list is how a coverage gate greens against a moved rule set.
namespace rule_catalog
{

    std::span<const std::string_view> composite_rule_ids() noexcept
    {
        static constexpr auto kIds{[]
                                   {
                                       std::array<std::string_view, kCompositeRules.size()> ids{};
                                       for (std::size_t pos{0}; pos < kCompositeRules.size(); ++pos)
                                           ids[pos] = kCompositeRules[pos].name;
                                       return ids;
                                   }()};
        return kIds;
    }

    std::string_view composite_rule_claiming(std::string_view token)
    {
        std::string scratch;
        std::string_view claimed{};
        const TokenShape shape{token};
        // note: the return value is dropped: `claimed` stays empty exactly when the step declines.
        (void)try_composite(token, shape, scratch, &claimed);
        return claimed;
    }

    std::span<const std::string_view> status_keywords() noexcept
    {
        return kStatusKeywords;
    }

    std::span<const std::string_view> currency_markers() noexcept
    {
        return kCurrencyMarkers;
    }

    std::span<const std::span<const std::string_view>> ephemeral_root_segments() noexcept
    {
        static constexpr auto kSegments{
            []
            {
                std::array<std::span<const std::string_view>, kEphemeralRoots.size()> segs{};
                for (std::size_t pos{0}; pos < kEphemeralRoots.size(); ++pos)
                    segs[pos] = kEphemeralRoots[pos].segments;
                return segs;
            }()};
        return kSegments;
    }

    std::size_t min_hash_length() noexcept
    {
        return kMinHashLen;
    }

} // namespace rule_catalog

} // namespace insight::tokenization
