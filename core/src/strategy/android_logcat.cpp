module;
#include "utils/log_macros.hpp" // textual macro layer (ADR-3.D4)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;

// AndroidLogcatStrategy — parses Android logcat format:
//   "03-17 16:13:38.811  1702  2395 D WindowManager: msg"
//
// Hot path: a hand-written std::string_view parser walks the line in a single
// pass with O(1) layout checks at fixed offsets — no regex, no heap.
// Malformed lines that fail the fast path are a parse miss (returned as error).

namespace insight::tokenization
{

namespace
{
    [[nodiscard]] constexpr bool is_digit(char chr) noexcept
    {
        return chr >= '0' && chr <= '9';
    }

    // Cheap layout check matching the first 18 bytes of a logcat line.
    // Used both by confidence() and as the fast-path gate in parse(); a single
    // implementation guarantees the two never disagree.
    [[nodiscard]] bool has_logcat_prefix(std::string_view line) noexcept
    {
        // ── Layout constants for "MM-DD HH:MM:SS.mmm" (18 chars) ──────────
        static constexpr std::size_t kMinimumCandidateLength{30};
        static constexpr std::size_t kMonthSep{2}; // '-'
        static constexpr std::size_t kDay1{3};
        static constexpr std::size_t kDay2{4};
        static constexpr std::size_t kDayHourSep{5}; // ' ' between date and time
        static constexpr std::size_t kHour1{6};
        static constexpr std::size_t kHour2{7};
        static constexpr std::size_t kHourMinSep{8}; // ':'
        static constexpr std::size_t kMin1{9};
        static constexpr std::size_t kMin2{10};
        static constexpr std::size_t kMinSecSep{11}; // ':'
        static constexpr std::size_t kSec1{12};
        static constexpr std::size_t kSec2{13};
        static constexpr std::size_t kSecMillisSep{14}; // '.'
        static constexpr std::size_t kMillis1{15};
        static constexpr std::size_t kMillis2{16};
        static constexpr std::size_t kMillis3{17};

        if (line.size() < kMinimumCandidateLength)
            return false;
        // "MM-DD HH:MM:SS.mmm"
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
        case 'S': // Silent — mapped to Trace (lowest severity)
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

    // Advance `pos` past any spaces/tabs. Returns false if we ran out of input.
    [[nodiscard]] bool skip_ws(std::string_view line, std::size_t& pos) noexcept
    {
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
            ++pos;
        return pos < line.size();
    }

    // Consume a run of decimal digits and advance `pos`. Returns false if no
    // digit was present at the current position.
    [[nodiscard]] bool skip_digits(std::string_view line, std::size_t& pos) noexcept
    {
        const std::size_t start{pos};
        while (pos < line.size() && is_digit(line[pos]))
            ++pos;
        return pos > start;
    }

    // Single-pass manual parser for logcat. Returns true on success and fills
    // `out_level`, `out_tag`, `out_message` with views into `line`. Performs
    // zero allocations.
    [[nodiscard]] bool parse_fast(std::string_view line, LogLevel& out_level,
                                  std::string_view& out_tag, std::string_view& out_message) noexcept
    {
        static constexpr std::size_t kTimestampLength{18}; // "MM-DD HH:MM:SS.mmm"
        if (!has_logcat_prefix(line)) [[unlikely]]
            return false;

        std::size_t pos{kTimestampLength};
        // PID
        if (!skip_ws(line, pos) || !skip_digits(line, pos))
            return false;
        // TID
        if (!skip_ws(line, pos) || !skip_digits(line, pos))
            return false;
        // Level char
        if (!skip_ws(line, pos) || pos >= line.size() || !is_level_char(line[pos]))
            return false;
        const char level_char{line[pos]};
        ++pos;
        // Mandatory single space between level and tag.
        if (pos >= line.size() || line[pos] != ' ')
            return false;
        ++pos;

        // Tag = up to ':'. Logcat tags do not contain ':'; if we don't find one,
        // the line is malformed.
        const std::size_t tag_start{pos};
        const std::size_t colon{line.find(':', pos)};
        if (colon == std::string_view::npos) [[unlikely]]
            return false;

        // Trim trailing spaces from the tag (logcat right-pads short tags).
        std::size_t tag_end{colon};
        while (tag_end > tag_start && line[tag_end - 1] == ' ')
            --tag_end;
        if (tag_end == tag_start)
            return false;

        // Message = everything after ':' + optional single leading space.
        std::size_t msg_start{colon + 1};
        if (msg_start < line.size() && line[msg_start] == ' ')
            ++msg_start;

        out_level = char_to_level(level_char);
        out_tag = line.substr(tag_start, tag_end - tag_start);
        out_message = line.substr(msg_start);
        return true;
    }

    // ── RE2 fallback removed ──────────────────────────────────────────────
    // The fast path handles all well-formed logcat lines. Malformed lines
    // are a parse miss; callers can discard or route to a catch-all strategy.

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

    // tag/message are views into `line`, which was arena-stored by the engine
    // before calling parse() — they are arena-stable already.
    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = std::nullopt;
    parsed_line.level = level;
    parsed_line.component = tag;
    parsed_line.content = message;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=AndroidLogcat parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level),
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
