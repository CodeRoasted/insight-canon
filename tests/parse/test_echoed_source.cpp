// NOLINTBEGIN
// Unit tests: allow short identifiers and test-specific patterns
//
// D-PROV-1 / A1 (detection_provenance_and_legibility.md §3.1) — the echoed-source
// register. The §6.7 P3 blocker: GitHub-Actions echoes every line of a run-step script
// body wrapped in the command-echo SGR (`\x1b[36;1m … \x1b[0m`), so a `failed` inside an
// echoed `echo "Download failed after 3 attempts"` is NOT a runtime event — yet "failed"
// self-anchors and fires bare, ranking the echoed shell source above the real failure.
// The wrapper dies at strip (Fact 1: strip_escape_sequences runs before any classifier),
// so recognition MUST happen on the RAW line, at the strip layer. Two altitudes pinned:
//   1. is_echoed_source_line(raw)        — the byte-exact SGR recognition predicate (scan).
//   2. LogParser::parse_line(raw).level  — the GATE: an echoed line's level is demoted to
//      Unknown, transitively killing NewErrorPattern across all three eidos channels
//      (Fact 2). This is the parse-layer attribute the diff actually consumes.
// The diff-altitude consequence (demoted level ⇒ no NewErrorPattern) is already pinned by
// the D-OUT-1b diff-altitude guard (eidos diff classify_test) — it holds for ANY Unknown
// level, so echoed-source inherits it; not re-asserted here.
// [[sift-failure-lexicon-must-be-outcome-aware]]

#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogLevel;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::is_echoed_source_line;
using insight::tokenization::LogParser;

namespace
{
// A GHA run-step log line: a 28-char RFC3339 hi-res timestamp ("YYYY-MM-DDTHH:MM:SS" +
// "." + 7 fractional digits + "Z") and a single separating space, then the visible body.
constexpr std::string_view kGhaTs{"2026-06-09T09:40:20.4309100Z "};

[[nodiscard]] std::string gha(std::string_view body) { return std::string{kGhaTs} + std::string{body}; }
} // namespace

// ── Recognition (byte-exact SGR, raw line) ─────────────────────────────────────
// A line is echoed-source iff, after an optional GHA timestamp prefix, its entire visible
// content is ONE command-echo-SGR-wrapped span: open `36;1` (or `1;36`), a content run
// (possibly empty), a closing reset (`0` / empty `\x1b[m` / `39`), no un-wrapped bytes.
TEST(EchoedSource, CommandEchoWrappedLineIsRecognized)
{
    // The exact P3 false positive — the failure branch of a retry loop that did not run.
    EXPECT_TRUE(is_echoed_source_line("\x1b[36;1m    echo \"Download failed after 3 attempts\" >&2\x1b[0m"))
        << "the GHA command-echo of an `echo \"… failed …\"` script line";
    EXPECT_TRUE(is_echoed_source_line("\x1b[36;1mset -e\x1b[0m")) << "a wrapped script line, no timestamp";
    EXPECT_TRUE(is_echoed_source_line(gha("\x1b[36;1m    exit 1\x1b[0m")))
        << "GHA timestamp prefix is skipped, then the wrapped span recognized";
    EXPECT_TRUE(is_echoed_source_line("\x1b[1;36mif [ $i -eq 3 ]; then\x1b[0m"))
        << "the `1;36` parameter ordering is the equivalent command-echo SGR";
    EXPECT_TRUE(is_echoed_source_line("\x1b[36;1m\x1b[0m")) << "an empty wrapped span — a blank script line";
    EXPECT_TRUE(is_echoed_source_line("\x1b[36;1mexit 1\x1b[m")) << "implicit-reset close `\\x1b[m`";
    EXPECT_TRUE(is_echoed_source_line("\x1b[36;1mexit 1\x1b[39m")) << "default-foreground reset `39`";
    EXPECT_TRUE(is_echoed_source_line("\x1b[36;1m  fi\x1b[0m\r\n")) << "trailing CR/LF after the span tolerated";
}

// Disconfirming: a real coloured runtime event, a marker line, and a partial wrap must NOT
// read as echoed-source — the precision face (an over-broad recognizer would demote real
// errors that happen to be cyan-bold).
TEST(EchoedSource, NonCommandEchoLinesAreNotEchoedSource)
{
    EXPECT_FALSE(is_echoed_source_line("\x1b[31mERROR\x1b[0m: db connection refused"))
        << "red `31` is NOT the command-echo SGR — a real coloured ERROR line stays a runtime event";
    EXPECT_FALSE(is_echoed_source_line("##[error]Process completed with exit code 1"))
        << "a GHA error marker has no command-echo wrapper";
    EXPECT_FALSE(is_echoed_source_line("\x1b[36;1mecho building\x1b[0m && make all"))
        << "un-wrapped visible bytes after the span (`&& make all`) — not a clean single span";
    EXPECT_FALSE(is_echoed_source_line("\x1b[36;1mset -e")) << "no closing reset — not a clean wrapped span";
    EXPECT_FALSE(is_echoed_source_line("    echo \"Download failed after 3 attempts\""))
        << "plain (unwrapped) text — the ONLY robust signal is the SGR, which is absent here";
}

// ── The gate (parse layer): echoed ⇒ level Unknown ─────────────────────────────
// is_echoed_source_line runs on the RAW line in LogParser before the wrapper is stripped;
// on a match the parser sets echoed_source and demotes level to Unknown (Fact 2 single
// root). A failure WORD in echoed shell source therefore confers no alerting level.
TEST(EchoedSource, ParserDemotesEchoedFailureLevelToUnknown)
{
    ArenaAllocator arena{256U * 1024U};
    LogParser parser{arena};

    const auto echoed{parser.parse_line(gha("\x1b[36;1m    echo \"Download failed after 3 attempts\" >&2\x1b[0m"))};
    ASSERT_TRUE(echoed.has_value()) << "parse_line failed: " << echoed.error();
    EXPECT_TRUE(echoed->echoed_source) << "the command-echo wrapper must set echoed_source";
    EXPECT_EQ(echoed->level, LogLevel::Unknown)
        << "an echoed `… failed …` script line must NOT confer an alerting level (got "
        << static_cast<int>(echoed->level) << ")";
}

// The disconfirming minimal pair: the SAME failure vocabulary as a REAL coloured runtime
// event (red SGR, not command-echo) keeps its Error level and echoed_source stays false —
// the recognizer is byte-exact, so a real error is never swallowed by A1.
TEST(EchoedSource, ParserKeepsRealColouredErrorAsError)
{
    ArenaAllocator arena{256U * 1024U};
    LogParser parser{arena};

    const auto real{parser.parse_line(gha("\x1b[31mERROR\x1b[0m: db connection refused"))};
    ASSERT_TRUE(real.has_value()) << "parse_line failed: " << real.error();
    EXPECT_FALSE(real->echoed_source) << "a red (`31`) coloured line is not echoed-source";
    EXPECT_EQ(real->level, LogLevel::Error)
        << "a real coloured ERROR keeps its alerting level after the SGR is stripped (got "
        << static_cast<int>(real->level) << ")";
}

// NOLINTEND
