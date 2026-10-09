// refs: DN-43.D21, ADR-20.D16, ADR-7.D4
// invariant: Apache 2.4's error-log line -- in the LEVEL seat only, a bracket holding a colon is
// `module:level`: the level is the segment after the last colon and the module is `component`.
// invariant: the clock takes an optional 1-9 digit fraction before the year, checked and never
// read, so the event time keeps one-second grain; a fraction-free clock reads as before.
// invariant: a colon-free seat reads its word as before, except Apache's trace1 to trace8, which
// read Trace in either shape of the seat and only there (ruling item 4, amended 2026-10-01).
// invariant: every expected value here is written from the BYTES and the ruling -- never read off
// canon's output -- so a table row cannot agree with the code by construction.
// invariant: R1's population is a byte copy of coderoast-corpora 5515deb's httpd 2.4 capture
// (`error_log`, first-party, Apache/2.4.68), read from this repository as ADR-7.D4 requires.
// note: byte-only and single-threaded: no RNG, no wall clock, the arena reset per line.
#include <gtest/gtest.h>

#include <picosha2.h>

import insight.canon.test;

namespace
{

using insight::LogFormat;
using insight::LogLevel;
using insight::Timestamp;
using insight::tokenization::ApacheErrorLogStrategy;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::ParsedLine;
using insight::tokenization::RawTextStrategy;
using insight::tokenization::Tokenizer;

constexpr std::size_t kArenaBytes{std::size_t{1} << 16U};

[[nodiscard]] Timestamp at_second(std::int64_t epoch_seconds)
{
    return Timestamp{std::chrono::seconds{epoch_seconds}};
}

[[nodiscard]] std::string render_time(const Timestamp& when)
{
    return std::to_string(
               std::chrono::floor<std::chrono::seconds>(when).time_since_epoch().count()) +
           " s";
}

[[nodiscard]] std::string render_time(const std::optional<Timestamp>& when)
{
    return when ? render_time(*when) : "none";
}

[[nodiscard]] std::string render(const ParsedLine& parsed)
{
    return "{time=" + (parsed.timestamp.has_value() ? render_time(*parsed.timestamp) : "none") +
           " level=" + std::string{to_string(parsed.level.value())} +
           " declared=" + (parsed.level.is_declared() ? "yes" : "no") + " component=\"" +
           std::string{parsed.component} + "\" content=\"" + std::string{parsed.content} + "\"}";
}

[[nodiscard]] std::expected<ParsedLine, std::string> parse_apache(std::string_view line,
                                                                  ArenaAllocator& arena)
{
    return ApacheErrorLogStrategy{}.parse(line, arena);
}

// invariant: R1's population -- the capture's identity, asserted before any reading of it.
struct CaptureIdentity
{
    std::string_view relative_path;
    std::uintmax_t bytes;
    std::size_t lines;
    std::string_view sha256;
};

constexpr CaptureIdentity kCapture{
    .relative_path = "fixtures/httpd24_error_capture/error_log",
    .bytes = 2909U,
    .lines = 21U,
    .sha256 = "3476e9ca35f7478eff5550a32422862f5a8104dddf01285558eb1366c1ee5010"};

// invariant: one row per line of the capture, in file order, written from its bytes: the clock with
// its fraction removed, the seat's last segment through ADR-20.D16's map, the seat's module.
struct CaptureRow
{
    std::size_t line;
    LogFormat format;
    std::optional<std::int64_t> event_seconds;
    LogLevel level;
    bool declared_level;
    std::string_view component;
    std::string_view content;
};

constexpr std::string_view kAh00558{
    "AH00558: httpd: Could not reliably determine the server's fully qualified domain name, "
    "using 172.17.0.2. Set the 'ServerName' directive globally to suppress this message"};
constexpr std::string_view kAh00489{
    "AH00489: Apache/2.4.68 (Unix) configured -- resuming normal operations"};
constexpr std::string_view kAh00490{"AH00490: Server built: Sep 19 2026 00:19:44"};
constexpr std::string_view kAh00094{"AH00094: Command line: 'httpd -D FOREGROUND'"};
constexpr std::string_view kAh02639{"log.c(1611): AH02639: Using SO_REUSEPORT: yes (1)"};
constexpr std::string_view kSetLogLevel{
    "core.c(3517): Setting LogLevel for module core.c to trace3"};

// invariant: lines 2 and 15 carry no clock bracket, so ApacheError does not claim them: they reach
// RawText whole, with no time, no component, and the level the lexicon infers -- Unknown.
// refs: DN-43.D21
constexpr std::array<CaptureRow, 21> kCaptureTable{{
    {1, LogFormat::ApacheError, 1790862866, LogLevel::Trace, true, "core", kSetLogLevel},
    {2, LogFormat::RawText, std::nullopt, LogLevel::Unknown, false, "", kAh00558},
    {3, LogFormat::ApacheError, 1790862866, LogLevel::Info, true, "mpm_event", kAh00489},
    {4, LogFormat::ApacheError, 1790862866, LogLevel::Info, true, "mpm_event", kAh00490},
    {5, LogFormat::ApacheError, 1790862866, LogLevel::Info, true, "core", kAh00094},
    {6, LogFormat::ApacheError, 1790862866, LogLevel::Debug, true, "core", kAh02639},
    {7, LogFormat::ApacheError, 1790862866, LogLevel::Trace, true, "core",
     "request.c(360): [client 172.17.0.1:49528] request authorized without authentication by "
     "access_checker_ex hook: /"},
    {8, LogFormat::ApacheError, 1790862867, LogLevel::Trace, true, "core",
     "request.c(360): [client 172.17.0.1:49544] request authorized without authentication by "
     "access_checker_ex hook: /"},
    {9, LogFormat::ApacheError, 1790862868, LogLevel::Trace, true, "core",
     "request.c(360): [client 172.17.0.1:49556] request authorized without authentication by "
     "access_checker_ex hook: /missing.html"},
    {10, LogFormat::ApacheError, 1790862868, LogLevel::Info, true, "core",
     "AH00128: File does not exist: /usr/local/apache2/htdocs/missing.html"},
    {11, LogFormat::ApacheError, 1790862869, LogLevel::Error, true, "authz_core",
     "AH01630: client denied by server configuration: /usr/local/apache2/htdocs/denied"},
    {12, LogFormat::ApacheError, 1790862869, LogLevel::Trace, true, "core",
     "request.c(120): [client 172.17.0.1:49558] auth phase 'check access' gave status 403: "
     "/denied"},
    {13, LogFormat::ApacheError, 1790862870, LogLevel::Info, true, "mpm_event",
     "AH00493: SIGUSR1 received.  Doing graceful restart"},
    {14, LogFormat::ApacheError, 1790862870, LogLevel::Trace, true, "core", kSetLogLevel},
    {15, LogFormat::RawText, std::nullopt, LogLevel::Unknown, false, "", kAh00558},
    {16, LogFormat::ApacheError, 1790862870, LogLevel::Info, true, "mpm_event", kAh00489},
    {17, LogFormat::ApacheError, 1790862870, LogLevel::Info, true, "mpm_event", kAh00490},
    {18, LogFormat::ApacheError, 1790862870, LogLevel::Info, true, "core", kAh00094},
    {19, LogFormat::ApacheError, 1790862870, LogLevel::Debug, true, "core", kAh02639},
    {20, LogFormat::ApacheError, 1790862873, LogLevel::Info, true, "core",
     "AH00096: removed PID file /usr/local/apache2/logs/httpd.pid (pid=1)"},
    {21, LogFormat::ApacheError, 1790862873, LogLevel::Info, true, "mpm_event",
     "AH00492: caught SIGWINCH, shutting down gracefully"},
}};

[[nodiscard]] std::string read_bytes(const std::filesystem::path& file)
{
    std::ifstream input{file, std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

// post: the newline-split lines of `bytes`; a final newline closes the last line, it opens none.
[[nodiscard]] std::vector<std::string_view> split_lines(std::string_view bytes)
{
    std::vector<std::string_view> lines;
    std::size_t begin{0};
    while (begin < bytes.size())
    {
        const std::size_t end{std::min(bytes.find('\n', begin), bytes.size())};
        lines.push_back(bytes.substr(begin, end - begin));
        begin = end + 1;
    }
    return lines;
}

// refs: DN-43.D21
// invariant: R1 -- the 19 clock-bracketed lines are claimed by ApacheError with the table's time,
// level and component; the 2 headerless lines stay RawText, the control a widened claim reds.
// invariant: the lines go through ONE tokenizer in file order, because detection latches and a
// headerless line re-runs it -- the order the shipping ingest reads them in.
TEST(ApacheErrorTwoFourCapture, EveryLineReadsAsTheTableWrittenFromItsBytes)
{
    const std::filesystem::path file{std::filesystem::path{INSIGHT_CANON_CORE_SOURCE_ROOT} /
                                     "tests" / "strategy" / kCapture.relative_path};
    ASSERT_TRUE(std::filesystem::is_regular_file(file)) << file.string() << " is missing.";
    const std::string bytes{read_bytes(file)};
    ASSERT_EQ(bytes.size(), kCapture.bytes) << file.string() << " is not the pinned capture.";
    ASSERT_EQ(picosha2::hash256_hex_string(bytes), kCapture.sha256)
        << file.string() << " is not byte-identical to coderoast-corpora 5515deb's error_log.";
    const std::vector<std::string_view> lines{split_lines(bytes)};
    ASSERT_EQ(lines.size(), kCapture.lines);
    ASSERT_EQ(kCaptureTable.size(), lines.size());

    const insight::semantic::ComposedSemantics composed{
        insight::test_support::degenerate_composition()};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, composed, insight::tokenization::StreamContext{}};
    std::size_t wrong_rows{0};
    for (std::size_t index{0}; index < lines.size(); ++index)
    {
        const CaptureRow& row{kCaptureTable.at(index)};
        ASSERT_EQ(row.line, index + 1) << "the table must be in file order.";
        const std::string_view line{lines.at(index)};
        const auto event{tokenizer.process_line(line)};
        ASSERT_TRUE(event.has_value())
            << "line " << row.line << " produced no event: " << event.error()
            << "\n  bytes: " << line;
        const std::optional<Timestamp> expected_time{
            row.event_seconds ? std::optional<Timestamp>{at_second(*row.event_seconds)}
                              : std::nullopt};
        const bool row_ok{event->format == row.format && event->timestamp == expected_time &&
                          event->level == row.level &&
                          event->declared_level == row.declared_level &&
                          event->component == row.component};
        EXPECT_TRUE(row_ok) << "R1 line " << row.line << ": got format=" << to_string(event->format)
                            << " time=" << render_time(event->timestamp)
                            << " level=" << to_string(event->level)
                            << " declared=" << event->declared_level << " component=\""
                            << event->component << "\"\n  expected format=" << to_string(row.format)
                            << " time=" << render_time(expected_time)
                            << " level=" << to_string(row.level)
                            << " declared=" << row.declared_level << " component=\""
                            << row.component << "\"\n  bytes: " << line;
        if (!row_ok)
            ++wrong_rows;
        arena.reset();

        const auto parsed{row.format == LogFormat::ApacheError
                              ? parse_apache(line, arena)
                              : RawTextStrategy{}.parse(line, arena)};
        ASSERT_TRUE(parsed.has_value()) << "line " << row.line << ": " << parsed.error();
        EXPECT_EQ(parsed->content, row.content)
            << "R1 line " << row.line << ": content moved. Parsed " << render(*parsed);
        EXPECT_EQ(parsed->timestamp.has_value(), row.event_seconds.has_value())
            << "R1 line " << row.line << ": parsed " << render(*parsed);
        arena.reset();
    }
    EXPECT_EQ(wrong_rows, 0U) << "R1: " << wrong_rows << " of " << lines.size()
                              << " lines read differently from the table written from the bytes.";
}

// refs: DN-43.D21
// invariant: R2, the neighbour control -- the pid and client brackets hold colons too, and only the
// bracket right after the clock is the seat, so none of their segments reaches the level.
TEST(ApacheErrorTwoFourSeat, AColonInALaterBracketNeverReachesTheLevel)
{
    constexpr std::string_view kLine{
        "[Wed Oct 11 14:32:52 2000] [error] [pid 1:tid 2] [client 10.0.0.1:5678] msg"};
    ArenaAllocator arena{kArenaBytes};
    const auto parsed{parse_apache(kLine, arena)};
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(parsed->level, LogLevel::Error) << "R2: " << render(*parsed);
    EXPECT_TRUE(parsed->level.is_declared()) << "R2: " << render(*parsed);
    EXPECT_EQ(parsed->component, "httpd") << "R2: " << render(*parsed);
    EXPECT_EQ(parsed->content, "msg") << "R2: " << render(*parsed);
    ASSERT_TRUE(parsed->timestamp.has_value()) << "R2: " << render(*parsed);
    EXPECT_EQ(*parsed->timestamp, at_second(971274772)) << "R2: " << render(*parsed);
}

// refs: DN-43.D21
// invariant: the 2.4 default line as the httpd ErrorLogFormat documentation prints it (synthetic):
// the compound seat, a microsecond clock and both neighbour brackets in one line.
TEST(ApacheErrorTwoFourSeat, TheDocumentedDefaultLineReadsItsModuleLevelAndClock)
{
    constexpr std::string_view kLine{
        "[Wed Oct 11 14:32:52.123456 2000] [core:error] [pid 35708:tid 4328636416] "
        "[client 72.15.99.187:5678] AH00128: File does not exist: /usr/local/apache2/htdocs/x"};
    ArenaAllocator arena{kArenaBytes};
    const auto parsed{parse_apache(kLine, arena)};
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(parsed->level, LogLevel::Error) << render(*parsed);
    EXPECT_TRUE(parsed->level.is_declared()) << render(*parsed);
    EXPECT_EQ(parsed->component, "core") << render(*parsed);
    EXPECT_EQ(parsed->content, "AH00128: File does not exist: /usr/local/apache2/htdocs/x")
        << render(*parsed);
    ASSERT_TRUE(parsed->timestamp.has_value()) << render(*parsed);
    EXPECT_EQ(*parsed->timestamp, at_second(971274772)) << render(*parsed);
}

struct ClockCase
{
    std::string_view fraction;
    bool has_event_time;
};

// invariant: one clock, Wed Oct 11 14:32:52 2000 = 971274772 s, with each fraction the ruling names
// and the two boundaries of 1-9 digits.
constexpr std::array<ClockCase, 9> kClockCases{{
    {.fraction = "", .has_event_time = true},
    {.fraction = ".123456", .has_event_time = true},
    {.fraction = ".123", .has_event_time = true},
    {.fraction = ".1", .has_event_time = true},
    {.fraction = ".123456789", .has_event_time = true},
    {.fraction = ".", .has_event_time = false},
    {.fraction = ".12a", .has_event_time = false},
    {.fraction = ".1234567890", .has_event_time = false},
    {.fraction = ",123", .has_event_time = false},
}};

// refs: DN-43.D21
// invariant: R4 -- a valid fraction gives the SAME instant as the fraction-free line, which gives
// today's instant; anything else gives no event time, and every case is still claimed.
TEST(ApacheErrorTwoFourClock, AFractionIsCheckedAndDroppedNeverRead)
{
    ArenaAllocator arena{kArenaBytes};
    for (const ClockCase& clock : kClockCases)
    {
        const std::string line{"[Wed Oct 11 14:32:52" + std::string{clock.fraction} +
                               " 2000] [error] [pid 7] msg"};
        const auto parsed{parse_apache(line, arena)};
        ASSERT_TRUE(parsed.has_value()) << "R4 \"" << line << "\": " << parsed.error();
        EXPECT_EQ(parsed->timestamp.has_value(), clock.has_event_time)
            << "R4 \"" << line << "\": " << render(*parsed);
        if (clock.has_event_time && parsed->timestamp.has_value())
            EXPECT_EQ(*parsed->timestamp, at_second(971274772))
                << "R4 \"" << line << "\": the fraction moved the instant. " << render(*parsed);
        arena.reset();
    }
}

// refs: DN-43.D21, DN-17.D42
// invariant: R4 at the public door on LogCraft's own ApacheError form -- a 2.2 level bracket after
// a millisecond clock -- the line the join's ApacheError cell reads.
TEST(ApacheErrorTwoFourClock, LogCraftsMillisecondLineIsClaimedWithItsEventTime)
{
    constexpr std::string_view kLine{"[Wed Oct 11 14:32:52.123 2000] [error] [pid 7] msg"};
    const insight::semantic::ComposedSemantics composed{
        insight::test_support::degenerate_composition()};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, composed, insight::tokenization::StreamContext{}};
    const auto event{tokenizer.process_line(kLine)};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->format, LogFormat::ApacheError) << to_string(event->format);
    EXPECT_EQ(event->timestamp, at_second(971274772))
        << "R4: LogCraft's .mmm clock gives no event time; got " << render_time(event->timestamp);
    EXPECT_EQ(event->level, LogLevel::Error) << to_string(event->level);
}

struct SeatCase
{
    std::string_view seat;
    LogLevel level;
    bool declared;
    std::string_view component;
};

// invariant: R5 (synthetic, labelled) -- an empty module keeps `httpd`, an empty level reads
// Unknown with its module kept, and the LAST colon splits a module that holds one itself.
constexpr std::array<SeatCase, 3> kSeatCases{{
    {.seat = "[:error]", .level = LogLevel::Error, .declared = true, .component = "httpd"},
    {.seat = "[core:]", .level = LogLevel::Unknown, .declared = false, .component = "core"},
    {.seat = "[a:b:warn]", .level = LogLevel::Warn, .declared = true, .component = "a:b"},
}};

// refs: DN-43.D21
TEST(ApacheErrorTwoFourSeat, SeatEdgeCasesSplitAtTheLastColon)
{
    constexpr std::string_view kContent{"AH00000: synthetic seat case"};
    ArenaAllocator arena{kArenaBytes};
    for (const SeatCase& seat : kSeatCases)
    {
        const std::string line{"[Wed Oct 11 14:32:52.123456 2000] " + std::string{seat.seat} +
                               " [pid 4242:tid 1] " + std::string{kContent}};
        const auto parsed{parse_apache(line, arena)};
        ASSERT_TRUE(parsed.has_value()) << "R5 " << seat.seat << ": " << parsed.error();
        EXPECT_EQ(parsed->level, seat.level) << "R5 " << seat.seat << ": " << render(*parsed);
        EXPECT_EQ(parsed->level.is_declared(), seat.declared)
            << "R5 " << seat.seat << ": " << render(*parsed);
        EXPECT_EQ(parsed->component, seat.component)
            << "R5 " << seat.seat << ": " << render(*parsed);
        EXPECT_EQ(parsed->content, kContent)
            << "R5 " << seat.seat << ": the seat's bytes must reach a field or stay in content, "
            << "never both. " << render(*parsed);
        arena.reset();
    }
}

// invariant: Apache's own trace levels, trace1 to trace8; R6 holds them out of the shared lexicon.
constexpr std::array<std::string_view, 8> kApacheTraceWords{"trace1", "trace2", "trace3", "trace4",
                                                            "trace5", "trace6", "trace7", "trace8"};

struct TraceSeatCase
{
    std::string_view word;
    bool compound;
    LogLevel level;
};

// invariant: R7 and ruling item 4 (synthetic, labelled) -- in this strategy's level seat, in
// either shape, trace1 to trace8 read Trace; trace0 and trace9 are not Apache levels, so Unknown.
// invariant: bare `trace` keeps the shared lexicon's reading, Trace at insight-canon b96af4c; a
// colon-free seat keeps `httpd` and `[core:...]` gives `core`, as ruling item 3 rules.
constexpr std::array<TraceSeatCase, 22> kTraceSeatCases{{
    {.word = "trace1", .compound = false, .level = LogLevel::Trace},
    {.word = "trace2", .compound = false, .level = LogLevel::Trace},
    {.word = "trace3", .compound = false, .level = LogLevel::Trace},
    {.word = "trace4", .compound = false, .level = LogLevel::Trace},
    {.word = "trace5", .compound = false, .level = LogLevel::Trace},
    {.word = "trace6", .compound = false, .level = LogLevel::Trace},
    {.word = "trace7", .compound = false, .level = LogLevel::Trace},
    {.word = "trace8", .compound = false, .level = LogLevel::Trace},
    {.word = "trace0", .compound = false, .level = LogLevel::Unknown},
    {.word = "trace9", .compound = false, .level = LogLevel::Unknown},
    {.word = "trace", .compound = false, .level = LogLevel::Trace},
    {.word = "trace1", .compound = true, .level = LogLevel::Trace},
    {.word = "trace2", .compound = true, .level = LogLevel::Trace},
    {.word = "trace3", .compound = true, .level = LogLevel::Trace},
    {.word = "trace4", .compound = true, .level = LogLevel::Trace},
    {.word = "trace5", .compound = true, .level = LogLevel::Trace},
    {.word = "trace6", .compound = true, .level = LogLevel::Trace},
    {.word = "trace7", .compound = true, .level = LogLevel::Trace},
    {.word = "trace8", .compound = true, .level = LogLevel::Trace},
    {.word = "trace0", .compound = true, .level = LogLevel::Unknown},
    {.word = "trace9", .compound = true, .level = LogLevel::Unknown},
    {.word = "trace", .compound = true, .level = LogLevel::Trace},
}};

// refs: DN-43.D21
// invariant: R7's own line is the colon-free `[trace3]` row; the compound rows are item 4's other
// shape, so a fix that covers one shape only leaves a red row.
TEST(ApacheErrorTwoFourSeat, ApacheTraceLevelsReadTraceInEitherShapeOfTheSeat)
{
    ArenaAllocator arena{kArenaBytes};
    std::size_t wrong_rows{0};
    for (const TraceSeatCase& seat : kTraceSeatCases)
    {
        const std::string_view component{seat.compound ? "core" : "httpd"};
        const std::string bracket{"[" + std::string{seat.compound ? "core:" : ""} +
                                  std::string{seat.word} + "]"};
        const std::string line{"[Wed Oct 11 14:32:52.123456 2000] " + bracket +
                               " [pid 1:tid 2] msg"};
        const auto parsed{parse_apache(line, arena)};
        ASSERT_TRUE(parsed.has_value()) << "R7 " << bracket << ": " << parsed.error();
        const bool declared{seat.level != LogLevel::Unknown};
        const bool row_ok{parsed->level == seat.level && parsed->level.is_declared() == declared &&
                          parsed->component == component && parsed->content == "msg"};
        EXPECT_TRUE(row_ok) << (seat.compound ? "item 4 " : "R7 ") << bracket << ": got "
                            << render(*parsed) << "\n  expected level=" << to_string(seat.level)
                            << " declared=" << declared << " component=\"" << component
                            << "\" content=\"msg\"";
        if (!row_ok)
            ++wrong_rows;
        arena.reset();
    }
    EXPECT_EQ(wrong_rows, 0U) << "R7: " << wrong_rows << " of " << kTraceSeatCases.size()
                              << " trace seats read differently from ruling item 4.";
}

// refs: DN-43.D21, ADR-20.D16
// invariant: R6 -- item 4's mapping lives in this strategy's level seat and nowhere else: R7 reads
// it there, and the shared lexicon and its free-text walk read as at insight-canon b96af4c.
// invariant: the walk is where a leak costs recall: a recognised word returns before the failure
// cues, a cost ADR-20.D16 declares for the syslog names only, never for Apache's trace levels.
// invariant: the two free-text lines are the ones a leak changes: a bracketed trace3 before a
// failure cue reads Error and a terminal trace3 reads Unknown.
TEST(ApacheErrorTwoFourLexicon, ApacheTraceLevelsStayInsideTheStrategysSeat)
{
    for (const std::string_view word : kApacheTraceWords)
        EXPECT_EQ(insight::utils::parse_log_level(word), LogLevel::Unknown)
            << "R6: the shared lexicon reads \"" << word << "\" as "
            << to_string(insight::utils::parse_log_level(word));
    EXPECT_EQ(insight::utils::parse_log_level("trace"), LogLevel::Trace)
        << "R6: the shared lexicon's own `trace` moved to "
        << to_string(insight::utils::parse_log_level("trace"));

    constexpr std::string_view kBeforeACue{"[trace3] connection to backend failed"};
    EXPECT_EQ(insight::utils::infer_leading_log_level(kBeforeACue), LogLevel::Error)
        << "R6: \"" << kBeforeACue << "\" reads "
        << to_string(insight::utils::infer_leading_log_level(kBeforeACue).value())
        << " -- a trace word in the lexicon returns before the failure cue.";
    constexpr std::string_view kTerminal{"worker state trace3"};
    EXPECT_EQ(insight::utils::infer_leading_log_level(kTerminal), LogLevel::Unknown)
        << "R6: \"" << kTerminal << "\" reads "
        << to_string(insight::utils::infer_leading_log_level(kTerminal).value());
}

} // namespace
