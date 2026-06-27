module insight.canon.detail.mask;
import insight.canon.internal; // std (via `export import std;` — includes std::memchr)
import insight.canon.api;

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

    [[nodiscard]] constexpr bool is_ascii_digit(char chr) noexcept
    {
        return static_cast<unsigned>(chr) - '0' < kDecimalBase;
    }

    [[nodiscard]] constexpr bool is_ascii_alpha(char chr) noexcept
    {
        const char lower{static_cast<char>(chr | static_cast<char>(kAsciiCaseMask))};
        return lower >= 'a' && lower <= 'z';
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

    // Composite-token masking, SOURCE_LOCATION class.
    //
    // The fixed masks mask only WHOLE tokens that are digit-leading / IPv4 / hex. A
    // compiler source location like `tokenizer.cpp:4500:30:` is one whitespace token that
    // is none of those, so it stays literal and every (file,line,col) becomes its
    // own template — a dominant cardinality blow-up.
    //
    // Recognize `<path>:<digits>[:<digits>]…[:]` where the prefix before the first
    // `:<digits>` is path-like (contains '.' or '/'), and normalize by masking each
    // numeric `:<digits>` run to `:<*>` while KEEPING the path (the semantic part:
    // which file). So `tokenizer.cpp:4500:30:` and `tokenizer.cpp:12:5:` collapse
    // to one template `tokenizer.cpp:<*>:<*>:`, but a different file stays distinct.
    //
    // Pure and deterministic (a function of the token bytes only — I5). Returns
    // true and fills `out` with the normalized form; returns false (leaving `out`
    // untouched) when `tok` is not a source location. The path-like requirement
    // keeps clock times (`12:30:45`) and bare ratios out — only `:<digits>` runs
    // behind a path prefix are masked.
    [[nodiscard]] inline bool normalize_source_location(std::string_view tok, std::string& out)
    {
        // Locate the first ':' that is immediately followed by a digit.
        std::size_t first{std::string_view::npos};
        for (std::size_t pos{0}; pos + 1 < tok.size(); ++pos)
        {
            if (tok[pos] == ':' && is_ascii_digit(tok[pos + 1]))
            {
                first = pos;
                break;
            }
        }
        if (first == std::string_view::npos)
            return false;

        const std::string_view prefix{tok.substr(0, first)};
        if (prefix.empty())
            return false;
        const bool path_like{
            std::ranges::any_of(prefix, [](char chr) { return chr == '.' || chr == '/'; })};
        if (!path_like)
            return false;

        out.clear();
        out.append(prefix);
        std::size_t cursor{first};
        while (cursor < tok.size())
        {
            if (tok[cursor] == ':' && cursor + 1 < tok.size() && is_ascii_digit(tok[cursor + 1]))
            {
                out.append(":<*>");
                ++cursor; // consume ':'
                while (cursor < tok.size() && is_ascii_digit(tok[cursor]))
                    ++cursor; // consume the digit run
            }
            else
            {
                out.push_back(tok[cursor]);
                ++cursor;
            }
        }
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
        if (!is_ascii_digit(tok[slash + 1]))
            return false; // version must start with a digit

        std::size_t cursor{slash + 1};
        bool saw_digit{false};
        while (cursor < tok.size() && (is_ascii_digit(tok[cursor]) || tok[cursor] == '.'))
        {
            saw_digit = saw_digit || is_ascii_digit(tok[cursor]);
            ++cursor;
        }
        if (!saw_digit)
            return false;
        // Anything after the version run must be non-alphanumeric (punctuation),
        // else this is a path segment ("v1/2x") not a terminal version.
        for (std::size_t pos{cursor}; pos < tok.size(); ++pos)
        {
            const char chr{tok[pos]};
            if (is_ascii_digit(chr) || (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z'))
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
        while (cursor < tok.size() && is_ascii_alpha(tok[cursor]))
            ++cursor; // optional class prefix inside the bracket ("gw", "worker")
        const std::string_view prefix{tok.substr(prefix_begin, cursor - prefix_begin)};
        bool saw_digit{false};
        while (cursor < tok.size() && is_ascii_digit(tok[cursor]))
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
        return pos < tok.size() && is_ascii_digit(tok[pos]);
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
        if (tok.size() < 2U || tok[0] != '#' || !is_ascii_digit(tok[1]))
            return false;
        std::size_t cursor{1};
        while (cursor < tok.size() && is_ascii_digit(tok[cursor]))
            ++cursor;
        for (std::size_t pos{cursor}; pos < tok.size(); ++pos)
            if (is_ascii_digit(tok[pos]) || is_ascii_alpha(tok[pos]))
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
        if (marker == 0 || !is_ascii_digit(tok[marker]))
            return false;
        std::size_t cursor{marker + 1};
        while (cursor < tok.size() && is_ascii_digit(tok[cursor]))
            ++cursor;
        if (cursor < tok.size() && tok[cursor] == '.')
        {
            const std::size_t frac{cursor + 1};
            cursor = frac;
            while (cursor < tok.size() && is_ascii_digit(tok[cursor]))
                ++cursor;
            if (cursor == frac)
                return false; // trailing '.' with no fraction → not a clean number
        }
        for (std::size_t pos{cursor}; pos < tok.size(); ++pos)
            if (is_ascii_digit(tok[pos]) || is_ascii_alpha(tok[pos]))
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
                       const auto mask{[&]
                                       {
                                           tmpl.append(kWildcard);
                                           params.push_back(tok);
                                       }};

                       // 1. status-value KEEP (identity): "exit code 0" stays distinct
                       //    from "exit code 1" — a green→red flip must not collapse.
                       if (is_all_digits(tok) && tok.size() <= kMaxStatusDigits &&
                           is_status_keyword(prev))
                       {
                           tmpl.append(tok);
                           prev = tok;
                           return;
                       }
                       // 2. composite → the normalized literal (KEEP class, mask instance):
                       //    source-location / versioned-ref / bracket-index / #-counter /
                       //    embedded UUID·hash / key=<numeric-value> / currency-marker number
                       //    (the `-` pre-gate admits dashed UUID tokens; `=` admits KV pairs; a
                       //    declared currency marker — D-TID-22 — admits `$463`).
                       const bool maybe_composite{
                           marker_prefix_len(tok) != 0 ||
                           std::ranges::any_of(tok, [](char chr) {
                               return chr == ':' || chr == '/' || chr == '[' || chr == '#' ||
                                      chr == '-' || chr == '=';
                           })};
                       if (maybe_composite && (normalize_source_location(tok, composite) ||
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
                       if (tok.empty() || is_uuid_or_long_hash(tok) ||
                           (config.mask_ip_addresses && is_ipv4_token(tok)) ||
                           (config.mask_hex_addresses && is_hex_token(tok)) || is_digit_leading(tok))
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
