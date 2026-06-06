// insight.canon.detail — SEALED internal surface (1.5.1 unwrap, §11.9). The former src/ headers
// (token_scan template, parsed_line, IFormatStrategy, fast_gates SIMD predicates, simdjson_scratch, the
// 20 format strategies, drain/format_detector/log_parser declarations). EXPORTED so canon's impl units
// can `import insight.canon.detail` — but the facade does NOT re-export it, so consumers never see it.
// Third-party + SIMD live in the textual GMF (not in import std); std comes from insight.canon.internal.
module;
#ifdef __SSE2__
#include <emmintrin.h> // SSE2 intrinsics (fast_gates) — guaranteed by -march=x86-64-v2
#endif

export module insight.canon.detail;
import insight.canon.internal; // std + global C types
import insight.canon.api;      // public types (LogFormat, ArenaAllocator, DrainConfig, CanonicalEvent, ...)

// ──────── from src/insight/tokenization/parsed_line.hpp ────────
export namespace insight::tokenization
{

// Intermediate representation produced by a format strategy.
//
// All string_view fields point into arena-managed storage. They remain
// valid until the owning ArenaAllocator is reset or destroyed.
//
// raw_line   — the original line, copied into the arena by LogParser before
//              the strategy is invoked.
// component  — component / tag extracted by the strategy and stored into the
//              arena via ArenaAllocator::store_string().
// content    — message body fed to the Drain tokeniser, also arena-stored.
struct ParsedLine
{
    std::string_view raw_line;
    std::optional<Timestamp> timestamp;
    LogLevel level{LogLevel::Unknown};
    std::string_view component;
    std::string_view content;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/format_strategy.hpp ────────
export namespace insight::tokenization
{

class IFormatStrategy
{
  public:
    IFormatStrategy() = default;
    IFormatStrategy(const IFormatStrategy&) = delete;
    IFormatStrategy& operator=(const IFormatStrategy&) = delete;
    IFormatStrategy(IFormatStrategy&&) = delete;
    IFormatStrategy& operator=(IFormatStrategy&&) = delete;
    virtual ~IFormatStrategy() = default;

    // Parse a single log line.
    //
    // The input string_view must remain valid for the duration of the call
    // (raw_line in the result borrows from it). Owned scalar fields
    // (component, content) are copied into the supplied arena via
    // ArenaAllocator::store_string(); their string_views remain valid until
    // the arena is reset or destroyed.
    [[nodiscard]] virtual std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const = 0;

    [[nodiscard]] virtual LogFormat format() const noexcept = 0;

    // Returns a [0,1] confidence score that this strategy matches the line.
    // Used by FormatDetector for majority-vote detection.  Must be O(1).
    [[nodiscard]] virtual double confidence(std::string_view line) const noexcept = 0;
};

} // namespace insight::tokenization

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

export namespace insight::tokenization::detail
{

// ── Digit / alpha ranges ──────────────────────────────────────────────────
constexpr unsigned kDecimalRadix{10U}; // range of decimal digits [0-9]
constexpr unsigned kAlphabetSize{26U}; // range of alpha letters [A-Z] / [a-z]

// ── Common timestamp field sizes ──────────────────────────────────────────
constexpr std::size_t kTimeLen{8U};     // "HH:MM:SS"
constexpr std::size_t kIsoDateLen{10U}; // "YYYY-MM-DD"

// ── Positions within HH:MM:SS at base `pos` (pos + X) ────────────────────
constexpr std::size_t kTimeColon2{5U}; // second ':' separator
constexpr std::size_t kTimeSec1{6U};   // first second digit
constexpr std::size_t kTimeSec2{7U};   // second second digit

// ── Positions within YYYY-MM-DD at base `pos` (pos + X) ──────────────────
constexpr std::size_t kIsoMon1{5U}; // first month digit
constexpr std::size_t kIsoMon2{6U}; // second month digit
constexpr std::size_t kIsoSep2{7U}; // '-' before day
constexpr std::size_t kIsoDd1{8U};  // first day digit
constexpr std::size_t kIsoDd2{9U};  // second day digit

// ── BSD syslog "Mon DD HH:MM:SS" ─────────────────────────────────────────
constexpr std::size_t kBsdMinLen{15U};

// ── RFC 3339 "YYYY-MM-DDTHH:MM:SS" ──────────────────────────────────────
constexpr std::size_t kRfc3339TAt{10U};    // position of 'T' separator
constexpr std::size_t kRfc3339TimeAt{11U}; // time field start

// ── GitHub Actions "YYYY-MM-DDTHH:MM:SS.fffffffZ" ───────────────────────
// RFC 3339 + .NET 100-ns ticks: exactly 7 fractional digits then 'Z'. This
// precise sub-second shape is GHA's signature and distinguishes it from
// syslog's whole-second / millisecond RFC 3339.
constexpr std::size_t kGhaDotAt{19U};     // '.' immediately after HH:MM:SS
constexpr std::size_t kGhaFracAt{20U};    // first of the 7 fractional digits
constexpr std::size_t kGhaFracLen{7U};    // exactly 7 digits (100-ns ticks)
constexpr std::size_t kGhaZAt{27U};       // trailing 'Z'
constexpr std::size_t kGhaPrefixLen{28U}; // full "...Z" timestamp length

// ── Nginx error "YYYY/MM/DD HH:MM:SS [" ─────────────────────────────────
constexpr std::size_t kNginxMinLen{22U};
constexpr std::size_t kNginxMon1{5U};
constexpr std::size_t kNginxMon2{6U};
constexpr std::size_t kNginxSlash2{7U};
constexpr std::size_t kNginxDd1{8U};
constexpr std::size_t kNginxDd2{9U};
constexpr std::size_t kNginxSpaceAt{10U};
constexpr std::size_t kNginxTimeAt{11U};
constexpr std::size_t kNginxScanFrom{19U};
constexpr std::size_t kNginxScanTo{32U};

// ── Spark "YY/MM/DD HH:MM:SS" ────────────────────────────────────────────
constexpr std::size_t kSparkMinLen{17U};
constexpr std::size_t kSparkSlash2{5U};
constexpr std::size_t kSparkMon1{6U};
constexpr std::size_t kSparkMon2{7U};
constexpr std::size_t kSparkSpaceAt{8U};
constexpr std::size_t kSparkTimeAt{9U};

// ── HDFS "YYMMDD HHMMSS digit" ───────────────────────────────────────────
constexpr std::size_t kHdfsMinLen{16U};
constexpr std::size_t kHdfsDateLen{6U};
constexpr std::size_t kHdfsSpaceAt{6U};
constexpr std::size_t kHdfsTimeStart{7U};
constexpr std::size_t kHdfsTimeEnd{13U};
constexpr std::size_t kHdfsLastSpace{13U};
constexpr std::size_t kHdfsLastDigit{14U};

// ── Proxifier "[DD.MM HH:MM:SS]" ─────────────────────────────────────────
constexpr std::size_t kProxifierMinLen{17U};
constexpr std::size_t kProxifierMm2{5U};
constexpr std::size_t kProxifierSpaceAt{6U};
constexpr std::size_t kProxifierTimeAt{7U};
constexpr std::size_t kProxifierBracket{15U};

// ── RFC 5424 "<NNN>D ..." ─────────────────────────────────────────────────
constexpr std::size_t kRfc5424MinLen{6U};

// ── Apache error "[Mon Mon DD HH:MM:SS YYYY]" ────────────────────────────
constexpr std::size_t kApacheMinLen{22U};
constexpr std::size_t kApacheMon1{5U};
constexpr std::size_t kApacheMon2{6U};
constexpr std::size_t kApacheMon3{7U};
constexpr std::size_t kApacheDayAt{8U};

// ── BGL "- N YYYY.MM.DD" ──────────────────────────────────────────────────
constexpr std::size_t kBglMinLen{14U};
constexpr std::size_t kBglMaxDigits{20U};
constexpr std::size_t kBglDateLen{10U};
constexpr std::size_t kBglMon1{5U};
constexpr std::size_t kBglMon2{6U};
constexpr std::size_t kBglSep2{7U};
constexpr std::size_t kBglDay1{8U};
constexpr std::size_t kBglDay2{9U};

// ── Health App "DDDDDDDD-H:MM:SS:MMM|" ───────────────────────────────────
constexpr std::size_t kHealthMinLen{18U};
constexpr std::size_t kHealthDateLen{8U};
constexpr std::size_t kHealthSepAt{8U};
constexpr std::size_t kHealthTimeAt{9U};

// ── HPC (long epoch timestamp) ────────────────────────────────────────────
constexpr std::size_t kHpcMaxDigits{20U};
constexpr std::size_t kHpcMinTsLen{10U};

// ── CLF "[DD/Mon/YYYY:HH:MM:SS" ──────────────────────────────────────────
// At bracket_start = i+1: [0..1]=day, [2]='/', [3..5]=month, [6]='/', [7..10]=year, [11]=':',
// [12..]=time
constexpr std::size_t kClfMinLen{22U};
constexpr std::size_t kClfMon3{5U};
constexpr std::size_t kClfSlash2{6U};
constexpr std::size_t kClfYear1{7U};
constexpr std::size_t kClfYear2{8U};
constexpr std::size_t kClfYear3{9U};
constexpr std::size_t kClfYear4{10U};
constexpr std::size_t kClfColon{11U};
constexpr std::size_t kClfTimeAt{12U};

// ── SSE2 SIMD ────────────────────────────────────────────────────────────
constexpr int kSseWidth{16};             // 128-bit SSE register = 16 bytes
constexpr unsigned kSseMaskAll{0xFFFFU}; // 16-bit lane mask for 16-byte block

// ── Character class primitives ───────────────────────────────────────────

[[nodiscard]] constexpr bool is_digit(char chr) noexcept
{
    return static_cast<unsigned>(chr) - '0' < kDecimalRadix;
}

[[nodiscard]] constexpr bool is_upper(char chr) noexcept
{
    return static_cast<unsigned>(chr) - 'A' < kAlphabetSize;
}

[[nodiscard]] constexpr bool is_lower(char chr) noexcept
{
    return static_cast<unsigned>(chr) - 'a' < kAlphabetSize;
}

[[nodiscard]] constexpr bool is_space(char chr) noexcept
{
    // POSIX [ \t]; the strategy patterns use \s+ but only space+tab occur
    // in practice for these prefix checks.
    return chr == ' ' || chr == '\t';
}

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

// ── Composite prefix predicates ──────────────────────────────────────────
// Each returns true iff `str` begins (at offset 0 unless noted) with the
// shape described by its name.

// "Mon DD HH:MM:SS" — BSD syslog date. e.g. "Jan 15 08:03:22"
[[nodiscard]] constexpr bool is_bsd_syslog_prefix(std::string_view str) noexcept
{
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
    // HH:MM:SS
    if (pos + kTimeLen > str.size())
        return false;
    return is_digit(str[pos]) && is_digit(str[pos + 1]) && str[pos + 2] == ':' &&
           is_digit(str[pos + 3]) && is_digit(str[pos + 4]) && str[pos + kTimeColon2] == ':' &&
           is_digit(str[pos + kTimeSec1]) && is_digit(str[pos + kTimeSec2]);
}

// "YYYY-MM-DD" at offset `pos`.
[[nodiscard]] constexpr bool match_iso_date_at(std::string_view str, std::size_t pos) noexcept
{
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
    if (pos + kTimeLen > str.size())
        return false;
    return is_digit(str[pos]) && is_digit(str[pos + 1]) && str[pos + 2] == ':' &&
           is_digit(str[pos + 3]) && is_digit(str[pos + 4]) && str[pos + kTimeColon2] == ':' &&
           is_digit(str[pos + kTimeSec1]) && is_digit(str[pos + kTimeSec2]);
}

// "YYYY-MM-DDTHH:MM:SS" — RFC 3339 prefix (T separator).
[[nodiscard]] constexpr bool is_rfc3339_prefix(std::string_view str) noexcept
{
    return match_iso_date_at(str, 0) && str.size() > kRfc3339TAt && str[kRfc3339TAt] == 'T' &&
           match_time_at(str, kRfc3339TimeAt);
}

// "YYYY-MM-DDTHH:MM:SS.fffffffZ" — GitHub Actions / Azure Pipelines line prefix.
// A *strict subset* of is_rfc3339_prefix (exactly 7 fractional digits + 'Z'),
// so GitHubActionsStrategy can outrank SyslogStrategy only on genuine GHA lines
// while leaving real RFC 3339 syslog to Syslog.
[[nodiscard]] constexpr bool is_github_actions_prefix(std::string_view str) noexcept
{
    if (str.size() < kGhaPrefixLen)
        return false;
    if (!is_rfc3339_prefix(str))
        return false;
    if (str[kGhaDotAt] != '.')
        return false;
    for (std::size_t pos{kGhaFracAt}; pos < kGhaFracAt + kGhaFracLen; ++pos)
        if (!is_digit(str[pos]))
            return false;
    if (str[kGhaZAt] != 'Z')
        return false;
    // Timestamp is the whole line (a blank GHA line) or is followed by a space.
    return str.size() == kGhaPrefixLen || is_space(str[kGhaPrefixLen]);
}

// "YYYY-MM-DD HH:MM:SS" — log4j / windows_cbs / iis_w3c shape.
// Optionally requires a trailing fractional separator ('.' or ',').
[[nodiscard]] constexpr bool is_iso_datetime_space_prefix(std::string_view str,
                                                          bool require_fraction) noexcept
{
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
// SIMD note: find_non_ws_ptr / find_ws_ptr use SSE2 16-byte scans.
// -march=x86-64-v2 guarantees __SSE2__; the scalar fallback compiles for
// other targets.

namespace simd_detail
{

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
                // At least one non-ws byte in this block.
                ptr += __builtin_ctz(~mask & kSseMaskAll);
                return ptr;
            }
            ptr += kSseWidth;
        }
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
        const __m128i vsp = _mm_set1_epi8(' ');
        const __m128i vtab = _mm_set1_epi8('\t');
        while (ptr + kSseWidth <= end)
        {
            const __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
            const auto mask = static_cast<unsigned>(_mm_movemask_epi8(
                _mm_or_si128(_mm_cmpeq_epi8(chunk, vsp), _mm_cmpeq_epi8(chunk, vtab))));
            if ((mask & kSseMaskAll) != 0U)
            {
                ptr += __builtin_ctz(mask & kSseMaskAll);
                return ptr;
            }
            ptr += kSseWidth;
        }
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

} // namespace insight::tokenization::detail

// ──────── from src/insight/tokenization/strategies/android_logcat.hpp ────────
export namespace insight::tokenization
{

/// Parses Android logcat format:
///   "03-17 16:13:38.811 1702 2395 D WindowManager: msg"
class AndroidLogcatStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/apache_error.hpp ────────
export namespace insight::tokenization
{

/// Parses Apache error-log format:
///   "[Sun Dec 04 04:47:44 2005] [notice] workerEnv.init() ok"
class ApacheErrorLogStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/bgl.hpp ────────
export namespace insight::tokenization
{

/// Parses BlueGene/L (BGL) and Thunderbird supercomputer log formats:
///   BGL:         "- 1117838570 2005.06.03 R02-M1 ... RAS KERNEL INFO msg"
///   Thunderbird: "- 1131566461 2005.11.09 dn228 Nov 9 12:01:01 dn228 crond:
///   msg"
class BGLStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/clf.hpp ────────
export namespace insight::tokenization
{

// Common Log Format / Combined Log Format (Apache/Nginx access logs)
class CLFStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;

  private:
    [[nodiscard]] static std::optional<Timestamp>
    parse_clf_timestamp(std::string_view timestamp_str);
    [[nodiscard]] static LogLevel status_code_to_level(int status);
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/cloudwatch.hpp ────────
export namespace insight::tokenization
{

/// Parses AWS CloudWatch JSON log format:
///   {"timestamp":1705312200000,"message":"User login","logGroup":"/aws/lambda/myFunc"}
class CloudWatchStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/github_actions.hpp ────────
export namespace insight::tokenization
{

// GitHubActionsStrategy — parses GitHub Actions / Azure Pipelines log lines.
//
// Every line is prefixed with an RFC 3339 UTC timestamp at .NET 100-ns
// precision (exactly 7 fractional digits + 'Z'), then a single space, then the
// raw message — which may carry workflow-command annotations
// ("##[error]", "##[warning]", "##[group]", "##[endgroup]", legacy "::error::").
//
// Without this strategy these lines are claimed by SyslogStrategy (they share
// the RFC 3339 prefix), which mis-parses them — eating the first message token
// as a fake hostname and collapsing timestamp-only lines into the empty
// template. CI logs are a primary product input, so GHA is first-class here:
//   - the timestamp is stripped and the full message is templated (real shape
//     preserved, e.g. "CODEROAST_IPC_REPO: <*>");
//   - the level is lifted from any "##[error]"/"##[warning]"/"##[notice]"/
//     "##[debug]" (or legacy "::error::") marker;
//   - a timestamp-only line is a blank line: parse() declines it so it is
//     dropped, never inflating an empty "" template cluster.
class GitHubActionsStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/health_app.hpp ────────
export namespace insight::tokenization
{

/// Parses pipe-delimited HealthApp log format:
///   "20171223-22:15:29:606|Step_LSC|30002312|onStandStepChanged 3579"
class HealthAppStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/hpc.hpp ────────
export namespace insight::tokenization
{

/// Parses HPC (High Performance Computing) log format:
///   "134681 node-246 unix.hw state_change.unavailable 1077804742 1 Component
///   State Change: ..."
class HPCStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/iis_w3c.hpp ────────
export namespace insight::tokenization
{

/// Parses IIS W3C Extended Log Format:
///   "2024-01-15 10:30:00 GET /index.html - 80 - 10.0.0.1 Mozilla/5.0 200 0 0 15"
class IISW3CStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/json.hpp ────────
export namespace insight::tokenization
{

class JsonStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;

  private:
    static constexpr std::array<std::string_view, 5> kTimestampKeys{"timestamp", "ts", "@timestamp",
                                                                    "time", "datetime"};
    static constexpr std::array<std::string_view, 4> kLevelKeys{"level", "severity", "loglevel",
                                                                "log_level"};
    static constexpr std::array<std::string_view, 5> kMessageKeys{"message", "msg", "log", "text",
                                                                  "body"};
    static constexpr std::array<std::string_view, 5> kComponentKeys{"component", "source", "logger",
                                                                    "service", "module"};
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/kv.hpp ────────
export namespace insight::tokenization
{

class KVStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;

  private:
    // Views into the original (arena-stable) line; values are un-quoted slices.
    struct KVPair
    {
        std::string_view key;
        std::string_view value;
    };
    [[nodiscard]] static std::vector<KVPair> extract_pairs(std::string_view line);
    [[nodiscard]] static std::optional<Timestamp> try_parse_timestamp(std::string_view value);
    [[nodiscard]] static LogLevel try_parse_level(std::string_view value);
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/log4j.hpp ────────
export namespace insight::tokenization
{

/// Parses Java Log4j / Python logging formats:
///   Hadoop:    "2015-10-18 18:01:47,978 INFO [main] org.apache.hadoop: msg"
///   Zookeeper: "2015-07-29 17:41:44,747 - INFO  [QuorumPeer] - msg"
///   OpenStack: "nova-api.log 2017-05-16 00:00:00.008 25746 INFO nova.osapi
///   [req-id] msg"
class Log4jStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/nginx_error.hpp ────────
export namespace insight::tokenization
{

/// Parses Nginx error-log format:
///   "2024/03/27 10:15:23 [error] 12345#0: *99 connect() failed"
class NginxErrorStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/proxifier.hpp ────────
export namespace insight::tokenization
{

/// Parses Proxifier log format:
///   "[10.30 16:49:06] chrome.exe - proxy.cse.cuhk.edu.hk:5070 open through
///   proxy ..."
class ProxifierStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/raw_text.hpp ────────
export namespace insight::tokenization
{

// Last-resort catch-all for unstructured text (CI / pytest / build logs).
//
// The FormatDetector selects this strategy ONLY when no structured strategy
// matches a non-empty line, so the tokenizer never silently drops a line.
//
// Performance: parse() is zero-copy — `content` is a subview of the (already
// arena-stable) input, produced by trimming leading ASCII whitespace with
// pointer arithmetic. No allocation, no full-line scan. confidence() is a
// constant 0.0, which both keeps it out of the normal majority vote and stops
// LogParser's sticky fast-path from ever latching onto it (a >0 confidence
// would greedily capture every following line).
class RawTextStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/rfc5424.hpp ────────
export namespace insight::tokenization
{

/// Parses RFC 5424 syslog format:
///   "<PRI>VERSION TIMESTAMP HOSTNAME APP-NAME PROCID MSGID [SD] MSG"
///   e.g. "<134>1 2024-01-15T10:30:00Z myhost myapp 1234 ID47 - User logged in"
class RFC5424Strategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/spark_hdfs.hpp ────────
export namespace insight::tokenization
{

/// Parses Spark and HDFS log formats:
///   Spark: "17/06/09 20:10:40 INFO executor.Backend: msg"
///   HDFS:  "081109 203615 148 INFO dfs.DataNode: msg"
class SparkHDFSStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/syslog.hpp ────────
export namespace insight::tokenization
{

class SyslogStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;

  private:
    [[nodiscard]] static std::optional<Timestamp>
    parse_bsd_timestamp(std::string_view timestamp_str);
    [[nodiscard]] static std::optional<Timestamp>
    parse_iso_timestamp(std::string_view timestamp_str);
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/systemd_journal.hpp ────────
export namespace insight::tokenization
{

/// Parses systemd journal export format (JSON):
///   {"__REALTIME_TIMESTAMP":"1705312200000000","PRIORITY":"6",
///    "_COMM":"nginx","MESSAGE":"Worker started"}
class SystemdJournalStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/strategies/windows_cbs.hpp ────────
export namespace insight::tokenization
{

/// Parses Windows CBS/CSI log format:
///   "2016-09-28 04:30:30, Info    CBS    Loaded Servicing Stack ..."
class WindowsCBSStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/drain.hpp ────────
export namespace insight::tokenization
{

// (ArenaAllocator forward-decl dropped — imported from insight.canon.api; redeclaring it in this
//  module's purview conflicts with the import under C++20 modules.)
class Drain
{
  public:
    explicit Drain(DrainConfig config = {});
    ~Drain();

    Drain(const Drain&) = delete;
    Drain& operator=(const Drain&) = delete;
    Drain(Drain&&) noexcept;
    Drain& operator=(Drain&&) noexcept;

    // Hot-path variant: returns arena-stable string_views, zero heap
    // allocations on the line path. Both `template_str` and `params` are
    // valid until `out_arena.reset()` (or destruction).
    struct ArenaMatchResult
    {
        TemplateID template_id{};
        std::string_view template_str; // empty when render == TemplateRender::Skip
        std::span<const std::string_view> params;
        bool new_cluster{false};
    };

    // Controls whether template_str is built in match_into_arena.
    // Most callers routing on template_id alone can pass Skip to save ~5-10 ns.
    enum class TemplateRender : std::uint8_t
    {
        Eager,
        Skip
    };

    [[nodiscard]] ArenaMatchResult match_into_arena(std::string_view content,
                                                    ArenaAllocator& out_arena,
                                                    TemplateRender render = TemplateRender::Eager);

    // Lookup
    [[nodiscard]] std::optional<std::string> get_template(TemplateID tmpl_id) const;
    [[nodiscard]] std::size_t cluster_count() const noexcept;
    [[nodiscard]] std::size_t total_matched() const noexcept;

    // Maintenance
    void prune(std::size_t max_clusters);
    void reset();

    // Dump all templates (for debugging/serialization)
    [[nodiscard]] std::map<TemplateID, std::string> all_templates() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/format_detector.hpp ────────
export namespace insight::tokenization
{

class FormatDetector
{
  public:
    FormatDetector();

    void register_strategy(std::unique_ptr<IFormatStrategy> strategy);

    // Returns best-matching strategy for a given line
    [[nodiscard]] IFormatStrategy* detect(std::string_view line) const;

    // Detect from a sample batch (majority vote)
    [[nodiscard]] IFormatStrategy*
    detect_from_batch(std::span<const std::string_view> sample) const;

    // Get all registered strategies
    [[nodiscard]] std::span<const std::unique_ptr<IFormatStrategy>> strategies() const noexcept;

  private:
    static constexpr std::size_t kFormatSlotCount =
        static_cast<std::size_t>(LogFormat::Unknown) + 1U;

    std::vector<std::unique_ptr<IFormatStrategy>> strategies_;
    std::vector<IFormatStrategy*> custom_strategies_;
    std::array<IFormatStrategy*, kFormatSlotCount> by_format_{};

    // Last-resort catch-all. Used only when no structured strategy scores on a
    // non-empty line, so unstructured text is templated rather than dropped.
    std::unique_ptr<IFormatStrategy> fallback_;
};

} // namespace insight::tokenization

// ──────── from src/insight/tokenization/log_parser.hpp ────────
export namespace insight::tokenization
{

// LogParser wraps arena + FormatDetector + active strategy.
// Thread-safety: NOT thread-safe; use one instance per thread / strand.
class LogParser
{
  public:
    explicit LogParser(ArenaAllocator& arena);

    // Force a specific format; disables auto-detection.
    void set_format(LogFormat fmt);

    // Enable / disable per-line auto-detection (default: enabled).
    void set_auto_detect(bool enabled);

    // Parse a single line. Once a strategy is selected, the line is copied into
    // the arena so string_views inside the returned ParsedLine are stable.
    [[nodiscard]] std::expected<ParsedLine, std::string> parse_line(std::string_view line);

    // Like parse_line() but skips the arena store_string() copy.
    // The caller guarantees that `stable_line` and all string_views sliced from
    // it remain valid for the arena's lifetime (e.g. mmap'd or pre-stored buffers).
    [[nodiscard]] std::expected<ParsedLine, std::string> parse_stable(std::string_view stable_line);

    [[nodiscard]] std::vector<std::expected<ParsedLine, std::string>>
    parse_batch(std::span<const std::string_view> lines);

    [[nodiscard]] std::size_t lines_parsed() const noexcept;
    [[nodiscard]] std::size_t lines_failed() const noexcept;
    [[nodiscard]] LogFormat detected_format() const noexcept;

  private:
    // Selects the active strategy for the given line, updating sticky/active
    // state as a side-effect. Returns nullptr if no strategy matches.
    [[nodiscard]] IFormatStrategy* select_strategy(std::string_view line);
    ArenaAllocator& arena_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members): parser is
                            // a non-owning facade over a caller-managed arena.
    FormatDetector detector_;
    IFormatStrategy* active_strategy_{nullptr};
    // Sticky: remembers the last auto-detected strategy. Tried first on each
    // line to short-circuit the O(strategies) detection scan for homogeneous
    // streams (the common case). Falls back to full detection when confidence
    // returns 0.0 (format change) or on the first line.
    IFormatStrategy* sticky_strategy_{nullptr};
    bool auto_detect_{true};
    std::size_t parsed_count_{0};
    std::size_t failed_count_{0};
};

} // namespace insight::tokenization
