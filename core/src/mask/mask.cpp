module insight.canon.detail.mask;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// invariant: the sole identity source - a pure function of a line's own whitespace-delimited
// tokens, each classified by its OWN class.
// refs: ADR-6.D8, ADR-16.D5, F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate
// refs: F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate
// refs: F-SRC-insight-canon:canon.api.cppm:normalize
// refs: F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate
// refs: F-SRC-insight-canon:mask.cpp:normalize_hash_counter, ADR-16.D5
// note: the composite-normalizer contracts are declared beside the exported masker they govern.
namespace insight::tokenization
{

namespace
{

    constexpr std::string_view kWildcard{kMaskWildcard};

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
        for (std::size_t taken{0};
             taken < kMaxShellTrailBytes && pos < str.size() &&
             (is_wrapper_close(str[pos]) || is_shell_trailing_punct(str[pos]));
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
    // refs: ADR-16.D5
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

    // refs: DN-134.D1, ADR-16.D5, F-SRC-insight-canon:canon.api.cppm:complete_shell_core
    // post: true for a complete shell whose core is digit-led, false for a short status value
    // behind a status keyword, as rule 1 keeps the bare form.
    // invariant: rule 5 read through the shell, as rule 4 reads an address - its acceptance set is
    // the set rule 5 masks bare, so it merges nothing canon does not merge without the shell.
    // pre: reached only for a token no composite claimed and rules 3-5 did not mask.
    [[nodiscard]] inline bool is_shelled_numeric(std::string_view tok,
                                                 std::string_view prev) noexcept
    {
        const std::optional<std::string_view> core{complete_shell_core(tok)};
        if (!core.has_value() || !is_digit(core->front()))
            return false;
        return !is_status_keyword(prev) || !is_all_digits(*core) || core->size() > kMaxStatusDigits;
    }

    // invariant: the ROOT is the decidable thing - no length or alphabet rule separates an
    // ephemeral path component from content.
    // invariant: ONE catalog, consulted as a per-segment predicate from every call site, so adding
    // a root extends them all with no second edit.
    // note: masking only and no semantics, so it is canon CORE and never a dialect package.
    // refs: ADR-16.D2, ADR-17.D4
    enum class RootAnchor : std::uint8_t
    {
        // note: the root's first component is the path's first component, behind a declared lead.
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
    // note: the macOS real paths of `/var/folders` and `/tmp`.
    // refs: DN-136.D4
    inline constexpr std::array<std::string_view, 3> kRootPrivateVarFolders{
        {std::string_view{"private"}, std::string_view{"var"}, std::string_view{"folders"}}};
    inline constexpr std::array<std::string_view, 2> kRootPrivateTmp{
        {std::string_view{"private"}, std::string_view{"tmp"}}};
    // invariant: the run names the Windows per-user temp folder and nothing else; the drive and
    // user components before it vary by machine.
    // refs: DN-136.D4
    inline constexpr std::array<std::string_view, 3> kRootWindowsUserTemp{
        {std::string_view{"AppData"}, std::string_view{"Local"}, std::string_view{"Temp"}}};

    // invariant: a root whose component is the build CONFIGURATION is deliberately absent - it is
    // stable per config, carries no hash, and masking it would destroy signal to fix nothing.
    inline constexpr std::array<EphemeralRoot, 8> kEphemeralRoots{{
        {.segments = kRootTmp, .anchor = RootAnchor::TokenStart, .scope = RootScope::Subtree},
        {.segments = kRootVarTmp, .anchor = RootAnchor::TokenStart, .scope = RootScope::Subtree},
        {.segments = kRootVarFolders,
         .anchor = RootAnchor::TokenStart,
         .scope = RootScope::Subtree},
        {.segments = kRootConan, .anchor = RootAnchor::Floating, .scope = RootScope::Instance},
        {.segments = kRootNixStore, .anchor = RootAnchor::TokenStart, .scope = RootScope::Instance},
        {.segments = kRootPrivateVarFolders,
         .anchor = RootAnchor::TokenStart,
         .scope = RootScope::Subtree},
        {.segments = kRootPrivateTmp,
         .anchor = RootAnchor::TokenStart,
         .scope = RootScope::Subtree},
        {.segments = kRootWindowsUserTemp,
         .anchor = RootAnchor::Floating,
         .scope = RootScope::Subtree},
    }};

    // invariant: the longest declared root, so this is the look-back window the matcher needs.
    inline constexpr std::size_t kMaxRootSegments{3U};
    static_assert(std::ranges::all_of(kEphemeralRoots, [](const EphemeralRoot& root)
                                      { return root.segments.size() <= kMaxRootSegments; }),
                  "a declared ephemeral root is longer than the matcher's look-back window");

    // refs: DN-136.D4, ADR-16.D2
    // post: true for a byte that separates path components for the root predicate - a run of `\`
    // is one separator, as JSON escaping doubles it.
    [[nodiscard]] constexpr bool is_root_separator(char chr) noexcept
    {
        return chr == '/' || chr == '\\';
    }

    // post: the offset just past the separator that starts at `pos` - one `/`, or a whole run of
    // `\`.
    // pre: `tok[pos]` is a root separator.
    [[nodiscard]] constexpr std::size_t skip_root_separator(std::string_view tok,
                                                            std::size_t pos) noexcept
    {
        if (tok[pos] == '/')
            return pos + 1U;
        while (pos < tok.size() && tok[pos] == '\\')
            ++pos;
        return pos;
    }

    // post: true for a byte of a lead's `<key>` - `[A-Za-z0-9_.-]`.
    [[nodiscard]] constexpr bool is_lead_key_byte(char chr) noexcept
    {
        return is_alpha(chr) || is_digit(chr) || chr == '_' || chr == '.' || chr == '-';
    }

    // note: a file URL with an empty authority, whose path is the absolute path after it.
    inline constexpr std::string_view kFileUrlScheme{"file://"};

    // refs: DN-136.D4, F-SRC-insight-canon:canon.detail.scan.cppm:kWrapperPairs
    // post: the offset of the PATH's first component - past the separator that opens the path at
    // byte 0 or right after a declared lead - or npos when the token opens no path there.
    // invariant: the lead grammar is closed - wrapper openers, then optionally `<key>=` and more
    // openers, then optionally `file://`; a lead moves WHERE a root sits, never what it matches.
    [[nodiscard]] constexpr std::size_t path_first_component(std::string_view tok) noexcept
    {
        std::size_t pos{0};
        while (pos < tok.size() && is_wrapper_open(tok[pos]))
            ++pos;
        std::size_t key_end{pos};
        while (key_end < tok.size() && is_lead_key_byte(tok[key_end]))
            ++key_end;
        if (key_end > pos && key_end < tok.size() && tok[key_end] == '=')
        {
            pos = key_end + 1U;
            while (pos < tok.size() && is_wrapper_open(tok[pos]))
                ++pos;
        }
        if (tok.substr(pos).starts_with(kFileUrlScheme))
            pos += kFileUrlScheme.size();
        if (pos >= tok.size() || !is_root_separator(tok[pos]))
            return std::string_view::npos;
        return skip_root_separator(tok, pos);
    }

    // post: one component of a segment walk - its core text, the separator byte immediately before
    // it, and whether it is the path's first component.
    struct PathComponent
    {
        std::string_view text;
        char sep_before{'\0'};
        bool at_token_start{false};
    };

    // pre: `window` ends at the CURRENT component and holds at most kMaxRootSegments of them.
    // post: the scope of the longest declared root ENDING at the current component, else nullopt -
    // longest wins, so the answer is order-independent.
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
            // component onward must be a path separator, so a colon coincidence never matches.
            // refs: ADR-16.D2, DN-136.D4
            for (std::size_t idx{first}; idx < window.size(); ++idx)
                if (!is_root_separator(window[idx].sep_before))
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
    // refs: LSRC-11, F-SRC-insight-canon:mask.cpp:root_scope_ending_at
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
        // refs: F-SRC-insight-canon:mask.cpp:root_scope_ending_at
        bool mask_next{false};
        const std::size_t first_comp{path_first_component(tok)};
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
        // post: the bounds `[lead, trail)` of the alphanumeric core of `part`, empty when it has
        // none.
        const auto core_bounds{
            [](std::string_view part)
            {
                std::size_t lead{0};
                while (lead < part.size() && !is_digit(part[lead]) && !is_alpha(part[lead]))
                    ++lead;
                std::size_t trail{part.size()};
                while (trail > lead && !is_digit(part[trail - 1]) && !is_alpha(part[trail - 1]))
                    --trail;
                return std::pair{lead, trail};
            }};
        // refs: DN-136.D4
        // post: appends a letter-led segment holding `\`, masking only a component directly under
        // a root its `\`-separated components reach; with no such root the bytes are unchanged.
        const auto append_backslash_segment{
            [&](std::string_view seg, std::size_t abs_start)
            {
                std::size_t part_start{0};
                while (true)
                {
                    const std::size_t found{seg.find('\\', part_start)};
                    const std::size_t part_end{found == std::string_view::npos ? seg.size()
                                                                               : found};
                    const std::string_view part{seg.substr(part_start, part_end - part_start)};
                    const auto [lead, trail]{core_bounds(part)};
                    if (lead == trail || !mask_next)
                        out.append(part);
                    else
                    {
                        out.append(part.substr(0, lead));
                        out.append(kWildcard);
                        out.append(part.substr(trail));
                        masked = true;
                    }
                    if (lead != trail)
                    {
                        const std::size_t abs{abs_start + part_start};
                        push_component(part, abs == 0 ? '\0' : tok[abs - 1U], abs == first_comp);
                        mask_next = root_ends_here();
                    }
                    if (part_end == seg.size())
                        return;
                    const std::size_t sep_end{skip_root_separator(seg, part_end)};
                    out.append(seg.substr(part_end, sep_end - part_end));
                    part_start = sep_end;
                }
            }};
        const auto flush_segment{
            [&](std::size_t end)
            {
                const std::string_view seg{tok.substr(seg_start, end - seg_start)};
                const auto [lead, trail]{core_bounds(seg)};
                const std::string_view core{seg.substr(lead, trail - lead)};
                const char sep_before{seg_start == 0 ? '\0' : tok[seg_start - 1U]};
                const bool at_token_start{seg_start == first_comp};

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
                    has_letter_anchor = true;
                    if (seg.contains('\\'))
                    {
                        prev_core = core;
                        append_backslash_segment(seg, seg_start);
                        return;
                    }
                    out.append(seg);
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

    struct RootHit
    {
        std::size_t end;
        RootScope scope;
    };

    // post: where the token's first declared root ends - the offset just past its last component -
    // and that root's scope, else nullopt.
    // refs: F-SRC-insight-canon:mask.cpp:root_scope_ending_at, DN-136.D4
    [[nodiscard]] inline std::optional<RootHit> first_root(std::string_view tok)
    {
        std::array<PathComponent, kMaxRootSegments> window{};
        std::size_t window_len{0};
        const std::size_t first_comp{path_first_component(tok)};
        std::size_t comp_start{0};
        for (std::size_t pos{0}; pos <= tok.size(); ++pos)
        {
            if (pos < tok.size() && !is_root_separator(tok[pos]))
                continue;
            const PathComponent comp{.text = tok.substr(comp_start, pos - comp_start),
                                     .sep_before = comp_start == 0 ? '\0' : tok[comp_start - 1U],
                                     .at_token_start = comp_start == first_comp};
            if (window_len < kMaxRootSegments)
                window[window_len++] = comp;
            else
            {
                for (std::size_t idx{1}; idx < kMaxRootSegments; ++idx)
                    window[idx - 1U] = window[idx];
                window.back() = comp;
            }
            if (const std::optional<RootScope> hit{root_scope_ending_at(
                    std::span<const PathComponent>{window.data(), window_len})};
                hit.has_value())
                return RootHit{.end = pos, .scope = *hit};
            if (pos < tok.size())
                pos = skip_root_separator(tok, pos) - 1U;
            comp_start = pos + 1U;
        }
        return std::nullopt;
    }

    // pre: reached only for a token the diagnostic composite did not claim.
    // post: honours the DECLARED scope - a subtree root collapses the whole remainder, an instance
    // root masks the one component under it and KEEPS the tail.
    // invariant: segment-anchored rather than prefix-matched, which is what lets a mid-path
    // floating root match; it is not a general absolute-path masker.
    // post: the instance keeps the separator before it verbatim, a `/` or a run of `\`.
    // refs: F-SRC-insight-canon:mask.cpp:root_scope_ending_at, DN-136.D4
    [[nodiscard]] inline bool normalize_ephemeral_root(std::string_view tok, std::string& out)
    {
        const std::optional<RootHit> hit{first_root(tok)};
        // assert: a non-empty instance component directly under the root is required, so a bare
        // root and a doubled separator are not collapsed.
        if (!hit.has_value() || hit->end >= tok.size() || !is_root_separator(tok[hit->end]))
            return false;
        const std::size_t inst_start{skip_root_separator(tok, hit->end)};
        if (inst_start >= tok.size() || is_root_separator(tok[inst_start]))
            return false;

        out.clear();
        out.append(tok.substr(0, inst_start));
        out.append(kWildcard);
        if (hit->scope == RootScope::Instance)
        {
            std::size_t inst_end{inst_start};
            while (inst_end < tok.size() && !is_root_separator(tok[inst_end]))
                ++inst_end;
            out.append(tok.substr(inst_end));
        }
        return true;
    }

    // post: keeps the name and masks the numeric version, so a version bump is not a new template.
    // pre: a separator whose suffix is a numeric version run, then punctuation only - an alphabetic
    // suffix is a path segment and is declined.
    // refs: F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate
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
            if (is_digit(chr) || is_alpha(chr))
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
    // refs: LSRC-12, ADR-23.D1
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
    // refs: LSRC-13
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
    // refs: ADR-16.D5, F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate
    // refs: F-SRC-insight-canon:mask.cpp:normalize_hash_counter
    // refs: ADR-16.D5
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
    // refs: F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate
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

    // post: the end of the digit run when `tok` is a bare counter - `#`, a digit run, then no
    // letter or digit - else nullopt, so a marker followed by a word is not a counter.
    [[nodiscard]] inline std::optional<std::size_t>
    bare_hash_counter_end(std::string_view tok) noexcept
    {
        if (tok.size() < 2U || tok[0] != '#' || !is_digit(tok[1]))
            return std::nullopt;
        std::size_t cursor{1};
        while (cursor < tok.size() && is_digit(tok[cursor]))
            ++cursor;
        for (std::size_t pos{cursor}; pos < tok.size(); ++pos)
            if (is_digit(tok[pos]) || is_alpha(tok[pos]))
                return std::nullopt;
        return cursor;
    }

    // refs: DN-136.D1, F-SRC-insight-canon:canon.api.cppm:complete_shell_core
    // post: keeps the counter marker and masks the index, bare or inside a complete wrapper shell
    // whose core is a bare counter - the shell, its closer and the trailing bytes stay verbatim.
    // invariant: the shelled acceptance set is the set the bare counter masks, so it merges
    // nothing canon does not merge when the counter is printed without the punctuation.
    [[nodiscard]] inline bool normalize_hash_counter(std::string_view tok, std::string& out)
    {
        std::size_t core_at{0};
        std::optional<std::size_t> digits_end{bare_hash_counter_end(tok)};
        if (!digits_end.has_value())
        {
            const std::optional<std::string_view> core{complete_shell_core(tok)};
            if (!core.has_value())
                return false;
            digits_end = bare_hash_counter_end(*core);
            if (!digits_end.has_value())
                return false;
            core_at = static_cast<std::size_t>(core->data() - tok.data());
        }
        out.clear();
        out.append(tok.substr(0, core_at));
        out.append("#<*>");
        out.append(tok.substr(core_at + *digits_end));
        return true;
    }

    // invariant: FROZEN, DECLARED byte sequences - byte-exact, with no Unicode property lookup,
    // which is what keeps the decision identical across standard libraries.
    // invariant: adding a marker here extends BOTH touch points, so there is one source of truth.
    // refs: F-SRC-insight-canon:canon.api.cppm:TemplateId
    // refs: F-SRC-insight-canon:mask.cpp:normalize_marker_number
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
    // refs: F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate
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
    // refs: F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate, ADR-16.D5
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

    // invariant: the sanitizer-family process tag - AddressSanitizer, libFuzzer and valgrind open a
    // line with `==<pid>==`, and the pid is a per-process instance by the convention itself.
    constexpr std::string_view kPidTagFence{"=="};

    // post: keeps both fences and masks the pid; the tag must open the token, and what follows it
    // is empty or letter-leading and kept verbatim, so a digit after the closing fence declines.
    // invariant: a DECIDABLE numeric - no low-cardinality keyword has the shape fence, digits,
    // fence at a token's start, so a pid joins the numerics the first-byte test misses.
    // refs: ADR-16.D5
    [[nodiscard]] inline bool normalize_sanitizer_pid(std::string_view tok, std::string& out)
    {
        if (!tok.starts_with(kPidTagFence) || tok.size() <= kPidTagFence.size() ||
            !is_digit(tok[kPidTagFence.size()]))
            return false;
        std::size_t cursor{kPidTagFence.size()};
        while (cursor < tok.size() && is_digit(tok[cursor]))
            ++cursor;
        if (!tok.substr(cursor).starts_with(kPidTagFence))
            return false;
        cursor += kPidTagFence.size();
        if (cursor < tok.size() && !is_alpha(tok[cursor]))
            return false;
        out.clear();
        out.append(kPidTagFence);
        out.append(kWildcard);
        out.append(kPidTagFence);
        out.append(tok.substr(cursor));
        return true;
    }

    // invariant: the segment step's delimiter - `,` was measured and refused as a second one.
    // refs: DN-134.D2
    constexpr char kSegmentDelimiter{';'};

    // post: true for a key opening with a letter or `_`, then letters, digits, `_`, `.` or `-`.
    [[nodiscard]] constexpr bool is_segment_key(std::string_view key) noexcept
    {
        if (key.empty() || (!is_alpha(key.front()) && key.front() != '_'))
            return false;
        return std::ranges::all_of(
            key, [](char chr)
            { return is_alpha(chr) || is_digit(chr) || chr == '_' || chr == '.' || chr == '-'; });
    }

    // post: true for a byte a number's extent runs over: an ASCII letter or digit, `.`, `_`, `+`,
    // `%` or `-`.
    // refs: DN-134.D11
    [[nodiscard]] constexpr bool is_extent_byte(char chr) noexcept
    {
        return is_alpha(chr) || is_digit(chr) || chr == '.' || chr == '_' || chr == '+' ||
               chr == '%' || chr == '-';
    }

    // post: true for a byte the extent crosses only when a digit or a wildcard directly follows it,
    // so a number list, a clock and a ratio stay one value.
    // refs: DN-134.D11
    [[nodiscard]] constexpr bool is_extent_joint(char chr) noexcept
    {
        return chr == ',' || chr == ':' || chr == '/';
    }

    // invariant: the remainder is decided WHOLE - swallowed when every byte of it is a wrapper
    // closer or trailing punctuation.
    // invariant: a CR is content: the line's ending never reaches the masker (DN-134.D13).
    struct ValueExtent
    {
        std::size_t length{0};
        bool swallows_remainder{false};
    };

    // pre: `value` is digit-leading after an optional sign (is_digit_leading).
    // post: the extent's length from the value's first byte, and whether the bytes after it are
    // swallowed into the mask; any other remainder stays literal behind the wildcard.
    // invariant: a wildcard an earlier composite wrote is a number already masked, so the run reads
    // it whole and crosses a joint into it.
    // invariant: one forward pass over fixed ASCII classes and the literal wildcard, so the extent
    // is a pure function of the value's bytes, bit-identical across standard libraries.
    // refs: DN-134.D11, F-SRC-insight-canon:canon.detail.scan.cppm:kWrapperPairs
    [[nodiscard]] constexpr ValueExtent value_extent(std::string_view value) noexcept
    {
        std::string_view remainder{value};
        while (!remainder.empty())
        {
            std::string_view next{remainder};
            next.remove_prefix(1);
            if (remainder.starts_with(kWildcard))
                remainder.remove_prefix(kWildcard.size());
            else if (is_extent_byte(remainder.front()) ||
                     (is_extent_joint(remainder.front()) && !next.empty() &&
                      (is_digit(next.front()) || next.starts_with(kWildcard))))
                remainder = next;
            else
                break;
        }
        return {.length = value.size() - remainder.size(),
                .swallows_remainder = std::ranges::all_of(
                    remainder, [](char chr)
                    { return is_wrapper_close(chr) || is_shell_trailing_punct(chr); })};
    }

    // post: true with `head`, the value's marker, the wildcard and any kept remainder appended to
    // `out`; false with `out` untouched when the value is not digit-leading after its marker.
    // post: false with `out` untouched for a status key whose extent is a short integer, so a
    // green-to-red flip stays distinct.
    // invariant: THE one value disposition - kv_value and the segment step locate their key and
    // value and call it, so the two rules give the same bytes after `=` for the same value.
    // invariant: a remainder the extent leaves unswallowed is appended byte for byte after the
    // wildcard, so a word behind a number stays in the template.
    // refs: DN-134.D15, DN-134.D11, LSRC-14, ADR-16.D5
    [[nodiscard]] inline bool append_masked_value(std::string_view key, std::string_view head,
                                                  std::string_view raw_value, std::string& out)
    {
        // assert: a declared currency marker is stripped off the value before the digit-leading
        // gate, so the key AND the marker are kept while the amount masks.
        // refs: F-SRC-insight-canon:mask.cpp:normalize_marker_number
        const std::size_t marker{marker_prefix_len(raw_value)};
        const std::string_view value{raw_value.substr(marker)};
        if (!is_digit_leading(value))
            return false;
        const ValueExtent extent{value_extent(value)};
        const std::string_view number{value.substr(0, extent.length)};
        if (is_status_keyword(key) && is_all_digits(number) && number.size() <= kMaxStatusDigits)
            return false;
        out.append(head);
        out.append(raw_value.substr(0, marker));
        out.append(kWildcard);
        if (!extent.swallows_remainder)
            out.append(value.substr(extent.length));
        return true;
    }

    // post: keeps every byte before the token's first `=` as the key and masks the value through
    // append_masked_value; a value WORD stays literal.
    // refs: DN-134.D15, LSRC-14, ADR-16.D5
    [[nodiscard]] inline bool normalize_kv_value(std::string_view tok, std::string& out)
    {
        const std::size_t eq_pos{tok.find('=')};
        if (eq_pos == 0 || eq_pos == std::string_view::npos)
            return false;
        out.clear();
        return append_masked_value(tok.substr(0, eq_pos), tok.substr(0, eq_pos + 1),
                                   tok.substr(eq_pos + 1), out);
    }

    // post: appends `seg`, its value masked through append_masked_value when it is `<key>=<value>`
    // with an identifier key (wrapper openers may lead the first segment); true when it masked.
    // refs: DN-134.D2, DN-134.D15
    [[nodiscard]] inline bool append_segment(std::string_view seg, bool first, std::string& out)
    {
        const std::size_t eq_pos{seg.find('=')};
        if (eq_pos == 0 || eq_pos == std::string_view::npos)
        {
            out.append(seg);
            return false;
        }
        std::size_t lead{0};
        while (first && lead < eq_pos && is_wrapper_open(seg[lead]))
            ++lead;
        const std::string_view key{seg.substr(lead, eq_pos - lead)};
        if (is_segment_key(key) &&
            append_masked_value(key, seg.substr(0, eq_pos + 1), seg.substr(eq_pos + 1), out))
            return true;
        out.append(seg);
        return false;
    }

    // refs: DN-134.D2
    // post: true with `out` holding `form` whose `;`-segments each passed append_segment; false
    // when no segment moved, `out` then unspecified.
    // invariant: NOT a claiming rule - it reads a token's normal form after rules 2 and 6, before
    // the declared-run step, and contributes no param.
    [[nodiscard]] bool mask_segment_values(std::string_view form, std::string& out)
    {
        if (form.find(kSegmentDelimiter) == std::string_view::npos ||
            form.find('=') == std::string_view::npos)
            return false;
        out.clear();
        bool moved{false};
        std::size_t seg_start{0};
        for (;;)
        {
            const std::size_t delim{form.find(kSegmentDelimiter, seg_start)};
            const std::size_t seg_end{delim == std::string_view::npos ? form.size() : delim};
            moved =
                append_segment(form.substr(seg_start, seg_end - seg_start), seg_start == 0, out) ||
                moved;
            if (delim == std::string_view::npos)
                return moved;
            out.push_back(kSegmentDelimiter);
            seg_start = delim + 1;
        }
    }

    // invariant: the array ORDER IS THE PRECEDENCE - tried top to bottom, the first rule that
    // claims the token wins.
    // invariant: this catalog DEFINES the composite layer of the generation the canonicalization
    // version names, so adding, reordering or removing a rule REQUIRES a version bump.
    // assert: the catalog's SHAPE is one limb of that obligation and not all of it - widening an
    // existing rule in place leaves this array byte-identical and owes the same bump.
    // refs: F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate
    // refs: F-SRC-insight-canon:canon.api.cppm:kCanonicalizationVersion
    struct CompositeRule
    {
        std::string_view name;
        bool (*normalize)(std::string_view tok, std::string& out);
    };
    // invariant: the two bracket rules are ADJACENT, most specific first, and non-overlapping
    // today, so future drift between them has a rule to violate loudly.
    // refs: LSRC-11, F-SRC-insight-canon:mask.cpp:normalize_ephemeral_root, LSRC-12
    // refs: F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate
    // refs: F-SRC-insight-canon:mask.cpp:normalize_hash_counter
    // refs: LSRC-13, LSRC-14, F-SRC-insight-canon:mask.cpp:normalize_marker_number
    constexpr std::array<CompositeRule, 10U> kCompositeRules{{
        {.name = "diagnostic_composite", .normalize = normalize_diagnostic_composite},
        {.name = "ephemeral_root", .normalize = normalize_ephemeral_root},
        {.name = "versioned_ref", .normalize = normalize_versioned_ref},
        {.name = "bracket_timestamp", .normalize = normalize_bracket_timestamp},
        {.name = "bracket_index", .normalize = normalize_bracket_index},
        {.name = "hash_counter", .normalize = normalize_hash_counter},
        {.name = "marker_number", .normalize = normalize_marker_number},
        {.name = "embedded_identity", .normalize = normalize_embedded_identity},
        {.name = "sanitizer_pid", .normalize = normalize_sanitizer_pid},
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

    // refs: DN-133.D1, DN-133.D7
    // post: true with `out` holding `form` whose claimed runs read as the wildcard; false when no
    // declared run claims anything, `out` then unspecified.
    [[nodiscard]] bool replace_declared_runs(std::string_view form,
                                             std::span<const DeclaredRun> declared_runs,
                                             std::string& out, std::vector<ClaimedRun>& claims)
    {
        claim_declared_runs(form, declared_runs, claims);
        if (claims.empty())
            return false;
        out.clear();
        std::size_t copied{0};
        for (const ClaimedRun& claim : claims)
        {
            out.append(form.substr(copied, claim.first - copied));
            out.append(kWildcard);
            copied = claim.last;
        }
        out.append(form.substr(copied));
        return true;
    }

    // refs: DN-134.D3
    // invariant: the JSON member-order normal form reads RFC 8259 text and nothing laxer: blanks
    // are the four the grammar names, a string holds no raw control byte and only declared escapes.
    namespace json_order
    {

        // invariant: the nesting bound past which a line is left as it is, so no line drives the
        // recursion without limit; far beyond any measured CI line.
        constexpr std::size_t kMaxDepth{128};
        constexpr unsigned kFirstPrintable{0x20U};
        constexpr std::size_t kUnicodeEscapeDigits{4};
        constexpr unsigned kHexRadix{16U};
        constexpr unsigned kHexLetterBase{10U};

        [[nodiscard]] constexpr bool is_blank(char chr) noexcept
        {
            return chr == ' ' || chr == '\t' || chr == '\n' || chr == '\r';
        }

        [[nodiscard]] constexpr std::size_t skip_blanks(std::string_view src,
                                                        std::size_t pos) noexcept
        {
            while (pos < src.size() && is_blank(src[pos]))
                ++pos;
            return pos;
        }

        // post: the value of one hex digit, or kHexRadix when `chr` is none.
        [[nodiscard]] constexpr unsigned hex_value(char chr) noexcept
        {
            if (is_digit(chr))
                return static_cast<unsigned>(chr - '0');
            const unsigned lower{static_cast<unsigned>(static_cast<unsigned char>(chr)) |
                                 kAsciiCaseMask};
            return lower - 'a' < kHexLetterCount ? lower - 'a' + kHexLetterBase : kHexRadix;
        }

        // pre: `pos` is at a `"`.
        // post: the index after the closing quote, or npos when the string is not RFC 8259.
        [[nodiscard]] constexpr std::size_t string_end(std::string_view src,
                                                       std::size_t pos) noexcept
        {
            for (++pos; pos < src.size(); ++pos)
            {
                const char chr{src[pos]};
                if (chr == '"')
                    return pos + 1U;
                if (static_cast<unsigned>(static_cast<unsigned char>(chr)) < kFirstPrintable)
                    return std::string_view::npos;
                if (chr != '\\')
                    continue;
                if (++pos >= src.size())
                    return std::string_view::npos;
                const char esc{src[pos]};
                if (esc == 'u')
                {
                    for (std::size_t digit{0}; digit < kUnicodeEscapeDigits; ++digit)
                    {
                        ++pos;
                        if (pos >= src.size() || hex_value(src[pos]) == kHexRadix)
                            return std::string_view::npos;
                    }
                }
                else if (esc != '"' && esc != '\\' && esc != '/' && esc != 'b' && esc != 'f' &&
                         esc != 'n' && esc != 'r' && esc != 't')
                    return std::string_view::npos;
            }
            return std::string_view::npos;
        }

        // post: the index after an RFC 8259 number at `pos`, or npos.
        [[nodiscard]] constexpr std::size_t number_end(std::string_view src,
                                                       std::size_t pos) noexcept
        {
            const auto digits{[&]
                              {
                                  const std::size_t from{pos};
                                  while (pos < src.size() && is_digit(src[pos]))
                                      ++pos;
                                  return pos - from;
                              }};
            if (pos < src.size() && src[pos] == '-')
                ++pos;
            if (pos < src.size() && src[pos] == '0')
                ++pos;
            else if (digits() == 0)
                return std::string_view::npos;
            if (pos < src.size() && src[pos] == '.')
            {
                ++pos;
                if (digits() == 0)
                    return std::string_view::npos;
            }
            if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E'))
            {
                ++pos;
                if (pos < src.size() && (src[pos] == '+' || src[pos] == '-'))
                    ++pos;
                if (digits() == 0)
                    return std::string_view::npos;
            }
            return pos;
        }

        // pre: `pos` is in `src` and not at `{` or `[`.
        // post: the index after the string, literal word or number at `pos`, or npos.
        [[nodiscard]] constexpr std::size_t scalar_end(std::string_view src,
                                                       std::size_t pos) noexcept
        {
            if (src[pos] == '"')
                return string_end(src, pos);
            for (const std::string_view word : {"true", "false", "null"})
                if (src.substr(pos).starts_with(word))
                    return pos + word.size();
            return number_end(src, pos);
        }

        // post: the index of the member's value after the name at `pos`, its colon and the blanks
        // around it, or npos when `pos` opens no `"name" :`.
        [[nodiscard]] constexpr std::size_t member_value_at(std::string_view src,
                                                            std::size_t pos) noexcept
        {
            if (pos >= src.size() || src[pos] != '"')
                return std::string_view::npos;
            pos = skip_blanks(src, string_end(src, pos));
            if (pos >= src.size() || src[pos] != ':')
                return std::string_view::npos;
            return skip_blanks(src, pos + 1U);
        }

        // post: the index after the value at `pos`, or npos when it is not one RFC 8259 value
        // nested at most kMaxDepth deep.
        // note: recursion is the grammar's own shape, and kMaxDepth bounds it.
        // NOLINTNEXTLINE(misc-no-recursion)
        [[nodiscard]] constexpr std::size_t value_end(std::string_view src, std::size_t pos,
                                                      std::size_t depth) noexcept
        {
            if (pos >= src.size())
                return std::string_view::npos;
            const char open{src[pos]};
            if (open != '{' && open != '[')
                return scalar_end(src, pos);
            if (depth >= kMaxDepth)
                return std::string_view::npos;
            const char close{open == '{' ? '}' : ']'};
            pos = skip_blanks(src, pos + 1U);
            if (pos < src.size() && src[pos] == close)
                return pos + 1U;
            for (;;)
            {
                if (open == '{')
                    pos = member_value_at(src, pos);
                const std::size_t end{value_end(src, pos, depth + 1U)};
                if (end == std::string_view::npos)
                    return end;
                pos = skip_blanks(src, end);
                if (pos < src.size() && src[pos] == close)
                    return pos + 1U;
                if (pos >= src.size() || src[pos] != ',')
                    return std::string_view::npos;
                pos = skip_blanks(src, pos + 1U);
            }
        }

        constexpr unsigned kSurrogateFirst{0xD800U};
        constexpr unsigned kLowSurrogateFirst{0xDC00U};
        constexpr unsigned kSurrogateEnd{0xE000U};
        constexpr unsigned kSurrogatePayloadBits{10U};
        constexpr unsigned kSupplementaryBase{0x10000U};
        constexpr unsigned kOneByteEnd{0x80U};
        constexpr unsigned kTwoByteEnd{0x800U};
        constexpr unsigned kThreeByteEnd{0x10000U};
        constexpr unsigned kContinuationBits{6U};
        constexpr unsigned kContinuationMask{0x3FU};
        constexpr unsigned kContinuationTag{0x80U};
        constexpr unsigned kTwoByteTag{0xC0U};
        constexpr unsigned kThreeByteTag{0xE0U};
        constexpr unsigned kFourByteTag{0xF0U};

        // post: `code` appended as UTF-8; a lone surrogate takes its 3-byte form.
        inline void append_utf8(unsigned code, std::string& out)
        {
            const auto byte{[&](unsigned value) { out.push_back(static_cast<char>(value)); }};
            const auto tail{[&](unsigned shift)
                            { byte(kContinuationTag | ((code >> shift) & kContinuationMask)); }};
            if (code < kOneByteEnd)
                byte(code);
            else if (code < kTwoByteEnd)
            {
                byte(kTwoByteTag | (code >> kContinuationBits));
                tail(0U);
            }
            else if (code < kThreeByteEnd)
            {
                byte(kThreeByteTag | (code >> (2U * kContinuationBits)));
                tail(kContinuationBits);
                tail(0U);
            }
            else
            {
                byte(kFourByteTag | (code >> (3U * kContinuationBits)));
                tail(2U * kContinuationBits);
                tail(kContinuationBits);
                tail(0U);
            }
        }

        // pre: `quoted` is one RFC 8259 string, quotes included.
        // post: its unescaped bytes - the key members sort by.
        [[nodiscard]] inline std::string unescaped(std::string_view quoted)
        {
            std::string out;
            const std::string_view body{quoted.substr(1U, quoted.size() - 2U)};
            const auto code_at{[&](std::size_t offset)
                               {
                                   unsigned code{0};
                                   for (std::size_t digit{0}; digit < kUnicodeEscapeDigits; ++digit)
                                       code = (code * kHexRadix) + hex_value(body[offset + digit]);
                                   return code;
                               }};
            for (std::size_t pos{0}; pos < body.size(); ++pos)
            {
                if (body[pos] != '\\')
                {
                    out.push_back(body[pos]);
                    continue;
                }
                const char esc{body[++pos]};
                if (esc != 'u')
                {
                    static constexpr std::string_view kFrom{"bfnrt"};
                    static constexpr std::string_view kTo{"\b\f\n\r\t"};
                    const std::size_t idx{kFrom.find(esc)};
                    out.push_back(idx == std::string_view::npos ? esc : kTo[idx]);
                    continue;
                }
                unsigned code{code_at(pos + 1U)};
                pos += kUnicodeEscapeDigits;
                const std::size_t pair_at{pos + 1U};
                // assert: the string passed string_end, so a `\u` here carries its four digits.
                if (code >= kSurrogateFirst && code < kLowSurrogateFirst &&
                    body.substr(pair_at).starts_with("\\u"))
                {
                    const unsigned low{code_at(pair_at + 2U)};
                    if (low >= kLowSurrogateFirst && low < kSurrogateEnd)
                    {
                        code = kSupplementaryBase +
                               ((code - kSurrogateFirst) << kSurrogatePayloadBits) +
                               (low - kLowSurrogateFirst);
                        pos = pair_at + 1U + kUnicodeEscapeDigits;
                    }
                }
                append_utf8(code, out);
            }
            return out;
        }

        // pre: the value at `pos` passed value_end.
        // post: appends the value with every object's members in name order and returns the index
        // after it; npos when an object repeats a name.
        // note: recursion is the grammar's own shape, bounded by the depth value_end checked.
        // NOLINTNEXTLINE(misc-no-recursion)
        [[nodiscard]] std::size_t emit(std::string_view src, std::size_t pos, std::string& out)
        {
            const char open{src[pos]};
            if (open != '{' && open != '[')
            {
                const std::size_t end{value_end(src, pos, kMaxDepth)};
                out.append(src.substr(pos, end - pos));
                return end;
            }
            const char close{open == '{' ? '}' : ']'};
            const std::size_t first{skip_blanks(src, pos + 1U)};
            out.push_back(open);
            out.append(src.substr(pos + 1U, first - pos - 1U));
            if (src[first] == close)
            {
                out.push_back(close);
                return first + 1U;
            }
            struct Member
            {
                std::string key;
                std::string text;
            };
            std::vector<Member> members;
            std::vector<std::string_view> separators;
            pos = first;
            for (;;)
            {
                Member member;
                std::string& sink{open == '{' ? member.text : out};
                if (open == '{')
                {
                    const std::size_t name_end{string_end(src, pos)};
                    member.key = unescaped(src.substr(pos, name_end - pos));
                    const std::size_t value_at{skip_blanks(src, skip_blanks(src, name_end) + 1U)};
                    sink.append(src.substr(pos, value_at - pos));
                    pos = value_at;
                }
                const std::size_t end{emit(src, pos, sink)};
                if (end == std::string_view::npos)
                    return end;
                const std::size_t after{skip_blanks(src, end)};
                const bool last{src[after] == close};
                const std::size_t next{last ? after : skip_blanks(src, after + 1U)};
                if (open == '{')
                {
                    members.push_back(std::move(member));
                    separators.push_back(src.substr(end, next - end));
                }
                else
                    out.append(src.substr(end, next - end));
                if (last)
                {
                    pos = after + 1U;
                    break;
                }
                pos = next;
            }
            if (open == '{')
            {
                std::ranges::sort(members, {}, &Member::key);
                const auto repeated{std::ranges::adjacent_find(members, {}, &Member::key)};
                if (repeated != members.end())
                    return std::string_view::npos;
                for (std::size_t idx{0}; idx < members.size(); ++idx)
                {
                    out.append(members[idx].text);
                    out.append(separators[idx]);
                }
            }
            out.push_back(close);
            return pos;
        }

    } // namespace json_order

    // refs: DN-134.D3
    // post: true with `out` holding `content` whose objects have their members PERMUTED into
    // ascending order of their unescaped names' bytes, recursively.
    // post: every other byte keeps its position - a member's text moves whole, the separator text
    // between the i-th and (i+1)-th member stays there, array elements never move.
    // post: false, `out` unspecified, when `content` is not, whole, one RFC 8259 object or array
    // (blanks around it allowed), when an object repeats a name, or when it is already in order.
    // invariant: a function of the line's bytes, stateless; member order is presentation (RFC 8259
    // section 4), and permuting rather than re-serializing keeps every whitespace token intact.
    [[nodiscard]] bool json_member_order(std::string_view content, std::string& out)
    {
        const std::size_t first{json_order::skip_blanks(content, 0)};
        if (first >= content.size() || (content[first] != '{' && content[first] != '['))
            return false;
        const std::size_t end{json_order::value_end(content, first, 0)};
        if (end == std::string_view::npos ||
            json_order::skip_blanks(content, end) != content.size())
            return false;
        out.clear();
        out.append(content.substr(0, first));
        if (json_order::emit(content, first, out) == std::string_view::npos)
            return false;
        out.append(content.substr(end));
        return out != content;
    }

    // invariant: rules 3 to 5 as one decision on a token the status KEEP and the composites left.
    enum class ValueDisposition : std::uint8_t
    {
        Mask,
        KeepLiteral,
        NotAValue
    };

    // pre: no status KEEP and no composite rule claimed `tok`.
    // post: rule 3's, rule 4's or rule 5's disposition of `tok`; NotAValue when none reaches it.
    // invariant: rule 4 decides its whole acceptance set, bare or shelled - MASK with the switch
    // on, KEEP literal with it off - so neither rule 5 nor the shelled reader reaches an address.
    // refs: ADR-16.D5, DN-134.D1, DN-134.D8
    [[nodiscard]] inline ValueDisposition value_disposition(std::string_view tok,
                                                            const TokenShape& shape,
                                                            std::string_view prev,
                                                            const MaskConfig& config) noexcept
    {
        if (shape.empty || is_uuid_or_long_hash(tok))
            return ValueDisposition::Mask;
        if (is_ipv4_token(tok))
            return config.mask_ip_addresses ? ValueDisposition::Mask
                                            : ValueDisposition::KeepLiteral;
        // assert: a hexadecimal-prefixed token needs no arm: it starts with a digit, so the
        // digit-leading test carries it.
        if (shape.digit_leading || is_shelled_numeric(tok, prev))
            return ValueDisposition::Mask;
        return ValueDisposition::NotAValue;
    }

} // namespace

// post: the joined per-token canonical forms; a masked position contributes a param, a kept or
// normalized position does not.
// post: a template token that is exactly the wildcard is a param and every param is one: param i
// is the (i + 1)-th such token, and a wildcard inside a token is a normalization with no param.
// invariant: a function of the content bytes only - no float, no map iteration, no state - so it is
// bit-identical across standard libraries and independent of order and stream.
// refs: ADR-16.D5, DN-128.D6, F-SRC-insight-canon:canon.detail.mask.cppm:StatelessTemplate
// refs: F-SRC-insight-canon:canon.api.cppm:TemplateId
StatelessTemplate stateless_template(std::string_view content, ArenaAllocator& out_arena,
                                     const MaskConfig& config,
                                     std::span<const DeclaredRun> declared_runs)
{
    // assert: the JSON member-order normal form runs before tokenization; a rewritten line is
    // stored in the arena, so every param views bytes that live as long as the template.
    // refs: DN-134.D3
    std::string reordered;
    const std::string_view line{
        json_member_order(content, reordered) ? out_arena.store_string(reordered) : content};
    std::string tmpl;
    tmpl.reserve(line.size() + kWildcard.size());
    std::vector<std::string_view> params;
    std::string composite;
    std::string segmented;
    std::string declared;
    std::vector<ClaimedRun> declared_claims;
    // post: `form` after the two non-claiming steps on a normal form, in order: the `;`-segment
    // key-value step, then the declared runs.
    // refs: DN-134.D2, DN-133.D1
    const auto normal_form_steps{
        [&](std::string_view form)
        {
            const std::string_view after_segments{
                mask_segment_values(form, segmented) ? std::string_view{segmented} : form};
            return !declared_runs.empty() && replace_declared_runs(after_segments, declared_runs,
                                                                   declared, declared_claims)
                       ? std::string_view{declared}
                       : after_segments;
        }};
    std::string_view prev{};
    bool first{true};

    // assert: the declared per-token classification in TOTAL precedence - the KEEP carve-outs win
    // first, then the masks.
    // refs: ADR-16.D5
    for_each_token(line,
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
                       // post: a normal form that is exactly the wildcard takes the mask path, so
                       // its SOURCE token is the param; any other normal form is appended as is.
                       // refs: DN-128.D6
                       const auto append_normal_form{[&](std::string_view form)
                                                     {
                                                         if (form == kWildcard)
                                                             mask();
                                                         else
                                                             tmpl.append(form);
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
                           // assert: the non-claiming steps read the composite's NORMAL FORM,
                           // once, after every claiming rule.
                           // refs: DN-133.D1, DN-134.D2
                           append_normal_form(normal_form_steps(composite));
                           prev = tok;
                           return;
                       }
                       switch (value_disposition(tok, shape, prev, config))
                       {
                       case ValueDisposition::Mask:
                           mask();
                           break;
                       case ValueDisposition::KeepLiteral:
                           tmpl.append(tok);
                           break;
                       case ValueDisposition::NotAValue:
                           // assert: a token no rule claimed is its own normal form.
                           // refs: DN-133.D1
                           append_normal_form(normal_form_steps(tok));
                           break;
                       }
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

    std::string composite_normal_form(std::string_view token)
    {
        std::string form;
        const TokenShape shape{token};
        if (!try_composite(token, shape, form, nullptr))
            form.clear();
        return form;
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
