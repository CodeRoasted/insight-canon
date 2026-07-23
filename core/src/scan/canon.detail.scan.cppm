// insight.canon.detail.scan — SEALED scanning foundation (1.5.2 domain decomposition, §11.9.11).
// The fast_gates layer: branch-light constexpr char/prefix predicates + the SSE2 sv_* parsing
// primitives the strategy confidence()/parse() hot paths ride. Interface-only (everything inline /
// constexpr — no impl units). Bottom of the canon detail DAG: imports internal only; SSE2
// intrinsics live in the textual GMF.
// Never re-exported by the facade and never installed (PRIVATE file set).
module;
// SSE2 is ABI-baseline on every x86-64 target, but the compilers ANNOUNCE it differently:
// gcc/clang define __SSE2__ (here implied by -march=x86-64-v2); MSVC never does — it signals x64
// via _M_X64 (and 32-bit SSE2 via _M_IX86_FP>=2). Detect the x86-64 target portably, then pull the
// SSE2 intrinsics from each toolchain's header (<emmintrin.h> on gcc/clang/MSVC; <intrin.h> is the
// MSVC umbrella that includes it). INSIGHT_CANON_SSE2 then gates BOTH the include AND the use
// below, so a non-x86 target compiles the scalar fallback rather than failing on undeclared
// intrinsics.
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
// A pure feature-test gate consumed only by `#ifdef INSIGHT_CANON_SSE2` (never as a value),
// so it carries no `1` — that is what makes it a compile-time switch rather than a constant.
#define INSIGHT_CANON_SSE2
#include <emmintrin.h> // SSE2 intrinsics (fast_gates)
#endif

export module insight.canon.detail.scan;
import insight.canon.internal; // std + global C types

// ──────── from src/insight/tokenization/strategies/detail/fast_gates.hpp ────────
// fast_gates.hpp
//
// Branch-light, allocation-free predicates used by strategy `confidence()`
// hot paths. All inline, all noexcept. The point is to shave the ~50–100 ns
// per-strategy-probe RE2 cost (PartialMatch + char class scans) down to a
// handful of cycles for non-matching strategies and ~10 ns for matches.
//
// Each predicate corresponds to a deterministic prefix/anchor pattern that
// the strategy's parse() already checks rigorously. confidence() only needs
// a fast yes/no — false positives are caught by parse(); false negatives are
// caught nowhere, so every predicate must be a *strict subset* of what its
// parse() pattern accepts at the same anchor position.
//
// The sv_* parsing primitives used in parse() hot paths are also here.
// They use SSE2 SIMD (guaranteed on -march=x86-64-v2) for whitespace
// scanning. The constexpr char predicates above remain constexpr; the
// sv_* parse helpers are plain inline (runtime-only).

// SSE2 is guaranteed by -march=x86-64-v2 (baseline x86-64 v2 ABI).

export namespace insight::tokenization
{

// ── Character class primitives ───────────────────────────────────────────

[[nodiscard]] constexpr bool is_digit(char chr) noexcept
{
    static constexpr unsigned kDecimalRadix{10U}; // range of decimal digits [0-9]
    return static_cast<unsigned>(chr) - '0' < kDecimalRadix;
}

[[nodiscard]] constexpr bool is_upper(char chr) noexcept
{
    static constexpr unsigned kAlphabetSize{26U}; // range of alpha letters [A-Z]
    return static_cast<unsigned>(chr) - 'A' < kAlphabetSize;
}

[[nodiscard]] constexpr bool is_lower(char chr) noexcept
{
    static constexpr unsigned kAlphabetSize{26U}; // range of alpha letters [a-z]
    return static_cast<unsigned>(chr) - 'a' < kAlphabetSize;
}

// ASCII letter [A-Za-z] — the canonical alpha predicate for the tokenization-detail world
// (mask / format_detector route here instead of carrying private copies). Composed from the
// trusted upper/lower pair, so a non-ASCII byte (signed-char-negative) is false by construction.
[[nodiscard]] constexpr bool is_alpha(char chr) noexcept
{
    return is_upper(chr) || is_lower(chr);
}

[[nodiscard]] constexpr bool is_space(char chr) noexcept
{
    // POSIX [ \t]; the strategy patterns use \s+ but only space+tab occur
    // in practice for these prefix checks.
    return chr == ' ' || chr == '\t';
}

// ── TokenShape — one-pass per-token byte profile (stateless_template_id.md §8.2) ──────
// The masker classifies each whitespace token KEEP / MASK / NORMALIZE in a fixed precedence
// (D-TID-12). Several steps of that dispatch each re-walked the token: is_all_digits (the
// status-value KEEP), the composite-trigger any_of, and is_digit_leading (the digit mask).
// TokenShape walks the token's bytes ONCE and records the facts they all need, so the dispatch
// reads fields instead of re-scanning. Each field is the byte-exact equivalent of the scan it
// replaces — same KEEP/MASK/NORMALIZE decision per token → masked template (hence template_id)
// unchanged. Pure byte-only, single-token, no float, order-independent → cross-stdlib + MSVC
// bit-identical (the D-TID-9 oracle). The composite normalizers keep their own segment walks;
// this is the shared primitive for the common-case dispatch, and the seam A2's rule catalog reads.
struct TokenShape
{
    bool empty{true};       // the token has no bytes
    bool all_digits{false}; // non-empty AND every byte is an ASCII digit (== is_all_digits)
    bool digit_leading{
        false}; // first byte after an optional +/- sign is a digit (== is_digit_leading)
    bool has_separator{
        false}; // contains a composite separator : / [ # - =  (the maybe_composite gate)

    explicit constexpr TokenShape(std::string_view tok) noexcept : empty(tok.empty())
    {
        if (empty)
            return;
        std::size_t lead{0};
        if (tok[0] == '+' || tok[0] == '-')
            ++lead;
        digit_leading = lead < tok.size() && is_digit(tok[lead]);
        all_digits = true;
        for (const char chr : tok)
        {
            all_digits = all_digits && is_digit(chr);
            has_separator = has_separator || chr == ':' || chr == '/' || chr == '[' || chr == '#' ||
                            chr == '-' || chr == '=';
        }
    }
};

// Skip 1+ leading spaces/tabs starting at pos. Returns new offset.
[[nodiscard]] constexpr std::size_t skip_spaces(std::string_view str, std::size_t pos) noexcept
{
    while (pos < str.size() && is_space(str[pos]))
        ++pos;
    return pos;
}

// Skip exactly N digits starting at pos. Returns true on success and advances pos.
[[nodiscard]] constexpr bool consume_digits(std::string_view str, std::size_t& pos,
                                            std::size_t min_n, std::size_t max_n) noexcept
{
    const std::size_t start{pos};
    while (pos < str.size() && (pos - start) < max_n && is_digit(str[pos]))
        ++pos;
    return (pos - start) >= min_n;
}

// ── Constants shared between predicate and parse() ───────────────────────
// Strategy parse() methods import this module and use these directly; they
// cannot be function-local.
constexpr std::size_t kBsdMinLen{15U};  // "Mon DD HH:MM:SS" minimum
constexpr std::size_t kHdfsMinLen{16U}; // "YYMMDD HHMMSS digit" minimum
// kGhaPrefixLen + is_github_actions_prefix relocated to insight_semantic_github (ADR 0024 §1.2 —
// GHA line-format detection is dialect knowledge). is_rfc3339_prefix below stays (Syslog uses it).

// ── Composite prefix predicates ──────────────────────────────────────────
// Each returns true iff `str` begins (at offset 0 unless noted) with the
// shape described by its name.

// "YYYY-MM-DD" at offset `pos`.
[[nodiscard]] constexpr bool match_iso_date_at(std::string_view str, std::size_t pos) noexcept
{
    static constexpr std::size_t kIsoDateLen{10U}; // "YYYY-MM-DD"
    static constexpr std::size_t kIsoMon1{5U};     // first month digit
    static constexpr std::size_t kIsoMon2{6U};     // second month digit
    static constexpr std::size_t kIsoSep2{7U};     // '-' before day
    static constexpr std::size_t kIsoDd1{8U};      // first day digit
    static constexpr std::size_t kIsoDd2{9U};      // second day digit
    if (pos + kIsoDateLen > str.size())
        return false;
    return is_digit(str[pos]) && is_digit(str[pos + 1]) && is_digit(str[pos + 2]) &&
           is_digit(str[pos + 3]) && str[pos + 4] == '-' && is_digit(str[pos + kIsoMon1]) &&
           is_digit(str[pos + kIsoMon2]) && str[pos + kIsoSep2] == '-' &&
           is_digit(str[pos + kIsoDd1]) && is_digit(str[pos + kIsoDd2]);
}

// "HH:MM:SS" at offset `pos`.
[[nodiscard]] constexpr bool match_time_at(std::string_view str, std::size_t pos) noexcept
{
    static constexpr std::size_t kTimeLen{8U};    // "HH:MM:SS"
    static constexpr std::size_t kTimeColon2{5U}; // second ':' separator
    static constexpr std::size_t kTimeSec1{6U};   // first second digit
    static constexpr std::size_t kTimeSec2{7U};   // second second digit
    if (pos + kTimeLen > str.size())
        return false;
    return is_digit(str[pos]) && is_digit(str[pos + 1]) && str[pos + 2] == ':' &&
           is_digit(str[pos + 3]) && is_digit(str[pos + 4]) && str[pos + kTimeColon2] == ':' &&
           is_digit(str[pos + kTimeSec1]) && is_digit(str[pos + kTimeSec2]);
}

// "Mon DD HH:MM:SS" — BSD syslog date. e.g. "Jan 15 08:03:22"
[[nodiscard]] constexpr bool is_bsd_syslog_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kTimeLen{
        8U}; // "HH:MM:SS" — for the size guard before match_time_at
    if (str.size() < kBsdMinLen)
        return false;
    if (!is_upper(str[0]) || !is_lower(str[1]) || !is_lower(str[2]))
        return false;
    std::size_t pos{3};
    pos = skip_spaces(str, pos);
    if (pos >= str.size() || !is_digit(str[pos]))
        return false;
    if (!consume_digits(str, pos, 1U, 2U))
        return false;
    pos = skip_spaces(str, pos);
    if (pos + kTimeLen > str.size())
        return false;
    return match_time_at(str, pos);
}

// "YYYY-MM-DDTHH:MM:SS" — RFC 3339 prefix (T separator).
[[nodiscard]] constexpr bool is_rfc3339_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kRfc3339TAt{10U};    // position of 'T' separator
    static constexpr std::size_t kRfc3339TimeAt{11U}; // time field start
    return match_iso_date_at(str, 0) && str.size() > kRfc3339TAt && str[kRfc3339TAt] == 'T' &&
           match_time_at(str, kRfc3339TimeAt);
}

// (is_github_actions_prefix relocated to insight_semantic_github — ADR 0024 §1.2.)

// "YYYY-MM-DD HH:MM:SS" — log4j / windows_cbs / iis_w3c shape.
// Optionally requires a trailing fractional separator ('.' or ',').
[[nodiscard]] constexpr bool is_iso_datetime_space_prefix(std::string_view str,
                                                          bool require_fraction) noexcept
{
    static constexpr std::size_t kIsoDateLen{10U}; // "YYYY-MM-DD"
    static constexpr std::size_t kTimeLen{8U};     // "HH:MM:SS"
    if (!match_iso_date_at(str, 0))
        return false;
    if (str.size() <= kIsoDateLen || !is_space(str[kIsoDateLen]))
        return false;
    std::size_t pos{skip_spaces(str, kIsoDateLen)};
    if (!match_time_at(str, pos))
        return false;
    if (!require_fraction)
        return true;
    pos += kTimeLen;
    return pos < str.size() && (str[pos] == '.' || str[pos] == ',');
}

// "YYYY/MM/DD HH:MM:SS [" — nginx error prefix.
[[nodiscard]] constexpr bool is_nginx_error_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kNginxMinLen{22U};
    static constexpr std::size_t kNginxMon1{5U};
    static constexpr std::size_t kNginxMon2{6U};
    static constexpr std::size_t kNginxSlash2{7U};
    static constexpr std::size_t kNginxDd1{8U};
    static constexpr std::size_t kNginxDd2{9U};
    static constexpr std::size_t kNginxSpaceAt{10U};
    static constexpr std::size_t kNginxTimeAt{11U};
    static constexpr std::size_t kNginxScanFrom{19U};
    static constexpr std::size_t kNginxScanTo{32U};
    if (str.size() < kNginxMinLen)
        return false;
    if (!(is_digit(str[0]) && is_digit(str[1]) && is_digit(str[2]) && is_digit(str[3]) &&
          str[4] == '/' && is_digit(str[kNginxMon1]) && is_digit(str[kNginxMon2]) &&
          str[kNginxSlash2] == '/' && is_digit(str[kNginxDd1]) && is_digit(str[kNginxDd2])))
        return false;
    if (!is_space(str[kNginxSpaceAt]))
        return false;
    if (!match_time_at(str, kNginxTimeAt))
        return false;
    // Find next '['; allow some space after time.
    for (std::size_t i{kNginxScanFrom}; i < str.size() && i < kNginxScanTo; ++i)
        if (str[i] == '[')
            return true;
    return false;
}

// "YY/MM/DD HH:MM:SS" — spark style (two-digit year-month-day).
[[nodiscard]] constexpr bool is_spark_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kSparkMinLen{17U};
    static constexpr std::size_t kSparkSlash2{5U};
    static constexpr std::size_t kSparkMon1{6U};
    static constexpr std::size_t kSparkMon2{7U};
    static constexpr std::size_t kSparkSpaceAt{8U};
    static constexpr std::size_t kSparkTimeAt{9U};
    if (str.size() < kSparkMinLen)
        return false;
    if (!(is_digit(str[0]) && is_digit(str[1]) && str[2] == '/' && is_digit(str[3]) &&
          is_digit(str[4]) && str[kSparkSlash2] == '/' && is_digit(str[kSparkMon1]) &&
          is_digit(str[kSparkMon2])))
        return false;
    if (!is_space(str[kSparkSpaceAt]))
        return false;
    return match_time_at(str, kSparkTimeAt);
}

// "YYMMDD HHMMSS digits..." — HDFS style (two 6-digit groups + a number).
[[nodiscard]] constexpr bool is_hdfs_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kHdfsDateLen{6U};
    static constexpr std::size_t kHdfsSpaceAt{6U};
    static constexpr std::size_t kHdfsTimeStart{7U};
    static constexpr std::size_t kHdfsTimeEnd{13U};
    static constexpr std::size_t kHdfsLastSpace{13U};
    static constexpr std::size_t kHdfsLastDigit{14U};
    if (str.size() < kHdfsMinLen)
        return false;
    for (std::size_t i{0}; i < kHdfsDateLen; ++i)
        if (!is_digit(str[i]))
            return false;
    if (!is_space(str[kHdfsSpaceAt]))
        return false;
    for (std::size_t i{kHdfsTimeStart}; i < kHdfsTimeEnd; ++i)
        if (!is_digit(str[i]))
            return false;
    return is_space(str[kHdfsLastSpace]) && is_digit(str[kHdfsLastDigit]);
}

// "[DD.MM HH:MM:SS]" — proxifier prefix.
[[nodiscard]] constexpr bool is_proxifier_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kProxifierMinLen{17U};
    static constexpr std::size_t kProxifierMm2{5U};
    static constexpr std::size_t kProxifierSpaceAt{6U};
    static constexpr std::size_t kProxifierTimeAt{7U};
    static constexpr std::size_t kProxifierBracket{15U};
    if (str.size() < kProxifierMinLen || str[0] != '[')
        return false;
    if (!(is_digit(str[1]) && is_digit(str[2]) && str[3] == '.' && is_digit(str[4]) &&
          is_digit(str[kProxifierMm2])))
        return false;
    if (!is_space(str[kProxifierSpaceAt]))
        return false;
    if (!match_time_at(str, kProxifierTimeAt))
        return false;
    return str.size() > kProxifierBracket && str[kProxifierBracket] == ']';
}

// "<NNN>D " — RFC 5424 PRI + version, then ISO date.
[[nodiscard]] constexpr bool is_rfc5424_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kRfc5424MinLen{6U};
    if (str.size() < kRfc5424MinLen || str[0] != '<')
        return false;
    std::size_t pos{1U};
    if (!consume_digits(str, pos, 1U, 3U))
        return false;
    if (pos >= str.size() || str[pos] != '>')
        return false;
    ++pos;
    if (pos >= str.size() || !is_digit(str[pos]))
        return false;
    ++pos;
    if (pos >= str.size() || !is_space(str[pos]))
        return false;
    return match_iso_date_at(str, skip_spaces(str, pos));
}

// "[Mon Mon DD HH:MM:SS" — apache error prefix. e.g. "[Tue Apr 27 10:15:22"
[[nodiscard]] constexpr bool is_apache_error_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kApacheMinLen{22U};
    static constexpr std::size_t kApacheMon1{5U};
    static constexpr std::size_t kApacheMon2{6U};
    static constexpr std::size_t kApacheMon3{7U};
    static constexpr std::size_t kApacheDayAt{8U};
    if (str.size() < kApacheMinLen || str[0] != '[')
        return false;
    if (!(is_upper(str[1]) && is_lower(str[2]) && is_lower(str[3]) && is_space(str[4]) &&
          is_upper(str[kApacheMon1]) && is_lower(str[kApacheMon2]) && is_lower(str[kApacheMon3])))
        return false;
    std::size_t pos{kApacheDayAt};
    pos = skip_spaces(str, pos);
    if (pos + 2U > str.size() || !is_digit(str[pos]) || !is_digit(str[pos + 1]))
        return false;
    pos += 2U;
    pos = skip_spaces(str, pos);
    return match_time_at(str, pos);
}

// "- N YYYY.MM.DD" — BGL prefix.
[[nodiscard]] constexpr bool is_bgl_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kBglMinLen{14U};
    static constexpr std::size_t kBglMaxDigits{20U};
    static constexpr std::size_t kBglDateLen{10U};
    static constexpr std::size_t kBglMon1{5U};
    static constexpr std::size_t kBglMon2{6U};
    static constexpr std::size_t kBglSep2{7U};
    static constexpr std::size_t kBglDay1{8U};
    static constexpr std::size_t kBglDay2{9U};
    if (str.size() < kBglMinLen || str[0] != '-')
        return false;
    std::size_t pos{1U};
    pos = skip_spaces(str, pos);
    if (!consume_digits(str, pos, 1U, kBglMaxDigits))
        return false;
    pos = skip_spaces(str, pos);
    if (pos + kBglDateLen > str.size())
        return false;
    return is_digit(str[pos]) && is_digit(str[pos + 1]) && is_digit(str[pos + 2]) &&
           is_digit(str[pos + 3]) && str[pos + 4] == '.' && is_digit(str[pos + kBglMon1]) &&
           is_digit(str[pos + kBglMon2]) && str[pos + kBglSep2] == '.' &&
           is_digit(str[pos + kBglDay1]) && is_digit(str[pos + kBglDay2]);
}

// "DDDDDDDD-H:MM:S:MMM|" — health app prefix. e.g. "20171223-22:15:29:606|"
[[nodiscard]] constexpr bool is_health_app_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kHealthMinLen{18U};
    static constexpr std::size_t kHealthDateLen{8U};
    static constexpr std::size_t kHealthSepAt{8U};
    static constexpr std::size_t kHealthTimeAt{9U};
    if (str.size() < kHealthMinLen)
        return false;
    for (std::size_t i{0}; i < kHealthDateLen; ++i)
        if (!is_digit(str[i]))
            return false;
    if (str[kHealthSepAt] != '-')
        return false;
    std::size_t pos{kHealthTimeAt};
    if (!consume_digits(str, pos, 1U, 2U))
        return false;
    if (pos >= str.size() || str[pos] != ':')
        return false;
    ++pos;
    if (!consume_digits(str, pos, 2U, 2U))
        return false;
    if (pos >= str.size() || str[pos] != ':')
        return false;
    ++pos;
    if (!consume_digits(str, pos, 1U, 2U))
        return false;
    if (pos >= str.size() || str[pos] != ':')
        return false;
    ++pos;
    if (!consume_digits(str, pos, 3U, 3U))
        return false;
    return pos < str.size() && str[pos] == '|';
}

// HPC prefix: "N <s> <s> <s> NNNNNNNNNN N "
// (id, source, target, ?, big-timestamp(>=10 digits), id, space).
[[nodiscard]] constexpr bool is_hpc_prefix(std::string_view str) noexcept
{
    static constexpr std::size_t kHpcMaxDigits{20U};
    static constexpr std::size_t kHpcMinTsLen{10U};
    std::size_t pos{0};
    if (!consume_digits(str, pos, 1U, kHpcMaxDigits))
        return false;
    // Three "non-space tokens".
    for (int i{0}; i < 3; ++i)
    {
        pos = skip_spaces(str, pos);
        if (pos >= str.size() || is_space(str[pos]))
            return false;
        while (pos < str.size() && !is_space(str[pos]))
            ++pos;
    }
    pos = skip_spaces(str, pos);
    // 10+ digit timestamp.
    const std::size_t ts_start{pos};
    while (pos < str.size() && is_digit(str[pos]))
        ++pos;
    if (pos - ts_start < kHpcMinTsLen)
        return false;
    pos = skip_spaces(str, pos);
    if (pos >= str.size() || !is_digit(str[pos]))
        return false;
    while (pos < str.size() && is_digit(str[pos]))
        ++pos;
    return pos < str.size() && is_space(str[pos]);
}

// CLF anchor: locate "[DD/Mon/YYYY:HH:MM:SS" anywhere in the line.
// Linear scan; in CLF the bracket is near the start, so this is cheap.
[[nodiscard]] constexpr bool has_clf_timestamp(std::string_view str) noexcept
{
    static constexpr std::size_t kClfMinLen{22U};
    static constexpr std::size_t kClfMon3{5U};
    static constexpr std::size_t kClfSlash2{6U};
    static constexpr std::size_t kClfYear1{7U};
    static constexpr std::size_t kClfYear2{8U};
    static constexpr std::size_t kClfYear3{9U};
    static constexpr std::size_t kClfYear4{10U};
    static constexpr std::size_t kClfColon{11U};
    static constexpr std::size_t kClfTimeAt{12U};
    if (str.size() < kClfMinLen)
        return false;
    const std::size_t limit{str.size() - 21U};
    for (std::size_t i{0}; i <= limit; ++i)
    {
        if (str[i] != '[')
            continue;
        const std::size_t pos{i + 1U};
        // DeMorgan: !(A && B && ...) → !A || !B || ...
        if (!is_digit(str[pos]) || !is_digit(str[pos + 1]) || str[pos + 2] != '/' ||
            !is_upper(str[pos + 3]) || !is_lower(str[pos + 4]) || !is_lower(str[pos + kClfMon3]) ||
            str[pos + kClfSlash2] != '/')
            continue;
        if (!is_digit(str[pos + kClfYear1]) || !is_digit(str[pos + kClfYear2]) ||
            !is_digit(str[pos + kClfYear3]) || !is_digit(str[pos + kClfYear4]) ||
            str[pos + kClfColon] != ':')
            continue;
        if (match_time_at(str, pos + kClfTimeAt))
            return true;
    }
    return false;
}

// Find a kv-style '=' between two non-space, non-quote chars.
// Used as the substring gate before running the more expensive RE2.
[[nodiscard]] constexpr std::size_t count_kv_pair_signatures(std::string_view str,
                                                             std::size_t cap) noexcept
{
    std::size_t found{0};
    for (std::size_t i{1U}; i + 1U < str.size() && found < cap; ++i)
    {
        if (str[i] != '=')
            continue;
        const char prev{str[i - 1]};
        const char next{str[i + 1]};
        // key char before
        const bool key_ok{is_lower(prev) || is_upper(prev) || is_digit(prev) || prev == '_' ||
                          prev == '.' || prev == '-'};
        // value start (non-space, non-equals)
        const bool val_ok{!is_space(next) && next != '='};
        if (key_ok && val_ok)
            ++found;
    }
    return found;
}

// ── Zero-copy scanning primitives for parse() hot paths ──────────────────
// `line` in parse() is already arena-stable (LogParser copies it before
// calling parse()). These helpers slice string_views directly from `line`
// — no heap copies needed for any split field.
//
// SIMD note: find_non_ws_ptr / find_ws_ptr use SSE2 16-byte scans when INSIGHT_CANON_SSE2 is
// defined (every x86-64 target — see the GMF guard), and a scalar byte loop otherwise. The two
// paths return identical pointers by construction (the scalar loop is the SIMD remainder handler,
// reused as the whole-range path), so the canonical digest is invariant to the SSE2 decision.

namespace simd_detail
{

    constexpr int kSseWidth{16};             // 128-bit SSE register = 16 bytes
    constexpr unsigned kSseMaskAll{0xFFFFU}; // 16-bit lane mask for 16-byte block

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast, bugprone-not-null-terminated-result)
    // SSE2 intrinsic path: raw pointer stride and _mm_loadu_si128 reinterpret_cast are required by
    // the intrinsic API. find_non_ws_ptr / find_ws_ptr operate on bounded ranges expressed as
    // string_view — no null termination is assumed or required.

    // Returns pointer to the first non-whitespace byte in str,
    // or str.data() + str.size() if all bytes are whitespace.
    [[nodiscard]] inline const char* find_non_ws_ptr(std::string_view str) noexcept
    {
        const char* ptr = str.data();
        const char* const end = str.data() + str.size();
#ifdef INSIGHT_CANON_SSE2
        const __m128i vsp = _mm_set1_epi8(' ');
        const __m128i vtab = _mm_set1_epi8('\t');
        while (ptr + kSseWidth <= end)
        {
            const __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
            // mask bit i = 1 means byte i is space or tab
            const auto mask = static_cast<unsigned>(_mm_movemask_epi8(
                _mm_or_si128(_mm_cmpeq_epi8(chunk, vsp), _mm_cmpeq_epi8(chunk, vtab))));
            if ((mask & kSseMaskAll) != kSseMaskAll)
            {
                // At least one non-ws byte in this block. countr_zero (std <bit>) replaces the
                // gcc/clang-only __builtin_ctz: bit-exact for the non-zero input guaranteed here
                // (~mask masked to kSseMaskAll has a clear bit ⇒ never 0), and portable to MSVC so
                // the SSE2 path is identical across all three compilers (cross-OS determinism leg).
                ptr += std::countr_zero(~mask & kSseMaskAll);
                return ptr;
            }
            ptr += kSseWidth;
        }
#endif
        // Scalar path: the SIMD remainder, or the WHOLE range on a non-SSE2 target. Byte-for-byte
        // identical result to the SIMD block above — it IS the correctness reference the SIMD path
        // mirrors — so the canonical digest is invariant to whether SSE2 was compiled in.
        while (ptr < end && is_space(*ptr))
            ++ptr;
        return ptr;
    }

    // Returns pointer to the first whitespace byte in str,
    // or str.data() + str.size() if no whitespace exists.
    [[nodiscard]] inline const char* find_ws_ptr(std::string_view str) noexcept
    {
        const char* ptr = str.data();
        const char* const end = str.data() + str.size();
#ifdef INSIGHT_CANON_SSE2
        const __m128i vsp = _mm_set1_epi8(' ');
        const __m128i vtab = _mm_set1_epi8('\t');
        while (ptr + kSseWidth <= end)
        {
            const __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
            const auto mask = static_cast<unsigned>(_mm_movemask_epi8(
                _mm_or_si128(_mm_cmpeq_epi8(chunk, vsp), _mm_cmpeq_epi8(chunk, vtab))));
            if ((mask & kSseMaskAll) != 0U)
            {
                // (mask & kSseMaskAll) != 0 guarantees a set bit ⇒ countr_zero input is non-zero ⇒
                // bit-exact with the retired __builtin_ctz, portable to MSVC.
                ptr += std::countr_zero(mask & kSseMaskAll);
                return ptr;
            }
            ptr += kSseWidth;
        }
#endif
        // Scalar path: SIMD remainder, or the whole range on a non-SSE2 target (see
        // find_non_ws_ptr).
        while (ptr < end && !is_space(*ptr))
            ++ptr;
        return ptr;
    }

    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast, bugprone-not-null-terminated-result)

} // namespace simd_detail

// Skip leading whitespace in-place (SIMD-accelerated).
inline void sv_skip_ws(std::string_view& str) noexcept
{
    const char* const ptr = simd_detail::find_non_ws_ptr(str);
    str = str.substr(static_cast<std::size_t>(ptr - str.data()));
}

// Take next non-whitespace token; advance str past token + trailing whitespace.
// Returns the token (empty if str is all-whitespace).
// Every substr pos is in-bounds: find_*_ptr returns a pointer in [data, data+size],
// and the all-whitespace case (non_ws_off == size) early-returns before any substr.
// Every substr has pos <= size, so this noexcept body cannot throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] inline std::string_view sv_take_token(std::string_view& str) noexcept
{
    const char* const non_ws = simd_detail::find_non_ws_ptr(str);
    const auto non_ws_off = static_cast<std::size_t>(non_ws - str.data());
    if (non_ws_off == str.size())
    {
        str = {};
        return {};
    }
    const std::string_view remaining{str.substr(non_ws_off)};
    const char* const ws_ptr = simd_detail::find_ws_ptr(remaining);
    const auto tok_size = static_cast<std::size_t>(ws_ptr - remaining.data());
    const std::string_view tok{remaining.substr(0, tok_size)};

    // Skip trailing whitespace; leave str positioned at the next token start.
    const std::string_view after_tok{remaining.substr(tok_size)};
    const char* const next = simd_detail::find_non_ws_ptr(after_tok);
    str = after_tok.substr(static_cast<std::size_t>(next - after_tok.data()));
    return tok;
}

// Take chars up to (not including) delim; advance str past delim.
// If delim not found, returns all of str and sets str to {}.
// substr(0,pos) has pos arg 0; substr(pos+1U) runs only when find() returned a valid
// index pos < size (npos returns early), so pos+1 <= size — the noexcept body cannot throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] constexpr std::string_view sv_take_until(std::string_view& str, char delim) noexcept
{
    const auto pos = str.find(delim);
    if (pos == std::string_view::npos)
    {
        const auto result = str;
        str = {};
        return result;
    }
    const auto result = str.substr(0, pos);
    str = str.substr(pos + 1U);
    return result;
}

// Take exactly n chars; advance str by n (capped at str.size()).
// substr(0,actual) has pos arg 0; substr(actual) has actual = min(n,size) <= size — cannot throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] constexpr std::string_view sv_take_n(std::string_view& str, std::size_t n) noexcept
{
    const auto actual = n < str.size() ? n : str.size();
    const auto result = str.substr(0, actual);
    str = str.substr(actual);
    return result;
}

// Expect '['; take until ']'; advance past ']'.
// Returns inner content; returns {} and leaves str advanced if not '['.
[[nodiscard]] constexpr std::string_view sv_take_bracketed(std::string_view& str) noexcept
{
    if (str.empty() || str[0] != '[')
        return {};
    str.remove_prefix(1U);
    return sv_take_until(str, ']');
}

// Expect '"'; take until closing '"'; advance past '"'.
// Returns inner content (without quotes); returns {} if str doesn't start with '"'.
[[nodiscard]] constexpr std::string_view sv_take_quoted(std::string_view& str) noexcept
{
    if (str.empty() || str[0] != '"')
        return {};
    str.remove_prefix(1U);
    return sv_take_until(str, '"');
}

// Parse a syslog "process[pid]:" tag — the daemon/program name (F3b functional source).
// Returns the name before '[' or ':' (the `[pid]` is identity → stripped); advances `str`
// past the ']' and ':' and trailing whitespace, so `str` is left at the message body. Shared
// by SyslogStrategy and BGLStrategy (the Thunderbird branch) — one tag extractor, no dup.
[[nodiscard]] inline std::string_view extract_syslog_tag(std::string_view& str) noexcept
{
    const auto delim{str.find_first_of("[:")};
    if (delim == std::string_view::npos)
    {
        const std::string_view tag{str};
        str = {};
        return tag;
    }
    // `delim` is a found index (< size), so these are noexcept in-place trims rather than `substr`,
    // whose out-of-range `throw` path the analyzer cannot rule out here (bugprone-exception-escape).
    std::string_view tag{str};
    tag.remove_suffix(str.size() - delim); // keep bytes [0, delim)
    while (!tag.empty() && is_space(tag.back()))
        tag.remove_suffix(1U);
    str.remove_prefix(delim);
    if (!str.empty() && str[0] == '[')
    {
        const auto rbracket{str.find(']')};
        str.remove_prefix(rbracket != std::string_view::npos ? rbracket + 1U : 1U);
    }
    if (!str.empty() && str[0] == ':')
        str.remove_prefix(1U);
    sv_skip_ws(str);
    return tag;
}

// ── ANSI / terminal escape stripping (stateless_template_id.md D-TID-11) ─────────
// The terminal escape-grammar byte ranges (ECMA-48): a CSI body is params then
// intermediates then one final byte; OSC runs to a BEL or ST terminator.
inline constexpr unsigned char kEsc{0x1bU};        // ESC, the escape introducer
inline constexpr unsigned char kBel{0x07U};        // BEL, an OSC terminator
inline constexpr unsigned char kCsiParamLo{0x30U}; // CSI parameter bytes 0–9:;<=>?
inline constexpr unsigned char kCsiParamHi{0x3fU};
inline constexpr unsigned char kCsiInterLo{0x20U}; // CSI intermediate bytes (space..'/')
inline constexpr unsigned char kCsiInterHi{0x2fU};
inline constexpr unsigned char kCsiFinalLo{0x40U}; // CSI final byte ('@'..'~', incl. SGR 'm')
inline constexpr unsigned char kCsiFinalHi{0x7eU};

// Advance past a CSI body (params* intermediates* final?). `pos` is the index just
// after the `ESC [` introducer; returns the index of the first post-sequence byte.
[[nodiscard]] inline std::size_t scan_csi_body(std::string_view line, std::size_t pos) noexcept
{
    const std::size_t len{line.size()};
    const auto byte_at{[&](std::size_t idx) { return static_cast<unsigned char>(line[idx]); }};
    while (pos < len && byte_at(pos) >= kCsiParamLo && byte_at(pos) <= kCsiParamHi)
        ++pos;
    while (pos < len && byte_at(pos) >= kCsiInterLo && byte_at(pos) <= kCsiInterHi)
        ++pos;
    if (pos < len && byte_at(pos) >= kCsiFinalLo && byte_at(pos) <= kCsiFinalHi)
        ++pos;
    return pos;
}

// Advance past an OSC body to its BEL or ST (ESC \) terminator (consumed). `pos` is
// the index just after the `ESC ]` introducer.
[[nodiscard]] inline std::size_t scan_osc_body(std::string_view line, std::size_t pos) noexcept
{
    const std::size_t len{line.size()};
    const auto byte_at{[&](std::size_t idx) { return static_cast<unsigned char>(line[idx]); }};
    while (pos < len)
    {
        if (byte_at(pos) == kBel)
            return pos + 1U;
        if (byte_at(pos) == kEsc && pos + 1U < len && line[pos + 1U] == '\\')
            return pos + 2U;
        ++pos;
    }
    return pos;
}

// Strip CSI / SGR / OSC and bare-ESC terminal escape sequences from a line as an
// UNCONDITIONAL content normalization at canon ingest — BEFORE tokenization. Colour
// is presentation, never content (D-TID-10); the escapes interleave within/between
// tokens (`\x1b[31mERROR\x1b[0m`) so a per-token mask cannot reach them — they must
// die here. A pure byte state machine: no float, order-independent → cross-stdlib
// bit-identical (the same grammar the TTY `sanitize()` drops, re-homed at ingest).
// Appends the cleaned bytes to `out` (cleared first); result ≤ input, so a reused
// buffer makes this allocation-free in steady state.
inline void strip_escape_sequences(std::string_view line, std::string& out)
{
    out.clear();
    out.reserve(line.size());
    const std::size_t len{line.size()};
    std::size_t pos{0};
    while (pos < len)
    {
        if (static_cast<unsigned char>(line[pos]) != kEsc)
        {
            out.push_back(line[pos]);
            ++pos;
            continue;
        }
        if (pos + 1U >= len)
            break; // a lone trailing ESC — drop it
        const char introducer{line[pos + 1U]};
        if (introducer == '[')
            pos = scan_csi_body(line, pos + 2U);
        else if (introducer == ']')
            pos = scan_osc_body(line, pos + 2U);
        else
            pos += 2U; // a simple two-byte ESC sequence (charset select, reset, …)
    }
}

// ── Echoed-source detection (D-PROV-1) relocated to insight_semantic_github (ADR 0024 §1.2) ──
// The GHA command-echo SGR catalog (`\x1b[36;1m … \x1b[0m`), parse_sgr_params, and
// is_echoed_source_line are dialect knowledge — they now live in the github package's code tier and
// reach LogParser as a composed ProvenanceHook. strip_escape_sequences (above) stays core: it is a
// universal ANSI ingest normalization, not dialect-specific.

} // namespace insight::tokenization
