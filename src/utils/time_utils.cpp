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
    inline constexpr std::size_t kCompactDateWidth{6};
    inline constexpr std::size_t kShortYearSlashMinLength{17};
    inline constexpr std::size_t kDottedDateMinLength{10};
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

    // Parse a fixed-width ASCII integer from [p, p+len).
    // Returns false if any character is not a digit.
    bool parse_fixed(const char* ptr, int length, int& out_value) noexcept
    {
        const std::string_view digits{ptr, static_cast<std::size_t>(length)};
        auto parse_result{std::from_chars(digits.begin(), digits.end(), out_value)};
        return parse_result.ec == std::errc{} && parse_result.ptr == digits.end();
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
        const auto* const month_it{std::ranges::find(kMonthNames, month_str)};
        if (month_it != kMonthNames.end())
            return static_cast<int>(month_it - kMonthNames.begin()) + 1;
        return -1;
    }

    [[nodiscard]] bool iequals(std::string_view lhs, std::string_view rhs) noexcept
    {
        if (lhs.size() != rhs.size())
            return false;
        return std::ranges::equal(
            lhs, rhs, {},
            [](char character) { return std::tolower(static_cast<unsigned char>(character)); },
            [](char character) { return std::tolower(static_cast<unsigned char>(character)); });
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
// construction — NO wall-clock read (insight_determinism_model.md § Event-time):
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
    auto parse_result{std::from_chars(timestamp_str.begin(), timestamp_str.end(), epoch)};
    if (parse_result.ec != std::errc{} || parse_result.ptr != timestamp_str.end())
    {
        return std::nullopt;
    }
    if (epoch < 0)
        return std::nullopt;
    return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(epoch));
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

// BGL dotted date: "YYYY.MM.DD" (10 chars)
std::optional<Timestamp> parse_dotted_date(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.size() < time_constants::kDottedDateMinLength)
        return std::nullopt;
    const char* ptr = timestamp_str.data();

    int year{0};
    int month{0};
    int day{0};
    if (!parse_fixed(ptr, 4, year))
        return std::nullopt;
    if (ptr[4] != '.')
        return std::nullopt;
    if (!parse_fixed(ptr + 5, 2, month))
        return std::nullopt;
    if (ptr[7] != '.')
        return std::nullopt;
    if (!parse_fixed(ptr + 8, 2, day))
        return std::nullopt;

    std::tm parsed_tm{};
    parsed_tm.tm_year = year - 1900;
    parsed_tm.tm_mon = month - 1;
    parsed_tm.tm_mday = day;
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

// Only throw path is for_each_token's substr(begin, ...) with begin <= line.size()
// (see token_scan.hpp); the noexcept body cannot throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
LogLevel infer_leading_log_level(std::string_view line) noexcept
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
    constexpr std::size_t kLeadingScanHead{40}; // covers an ISO-8601 timestamp + the level start
    constexpr std::size_t kKeywordHead{64};     // head scanned for a failure/warning cue

    // Stage 1 — first exact level token whose start lies within the head.
    LogLevel leading{LogLevel::Unknown};
    if (for_each_token(line, kLeadingScanHead,
                       [&leading](std::string_view token) noexcept
                       {
                           const LogLevel level{parse_log_level(token)};
                           if (level == LogLevel::Unknown)
                               return false;
                           leading = level;
                           return true; // first level token wins
                       }))
        return leading;

    // Stage 2 — a failure/warning cue as a standalone word in the head. The match
    // is TOKEN-aware (see failure_lexicon.hpp), not a raw substring: a benign line
    // whose path or identifier merely contains "error" (`Writing
    // tsc-error-report.json`) is not misread as Error — that substring over-match
    // spuriously promoted new templates to HIGH "New error" downstream in the diff.
    if (contains_failure_cue(line, kKeywordHead))
        return LogLevel::Error;
    if (contains_warning_cue(line, kKeywordHead))
        return LogLevel::Warn;
    return LogLevel::Unknown;
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
