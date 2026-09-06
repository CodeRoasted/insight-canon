module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;

// post: an Android logcat record — a month-day clock, a pid and a tid, a one-letter level, then a
// colon-terminated tag.
// invariant: a single-pass hand-written scanner with O(1) layout checks at fixed offsets — no
// regex and no heap on the hot path.
// invariant: a malformed line is a parse MISS returned as an error, never a partial record.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{
    [[nodiscard]] constexpr bool is_digit(char chr) noexcept
    {
        return chr >= '0' && chr <= '9';
    }

    // post: true iff the first 18 bytes match the logcat clock layout.
    // invariant: ONE implementation serves both the confidence score and the parse fast-path gate,
    // so the two cannot disagree ABOUT THE CLOCK PREFIX.
    // invariant: that is narrower than the spi's confidence contract — the parse applies four
    // further checks this predicate does not, so a prefix-valid line can still score and then fail.
    [[nodiscard]] bool has_logcat_prefix(std::string_view line) noexcept
    {
        static constexpr std::size_t kMinimumCandidateLength{30};
        static constexpr std::size_t kMonthSep{2};
        static constexpr std::size_t kDay1{3};
        static constexpr std::size_t kDay2{4};
        static constexpr std::size_t kDayHourSep{5};
        static constexpr std::size_t kHour1{6};
        static constexpr std::size_t kHour2{7};
        static constexpr std::size_t kHourMinSep{8};
        static constexpr std::size_t kMin1{9};
        static constexpr std::size_t kMin2{10};
        static constexpr std::size_t kMinSecSep{11};
        static constexpr std::size_t kSec1{12};
        static constexpr std::size_t kSec2{13};
        static constexpr std::size_t kSecMillisSep{14};
        static constexpr std::size_t kMillis1{15};
        static constexpr std::size_t kMillis2{16};
        static constexpr std::size_t kMillis3{17};

        if (line.size() < kMinimumCandidateLength)
            return false;
        return is_digit(line[0]) && is_digit(line[1]) && line[kMonthSep] == '-' &&
               is_digit(line[kDay1]) && is_digit(line[kDay2]) && line[kDayHourSep] == ' ' &&
               is_digit(line[kHour1]) && is_digit(line[kHour2]) && line[kHourMinSep] == ':' &&
               is_digit(line[kMin1]) && is_digit(line[kMin2]) && line[kMinSecSep] == ':' &&
               is_digit(line[kSec1]) && is_digit(line[kSec2]) && line[kSecMillisSep] == '.' &&
               is_digit(line[kMillis1]) && is_digit(line[kMillis2]) && is_digit(line[kMillis3]);
    }

    [[nodiscard]] LogLevel char_to_level(char level_code) noexcept
    {
        switch (level_code)
        {
        case 'V':
            return LogLevel::Trace;
        case 'D':
            return LogLevel::Debug;
        case 'I':
            return LogLevel::Info;
        case 'W':
            return LogLevel::Warn;
        case 'E':
            return LogLevel::Error;
        case 'F':
            return LogLevel::Fatal;
        // invariant: the silent level maps to the LOWEST severity canon has rather than to an
        // absence, because the producer did declare a level.
        case 'S':
            return LogLevel::Trace;
        default:
            return LogLevel::Unknown;
        }
    }

    [[nodiscard]] constexpr bool is_level_char(char chr) noexcept
    {
        switch (chr)
        {
        case 'V':
        case 'D':
        case 'I':
        case 'W':
        case 'E':
        case 'F':
        case 'S':
            return true;
        default:
            return false;
        }
    }

    // post: false when the input ran out before a non-space byte.
    [[nodiscard]] bool skip_ws(std::string_view line, std::size_t& pos) noexcept
    {
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
            ++pos;
        return pos < line.size();
    }

    // post: false when no decimal digit was present at the current position.
    [[nodiscard]] bool skip_digits(std::string_view line, std::size_t& pos) noexcept
    {
        const std::size_t start{pos};
        while (pos < line.size() && is_digit(line[pos]))
            ++pos;
        return pos > start;
    }

    // post: on success the level, tag and message are views into the caller's line, and nothing is
    // allocated.
    // post: FALSE on a malformed line — there is NO regex fallback behind this, so a malformed
    // line is a parse MISS the caller may discard or route to the catch-all.
    [[nodiscard]] bool parse_fast(std::string_view line, LogLevel& out_level,
                                  std::string_view& out_tag, std::string_view& out_message) noexcept
    {
        static constexpr std::size_t kTimestampLength{18};
        if (!has_logcat_prefix(line)) [[unlikely]]
            return false;

        std::size_t pos{kTimestampLength};
        if (!skip_ws(line, pos) || !skip_digits(line, pos))
            return false;
        if (!skip_ws(line, pos) || !skip_digits(line, pos))
            return false;
        if (!skip_ws(line, pos) || pos >= line.size() || !is_level_char(line[pos]))
            return false;
        const char level_char{line[pos]};
        ++pos;
        if (pos >= line.size() || line[pos] != ' ')
            return false;
        ++pos;

        // invariant: a logcat tag cannot contain a colon, so a missing colon means the line is
        // malformed rather than that the tag runs to the end.
        const std::size_t tag_start{pos};
        const std::size_t colon{line.find(':', pos)};
        if (colon == std::string_view::npos) [[unlikely]]
            return false;

        // invariant: logcat right-pads short tags, so the trailing spaces are the producer's
        // padding and not part of the name.
        std::size_t tag_end{colon};
        while (tag_end > tag_start && line[tag_end - 1] == ' ')
            --tag_end;
        if (tag_end == tag_start)
            return false;

        std::size_t msg_start{colon + 1};
        if (msg_start < line.size() && line[msg_start] == ' ')
            ++msg_start;

        out_level = char_to_level(level_char);
        out_tag = line.substr(tag_start, tag_end - tag_start);
        out_message = line.substr(msg_start);
        return true;
    }

} // namespace

std::expected<ParsedLine, std::string> AndroidLogcatStrategy::parse(std::string_view line,
                                                                    ArenaAllocator& /*arena*/) const
{
    LogLevel level{LogLevel::Unknown};
    std::string_view tag;
    std::string_view message;

    if (!parse_fast(line, level, tag, message)) [[unlikely]]
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=AndroidLogcat parse miss");
        return std::unexpected(
            std::string("AndroidLogcatStrategy: line does not match logcat format"));
    }

    // invariant: the tag and message view the caller's line, which the engine arena-stored before
    // calling, so they are already arena-stable.
    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = EventTime::parsed(std::nullopt);
    parsed_line.level = EventLevel::declared(level);
    parsed_line.component = tag;
    parsed_line.content = message;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=AndroidLogcat parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level.value()),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat AndroidLogcatStrategy::format() const noexcept
{
    return LogFormat::AndroidLogcat;
}

double AndroidLogcatStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr double kLogcatConfidence{0.90};
    static constexpr double kNoConfidence{0.0};

    return has_logcat_prefix(line) ? kLogcatConfidence : kNoConfidence;
}

} // namespace insight::tokenization
