module;

module insight.canon.api;
import insight.canon.internal;

// note: parse_iso8601 is over the cognitive-complexity threshold; its shape is the format's.
// NOLINTBEGIN(readability-function-cognitive-complexity)
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
    // invariant: system_clock's int64 nanosecond duration reaches about 292 years either side of
    // 1970, so a year outside this window would overflow the conversion.
    inline constexpr int kMinReprYear{1678};
    inline constexpr int kMaxReprYear{2261};
    inline constexpr int kMaxDayOfMonth{31};
    inline constexpr std::size_t kIso8601MinLength{19};
    inline constexpr std::size_t kBsdSyslogMinLength{15};
    inline constexpr std::size_t kClfMinLength{20};
    inline constexpr std::size_t kEpochTimestampMaxDigits{12};
    // invariant: 20 digits or more are refused here; a 19-digit value past int64 is refused by
    // from_chars, which reports out-of-range.
    inline constexpr std::size_t kUnixNanoMaxDigits{19};
    inline constexpr std::size_t kCompactDateWidth{6};
    inline constexpr std::size_t kShortYearSlashMinLength{17};
    inline constexpr std::size_t kApacheErrorMinLength{24};
    // invariant: 8 date digits, the dash, three 1-digit clock fields and their three colons; the
    // millisecond digits terminate the second field and are never read.
    // refs: DN-43.O5
    inline constexpr std::size_t kHealthAppMinLength{15};
    inline constexpr std::size_t kLog4jMinLength{23};
    inline constexpr std::size_t kNginxErrorMinLength{19};

} // namespace time_constants

namespace
{

    // post: true only when from_chars consumed exactly `length` bytes.
    // note: raw pointers, never string_view iterators - MSVC's are a class, not const char*.
    bool parse_fixed(const char* ptr, int length, int& out_value) noexcept
    {
        const char* const last{ptr + length};
        const auto parse_result{std::from_chars(ptr, last, out_value)};
        return parse_result.ec == std::errc{} && parse_result.ptr == last;
    }

    [[nodiscard]] inline bool parse2d(const char* ptr, int& out) noexcept
    {
        const unsigned dig0{static_cast<unsigned>(ptr[0]) - '0'};
        const unsigned dig1{static_cast<unsigned>(ptr[1]) - '0'};
        if (dig0 > 9U || dig1 > 9U)
            return false;
        out = static_cast<int>((dig0 * 10U) + dig1);
        return true;
    }

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

    // post: the UTC instant for `utc_tm`, with no local-timezone offset applied; an out-of-range
    // field returns the epoch sentinel 0 instead of a normalized wrong instant.
    std::time_t utc_mktime(std::tm& utc_tm) noexcept
    {
        const int year{utc_tm.tm_year + time_constants::kTmYearOffset};
        const int month{utc_tm.tm_mon + 1};
        const int prev_year{year - 1};

        // assert: this guard is what keeps the kMonthOffset index and the nanosecond conversion in
        // range; both would otherwise be undefined behaviour.
        if (month < 1 || month > time_constants::kMonthsPerYear)
            return 0;
        if (year < time_constants::kMinReprYear || year > time_constants::kMaxReprYear)
            return 0;
        if (utc_tm.tm_mday < 1 || utc_tm.tm_mday > time_constants::kMaxDayOfMonth)
            return 0;

        // note: 492, 19 and 4 are floor(1969/4), floor(1969/100) and floor(1969/400).
        const std::int64_t days_to_year{
            (static_cast<std::int64_t>(365) * (year - time_constants::kUnixEpochYear)) +
            ((prev_year / 4) - 492) - ((prev_year / 100) - 19) + ((prev_year / 400) - 4)};

        constexpr std::array<int, 12> kMonthOffset{0,   31,  59,  90,  120, 151,
                                                   181, 212, 243, 273, 304, 334};
        const bool leap{(year % 4 == 0 && year % time_constants::kCenturyYears != 0) ||
                        year % time_constants::kLeapCycleYears == 0};
        const int yday{kMonthOffset[static_cast<std::size_t>(month - 1)] +
                       (month > 2 && leap ? 1 : 0) + utc_tm.tm_mday - 1};

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
        // note: `auto`, never `auto*` - MSVC's iterator is a wrapper class, not a raw pointer.
        // NOLINTNEXTLINE(readability-qualified-auto)
        const auto month_it{std::ranges::find(kMonthNames, month_str)};
        if (month_it != kMonthNames.end())
            return static_cast<int>(std::distance(kMonthNames.begin(), month_it)) + 1;
        return -1;
    }

    // invariant: locale-independent - std::tolower reads the global C locale, which would make a
    // non-ASCII byte's fold ambient and the output cross-OS unstable.
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

    std::size_t pos{19};
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
    }

    return std::chrono::system_clock::from_time_t(parsed_time);
}

// pre: `reference_year` is supplied by the caller - RFC3164 carries no year.
// invariant: no wall-clock read, so the parsed instant is reproducible across a year rollover.
// refs: BIB:determinism_model
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

    std::tm parsed_tm{};
    parsed_tm.tm_year = reference_year - time_constants::kTmYearOffset;
    parsed_tm.tm_mon = month - 1;
    parsed_tm.tm_mday = day;
    parsed_tm.tm_hour = hour;
    parsed_tm.tm_min = minute;
    parsed_tm.tm_sec = second;

    return std::chrono::system_clock::from_time_t(utc_mktime(parsed_tm));
}

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

std::optional<Timestamp> parse_epoch_timestamp(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.empty() || timestamp_str.size() > time_constants::kEpochTimestampMaxDigits)
    {
        return std::nullopt;
    }
    std::int64_t epoch{0};
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

// invariant: integer only, never float - the duration_cast truncation is deterministic and the
// producer's millisecond-granular nanos make it lossless.
// refs: ADR-29.D5
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

std::optional<Timestamp> parse_apache_error_ts(std::string_view timestamp_str) noexcept
{
    if (timestamp_str.size() < time_constants::kApacheErrorMinLength)
        return std::nullopt;
    const char* ptr = timestamp_str.data();

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

// invariant: all three clock fields are variable-width, as is the millisecond terminator.
// refs: DN-43.O5
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

    // assert: the digit test before from_chars is load-bearing - from_chars accepts a leading '-',
    // which would publish a normalized wrong instant instead of refusing the field.
    // note: two digits at most keeps the accepted language equal to is_health_app_prefix's.
    const char* time_ptr = ptr + 9;
    const char* const end_ptr = ptr + timestamp_str.size();

    const auto take_clock_field{
        [end_ptr](const char*& cursor, int& out_value) noexcept
        {
            static constexpr std::ptrdiff_t kMaxFieldDigits{2};
            static constexpr unsigned kDecimalRadix{10U};
            if (cursor >= end_ptr || static_cast<unsigned>(*cursor) - '0' >= kDecimalRadix)
            {
                return false;
            }
            const char* const scan_end{std::min(cursor + kMaxFieldDigits, end_ptr)};
            const auto result{std::from_chars(cursor, scan_end, out_value)};
            if (result.ec != std::errc{} || result.ptr >= end_ptr || *result.ptr != ':')
            {
                return false;
            }
            cursor = result.ptr + 1;
            return true;
        }};

    int hour{0};
    int minute{0};
    int second{0};
    if (!take_clock_field(time_ptr, hour))
        return std::nullopt;
    if (!take_clock_field(time_ptr, minute))
        return std::nullopt;
    if (!take_clock_field(time_ptr, second))
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

LogLevel parse_log_level(std::string_view level_str) noexcept
{
    if (level_str.empty())
        return LogLevel::Unknown;

    switch (static_cast<unsigned char>(level_str[0]) | 0x20U)
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
        // note: BGL emits FAILURE as a top RAS severity, beside FATAL.
        // refs: DN-43.D14
        return (iequals(level_str, "fatal") || iequals(level_str, "failure")) ? LogLevel::Fatal
                                                                              : LogLevel::Unknown;
    case 's':
        // note: BGL's SEVERE sits between ERROR and FATAL, and LogLevel has no tier there.
        // refs: DN-43.D14
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
    // invariant: only an alerting tier can be falsely earned, so the outcome guard is paid on a
    // would-be-positive result only.
    // refs: SRC-D-OUT-1, SRC-D-OUT-1b
    [[nodiscard]] constexpr bool is_alerting_level(LogLevel level) noexcept
    {
        return level == LogLevel::Warn || level == LogLevel::Error || level == LogLevel::Fatal;
    }

    // pre: `token` is a sub-view of `line`; cold - reached only once a level word matched.
    // post: true iff any token starts after `token` ends, derived over the WHOLE line.
    // invariant: terminality is a property of the line - deriving it inside a bounded head made the
    // verdict move with the prefix's byte count.
    // refs: ADR-20.D3, SRC-D-OUT-4c
    // note: for_each_token's substr is the only throw path and its bound is checked.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] bool token_follows(std::string_view line, std::string_view token) noexcept
    {
        const std::size_t end{static_cast<std::size_t>(token.data() - line.data()) + token.size()};
        return for_each_token(line.substr(end), 0U, [](std::string_view) noexcept { return true; });
    }
} // namespace

// note: for_each_token's substr is the only throw path and its bound is checked.
// NOLINTNEXTLINE(bugprone-exception-escape)
EventLevel infer_leading_log_level(std::string_view line) noexcept
{
    // assert: both stages read ONE tokenisation - identifier and path bytes stay inside a token, so
    // a dotted file name is one atom while a bracketed level still exposes its word.
    // invariant: the budget counts significant tokens, never raw bytes - it decides whether the
    // producer's own kind word is read at all, which makes it part of the claim.
    // refs: ADR-16.D7, ADR-16.D8, DN-54.D22
    // note: the value is a measurement, not a judgment - re-run the instrument before moving it.
    // refs: F-SRC-insight-canon:leading_level_token_index_measure.cpp
    constexpr std::size_t kLeadingScanTokens{8};
    // invariant: a COST bound over canon's own cue vocabulary, so raw bytes are admissible here; a
    // prefix longer than this is the declared residual.
    // note: 128 aligns with kOutcomeHead - a verdict anchor can sit at the end of a long line.
    // refs: ADR-16.D7, ADR-16.D8, SRC-D-RNK-2
    constexpr std::size_t kKeywordHead{128};

    // assert: a leading level word is authoritative only when verdict-anchored or terminal -
    // parse_log_level is outcome-blind, so "error handling enabled" would otherwise alert.
    // refs: SRC-D-OUT-4
    LogLevel leading{LogLevel::Unknown};
    std::string_view level_token{};
    std::size_t visited{0};
    // assert: the byte limit is 0 (the whole line) and the visitor bounds the walk itself, so token
    // index kLeadingScanTokens is never visited whatever byte it starts at.
    (void)for_each_token(line, 0U,
                         [&](std::string_view token) noexcept
                         {
                             const LogLevel level{parse_log_level(token)};
                             if (level == LogLevel::Unknown)
                                 return ++visited == kLeadingScanTokens;
                             leading = level;
                             level_token = token;
                             return true;
                         });
    const bool token_follows_level{leading != LogLevel::Unknown &&
                                   token_follows(line, level_token)};
    // assert: a leading level word in count register is a summary, not a verdict - it is checked
    // before the anchors and falls through to Stage 2.
    // refs: SRC-D-CNT-1
    if (leading != LogLevel::Unknown &&
        (detail::is_verdict_anchored(line, level_token) || !token_follows_level) &&
        !(is_alerting_level(leading) && detail::is_count_register(line, level_token)))
    {
        // assert: a leading pass glyph demotes an alerting level to Unknown; a genuine "ERROR:"
        // leads with the word, so nothing is lost.
        // refs: SRC-D-OUT-1b
        if (is_alerting_level(leading) && detail::leading_outcome_is_pass(line))
            return EventLevel::inferred(LogLevel::Unknown);
        return EventLevel::inferred(leading);
    }

    if (contains_failure_cue(line, kKeywordHead))
        // assert: contains_failure_cue self-guards on the outcome, so it is not applied twice.
        return EventLevel::inferred(LogLevel::Error);
    // assert: a count-register failure word caps the line at Warn - demote, never suppress.
    // refs: SRC-D-CNT-1
    if (detail::contains_failure_summary_cue(line, kKeywordHead))
        return EventLevel::inferred(detail::leading_outcome_is_pass(line) ? LogLevel::Unknown
                                                                          : LogLevel::Warn);
    // assert: contains_warning_cue has no outcome guard, so the caller applies it here.
    // refs: SRC-D-OUT-1b
    if (contains_warning_cue(line, kKeywordHead))
        return EventLevel::inferred(detail::leading_outcome_is_pass(line) ? LogLevel::Unknown
                                                                          : LogLevel::Warn);
    return EventLevel::inferred(LogLevel::Unknown);
}

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
// NOLINTEND(readability-function-cognitive-complexity)
