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
        const auto flush_segment{[&](std::size_t end)
                                 {
                                     const std::string_view seg{tok.substr(seg_start, end - seg_start)};
                                     // core = segment with leading/trailing non-alnum stripped
                                     std::size_t lead{0};
                                     while (lead < seg.size() && !is_digit(seg[lead]) &&
                                            !is_alpha(seg[lead]))
                                         ++lead;
                                     std::size_t trail{seg.size()};
                                     while (trail > lead && !is_digit(seg[trail - 1]) &&
                                            !is_alpha(seg[trail - 1]))
                                         --trail;
                                     const std::string_view core{seg.substr(lead, trail - lead)};
                                     if (core.empty())
                                     {
                                         out.append(seg); // pure punctuation — keep verbatim
                                         prev_core = {};
                                         return;
                                     }
                                     if (is_alpha(core.front()))
                                     {
                                         out.append(seg); // letter-leading → KEEP (class anchor)
                                         has_letter_anchor = true;
                                         prev_core = core;
                                         return;
                                     }
                                     // digit-leading core: status-value carve-out, else mask.
                                     if (is_status_keyword(prev_core) && is_all_digits(core) &&
                                         core.size() <= kMaxStatusDigits)
                                     {
                                         out.append(seg); // exit:0 / status:500 → KEEP (categorical)
                                         prev_core = core;
                                         return;
                                     }
                                     out.append(seg.substr(0, lead)); // leading punct, e.g. '['
                                     out.append(kWildcard);
                                     out.append(seg.substr(trail)); // trailing punct, e.g. ']'
                                     masked = true;
                                     prev_core = core;
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

    // Ephemeral-root path catalog (D-MSK-2). A path token under a declared ephemeral root is a
    // per-run instance by construction (a randomized temp dir `/tmp/pw-electron-userdata-Kw9v4a`,
    // base-62 suffix — letter-leading, not hex/UUID, so the existing masks keep it literal → a new
    // template per run → false novelty). The random SUFFIX is undecidable (D-TID-18), but the
    // ROOT is an enumerable, byte-exact catalog (the kCurrencyMarkers discipline) — there is no
    // low-card stable keyword "child of /tmp" worth protecting, and a stable temp file is itself
    // non-diffable, so masking the post-root remainder is lossless for diffing. Mask `<root>/<*>`.
    // NOT a random-string classifier and NOT a general absolute-path masker (`/etc/hosts`,
    // `/usr/bin/foo` untouched). Checked AFTER the diagnostic composite, so a `/tmp/…:42` source
    // path keeps its `file:line` shape; a `/tmp/…` dir without `:digit` falls to this rule.
    inline constexpr std::array<std::string_view, 3> kEphemeralRoots{
        std::string_view{"/tmp"}, std::string_view{"/var/tmp"}, std::string_view{"/var/folders"}};

    [[nodiscard]] inline bool normalize_ephemeral_root(std::string_view tok, std::string& out)
    {
        for (const std::string_view root : kEphemeralRoots)
            // `<root>/<non-empty remainder>` — the '/' guard rejects `/tmpfoo`; the size guard
            // requires a remainder (a bare `/tmp` / `/tmp/` is not collapsed).
            if (tok.size() > root.size() + 1U && tok.starts_with(root) && tok[root.size()] == '/')
            {
                out.clear();
                out.append(root);
                out.push_back('/');
                out.append(kWildcard);
                return true;
            }
        return false;
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
    // digit core → returns false → kept literal. Byte-only, single-token → cross-stdlib bit-identical.
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
        const auto uuid_at{[&](std::size_t pos) -> bool
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
        const std::size_t eq{tok.find('=')};
        if (eq == 0 || eq == std::string_view::npos || eq + 1 >= tok.size())
            return false;
        const std::string_view key{tok.substr(0, eq)};
        const std::string_view raw_value{tok.substr(eq + 1)};
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
    for_each_token(content,
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
                       if (shape.all_digits && tok.size() <= kMaxStatusDigits &&
                           is_status_keyword(prev))
                       {
                           tmpl.append(tok);
                           prev = tok;
                           return;
                       }
                       // 2. composite → the normalized literal (KEEP class, mask instance):
                       //    diagnostic-composite (source-location + Chromium/glog prefix, D-MSK-1) /
                       //    ephemeral-root path (D-MSK-2) / versioned-ref / bracket-index / #-counter /
                       //    embedded UUID·hash / key=<numeric-value> / currency-marker number
                       //    (the `-` pre-gate admits dashed UUID tokens; `=` admits KV pairs; a
                       //    declared currency marker — D-TID-22 — admits `$463`).
                       const bool maybe_composite{marker_prefix_len(tok) != 0 || shape.has_separator};
                       if (maybe_composite && (normalize_diagnostic_composite(tok, composite) ||
                                               normalize_ephemeral_root(tok, composite) ||
                                               normalize_versioned_ref(tok, composite) ||
                                               normalize_bracket_index(tok, composite) ||
                                               normalize_hash_counter(tok, composite) ||
                                               normalize_marker_number(tok, composite) ||
                                               normalize_embedded_identity(tok, composite) ||
                                               normalize_kv_value(tok, composite)))
                       {
                           tmpl.append(composite);
                           prev = tok;
                           return;
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
        auto* buf{static_cast<std::string_view*>(
            out_arena.allocate(params.size() * sizeof(std::string_view), alignof(std::string_view)))};
        const auto buf_span{std::span<std::string_view>{buf, params.size()}};
        std::ranges::copy(params, buf_span.begin());
        params_span = buf_span;
    }
    return {.template_str = tmpl_view, .params = params_span};
}

} // namespace insight::tokenization
