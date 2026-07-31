module insight.canon.detail.mask;
import insight.canon.internal; // std (via `export import std;` — includes std::memchr)
import insight.canon.api;
import insight.canon.detail.scan; // canonical char-class predicates (is_digit / is_alpha)

// mask.cpp — the stateless per-line template masker
//
// `stateless_template(content, arena, config)` is the sole identity source: a PURE
// function of a line's own whitespace-delimited tokens, each classified KEEP / MASK /
// composite-normalize by its OWN class (no cross-line state, no clustering). The joined
// masked sequence is the template; its SHA-256 (computed downstream, unchanged) is the
// run-independent template_id. See stateless_template_id.md (D-TID-1/2; §8 D-TID-11..14).
//
// History: this file was the stateful Drain online log-template miner (intern table +
// SoA cluster store + bucket index + similarity match + absorb_into wildcard learning).
// That learning made the template — and thus template_id — order-dependent across runs,
// manufacturing the "phantom pair" false-diff (D-TID-3). The clustering machinery was
// RIPPED; only the per-token mask predicates and the masker survive. Token masking
// (IPv4, hex, digit-leading, UUID/long-hash, composites) uses constexpr hand-written
// scanners — zero RE2.

namespace insight::tokenization
{

namespace
{

    // ── Constants ────────────────────────────────────────────────────────────

    constexpr std::string_view kWildcard{"<*>"};

    constexpr unsigned kDecimalBase{10U};
    constexpr unsigned kAsciiCaseMask{32U}; // bit that toggles upper/lower case
    constexpr unsigned kHexLetterCount{6U}; // hex letters a-f / A-F

    // ── Hand-written token-mask predicates ───────────────────────────────────
    // Pure, byte-only classifiers over a single token (zero RE2). Each masks or
    // normalizes a structurally high-card token class so logically-identical lines
    // share a template (stateless_template_id.md §8).

    [[nodiscard]] constexpr bool is_hex_char(char chr) noexcept
    {
        return (static_cast<unsigned>(chr) - '0' < kDecimalBase) ||
               (static_cast<unsigned>(static_cast<unsigned char>(chr) | kAsciiCaseMask) - 'a' <
                kHexLetterCount);
    }

    // Consumes 1–3 decimal digits from str at pos; returns false if no digit found.
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

    // IPv4: \[?\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}(:\d+)?\]?[,;:\.\]]?
    [[nodiscard]] constexpr bool is_ipv4_token(std::string_view str) noexcept
    {
        if (str.empty())
            return false;
        std::size_t pos{0};
        if (str[0] == '[')
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
        if (pos < str.size() && str[pos] == ']')
            ++pos;
        if (pos < str.size() && (str[pos] == ',' || str[pos] == ';' || str[pos] == ':' ||
                                 str[pos] == '.' || str[pos] == ']'))
            ++pos;
        return pos == str.size();
    }

    // Hex: 0x[0-9a-fA-F]+[,;:\.\]]?
    [[nodiscard]] constexpr bool is_hex_token(std::string_view str) noexcept
    {
        if (str.size() < 3U || str[0] != '0' || str[1] != 'x')
            return false;
        std::size_t pos{2U};
        if (!is_hex_char(str[pos]))
            return false;
        while (pos < str.size() && is_hex_char(str[pos]))
            ++pos;
        if (pos < str.size() && (str[pos] == ',' || str[pos] == ';' || str[pos] == ':' ||
                                 str[pos] == '.' || str[pos] == ']'))
            ++pos;
        return pos == str.size();
    }

    // ── Helpers ──────────────────────────────────────────────────────────────
    [[nodiscard]] inline bool is_all_digits(std::string_view str) noexcept
    {
        if (str.empty())
            return false;
        return std::ranges::all_of(str, [](const char chr)
                                   { return static_cast<unsigned>(chr) - '0' < kDecimalBase; });
    }

    // Value-aware KEEP of low-cardinality status integers.
    //
    // A bare integer is masked to `<*>` (the digit-leading rule), which collapses
    // `exit code 0` and `exit code 1` into one template — a green→red flip then
    // vanishes at the template level. To keep such values DISTINCT we KEEP an
    // integer literal when it immediately follows a status keyword AND is small
    // (≤ kMaxStatusDigits). Size-gating bounds cardinality (exit codes ≤ 255,
    // HTTP status ≤ 599 — both ≤ 3 digits); the keyword gate keeps bare counts
    // ("port 8080", "took 200 ms") masked. The lexicon is a seed and will grow
    // during calibration (D-TID-14: categorical numbers stay a KEEP-lexicon concern).
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

    [[nodiscard]] inline bool is_status_keyword(std::string_view tok) noexcept
    {
        return equals_ascii_lower(tok, "code") || equals_ascii_lower(tok, "status") ||
               equals_ascii_lower(tok, "exit") || equals_ascii_lower(tok, "signal");
    }

    // ── Ephemeral-root catalog + matcher (D-MSK-4) ───────────────────────────
    // A path component sitting DIRECTLY under a declared ephemeral root is a per-run instance by
    // construction (a conan build dir `.conan2/p/b/insig247e3d1dffc33`, a nix store hash, a random
    // `/tmp/pw-electron-userdata-Kw9v4a`). study 011 proved NO hex/length rule can separate
    // ephemeral from content — a 40-char SHA is a pinned dependency in one path and per-run junk in
    // another, same length/alphabet. The DECIDABLE thing is the ROOT: an enumerable, byte-exact
    // catalog. It is the single source of truth, consulted as a per-segment PREDICATE from BOTH the
    // diagnostic-composite segment walk (call site A, below) and the standalone
    // `normalize_ephemeral_root` (call site B) — adding a root here extends both with no second
    // edit (the kCurrencyMarkers discipline). Masking-only, no semantics ⇒ canon CORE, not a
    // dialect package (ADR 0024 cl.4: kCanonicalizationVersion IS the core masking generation).
    // See technical_docs/adr/016-canon-canonicalization-internals.md.

    enum class RootAnchor : std::uint8_t
    {
        TokenStart, // the root's first component is the first component after a leading '/'
        Floating,   // the root matches at ANY '/' component boundary (a mid-path root)
    };
    enum class RootScope : std::uint8_t
    {
        Subtree,  // everything under the root is ephemeral (a namespace of ephemeral trees: /tmp)
        Instance, // exactly the ONE component under the root is ephemeral; the tail resumes normal
                  // classification (a content-addressed store whose subtree is stable: conan, nix)
    };

    struct EphemeralRoot
    {
        std::span<const std::string_view> segments; // the root as ordered path components
        RootAnchor anchor;
        RootScope scope;
    };

    // The declared roots, segment-wise (a root is components, never a string containing '/', so it
    // matches segment-wise). Each backing array has static storage so the spans are
    // constexpr-valid. `anchor`/`scope` are EXPLICIT, never inferred: a mis-spelled root that
    // silently floated would over-mask, and over-masking destroys signal irrecoverably (ADR 0013) —
    // the dangerous axes are named on purpose.
    inline constexpr std::array<std::string_view, 1> kRootTmp{{std::string_view{"tmp"}}};
    inline constexpr std::array<std::string_view, 2> kRootVarTmp{
        {std::string_view{"var"}, std::string_view{"tmp"}}};
    inline constexpr std::array<std::string_view, 2> kRootVarFolders{
        {std::string_view{"var"}, std::string_view{"folders"}}};
    inline constexpr std::array<std::string_view, 3> kRootConan{
        {std::string_view{".conan2"}, std::string_view{"p"}, std::string_view{"b"}}};
    inline constexpr std::array<std::string_view, 2> kRootNixStore{
        {std::string_view{"nix"}, std::string_view{"store"}}};

    // `bazel-out` is deliberately ABSENT: its component is the build CONFIGURATION (`k8-fastbuild`,
    // `ppc-opt`) — stable per config, carries no hash, and masking it would destroy which-config
    // signal to fix nothing (canon_ephemeral_root_masking.md §3 M7).
    inline constexpr std::array<EphemeralRoot, 5> kEphemeralRoots{{
        {.segments = kRootTmp, .anchor = RootAnchor::TokenStart, .scope = RootScope::Subtree},
        {.segments = kRootVarTmp, .anchor = RootAnchor::TokenStart, .scope = RootScope::Subtree},
        {.segments = kRootVarFolders,
         .anchor = RootAnchor::TokenStart,
         .scope = RootScope::Subtree},
        {.segments = kRootConan, .anchor = RootAnchor::Floating, .scope = RootScope::Instance},
        {.segments = kRootNixStore, .anchor = RootAnchor::TokenStart, .scope = RootScope::Instance},
    }};

    inline constexpr std::size_t kMaxRootSegments{3U}; // longest declared root = look-back window

    // One path component seen by a segment walk: its core text, the separator byte immediately
    // before it ('/' , ':' , or '\0' at token start), and whether it is the first component after a
    // leading '/' (needed to decide a TokenStart anchor).
    struct PathComponent
    {
        std::string_view text;
        char sep_before{'\0'};
        bool at_token_start{false};
    };

    // THE MATCHER (D-MSK-4 M2). Given the trailing window of up to kMaxRootSegments components
    // ending at the CURRENT one (window.back()), does a declared root END here, and with what
    // scope? Longest declared root ending here wins (order-independent). Returns nullopt otherwise.
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
            // Hot-path gate (§5 MUST): the current component must be the root's LAST segment — a
            // size-then-byte string compare that rejects almost every component in ~one length
            // check. Only a strictly longer match than one already found can win.
            if (klen > window.size() || klen <= best_len ||
                window.back().text != root.segments[klen - 1U])
                continue;
            const std::size_t first{window.size() - klen}; // window index of the root's 1st comp
            bool matched{true};
            for (std::size_t seg{0}; seg + 1U < klen; ++seg) // last segment already matched
                if (window[first + seg].text != root.segments[seg])
                {
                    matched = false;
                    break;
                }
            if (!matched)
                continue;
            // MUST: consecutive AND '/'-separated — every separator from the root's first component
            // through its last must be '/', so a ':'-coincidence (`foo:tmp:bar`) never matches.
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

    // Composite-token masking, DIAGNOSTIC_COMPOSITE class (D-MSK-1).
    //
    // The fixed masks mask only WHOLE tokens that are digit-leading / IPv4 / hex. A
    // `:`/`/`-structured diagnostic token is none of those, so every numeric instance
    // becomes its own template — a dominant cardinality blow-up. Two shapes:
    //   • a compiler source location `tokenizer.cpp:4500:30:` (path then `:line:col`); and
    //   • the Chromium/glog/Electron prefix `[6226:0609/094020.430910:ERROR:dbus/bus.cc:408]`
    //     (PID:DATE/TIME:LEVEL:file.cc:line) — `normalize_source_location` kept the whole
    //     PID/date/time prefix because it isn't "path-like", so it never collapsed.
    //
    // GENERALIZED RULE: split the token on `:` and `/` into sub-segments and classify EACH
    // independently — a digit-leading sub-segment → `<*>` (mask the instance), a letter-leading
    // sub-segment → KEEP (the stable class anchor: a filename, a level, a subsystem). Rejoin
    // with the original separators. This SUBSUMES source-location exactly
    // (`tokenizer.cpp:4500:30:` → `tokenizer.cpp:<*>:<*>:`, unchanged) and collapses the
    // Chromium prefix (→ `[<*>:<*>/<*>:ERROR:dbus/bus.cc:<*>]`, byte-identical across PIDs/times).
    //
    // SCOPING (keeps the blast radius near genuine diagnostic composites, not all paths):
    //   • TRIGGER — the token must contain a `:` immediately followed by a digit (the
    //     source-location/diagnostic signature). A plain numeric path `/foo/12345/bar` (no
    //     `:digit`) is NOT triggered → handled by the whole-token digit rule, unchanged.
    //   • ANCHOR — at least one letter-leading sub-segment must exist. A pure-numeric colon
    //     token (a clock `12:30:45`) has no anchor → falls through to the digit-leading whole-
    //     token mask (`<*>`), unchanged. Letter-leading keywords (`arm64:v8`) never trigger.
    //   • STATUS-VALUE CARVE-OUT (per segment) — a digit sub-segment that is a status value
    //     (≤ kMaxStatusDigits, immediately preceded WITHIN the composite by a status keyword
    //     segment `exit`/`code`/`signal`/`status`) is KEPT, so `exit:0`→`exit:1` /
    //     `status:200`→`status:500` stay split (the green→red flip never collapses).
    //
    // Pure, byte-only, single-token (a function of the token bytes — I5) → cross-stdlib + MSVC
    // bit-identical. Returns true and fills `out` only when ≥1 segment was masked AND an anchor
    // exists; false (leaving the dispatch to fall through) otherwise.
    // One coherent per-token masking routine: the `:`/`/` segment walk, the letter-KEEP /
    // digit-MASK / status carve-out classification, and the D-MSK-4 ephemeral-root instance masking
    // all share the same left-to-right pass over `out`/prev_core/window — a split fragments the
    // single scan and its determinism.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    [[nodiscard]] inline bool normalize_diagnostic_composite(std::string_view tok, std::string& out)
    {
        // TRIGGER: a ':' immediately followed by a digit (subsumes source-location).
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
        std::string_view prev_core{}; // the previous segment's core — for the status carve-out
        std::size_t seg_start{0};
        // D-MSK-4 M3/M4: the ephemeral-root matcher over the SAME catalog as call site B. When a
        // declared root ends at a component, the NEXT component is a per-run instance and masks to
        // <*> regardless of its leading char (overriding the letter-leading KEEP); scope is CLAMPED
        // to Instance here so the file:line tail is never masked, and classification resumes from
        // the component after the instance. Root matching uses the FULL segment (`.conan2` keeps
        // its dot), not the alnum-stripped `core`.
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
                // core = segment with leading/trailing non-alnum stripped (for classification only)
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
                    out.append(seg); // pure punctuation — keep verbatim
                    prev_core = {};
                    // an empty/punct-only component is never a root segment: it neither ends a root
                    // nor consumes mask_next (a stray `//` leaves the instance to the next real
                    // component). Leave the window and flags untouched.
                    return;
                }

                if (mask_next)
                {
                    // this component is the per-run instance directly under a declared root → mask
                    // it whole (M3), regardless of its leading character.
                    out.append(seg.substr(0, lead));
                    out.append(kWildcard);
                    out.append(seg.substr(trail));
                    masked = true;
                    prev_core = core;
                    push_component(seg, sep_before, at_token_start);
                    mask_next = root_ends_here(); // resumes normally unless (pathologically) the
                                                  // instance itself also ends a root
                    return;
                }

                if (is_alpha(core.front()))
                {
                    out.append(seg); // letter-leading → KEEP (class anchor)
                    has_letter_anchor = true;
                }
                else if (is_status_keyword(prev_core) && is_all_digits(core) &&
                         core.size() <= kMaxStatusDigits)
                {
                    out.append(seg); // exit:0 / status:500 → KEEP (categorical)
                }
                else
                {
                    out.append(seg.substr(0, lead)); // leading punct, e.g. '['
                    out.append(kWildcard);
                    out.append(seg.substr(trail)); // trailing punct, e.g. ']'
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
                out.push_back(tok[pos]); // the separator, in its original position
                seg_start = pos + 1;
            }
        flush_segment(tok.size()); // the final segment
        return masked && has_letter_anchor;
    }

    // EPHEMERAL_ROOT standalone rule (D-MSK-2, re-expressed for D-MSK-4 M5). Reached only for
    // tokens rule #1 did not claim (no ':digit'). Split the path on '/', run the shared matcher
    // (kEphemeralRoots above), and honor the DECLARED scope: `Subtree` collapses the whole
    // remainder (`<root>/<*>`, byte-identical to -5 for every existing entry); `Instance` masks the
    // one component under the root and KEEPS the tail (`<root>/<*>/<tail…>`). Being segment-
    // anchored (not `starts_with`) is what lets a mid-path Floating root like `.conan2/p/b` match.
    // NOT a random-string classifier and NOT a general absolute-path masker (`/etc/hosts`,
    // `/usr/bin/foo` untouched — no declared root). Checked AFTER the diagnostic composite, so a
    // `/tmp/…:42` source path keeps its `file:line` shape; a `/tmp/…` dir without `:digit` lands
    // here.
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
        std::size_t root_end{std::string_view::npos}; // byte index of the '/' after the root
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
            return false; // no declared root in this token

        // Require a non-empty INSTANCE component directly under the root (`<root>/<x>`), so a bare
        // `/tmp` / `/tmp/` and a `//` are not collapsed (preserves the -5 size guard).
        if (root_end >= tok.size() || tok[root_end] != '/')
            return false;
        const std::size_t inst_start{root_end + 1U};
        if (inst_start >= tok.size() || tok[inst_start] == '/')
            return false;

        out.clear();
        out.append(tok.substr(0, root_end)); // `<root…>`
        out.push_back('/');
        out.append(kWildcard);
        if (scope == RootScope::Instance)
        {
            // keep the tail after the ONE masked instance component (M5 Instance form)
            std::size_t inst_end{inst_start};
            while (inst_end < tok.size() && tok[inst_end] != '/')
                ++inst_end;
            out.append(tok.substr(inst_end)); // `/…tail`, or empty if the instance is last
        }
        // Subtree: the whole remainder is already the single `<*>` just appended (M5 Subtree form).
        return true;
    }

    // VERSIONED_REF composite: `<name>/<numeric-version>[trailing punct]`.
    // Conan/cmake/package output ("zlib/3", "boost/1.83.0:") keeps a literal token
    // per (name,version), so a bumped version is a phantom new template. Normalize
    // by KEEPing the name and masking the numeric version → `zlib/<*>`, `boost/<*>:`.
    // Requires: a '/' whose suffix is a numeric (digits/dots) version run, then only
    // punctuation to end (so paths like "src/foo.cpp" — alpha suffix — are NOT hit).
    [[nodiscard]] inline bool normalize_versioned_ref(std::string_view tok, std::string& out)
    {
        const std::size_t slash{tok.rfind('/')};
        if (slash == std::string_view::npos || slash + 1 >= tok.size())
            return false;
        if (!is_digit(tok[slash + 1]))
            return false; // version must start with a digit

        std::size_t cursor{slash + 1};
        bool saw_digit{false};
        while (cursor < tok.size() && (is_digit(tok[cursor]) || tok[cursor] == '.'))
        {
            saw_digit = saw_digit || is_digit(tok[cursor]);
            ++cursor;
        }
        if (!saw_digit)
            return false;
        // Anything after the version run must be non-alphanumeric (punctuation),
        // else this is a path segment ("v1/2x") not a terminal version.
        for (std::size_t pos{cursor}; pos < tok.size(); ++pos)
        {
            const char chr{tok[pos]};
            if (is_digit(chr) || (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z'))
                return false;
        }

        out.clear();
        out.append(tok.substr(0, slash + 1)); // "zlib/"
        out.append("<*>");
        out.append(tok.substr(cursor)); // trailing punctuation, e.g. ":"
        return true;
    }

    // BRACKET_TIMESTAMP composite (D-MSK-5; bibles/jenkins_dialect.md §4, adr/0053 erratum 2 —
    // "the bracket is the entire difference"). The WHOLE-token bracketed RFC3339 stamp
    // `[2026-06-23T15:11:09.020Z]` used to fall through EVERY rule to literal KEEP: the
    // diagnostic composite declines it (its `:digit` trigger fires but no sub-segment is
    // letter-leading, so the anchor gate fails), bracket_index declines at the first `-`, and the
    // digit-leading whole-token mask never sees a `[`-leading byte. On a Jenkins timestamper
    // stream reaching the masker WITHOUT the dialect declared (the RawText floor, fail-closed)
    // every stamped line was therefore its own template — measured at 95.9% of the no-collapse
    // ceiling on the payload-stamped slice (adr/0046 clause 2), a live adr/0013 precision-first
    // regression. The TRIGGER is deliberately NARROW (precision-first: claim the stamp class and
    // nothing adjacent to it): the token is exactly `[` + a COMPLETE RFC3339 full datetime + `]`.
    // Date-only, time-only, bare-integer (`[42]` stays bracket_index's), word (`[INFO]`,
    // `[Pipeline]`), version (`[v1.2.3]`) interiors and any trailing punctuation are all
    // declined. The byte grammar is insight::utils::rfc3339_datetime_length — ONE owner, shared
    // with the Jenkins strategy's timestamper_prefix_end, so the shape is never spelled twice.
    // Normal form `[<*>]`: the KEEP-class bracket convention its neighbor set — the bracket (the
    // class) survives, the instance masks. The output-class collision with bracket_index's `[<*>]`
    // is NAMED AND ACCEPTED: both are masked-instance-inside-brackets, and inventing a second
    // placeholder vocabulary for one rule is worse than sharing the normal form.
    [[nodiscard]] inline bool normalize_bracket_timestamp(std::string_view tok, std::string& out)
    {
        if (tok.size() < 3U || tok.front() != '[' || tok.back() != ']')
            return false;
        const std::size_t interior{insight::utils::rfc3339_datetime_length(tok, 1U)};
        if (interior == 0 || 1U + interior + 1U != tok.size())
            return false; // the interior is not EXACTLY one complete full datetime — declined
        out.clear();
        out.push_back('[');
        out.append(kWildcard);
        out.push_back(']');
        return true;
    }

    // BRACKET_INDEX composite: `<word>[<short-alpha>?<digits>]<rest>`. Recursion depth,
    // worker/shard indices ("make[2]:", "thread[15]", pytest-xdist "[gw0]") otherwise
    // template per index. Normalize the bracketed digit run, KEEPING any short alpha
    // class-prefix inside the bracket → `make[<*>]:`, `[gw<*>]` (D-TID-13b generalizes
    // pure-`[N]` to `[<prefix><N>]`: keep the stable class marker, mask the varying index).
    [[nodiscard]] inline bool normalize_bracket_index(std::string_view tok, std::string& out)
    {
        const std::size_t open{tok.find('[')};
        if (open == std::string_view::npos || open + 1 >= tok.size())
            return false;
        std::size_t cursor{open + 1};
        const std::size_t prefix_begin{cursor};
        while (cursor < tok.size() && is_alpha(tok[cursor]))
            ++cursor; // optional class prefix inside the bracket ("gw", "worker")
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
        out.append(tok.substr(0, open + 1)); // "make[" / "["
        out.append(prefix);                  // "" for make[2]; "gw" for [gw0]
        out.append("<*>");
        out.append(tok.substr(cursor)); // "]:" and anything after
        return true;
    }

    // ── F13 composite-masking (stateless_template_id.md §8 / D-TID-12,13) ────────
    // Per-line masking is the SOLE generalizer once Drain's learning is retired, so
    // these classify the high-card SYNTACTIC token classes the fixed masks missed.
    // Boundary (D-TID-14): syntactic classes only — varying WORDS stay literal (the
    // deferred registry's job). All byte-only, single-token → cross-stdlib identical.

    // D-TID-12 #5: digit-leading (after an optional sign) ⇒ a number / measurement /
    // version / timestamp, intrinsically high-card. Subsumes the all-digit mask AND
    // numbers-with-separators / decimals / number+unit / versions in ONE rule, no unit
    // lexicon (`512MB`/`6.2s`/`0.25.5-3` mask; `sha256`/`x86` are letter-leading → keep).
    [[nodiscard]] inline bool is_digit_leading(std::string_view tok) noexcept
    {
        std::size_t pos{0};
        if (pos < tok.size() && (tok[pos] == '+' || tok[pos] == '-'))
            ++pos;
        return pos < tok.size() && is_digit(tok[pos]);
    }

    // D-TID-12 #3: a standalone UUID (8-4-4-4-12 hex-with-dashes) or a hex-only run
    // ≥ 16 chars (a git SHA / content hash). The ≥16 floor keeps short hex-looking
    // words ("deadbeef", "cafe") literal — only genuinely high-card hashes mask.
    [[nodiscard]] inline bool is_uuid_or_long_hash(std::string_view tok) noexcept
    {
        static constexpr std::size_t kUuidLen{36};
        static constexpr std::size_t kUuidDash1{8}, kUuidDash2{13}, kUuidDash3{18}, kUuidDash4{23};
        static constexpr std::size_t kMinHashLen{16};
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

    // D-TID-13(a): `#`-counter (`#42`, buildkit `#NN`) → `#<*>` — keep the marker, mask
    // the index. The digit run must run to end-or-punctuation (so `#main` is not a counter).
    [[nodiscard]] inline bool normalize_hash_counter(std::string_view tok, std::string& out)
    {
        if (tok.size() < 2U || tok[0] != '#' || !is_digit(tok[1]))
            return false;
        std::size_t cursor{1};
        while (cursor < tok.size() && is_digit(tok[cursor]))
            ++cursor;
        for (std::size_t pos{cursor}; pos < tok.size(); ++pos)
            if (is_digit(tok[pos]) || is_alpha(tok[pos]))
                return false; // `#42abc` is not a clean counter
        out.clear();
        out.append("#<*>");
        out.append(tok.substr(cursor));
        return true;
    }

    // D-TID-22 currency-marker catalog: FROZEN, DECLARED byte sequences. ASCII `$` only for now;
    // `€`/`£`/`¥` would be added here as their literal UTF-8 byte strings ("€" etc.) iff a
    // corpus shows them — byte-exact, NO Unicode property lookup (cross-stdlib determinism +
    // portability, the D-TID-9 oracle). Adding a marker here auto-extends both touch points (the
    // pre-gate + normalize_marker_number) — single source of truth.
    inline constexpr std::array<std::string_view, 1> kCurrencyMarkers{std::string_view{"$"}};

    // Length in BYTES of the declared currency marker prefixing `tok` (0 if none). Requires at
    // least one byte after the marker (the numeric core), so a lone `$` is not a marker.
    [[nodiscard]] inline std::size_t marker_prefix_len(std::string_view tok) noexcept
    {
        for (const std::string_view marker : kCurrencyMarkers)
            if (tok.size() > marker.size() && tok.starts_with(marker))
                return marker.size();
        return 0;
    }

    // D-TID-22: a declared currency MARKER glued to a digit-led numeric core (`$463`, `$1.50`) →
    // `$<*>` — keep the marker, mask the high-card amount (the D-TID-13 `#42 → #<*>` keep-class/
    // mask-instance shape). A DECIDABLE numeric: there is no low-card *keyword* of shape
    // `<marker><digits>` worth protecting (shell positionals `$1`/`$2` are negligible-in-logs and
    // lossless to mask), so it joins the D-TID-12 #5 digit-leading numerics that the first-char
    // `is_digit_leading` test misses on a leading marker. The core is digits + one optional
    // `.`-fraction; a trailing alpha/digit after a clean core rejects (`$42abc` is not a counter,
    // like `#42abc`), trailing punctuation is kept (`$463,` → `$<*>,`). `$HOME` (`$`+letter) has no
    // digit core → returns false → kept literal. Byte-only, single-token → cross-stdlib
    // bit-identical.
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
                return false; // trailing '.' with no fraction → not a clean number
        }
        for (std::size_t pos{cursor}; pos < tok.size(); ++pos)
            if (is_digit(tok[pos]) || is_alpha(tok[pos]))
                return false; // `$42abc` is not a clean marker-number
        out.clear();
        out.append(tok.substr(0, marker)); // keep the marker bytes
        out.append(kWildcard);
        out.append(tok.substr(cursor)); // keep trailing punctuation
        return true;
    }

    // D-TID-12 #3, reached INSIDE a token: mask a UUID (8-4-4-4-12) or a long hex-run
    // (≥ 16, delimiter-bounded) that is EMBEDDED in a larger token — temp-dir paths
    // (`/…/_temp/<uuid>/cache.tzst`), `git-credentials-<uuid>.config`, `{worker-uuid}`,
    // `builder-<uuid>` — keeping the surrounding path/structure, masking the identity
    // instance (the same keep-class/mask-instance pattern as source-location). A UUID is
    // a UUID whether standalone or in a path; this is the largest re-measured residual
    // chunk and is squarely a SYNTACTIC class (D-TID-14), not a varying word.
    [[nodiscard]] inline bool normalize_embedded_identity(std::string_view tok, std::string& out)
    {
        static constexpr std::size_t kUuidLen{36};
        static constexpr std::array<std::size_t, 4> kUuidDashes{8, 13, 18, 23};
        static constexpr std::size_t kMinHashLen{16};
        const auto uuid_at{
            [&](std::size_t pos) -> bool
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
            }};
        out.clear();
        bool masked{false};
        std::size_t pos{0};
        while (pos < tok.size())
        {
            if (uuid_at(pos))
            {
                out.append(kWildcard);
                pos += kUuidLen;
                masked = true;
                continue;
            }
            // A maximal hex-only run ≥ kMinHashLen, bounded left by a non-hex char.
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

    // D-TID-13 extension (discovered at playground integration — LogCraft's KV-heavy
    // message templates + the pre-existing KV regression guards in raw_log_fidelity):
    // `<key>=<digit-leading-value>` → `<key>=<*>` (keep the key, mask the high-card
    // numeric value). A SYNTACTIC class (D-TID-14 IN scope), the same keep-class/
    // mask-instance pattern as the composites above. Without it a `key=<id>` over-splits
    // per value — and for an ERROR line that REINTRODUCES the singleton false-diff this
    // whole epic exists to kill (40 distinct `txn=<id>` error singletons → a phantom
    // new/vanished pair per run). EXCLUDES a status value (`code=0` / `status=500`) so a
    // green→red flip stays distinct — the KV form of the #1 status-value KEEP carve-out.
    // A value-WORD (`user=alice`) is NOT masked (value not digit-leading → kept); that
    // varying word is the deferred registry's job (D-TID-5/14). The CI-revert re-measure
    // (§8) did not surface this — CI tokens are space-separated, LogCraft wraps in `key=`.
    [[nodiscard]] inline bool normalize_kv_value(std::string_view tok, std::string& out)
    {
        const std::size_t eq_pos{tok.find('=')};
        if (eq_pos == 0 || eq_pos == std::string_view::npos || eq_pos + 1 >= tok.size())
            return false;
        const std::string_view key{tok.substr(0, eq_pos)};
        const std::string_view raw_value{tok.substr(eq_pos + 1)};
        // D-TID-22: strip a declared currency marker off the value before the digit-leading gate,
        // so `total=$463 → total=$<*>` (keep key AND marker, mask the amount). marker=0 for a bare
        // value → `total=463 → total=<*>` unchanged. A non-numeric core (`total=$HOME`) fails the
        // gate below → kept literal.
        const std::size_t marker{marker_prefix_len(raw_value)};
        const std::string_view value{raw_value.substr(marker)};
        if (!is_digit_leading(value))
            return false; // a value-word → kept literal (the registry's job), not masked
        // Status-value KEEP (KV form): code=0 / status=500 must stay DISTINCT (the green→red flip),
        // same context+size gate as the space-separated #1 carve-out. (Status values never carry a
        // currency marker, so the strip above is a no-op for them.)
        if (is_status_keyword(key) && is_all_digits(value) && value.size() <= kMaxStatusDigits)
            return false;
        out.clear();
        out.append(key);
        out.push_back('=');
        out.append(raw_value.substr(0, marker)); // keep the marker (empty when none)
        out.append(kWildcard);
        return true;
    }

    // ── The composite normalizer catalog (D-TID-12 step #2) ──────────────────────────
    // The KEEP-class / mask-instance rules, as a DECLARED array whose ORDER IS THE PRECEDENCE:
    // tried top-to-bottom, the first rule that claims the token wins (the former `||`
    // short-circuit, now data). Each rule is a pure `bool(tok, out&)` — fills `out` with the
    // normalized literal and returns true iff it claims the token. This catalog DEFINES the
    // composite layer of the ruleset generation named by kCanonicalizationVersion; adding,
    // reordering, or removing a rule is an output-affecting change that REQUIRES a version bump —
    // the single enumerable place that rule can be stated (closing the D-TID-16 "rules changed,
    // version didn't" gap for the rule set itself). Each entry names its governing ruling + the
    // generation that introduced it.
    struct CompositeRule
    {
        std::string_view name; // stable rule id (diagnostics / the canon bible)
        bool (*normalize)(std::string_view tok, std::string& out); // fills out; true iff claimed
    };
    // The two bracket rules are ADJACENT, most-specific first, and non-overlapping today
    // (bracket_timestamp requires a `-` where bracket_index requires `]`) — the ordering is
    // stated so future drift between them has a rule to violate loudly.
    constexpr std::array<CompositeRule, 9U> kCompositeRules{{
        {.name = "diagnostic_composite",
         .normalize = normalize_diagnostic_composite},                           // D-MSK-1  (-4)
        {.name = "ephemeral_root", .normalize = normalize_ephemeral_root},       // D-MSK-2  (-4)
        {.name = "versioned_ref", .normalize = normalize_versioned_ref},         // D-TID-12 #2
        {.name = "bracket_timestamp", .normalize = normalize_bracket_timestamp}, // D-MSK-5  (-8)
        {.name = "bracket_index", .normalize = normalize_bracket_index},         // D-TID-13(b)
        {.name = "hash_counter", .normalize = normalize_hash_counter},           // D-TID-13(a)
        {.name = "marker_number", .normalize = normalize_marker_number},         // D-TID-22 (-3)
        {.name = "embedded_identity", .normalize = normalize_embedded_identity}, // D-TID-12 #3
        {.name = "kv_value", .normalize = normalize_kv_value},                   // D-TID-17
    }};

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
            // Scan to the next space with memchr (vectorised in libc) — token bodies
            // (paths, ids, JSON) are the long runs; bit-identical boundaries to the
            // byte loop. memchr is never asymptotically worse for the short tokens.
            const void* const space{std::memchr(base + cursor, ' ', len - cursor)};
            cursor = space != nullptr
                         ? static_cast<std::size_t>(static_cast<const char*>(space) - base)
                         : len;
            callback(content.substr(start, cursor - start)); // lvalue: invoked per token
        }
    }

} // namespace

// ── Stateless per-line template masker (D-TID-1/2) ──────────────────────────────
// Per token, EMIT its canonical form by its OWN class — DECIDED per token (no cluster
// lookup, no cross-line discovery): status-value KEEP → the literal; bare-number / empty
// / IPv4 / hex / UUID / long-hash → "<*>" (a param); source-location / versioned-ref /
// bracket-index / #-counter / embedded-identity → the normalized literal (KEPT, NOT a
// param — it carries its own embedded "<*>"); any other token → the literal. The joined
// sequence is the template; the SHA-256 of it (computed downstream, unchanged) is the
// run-independent template_id. Pure: a function of `content`'s bytes only — no float, no
// map iteration, no state — so it is cross-stdlib bit-identical and order-/stream-
// independent by construction (D-TID-9).
StatelessTemplate stateless_template(std::string_view content, ArenaAllocator& out_arena,
                                     const MaskConfig& config)
{
    std::string tmpl;
    tmpl.reserve(content.size() + kWildcard.size());
    std::vector<std::string_view> params; // raw tokens at fully-masked positions
    std::string composite;                // scratch for composite normalization
    std::string_view prev{};              // raw previous token — context for status KEEP
    bool first{true};

    // The declared per-token classification in TOTAL precedence (D-TID-12 §8.2): KEEP
    // carve-outs win first, then the F13 masks. A masked position contributes a param
    // (the raw token); a kept/normalized position does not.
    for_each_token(
        content,
        [&](std::string_view tok)
        {
            if (!first)
                tmpl.push_back(' ');
            first = false;
            // One pass over the token's bytes → the shape facts the dispatch below
            // reads, replacing the separate is_all_digits / composite-trigger any_of /
            // is_digit_leading scans (byte-exact equivalents → identity unchanged).
            const TokenShape shape{tok};
            const auto mask{[&]
                            {
                                tmpl.append(kWildcard);
                                params.push_back(tok);
                            }};

            // 1. status-value KEEP (identity): "exit code 0" stays distinct
            //    from "exit code 1" — a green→red flip must not collapse.
            if (shape.all_digits && tok.size() <= kMaxStatusDigits && is_status_keyword(prev))
            {
                tmpl.append(tok);
                prev = tok;
                return;
            }
            // 2. composite → the normalized literal (KEEP class, mask instance). The
            //    declared rule set AND its precedence are kCompositeRules (tried in array
            //    order, first claim wins — the former `||` short-circuit, now data).
            //    maybe_composite is the cheap pre-gate that skips the whole catalog for a
            //    token carrying no separator (shape) and no declared currency marker.
            const bool maybe_composite{marker_prefix_len(tok) != 0 || shape.has_separator};
            if (maybe_composite)
            {
                for (const CompositeRule& rule : kCompositeRules)
                    if (rule.normalize(tok, composite))
                    {
                        tmpl.append(composite);
                        prev = tok;
                        return;
                    }
            }
            // 3. UUID / long hash → MASK.
            // 4. IPv4 / 0x-hex → MASK.
            // 5. digit-leading numeric (or empty) → MASK.
            if (shape.empty || is_uuid_or_long_hash(tok) ||
                (config.mask_ip_addresses && is_ipv4_token(tok)) ||
                (config.mask_hex_addresses && is_hex_token(tok)) || shape.digit_leading)
            {
                mask();
                prev = tok;
                return;
            }
            // 6. literal KEEP.
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

} // namespace insight::tokenization
