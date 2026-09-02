module;

module insight.canon.api;
import insight.canon.internal;

// core/src/insight/utils/time_utils.cpp
// Timestamp parsing helpers.  All functions are noexcept and return
// std::nullopt on any malformed input rather than throwing.

// NOLINTBEGIN(readability-function-cognitive-complexity)
// NOLINTBEGIN(readability-magic-numbers)
namespace insight::utils
{

namespace time_constants
{

    inline constexpr int kUnixEpochYear{1970};
    inline constexpr int kTmYearOffset{1900};
    inline constexpr int kPivotShortYear{70};
    inline constexpr int kCenturyYears{100};
    inline constexpr int kLeapCycleYears{400};
    inline constexpr int kMonthsPerYear{12};
    inline constexpr int kDaysPerCommonYear{365};
    inline constexpr int kDaysPerLeapYear{366};
    inline constexpr std::int64_t kSecondsPerMinute{60};
    inline constexpr std::int64_t kMinutesPerHour{60};
    inline constexpr std::int64_t kSecondsPerHour{kMinutesPerHour * kSecondsPerMinute};
    inline constexpr std::int64_t kHoursPerDay{24};
    inline constexpr std::int64_t kSecondsPerDay{kHoursPerDay * kSecondsPerHour};
    // Representable-year window for a system_clock time_point. Its duration is
    // signed-int64 nanoseconds (~292 years either side of 1970), so a parsed year
    // outside this range overflows the seconds→nanoseconds conversion (UB). A
    // malformed far-future/past year is not a valid log timestamp; clamp to the
    // safe window with margin for the in-month day/time arithmetic.
    inline constexpr int kMinReprYear{1678};
    inline constexpr int kMaxReprYear{2261};
    inline constexpr int kMaxDayOfMonth{31};
    inline constexpr std::size_t kIso8601MinLength{19};
    inline constexpr std::size_t kBsdSyslogMinLength{15};
    inline constexpr std::size_t kClfMinLength{20};
    inline constexpr std::size_t kEpochTimestampMaxDigits{12};
    // OTLP timeUnixNano: epoch nanoseconds. ~19 digits at the current epoch; 19 keeps it within
    // int64 (max ≈ 9.2e18) and rejects an overflowing 20-digit value at the length gate.
    inline constexpr std::size_t kUnixNanoMaxDigits{19};
    inline constexpr std::size_t kCompactDateWidth{6};
    inline constexpr std::size_t kShortYearSlashMinLength{17};
    inline constexpr std::size_t kApacheErrorMinLength{24};
    inline constexpr std::size_t kHealthAppMinLength{18};
    inline constexpr std::size_t kLog4jMinLength{23};
    inline constexpr std::size_t kNginxErrorMinLength{19};

} // namespace time_constants

namespace
{

    // ─────────────────────────────────────────────────────────────────────────────
    // Internal helpers
    // ─────────────────────────────────────────────────────────────────────────────

    // Parse a fixed-width ASCII integer from [ptr, ptr+length).
    // Returns false if any character is not a digit.
    bool parse_fixed(const char* ptr, int length, int& out_value) noexcept
    {
        // from_chars takes `const char*` by the standard. Pass the raw pointers directly: a
        // string_view's begin()/end() are `const char*` on libstdc++/libc++ but a wrapper class
        // (_String_view_iterator) on MSVC's STL that does NOT convert to const char* — the bytes
        // and parse are identical, this is just the portable spelling of the same call.
        const char* const last{ptr + length};
        const auto parse_result{std::from_chars(ptr, last, out_value)};
        return parse_result.ec == std::errc{} && parse_result.ptr == last;
    }

    // Fast 2-digit ASCII integer parser — avoids from_chars overhead for
    // fixed-width fields in the ISO-8601 hot path.
    [[nodiscard]] inline bool parse2d(const char* ptr, int& out) noexcept
    {
        const unsigned dig0{static_cast<unsigned>(ptr[0]) - '0'};
        const unsigned dig1{static_cast<unsigned>(ptr[1]) - '0'};
        if (dig0 > 9U || dig1 > 9U)
            return false;
        out = static_cast<int>((dig0 * 10U) + dig1);
        return true;
    }

    // Fast 4-digit ASCII integer parser.
    [[nodiscard]] inline bool parse4d(const char* ptr, int& out) noexcept
    {
        const unsigned dig0{static_cast<unsigned>(ptr[0]) - '0'};
        const unsigned dig1{static_cast<unsigned>(ptr[1]) - '0'};
        const unsigned dig2{static_cast<unsigned>(ptr[2]) - '0'};
        const unsigned dig3{static_cast<unsigned>(ptr[3]) - '0'};
        if (dig0 > 9U || dig1 > 9U || dig2 > 9U || dig3 > 9U)
            return false;
        out = static_cast<int>((dig0 * 1000U) + (dig1 * 100U) + (dig2 * 10U) + dig3);
        return true;
    }

    // Portable UTC mktime: converts a broken-down UTC struct tm to time_t
    // without any local-timezone offset.
    // Uses a closed-form Gregorian day-count formula — O(1), no year loop.
    std::time_t utc_mktime(std::tm& utc_tm) noexcept
    {
        const int year{utc_tm.tm_year + time_constants::kTmYearOffset};
        const int month{utc_tm.tm_mon + 1};
        const int prev_year{year - 1};

        // Field-range guard (found by the fuzz/ASan gate). Malformed fields in an
        // otherwise well-formed timestamp must not (a) index kMonthOffset[month-1]
        // out of bounds for month ∉ [1,12], nor (b) produce a time_t whose ns
        // time_point overflows for a far-future/past year. Both are UB. An
        // out-of-range field is not a valid date: return the epoch sentinel so the
        // caller's from_time_t() yields Timestamp{} (un-timestamped downstream).
        if (month < 1 || month > time_constants::kMonthsPerYear)
            return 0;
        if (year < time_constants::kMinReprYear || year > time_constants::kMaxReprYear)
            return 0;
        if (utc_tm.tm_mday < 1 || utc_tm.tm_mday > time_constants::kMaxDayOfMonth)
            return 0;

        // Days from Unix epoch (1970-01-01) to the start of `year`.
        // Formula: 365*(year-1970) + (y/4 - 492) - (y/100 - 19) + (y/400 - 4)
        // where 492, 19, 4 = floor(1969/4), floor(1969/100), floor(1969/400).
        // NOLINTBEGIN(readability-magic-numbers)
        const std::int64_t days_to_year{
            (static_cast<std::int64_t>(365) * (year - time_constants::kUnixEpochYear)) +
            ((prev_year / 4) - 492) - ((prev_year / 100) - 19) + ((prev_year / 400) - 4)};

        // Day-of-year prefix from month table (non-leap); +1 if leap and past Feb.
        constexpr std::array<int, 12> kMonthOffset{0,   31,  59,  90,  120, 151,
                                                   181, 212, 243, 273, 304, 334};
        const bool leap{(year % 4 == 0 && year % time_constants::kCenturyYears != 0) ||
                        year % time_constants::kLeapCycleYears == 0};
        const int yday{kMonthOffset[static_cast<std::size_t>(month - 1)] +
                       (month > 2 && leap ? 1 : 0) + utc_tm.tm_mday - 1};
        // NOLINTEND(readability-magic-numbers)

        return static_cast<std::time_t>(
            ((days_to_year + yday) * time_constants::kSecondsPerDay) +
            (static_cast<std::int64_t>(utc_tm.tm_hour) * time_constants::kSecondsPerHour) +
            (static_cast<std::int64_t>(utc_tm.tm_min) * time_constants::kSecondsPerMinute) +
            utc_tm.tm_sec);
    }

    constexpr std::array<std::string_view, 12> kMonthNames{
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    int parse_month_name(std::string_view month_str) noexcept
    {
        // `const auto` (not `const auto*`): ranges::find returns an ITERATOR, which is a raw
        // pointer on libstdc++/libc++ but a wrapper class on MSVC's STL — forcing pointer deduction
        // breaks there. std::distance works for any random-access iterator. NOLINT: clang-tidy's
        // qualified-auto wants `auto*`, the very libstdc++-ism that broke MSVC — keep `auto`.
        // NOLINTNEXTLINE(readability-qualified-auto)
        const auto month_it{std::ranges::find(kMonthNames, month_str)};
        if (month_it != kMonthNames.end())
            return static_cast<int>(std::distance(kMonthNames.begin(), month_it)) + 1;
        return -1;
    }

    // ASCII-only lowercase — DETERMINISM-CRITICAL. `std::tolower(int)` consults the global C
    // locale, so on non-ASCII bytes (e.g. a UTF-8 em-dash 0xE2 0x80 0x94) it can transform them
    // differently under a non-C locale (a French/Windows-1252 CI runner) than under the C locale
    // the Linux golden was built with — a flaky cross-OS divergence in any output computed from a
    // comparison it drives (e.g. level-alias matching feeding a report's level field). This maps
    // A-Z only and passes every other byte through verbatim; under the C locale it is identical to
    // std::tolower, so the golden is unchanged, but it no longer depends on the ambient locale.
    [[nodiscard]] constexpr char ascii_tolower(char character) noexcept
    {
        const auto uch{static_cast<unsigned char>(character)};
        return (uch >= 'A' && uch <= 'Z') ? static_cast<char>(uch - 'A' + 'a') : character;
    }

    [[nodiscard]] bool iequals(std::string_view lhs, std::string_view rhs) noexcept
    {
        if (lhs.size() != rhs.size())
            return false;
        return std::ranges::equal(lhs, rhs, {}, ascii_tolower, ascii_tolower);
    }

    struct LevelAlias
    {
        std::string_view name;
        LogLevel level;
    };

    inline constexpr std::array<LevelAlias, 10> kLevelAliases{
        {{.name = "trace", .level = LogLevel::Trace},
         {.name = "debug", .level = LogLevel::Debug},
         {.name = "dbg", .level = LogLevel::Debug},
         {.name = "info", .level = LogLevel::Info},
         {.name = "information", .level = LogLevel::Info},
         {.name = "warn", .level = LogLevel::Warn},
         {.name = "warning", .level = LogLevel::Warn},
         {.name = "error", .level = LogLevel::Error},
         {.name = "err", .level = LogLevel::Error},
         {.name = "fatal", .level = LogLevel::Fatal}}};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

// ISO 8601 / RFC 3339
// Accepted: "2024-01-15T10:30:00Z"  "2024-01-15T10:30:00.123Z"
//           "2024-01-15T10:30:00+05:30"  "2024-01-15 10:30:00"
// These parsers intentionally use literal field offsets and pointer slices mandated by
// external timestamp formats.
std::optional<Timestamp> parse_iso8601(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.size() < time_constants::kIso8601MinLength)
        return std::nullopt;
    const char* ptr = timestamp_str.data();

    int year{0};
    int month{0};
    int day{0};
    int hour{0};
    int minute{0};
    int second{0};
    if (!parse4d(ptr, year))
        return std::nullopt;
    if (ptr[4] != '-')
        return std::nullopt;
    if (!parse2d(ptr + 5, month))
        return std::nullopt;
    if (ptr[7] != '-')
        return std::nullopt;
    if (!parse2d(ptr + 8, day))
        return std::nullopt;
    if (ptr[10] != 'T' && ptr[10] != ' ')
        return std::nullopt;
    if (!parse2d(ptr + 11, hour))
        return std::nullopt;
    if (ptr[13] != ':')
        return std::nullopt;
    if (!parse2d(ptr + 14, minute))
        return std::nullopt;
    if (ptr[16] != ':')
        return std::nullopt;
    if (!parse2d(ptr + 17, second))
        return std::nullopt;

    std::tm parsed_tm{};
    parsed_tm.tm_year = year - 1900;
    parsed_tm.tm_mon = month - 1;
    parsed_tm.tm_mday = day;
    parsed_tm.tm_hour = hour;
    parsed_tm.tm_min = minute;
    parsed_tm.tm_sec = second;

    std::time_t parsed_time{utc_mktime(parsed_tm)};

    // Parse optional timezone offset to adjust to UTC.
    std::size_t pos{19};
    // Skip fractional seconds.
    if (pos < timestamp_str.size() && timestamp_str[pos] == '.')
    {
        ++pos;
        while (pos < timestamp_str.size() && timestamp_str[pos] >= '0' && timestamp_str[pos] <= '9')
        {
            ++pos;
        }
    }
    if (pos < timestamp_str.size())
    {
        const char timezone_designator{timestamp_str[pos]};
        if (timezone_designator == '+' || timezone_designator == '-')
        {
            if (pos + 5 <= timestamp_str.size())
            {
                int timezone_hour{0};
                int timezone_minute{0};
                static_cast<void>(parse2d(timestamp_str.data() + pos + 1, timezone_hour));
                // Support both "+0530" and "+05:30"
                const std::size_t minute_offset{(timestamp_str[pos + 3] == ':') ? pos + 4
                                                                                : pos + 3};
                if (minute_offset + 2 <= timestamp_str.size())
                {
                    static_cast<void>(
                        parse2d(timestamp_str.data() + minute_offset, timezone_minute));
                }
                const int offset_seconds{(timezone_hour * 3600) + (timezone_minute * 60)};
                parsed_time += (timezone_designator == '+') ? -offset_seconds : offset_seconds;
            }
        }
        // 'Z' → already UTC, no adjustment needed.
    }

    return std::chrono::system_clock::from_time_t(parsed_time);
}

// BSD syslog: "Jan  1 12:00:00" or "Jan 15 08:03:22"
// Yearless (RFC3164); the year is the injected `reference_year`. Deterministic by
// construction — NO wall-clock read (bibles/determinism_model.md §7):
// a `std::time(nullptr)`-derived year made the parsed timestamp non-reproducible
// across the year rollover and (cached in a thread_local) invisible to the
// differential oracle. Default = kDefaultReferenceYear; a live consumer injects
// the real current year once at stream open.
std::optional<Timestamp> parse_bsd_syslog_ts(std::string_view timestamp_str,
                                             int reference_year) noexcept
{
    if (timestamp_str.size() < time_constants::kBsdSyslogMinLength)
        return std::nullopt;
    const char* ptr = timestamp_str.data();

    const int month{parse_month_name({ptr, 3})};
    if (month < 0)
        return std::nullopt;
    if (ptr[3] != ' ')
        return std::nullopt;

    // Day field: " D" or "DD" (positions 4-5)
    int day{0};
    {
        const char* day_ptr = ptr + 4;
        if (*day_ptr == ' ')
            ++day_ptr;
        auto parse_result{std::from_chars(day_ptr, ptr + 6, day)};
        if (parse_result.ec != std::errc{})
            return std::nullopt;
    }

    if (ptr[6] != ' ')
        return std::nullopt;
    // HH:MM:SS at positions 7-14
    int hour{0};
    int minute{0};
    int second{0};
    if (!parse_fixed(ptr + 7, 2, hour))
        return std::nullopt;
    if (ptr[9] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 10, 2, minute))
        return std::nullopt;
    if (ptr[12] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 13, 2, second))
        return std::nullopt;

    // Year = the injected deterministic reference (no wall-clock read).
    std::tm parsed_tm{};
    parsed_tm.tm_year = reference_year - time_constants::kTmYearOffset;
    parsed_tm.tm_mon = month - 1;
    parsed_tm.tm_mday = day;
    parsed_tm.tm_hour = hour;
    parsed_tm.tm_min = minute;
    parsed_tm.tm_sec = second;

    return std::chrono::system_clock::from_time_t(utc_mktime(parsed_tm));
}

// CLF / Combined Log Format: "10/Oct/2000:13:55:36 -0700"
std::optional<Timestamp> parse_clf_timestamp(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.size() < time_constants::kClfMinLength)
        return std::nullopt;
    const char* ptr = timestamp_str.data();

    int day{0};
    int year{0};
    int hour{0};
    int minute{0};
    int second{0};
    if (!parse_fixed(ptr, 2, day))
        return std::nullopt;
    if (ptr[2] != '/')
        return std::nullopt;
    const int month{parse_month_name({ptr + 3, 3})};
    if (month < 0)
        return std::nullopt;
    if (ptr[6] != '/')
        return std::nullopt;
    if (!parse_fixed(ptr + 7, 4, year))
        return std::nullopt;
    if (ptr[11] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 12, 2, hour))
        return std::nullopt;
    if (ptr[14] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 15, 2, minute))
        return std::nullopt;
    if (ptr[17] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 18, 2, second))
        return std::nullopt;

    std::tm parsed_tm{};
    parsed_tm.tm_year = year - 1900;
    parsed_tm.tm_mon = month - 1;
    parsed_tm.tm_mday = day;
    parsed_tm.tm_hour = hour;
    parsed_tm.tm_min = minute;
    parsed_tm.tm_sec = second;

    std::time_t parsed_time{utc_mktime(parsed_tm)};

    // Parse timezone offset " ±HHMM" if present (position 20 onward).
    if (timestamp_str.size() >= 26 && timestamp_str[20] == ' ')
    {
        const char sign{timestamp_str[21]};
        if (sign == '+' || sign == '-')
        {
            int timezone_hour{0};
            int timezone_minute{0};
            parse_fixed(ptr + 22, 2, timezone_hour);
            parse_fixed(ptr + 24, 2, timezone_minute);
            const int offset_seconds{(timezone_hour * 3600) + (timezone_minute * 60)};
            parsed_time += (sign == '+') ? -offset_seconds : offset_seconds;
        }
    }

    return std::chrono::system_clock::from_time_t(parsed_time);
}

// Unix epoch seconds: "1117838570"
std::optional<Timestamp> parse_epoch_timestamp(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.empty() || timestamp_str.size() > time_constants::kEpochTimestampMaxDigits)
    {
        return std::nullopt;
    }
    std::int64_t epoch{0};
    // const char* (not begin()/end()): portable across stdlibs — see parse_fixed.
    const char* const ts_last{timestamp_str.data() + timestamp_str.size()};
    auto parse_result{std::from_chars(timestamp_str.data(), ts_last, epoch)};
    if (parse_result.ec != std::errc{} || parse_result.ptr != ts_last)
    {
        return std::nullopt;
    }
    if (epoch < 0)
        return std::nullopt;
    return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(epoch));
}

// OTLP timeUnixNano: epoch nanoseconds, "1705312200000000000". Integer-only — no float
// (ADR-29 D-OTEL-3). The integer duration_cast truncates to system_clock's
// resolution deterministically per stdlib; the OTLP producer emits millisecond-granular nanos,
// so the truncation is lossless and the derived window membership is bit-identical cross-stdlib.
std::optional<Timestamp> parse_unix_nano_timestamp(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.empty() || timestamp_str.size() > time_constants::kUnixNanoMaxDigits)
    {
        return std::nullopt;
    }
    std::int64_t nanos{0};
    const char* const ts_last{timestamp_str.data() + timestamp_str.size()};
    auto parse_result{std::from_chars(timestamp_str.data(), ts_last, nanos)};
    if (parse_result.ec != std::errc{} || parse_result.ptr != ts_last)
    {
        return std::nullopt;
    }
    if (nanos < 0)
        return std::nullopt;
    return Timestamp{std::chrono::duration_cast<Duration>(std::chrono::nanoseconds{nanos})};
}

// HDFS compact: date="YYMMDD", time="HHMMSS"
std::optional<Timestamp> parse_compact_date_time(std::string_view date,
                                                 std::string_view time) noexcept
{
    if (date.size() != time_constants::kCompactDateWidth ||
        time.size() != time_constants::kCompactDateWidth)
    {
        return std::nullopt;
    }
    const char* date_ptr = date.data();
    const char* time_ptr = time.data();

    int year_short{0};
    int month{0};
    int day{0};
    int hour{0};
    int minute{0};
    int second{0};
    if (!parse_fixed(date_ptr, 2, year_short))
        return std::nullopt;
    if (!parse_fixed(date_ptr + 2, 2, month))
        return std::nullopt;
    if (!parse_fixed(date_ptr + 4, 2, day))
        return std::nullopt;
    if (!parse_fixed(time_ptr, 2, hour))
        return std::nullopt;
    if (!parse_fixed(time_ptr + 2, 2, minute))
        return std::nullopt;
    if (!parse_fixed(time_ptr + 4, 2, second))
        return std::nullopt;

    const int year{(year_short >= time_constants::kPivotShortYear)
                       ? time_constants::kTmYearOffset + year_short
                       : 2000 + year_short};

    std::tm parsed_tm{};
    parsed_tm.tm_year = year - 1900;
    parsed_tm.tm_mon = month - 1;
    parsed_tm.tm_mday = day;
    parsed_tm.tm_hour = hour;
    parsed_tm.tm_min = minute;
    parsed_tm.tm_sec = second;
    return std::chrono::system_clock::from_time_t(utc_mktime(parsed_tm));
}

// Spark: "YY/MM/DD HH:MM:SS" (17 chars)
std::optional<Timestamp> parse_short_year_slash(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.size() < time_constants::kShortYearSlashMinLength)
        return std::nullopt;
    const char* ptr = timestamp_str.data();

    int year_short{0};
    int month{0};
    int day{0};
    int hour{0};
    int minute{0};
    int second{0};
    if (!parse_fixed(ptr, 2, year_short))
        return std::nullopt;
    if (ptr[2] != '/')
        return std::nullopt;
    if (!parse_fixed(ptr + 3, 2, month))
        return std::nullopt;
    if (ptr[5] != '/')
        return std::nullopt;
    if (!parse_fixed(ptr + 6, 2, day))
        return std::nullopt;
    if (ptr[8] != ' ')
        return std::nullopt;
    if (!parse_fixed(ptr + 9, 2, hour))
        return std::nullopt;
    if (ptr[11] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 12, 2, minute))
        return std::nullopt;
    if (ptr[14] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 15, 2, second))
        return std::nullopt;

    const int year{(year_short >= time_constants::kPivotShortYear)
                       ? time_constants::kTmYearOffset + year_short
                       : 2000 + year_short};

    std::tm parsed_tm{};
    parsed_tm.tm_year = year - 1900;
    parsed_tm.tm_mon = month - 1;
    parsed_tm.tm_mday = day;
    parsed_tm.tm_hour = hour;
    parsed_tm.tm_min = minute;
    parsed_tm.tm_sec = second;
    return std::chrono::system_clock::from_time_t(utc_mktime(parsed_tm));
}

// Apache error-log: "Sun Dec 04 04:47:44 2005" (24 chars)
std::optional<Timestamp> parse_apache_error_ts(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.size() < time_constants::kApacheErrorMinLength)
        return std::nullopt;
    const char* ptr = timestamp_str.data();

    // Skip 3-char weekday + space
    if (ptr[3] != ' ')
        return std::nullopt;

    const int month{parse_month_name({ptr + 4, 3})};
    if (month < 0)
        return std::nullopt;
    if (ptr[7] != ' ')
        return std::nullopt;

    int day{0};
    if (!parse_fixed(ptr + 8, 2, day))
        return std::nullopt;
    if (ptr[10] != ' ')
        return std::nullopt;

    int hour{0};
    int minute{0};
    int second{0};
    if (!parse_fixed(ptr + 11, 2, hour))
        return std::nullopt;
    if (ptr[13] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 14, 2, minute))
        return std::nullopt;
    if (ptr[16] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 17, 2, second))
        return std::nullopt;
    if (ptr[19] != ' ')
        return std::nullopt;

    int year{0};
    if (!parse_fixed(ptr + 20, 4, year))
        return std::nullopt;

    std::tm parsed_tm{};
    parsed_tm.tm_year = year - 1900;
    parsed_tm.tm_mon = month - 1;
    parsed_tm.tm_mday = day;
    parsed_tm.tm_hour = hour;
    parsed_tm.tm_min = minute;
    parsed_tm.tm_sec = second;
    return std::chrono::system_clock::from_time_t(utc_mktime(parsed_tm));
}

// HealthApp: "YYYYMMDD-HH:MM:SS:mmm" or "YYYYMMDD-H:MM:S:mmm" (variable-width
// hour/second)
std::optional<Timestamp> parse_health_app_ts(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.size() < time_constants::kHealthAppMinLength)
        return std::nullopt;
    const char* ptr = timestamp_str.data();

    int year{0};
    int month{0};
    int day{0};
    if (!parse_fixed(ptr, 4, year))
        return std::nullopt;
    if (!parse_fixed(ptr + 4, 2, month))
        return std::nullopt;
    if (!parse_fixed(ptr + 6, 2, day))
        return std::nullopt;
    if (ptr[8] != '-')
        return std::nullopt;

    // Parse variable-width "H:MM:S" or "HH:MM:SS" portion after the dash.
    const char* time_ptr = ptr + 9;
    const char* end_ptr = ptr + timestamp_str.size();

    // Hour: 1 or 2 digits before first ':'
    int hour{0};
    auto hour_result{std::from_chars(time_ptr, end_ptr, hour)};
    if (hour_result.ec != std::errc{} || hour_result.ptr >= end_ptr || *hour_result.ptr != ':')
    {
        return std::nullopt;
    }
    time_ptr = hour_result.ptr + 1; // skip ':'

    // Minute: always 2 digits
    int minute{0};
    if (time_ptr + 2 > end_ptr)
        return std::nullopt;
    if (!parse_fixed(time_ptr, 2, minute))
        return std::nullopt;
    time_ptr += 2;
    if (time_ptr >= end_ptr || *time_ptr != ':')
        return std::nullopt;
    ++time_ptr; // skip ':'

    // Second: 1 or 2 digits before next ':'
    int second{0};
    auto second_result{std::from_chars(time_ptr, end_ptr, second)};
    if (second_result.ec != std::errc{} || second_result.ptr >= end_ptr ||
        *second_result.ptr != ':')
    {
        return std::nullopt;
    }

    std::tm parsed_tm{};
    parsed_tm.tm_year = year - 1900;
    parsed_tm.tm_mon = month - 1;
    parsed_tm.tm_mday = day;
    parsed_tm.tm_hour = hour;
    parsed_tm.tm_min = minute;
    parsed_tm.tm_sec = second;
    return std::chrono::system_clock::from_time_t(utc_mktime(parsed_tm));
}

// Log4j: "2024-01-15 10:30:00,123" or "2024-01-15 10:30:00.123" (23 chars)
std::optional<Timestamp> parse_log4j_timestamp(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.size() < time_constants::kLog4jMinLength)
        return std::nullopt;
    const char* ptr = timestamp_str.data();

    int year{0};
    int month{0};
    int day{0};
    int hour{0};
    int minute{0};
    int second{0};
    if (!parse_fixed(ptr, 4, year))
        return std::nullopt;
    if (ptr[4] != '-')
        return std::nullopt;
    if (!parse_fixed(ptr + 5, 2, month))
        return std::nullopt;
    if (ptr[7] != '-')
        return std::nullopt;
    if (!parse_fixed(ptr + 8, 2, day))
        return std::nullopt;
    if (ptr[10] != ' ')
        return std::nullopt;
    if (!parse_fixed(ptr + 11, 2, hour))
        return std::nullopt;
    if (ptr[13] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 14, 2, minute))
        return std::nullopt;
    if (ptr[16] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 17, 2, second))
        return std::nullopt;
    if (ptr[19] != ',' && ptr[19] != '.')
        return std::nullopt;

    std::tm parsed_tm{};
    parsed_tm.tm_year = year - 1900;
    parsed_tm.tm_mon = month - 1;
    parsed_tm.tm_mday = day;
    parsed_tm.tm_hour = hour;
    parsed_tm.tm_min = minute;
    parsed_tm.tm_sec = second;
    return std::chrono::system_clock::from_time_t(utc_mktime(parsed_tm));
}

// Case-insensitive log-level parser.
LogLevel parse_log_level(std::string_view level_str) noexcept
{
    if (level_str.empty())
        return LogLevel::Unknown;

    // Switch on the lowercased first character for O(1) dispatch; then
    // use iequals for exact matching within the family.
    // Eliminates the 10-entry linear scan on every ingest call.
    switch (static_cast<unsigned char>(level_str[0]) | 0x20U) // safe ASCII lowercase
    {
    case 't':
        return iequals(level_str, "trace") ? LogLevel::Trace : LogLevel::Unknown;
    case 'd':
        return (iequals(level_str, "debug") || iequals(level_str, "dbg")) ? LogLevel::Debug
                                                                          : LogLevel::Unknown;
    case 'i':
        return (iequals(level_str, "info") || iequals(level_str, "information"))
                   ? LogLevel::Info
                   : LogLevel::Unknown;
    case 'w':
        return (iequals(level_str, "warn") || iequals(level_str, "warning")) ? LogLevel::Warn
                                                                             : LogLevel::Unknown;
    case 'e':
        return (iequals(level_str, "error") || iequals(level_str, "err")) ? LogLevel::Error
                                                                          : LogLevel::Unknown;
    case 'f':
        // BGL emits FAILURE as its top RAS severity (fatal-class) — F3b D-F3b-3 lexicon (was a
        // gap: FAILURE → Unknown lost the level on those lines).
        return (iequals(level_str, "fatal") || iequals(level_str, "failure")) ? LogLevel::Fatal
                                                                              : LogLevel::Unknown;
    case 's':
        // BGL SEVERE (between ERROR and FATAL) — mapped error-class (no enum tier between the
        // two); F3b D-F3b-3 lexicon. Keeps SEVERE distinct from FATAL/FAILURE (which are fatal).
        return iequals(level_str, "severe") ? LogLevel::Error : LogLevel::Unknown;
    case 'c':
        return (iequals(level_str, "critical") || iequals(level_str, "crit")) ? LogLevel::Fatal
                                                                              : LogLevel::Unknown;
    default:
        return LogLevel::Unknown;
    }
}

namespace
{
    // An alerting tier (Warn/Error/Fatal) is the only severity a pass-glyph-led line can FALSELY
    // earn (SRC-D-OUT-1b): Info/Debug/Trace never alert, so the outcome guard below is paid only on
    // a would-be-positive result — the SRC-D-OUT-1 hot-path discipline.
    [[nodiscard]] constexpr bool is_alerting_level(LogLevel level) noexcept
    {
        return level == LogLevel::Warn || level == LogLevel::Error || level == LogLevel::Fatal;
    }

    // Is there any token left on the line after `token` ends? Stage 1's "…or it is the terminal /
    // sole significant token" branch is a claim about the LINE, so it is derived over the whole
    // line. Deriving it inside the head-bounded scan instead made a level word at content offset
    // 35 whose successor starts at 41 read as TERMINAL — and therefore authoritative — so the
    // verdict moved with the byte count of the prefix rather than with its structure. That is the
    // presentation-dependence SRC-D-OUT-4c names, in the one branch the kind-slot rule does not
    // cover (ADR-20: bound the scan, never the claim). Cold — reached only once a level word
    // matched — and self-terminating: for_each_token stops at the first token it finds.
    // PRECONDITION: `token` is a sub-view of `line`.
    // Only throw path is for_each_token's substr (begin <= size); the noexcept body cannot throw.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] bool token_follows(std::string_view line, std::string_view token) noexcept
    {
        const std::size_t end{static_cast<std::size_t>(token.data() - line.data()) + token.size()};
        return for_each_token(line.substr(end), 0U, [](std::string_view) noexcept { return true; });
    }
} // namespace

// Only throw path is for_each_token's substr(begin, ...) with begin <= line.size()
// (see token_scan.hpp); the noexcept body cannot throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
EventLevel infer_leading_log_level(std::string_view line) noexcept
{
    // Two bounded, alloc-free stages over a SHARED tokenisation (token_scan.hpp):
    // tokens split on whitespace + structural punctuation, but identifier/path
    // chars (`-_./`) stay inside a token — so "tsc-error-report.json" is one atom
    // (never a level, never a cue) while "##[error]" / `level=error` / pytest
    // "…OperationalError:" still expose the inner word. Both stages agree on what
    // a standalone word is; the old per-stage scans diverged and over-matched.
    //  1) A LEADING explicit level token (INFO/ERROR/WARN/…) is authoritative.
    //     The level is the first token, or sits right after a leading timestamp
    //     ("2026-05-29T10:00:00 INFO …"). parse_log_level matches only the closed
    //     level vocabulary, so timestamp/hostname tokens are skipped, never
    //     misread; the first match wins, so a mid-line "error" after a leading
    //     "INFO" can't override it.
    //  2) Otherwise, scan the head for an error/warning CUE as a standalone token
    //     (pytest "…OperationalError", tracebacks, bare "connection refused").
    // THE LEADING-LEVEL BUDGET COUNTS SIGNIFICANT TOKENS, NEVER RAW BYTES (ADR-16.D7; DN-54.D22 is
    // the argument). What it bounds: how many significant tokens may precede the producer's own
    // level word before Stage 1 stops looking. What it costs: a level word further in is not read
    // and the line falls through to Stage 2. It is the budget that decides whether the producer's
    // kind word is read AT ALL, which makes it part of the CLAIM — and as a 40-byte head it made
    // that verdict a function of the prefix's LENGTH: `<path>:<line>:<col>: warning:` with a
    // 34-byte build path put `warning` at byte 43, past the head, so Stage 2 answered from the
    // message body and one producer's one diagnostic classified Warn or Error by how long its
    // path was (an ANSI run is one more spender of the same budget, not the class). The register
    // decision (SRC-D-OUT-4c's kind slot) and the terminality decision below stay derived over the
    // WHOLE line: bound the scan, never the claim (ADR-20). Bounded ⇒ alloc-free hot-path
    // discipline (ADR-9); for_each_token's byte limit is untouched — the scan passes 0 (the whole
    // line) and the visitor counts its own visits.
    //
    // THE VALUE IS A MEASUREMENT, NOT A JUDGMENT — the instrument is
    // tools/leading_level_token_index_measure (self-testing; re-run it before moving this). Over
    // 12 corpora, 62 187 513 lines (insight-canon e084a31, 2026-09-02): 8 tokens — indices 0..7 —
    // reads every level word the 40-byte head turned into a verdict (the lines it no longer
    // reaches are bare, non-terminal words that fell through to Stage 2 under the head too: 0
    // verdict changes) and reads the path-prefixed diagnostic class the head dropped — 31 % of
    // GitHub Actions' level words and 91 % of gcc's. ADR-16.D7's pre-registered 7 would lose 30
    // terminal-bare GitLab verdicts, which is why 8. The declared residual, level words at index
    // >= 8 that Stage 1 does not read, as a share of level-bearing lines: GitHub Actions 5.85 %,
    // Jenkins 2.06 %, GitLab 1.38 %, gcc 0.29 %. Its verdict-shaped part (GitHub Actions 23 324
    // lines, 2.05 %) is a NESTED record — a CI line wrapping an application or syslog record,
    // level word at token 8-15 — and Eqya ruled it a residual (2026-09-02, as the claim
    // boundary's owner): that word is the INNER record's level, and reading it as the line's
    // verdict would attribute the inner severity to the outer line. Reversible by the Founder;
    // the cost of moving the value is one re-run of the instrument.
    constexpr std::size_t kLeadingScanTokens{8};
    // Head scanned for a failure/warning cue — the other side of ADR-16.D7's criterion: a COST
    // bound over canon's OWN cue vocabulary, which may stay in raw bytes because its residual is
    // declared below, unlike the claim budget above. 128 (not 64), aligned with the pass/fail-glyph
    // kOutcomeHead: a VERDICT ANCHOR can sit at the END of a long line — a deeply-namespaced CI
    // test verdict (`Tests\E2E\…\DocumentsDBCustomServerTest::testTimeout (FAILED)`) puts the
    // `(FAILED)` anchor at col ~69, past the old 64 head, so the strongest possible failure signal
    // was invisible and the line classified as a benign new template (SRC-D-RNK-2
    // measure-first: the P5 `testTimeout (FAILED)` recall miss is THIS head bound, not the
    // significance cut). 128
    // covers realistic verdict lines with margin; a pathological >128-char prefix is the accepted
    // residual (the measure-first catalog discipline). Still bounded → no hot-path scan blow-up.
    constexpr std::size_t kKeywordHead{128};

    // Stage 1 — the first exact level token among the first kLeadingScanTokens significant
    // tokens, but authoritative only IN VERDICT REGISTER (SRC-D-OUT-4). parse_log_level is
    // outcome-blind:
    // it maps the bare words failure/fatal/critical → Fatal, error → Error, warn → Warn,
    // so a descriptive line ("error handling enabled", "failure modes documented") whose
    // FIRST token is a level WORD would otherwise be classified alerting and authoritative,
    // bypassing the Stage-2 cue guard. So a leading level word is authoritative only when
    // it is verdict-anchored (caps / a kind-slot `:` / bracket — e.g. "ERROR", "error:",
    // "##[error]") OR it is the terminal/sole significant token ON THE LINE (a bare one-word
    // status). An unanchored, non-terminal level word falls THROUGH to Stage 2's anchored cue scan.
    LogLevel leading{LogLevel::Unknown};
    std::string_view level_token{};
    std::size_t visited{0};
    // Scan limit 0 = the whole line; the visitor ends the walk itself — at the first level word,
    // or once kLeadingScanTokens tokens have been examined, so token index kLeadingScanTokens is
    // never visited whatever byte it starts at. The return (did-stop-early) is unused: the
    // captured leading/level_token, set as side effects, are what Stage 1 reads.
    (void)for_each_token(line, 0U,
                         [&](std::string_view token) noexcept
                         {
                             const LogLevel level{parse_log_level(token)};
                             if (level == LogLevel::Unknown)
                                 return ++visited == kLeadingScanTokens; // budget spent: stop
                             leading = level; // first level token wins
                             level_token = token;
                             return true; // stop — Stage 1 has everything it reads
                         });
    // Terminality is a property of the LINE, not of the head — see token_follows.
    const bool token_follows_level{leading != LogLevel::Unknown &&
                                   token_follows(line, level_token)};
    // SRC-D-CNT-1: a leading level WORD in COUNT register ("There was 1 failure:") is a SUMMARY,
    // not a per-item verdict — even though it is verdict-anchored (the trailing colon), a counted
    // noun is checked first and is NOT authoritative. Fall THROUGH to Stage 2, which still catches
    // a genuine non-count verdict elsewhere on the line ("Build failed with 1 error" → "failed"
    // fires → Error) or, absent one, caps the summary at Warn. Only alerting leading levels need
    // this guard (Info/Debug never outrank).
    if (leading != LogLevel::Unknown &&
        (detail::is_verdict_anchored(line, level_token) || !token_follows_level) &&
        !(is_alerting_level(leading) && detail::is_count_register(line, level_token)))
    {
        // Authoritative leading level. SRC-D-OUT-1b: a leading pass GLYPH ("✔ … ERROR …") is an
        // unambiguous pass verdict whose name embeds a level word ⇒ demote an alerting level
        // to Unknown. A genuine "ERROR:"/"FATAL:" leads with the WORD, so
        // leading_outcome_is_pass returns false and the level is preserved (no recall loss).
        if (is_alerting_level(leading) && detail::leading_outcome_is_pass(line))
            return EventLevel::inferred(LogLevel::Unknown);
        return EventLevel::inferred(leading);
    }

    // Stage 2 — a failure/warning cue as a standalone word in the head. The match
    // is TOKEN-aware (see failure_lexicon.hpp), not a raw substring: a benign line
    // whose path or identifier merely contains "error" (`Writing
    // tsc-error-report.json`) is not misread as Error — that substring over-match
    // spuriously promoted new templates to HIGH "New error" downstream in the diff.
    if (contains_failure_cue(line, kKeywordHead))
        // contains_failure_cue self-guards (SRC-D-OUT-1) — no double call
        return EventLevel::inferred(LogLevel::Error);
    // SRC-D-CNT-1: a count-register failure word ("1 failure", "5 failed") is a SUMMARY — it did
    // not fire as a verdict cue above, but it still surfaces, capped at Warn (below per-item
    // verdicts; demote, never suppress — the "25 passed, 5 failed" dual). A leading pass GLYPH
    // still demotes.
    if (detail::contains_failure_summary_cue(line, kKeywordHead))
        return EventLevel::inferred(detail::leading_outcome_is_pass(line) ? LogLevel::Unknown
                                                                          : LogLevel::Warn);
    if (contains_warning_cue(line, kKeywordHead))
        // SRC-D-OUT-1b: contains_warning_cue has no outcome guard, so apply it here (Warn alerts).
        return EventLevel::inferred(detail::leading_outcome_is_pass(line) ? LogLevel::Unknown
                                                                          : LogLevel::Warn);
    return EventLevel::inferred(LogLevel::Unknown);
}

// Parse Nginx error-log timestamp: "YYYY/MM/DD HH:MM:SS"
// Minimum length: 19 characters.
std::optional<Timestamp> parse_nginx_error_ts(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.size() < time_constants::kNginxErrorMinLength)
        return std::nullopt;
    const char* ptr = timestamp_str.data();

    int year{0};
    int month{0};
    int day{0};
    int hour{0};
    int minute{0};
    int second{0};
    if (!parse_fixed(ptr + 0, 4, year))
        return std::nullopt;
    if (ptr[4] != '/')
        return std::nullopt;
    if (!parse_fixed(ptr + 5, 2, month))
        return std::nullopt;
    if (ptr[7] != '/')
        return std::nullopt;
    if (!parse_fixed(ptr + 8, 2, day))
        return std::nullopt;
    if (ptr[10] != ' ')
        return std::nullopt;
    if (!parse_fixed(ptr + 11, 2, hour))
        return std::nullopt;
    if (ptr[13] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 14, 2, minute))
        return std::nullopt;
    if (ptr[16] != ':')
        return std::nullopt;
    if (!parse_fixed(ptr + 17, 2, second))
        return std::nullopt;

    std::tm parsed_tm{};
    parsed_tm.tm_year = year - 1900;
    parsed_tm.tm_mon = month - 1;
    parsed_tm.tm_mday = day;
    parsed_tm.tm_hour = hour;
    parsed_tm.tm_min = minute;
    parsed_tm.tm_sec = second;
    return std::chrono::system_clock::from_time_t(utc_mktime(parsed_tm));
}

} // namespace insight::utils
// NOLINTEND(readability-magic-numbers)
// NOLINTEND(readability-function-cognitive-complexity)
