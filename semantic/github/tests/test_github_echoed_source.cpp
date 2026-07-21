// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_github_echoed_source.cpp — the GitHub-Actions echoed-source CODE TIER (ADR 0024 §2.3.2,
// D-PROV-1). Migrated from canon tests/parse/test_echoed_source.cpp. Two altitudes:
//   1. RECOGNITION — github::is_echoed_source(raw): the byte-exact command-echo SGR predicate,
//   relocated
//      into this package (the `\x1b[36;1m … \x1b[0m` grammar is GHA dialect knowledge).
//   2. THE GATE — the composed provenance hook, threaded through the PUBLIC Tokenizer
//   (compose({github})),
//      demotes an echoed failure line's level to Unknown and sets CanonicalEvent.echoed_source.
//      This is a composed integration: canon's parser layer consulting THIS package's hook.
// A red (`31`) coloured REAL error is byte-distinct from the command-echo SGR, so it is never
// swallowed — the precision face. Determinism: byte-exact state machine, no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;           // Tokenizer / ArenaAllocator / MaskConfig / LogLevel / compose
import insight.semantic.github; // is_echoed_source + kManifest

using insight::LogLevel;
using insight::semantic::ComposedSemantics;
using insight::semantic::github::is_echoed_source;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

namespace
{
constexpr std::string_view kGhaTs{"2026-06-09T09:40:20.4309100Z "};
[[nodiscard]] std::string gha(std::string_view body)
{
    return std::string{kGhaTs} + std::string{body};
}

[[nodiscard]] ComposedSemantics github_only()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests);
}
} // namespace

// ── Recognition (byte-exact SGR, raw line) ──
TEST(GithubEchoedSource, CommandEchoWrappedLineIsRecognized)
{
    EXPECT_TRUE(
        is_echoed_source("\x1b[36;1m    echo \"Download failed after 3 attempts\" >&2\x1b[0m"))
        << "the GHA command-echo of an `echo \"… failed …\"` script line";
    EXPECT_TRUE(is_echoed_source("\x1b[36;1mset -e\x1b[0m"))
        << "a wrapped script line, no timestamp";
    EXPECT_TRUE(is_echoed_source(gha("\x1b[36;1m    exit 1\x1b[0m")))
        << "GHA timestamp prefix skipped, then the wrapped span recognized";
    EXPECT_TRUE(is_echoed_source("\x1b[1;36mif [ $i -eq 3 ]; then\x1b[0m"))
        << "the `1;36` parameter ordering is the equivalent command-echo SGR";
    EXPECT_TRUE(is_echoed_source("\x1b[36;1m\x1b[0m"))
        << "an empty wrapped span — a blank script line";
    EXPECT_TRUE(is_echoed_source("\x1b[36;1mexit 1\x1b[m")) << "implicit-reset close `\\x1b[m`";
    EXPECT_TRUE(is_echoed_source("\x1b[36;1mexit 1\x1b[39m")) << "default-foreground reset `39`";
    EXPECT_TRUE(is_echoed_source("\x1b[36;1m  fi\x1b[0m\r\n"))
        << "trailing CR/LF after the span tolerated";
}

TEST(GithubEchoedSource, NonCommandEchoLinesAreNotEchoedSource)
{
    EXPECT_FALSE(is_echoed_source("\x1b[31mERROR\x1b[0m: db connection refused"))
        << "red `31` is NOT the command-echo SGR — a real coloured ERROR stays a runtime event";
    EXPECT_FALSE(is_echoed_source("##[error]Process completed with exit code 1"))
        << "a GHA error marker has no command-echo wrapper";
    EXPECT_FALSE(is_echoed_source("\x1b[36;1mecho building\x1b[0m && make all"))
        << "un-wrapped visible bytes after the span (`&& make all`) — not a clean single span";
    EXPECT_FALSE(is_echoed_source("\x1b[36;1mset -e"))
        << "no closing reset — not a clean wrapped span";
    EXPECT_FALSE(is_echoed_source("    echo \"Download failed after 3 attempts\""))
        << "plain (unwrapped) text — the ONLY robust signal is the SGR, absent here";
}

// ── The gate (composed, public Tokenizer): echoed ⇒ level Unknown + echoed_source set ──
// The github provenance hook, composed into the tokenizer, demotes an echoed `… failed …` script
// line so its failure WORD confers no alerting level.
TEST(GithubEchoedSource, TokenizerDemotesEchoedFailureLevelToUnknown)
{
    ArenaAllocator arena{256U * 1024U};
    const ComposedSemantics gh{github_only()};
    Tokenizer tokenizer{arena, MaskConfig{}, gh};

    const auto echoed{tokenizer.process_line(
        gha("\x1b[36;1m    echo \"Download failed after 3 attempts\" >&2\x1b[0m"))};
    ASSERT_TRUE(echoed.has_value()) << "process_line failed: " << echoed.error();
    EXPECT_TRUE(echoed->echoed_source) << "the command-echo wrapper must set echoed_source";
    EXPECT_EQ(echoed->level, LogLevel::Unknown)
        << "an echoed `… failed …` script line must NOT confer an alerting level (got "
        << static_cast<int>(echoed->level) << ")";
}

// ── The disconfirming minimal pair: a REAL coloured error keeps Error, echoed_source stays false
// ──
TEST(GithubEchoedSource, TokenizerKeepsRealColouredErrorAsError)
{
    ArenaAllocator arena{256U * 1024U};
    const ComposedSemantics gh{github_only()};
    Tokenizer tokenizer{arena, MaskConfig{}, gh};

    const auto real{tokenizer.process_line(gha("\x1b[31mERROR\x1b[0m: db connection refused"))};
    ASSERT_TRUE(real.has_value()) << "process_line failed: " << real.error();
    EXPECT_FALSE(real->echoed_source) << "a red (`31`) coloured line is not echoed-source";
    EXPECT_EQ(real->level, LogLevel::Error)
        << "a real coloured ERROR keeps its alerting level after the SGR is stripped (got "
        << static_cast<int>(real->level) << ")";
}
// NOLINTEND
