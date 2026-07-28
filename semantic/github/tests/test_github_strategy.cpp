// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_github_strategy.cpp — the GitHub-Actions dialect CODE TIER (ADR 0024 §2.3): the format
// strategy (make_strategy) + this package's LEVEL-LIFT vocabulary (kLevelLifts, whose WALK is
// canon's — the vocabulary cases below therefore drive the composed pipeline). Migrated from the
// GitHubActionsStrategyTest block of canon tests/strategy/test_strategies.cpp: the strategy class
// moved into this package (github_strategy.cpp, exposed only via make_strategy()), so its tests
// home with it. Drives the strategy directly against the spi IFormatStrategy contract. Determinism:
// byte-only parse, no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;           // ArenaAllocator / LogFormat / LogLevel / Tokenizer / compose
import insight.semantic.github; // make_strategy + (via export import spi) IFormatStrategy / ParsedLine

using insight::LogFormat;
using insight::LogLevel;
using insight::semantic::ComposedSemantics;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

namespace
{
[[nodiscard]] ComposedSemantics github_only()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests);
}

constexpr std::string_view kGHALine{
    "2026-05-27T15:26:41.7842152Z   CODEROAST_IPC_REPO: CodeRoasted/coderoast-ipc"};
// A whole-second RFC3339 syslog line (no 7-digit fraction) and a JSON line — NOT GHA.
constexpr std::string_view kRFC3339Line{
    "2024-01-15T10:30:00Z myhost app[42]: User alice logged in"};
constexpr std::string_view kJSONLine{
    R"({"ts":"2024-01-15T10:30:00Z","level":"INFO","component":"auth","message":"User logged in"})"};
constexpr std::string_view kGHAError{
    "2026-05-27T15:26:41.7842152Z ##[error]connection refused to db host 10.0.0.5"};
// Bare, UNMARKED bodies — the strategy falls back to canon's failure-cue inference. Only the
// 2-token "segmentation fault" adjacency is a cue; the distractors carry the words non-adjacently
// and stay benign.
constexpr std::string_view kGHASegfault{
    "2026-05-27T15:26:41.7842152Z Segmentation fault (core dumped)"};
constexpr std::string_view kGHASegPipe{
    "2026-05-27T15:26:41.7842152Z image segmentation pipeline complete"};
constexpr std::string_view kGHAPageFault{
    "2026-05-27T15:26:41.7842152Z page fault handler registered"};
// A timestamp with no body is a blank line — declined (never collapsed into an empty "" template).
constexpr std::string_view kGHABlankWithSpace{"2026-05-27T15:26:41.7842152Z "};
constexpr std::string_view kGHABlankNoSpace{"2026-05-27T15:26:41.7842152Z"};
} // namespace

TEST(GithubStrategy, FormatReturnsGitHubActions)
{
    const auto strategy{insight::semantic::github::make_strategy()};
    EXPECT_EQ(strategy->format(), LogFormat::GitHubActions);
}

TEST(GithubStrategy, StripsTimestampAndTemplatesRealContent)
{
    ArenaAllocator arena{4096};
    const auto strategy{insight::semantic::github::make_strategy()};
    const auto result{strategy->parse(kGHALine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.content, "CODEROAST_IPC_REPO: CodeRoasted/coderoast-ipc")
        << "the whole message must survive, leading GHA indentation stripped";
}

// ── Level lift: the workflow-command vocabulary (kLevelLifts) → LogLevel, over the PRODUCTION path
// ──
// The rows are this package's DATA; the walk is canon's (`insight::tokenization::lift_level` over
// the composed table, applied by LogParser — ADR 0063 clause 2). So the gate drives the composed
// pipeline end to end rather than the strategy in isolation: that is the only place the declared
// rows now reach a decision, and it is the path a product binary takes.
//
// All EIGHT rows, because the `::…::` half and `##[notice]` are the ones with no safety net — a
// line that loses its lift falls through to `infer_leading_log_level`, whose vocabulary has no
// `notice` at all, so `##[notice]`/`::notice::` → Info is unrecoverable by inference.
TEST(GithubStrategy, LiftsDeclaredLevelsFromWorkflowCommands)
{
    struct LiftCase
    {
        std::string_view marker;
        LogLevel expected;
    };
    constexpr std::array<LiftCase, 8> kCases{{
        {.marker = "##[error]", .expected = LogLevel::Error},
        {.marker = "::error::", .expected = LogLevel::Error},
        {.marker = "##[warning]", .expected = LogLevel::Warn},
        {.marker = "::warning::", .expected = LogLevel::Warn},
        {.marker = "##[debug]", .expected = LogLevel::Debug},
        {.marker = "::debug::", .expected = LogLevel::Debug},
        {.marker = "##[notice]", .expected = LogLevel::Info},
        {.marker = "::notice::", .expected = LogLevel::Info},
    }};

    ArenaAllocator arena{64U * 1024U};
    const ComposedSemantics gh{github_only()};
    Tokenizer tokenizer{arena, MaskConfig{}, gh};
    for (const LiftCase& probe : kCases)
    {
        // A neutral body: no level token, no failure cue — so the level can ONLY come from the
        // declared lift. If the lift stops firing this reads Unknown, never the right answer by
        // accident.
        const std::string line{std::string{"2026-05-27T15:26:41.7842152Z "} +
                               std::string{probe.marker} + "the quick brown fox"};
        const auto event{tokenizer.process_line(line)};
        ASSERT_TRUE(event.has_value()) << "marker=" << probe.marker << " line=\"" << line
                                       << "\" parse failed: " << event.error();
        EXPECT_EQ(event->format, LogFormat::GitHubActions)
            << "marker=" << probe.marker << " must route to the GHA strategy, got "
            << insight::to_string(event->format);
        EXPECT_EQ(event->level, probe.expected)
            << "marker=" << probe.marker << " expected " << insight::to_string(probe.expected)
            << ", got " << insight::to_string(event->level) << " (template=\""
            << event->template_str << "\")";
        EXPECT_TRUE(event->template_str.starts_with(probe.marker))
            << "the marker stays in the templated content; template=\"" << event->template_str
            << "\"";
    }
}

// ── …and the LIFT beats the body inference, exactly as it did when the walk lived in parse() ──
// `##[notice]` on a body whose leading token would infer Error: the declared row must win. This is
// the precedence the relocation had to preserve — LogParser applies the lift AFTER the strategy has
// already inferred, so an implementation that let the inference stand would pass every case above
// and fail only here.
TEST(GithubStrategy, DeclaredLiftOutranksBodyInference)
{
    ArenaAllocator arena{64U * 1024U};
    const ComposedSemantics gh{github_only()};
    Tokenizer tokenizer{arena, MaskConfig{}, gh};

    const auto lifted{tokenizer.process_line(
        "2026-05-27T15:26:41.7842152Z ##[notice]ERROR the deploy step was skipped")};
    ASSERT_TRUE(lifted.has_value()) << lifted.error();
    EXPECT_EQ(lifted->level, LogLevel::Info)
        << "the declared ##[notice] row must outrank the leading ERROR token the body inference "
           "would read; got "
        << insight::to_string(lifted->level);

    // The control: the SAME body without the marker does infer Error — so the case above is a
    // genuine contest between the two sources, not a body the inference ignores anyway.
    const auto unlifted{
        tokenizer.process_line("2026-05-27T15:26:41.7842152Z ERROR the deploy step was skipped")};
    ASSERT_TRUE(unlifted.has_value()) << unlifted.error();
    EXPECT_EQ(unlifted->level, LogLevel::Error)
        << "control: the unmarked body must infer Error, else the contest above is vacuous; got "
        << insight::to_string(unlifted->level);
}

// ── The algorithm left the PACKAGE: the strategy alone no longer decides a lifted level ──
// ADR 0063 clause 2: LevelLiftRow was the last row kind whose matching algorithm lived in a
// package, and that is what made the rows invisible to two scope passes while still feeding
// `semantic_identity`. This pins the relocation itself — re-adding a package-local walk to
// `parse()` would restore the inconsistency and turn the composed reader back into dead weight.
TEST(GithubStrategy, StrategyAloneDoesNotWalkTheLevelLiftRows)
{
    ArenaAllocator arena{4096};
    const auto strategy{insight::semantic::github::make_strategy()};
    const auto result{strategy->parse(kGHAError, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& parsed{result.value()};
    EXPECT_TRUE(parsed.content.starts_with("##[error]"))
        << "the marker stays in the content; content=\"" << parsed.content << "\"";
    // `##[error]connection refused …` carries a failure CUE ("connection refused"), so the
    // strategy's own inference still reaches Error here — the discriminating probe is a marker with
    // a neutral body, which must come back Unmarked from the strategy alone.
    const auto neutral{
        strategy->parse("2026-05-27T15:26:41.7842152Z ##[notice]the quick brown fox", arena)};
    ASSERT_TRUE(neutral.has_value()) << neutral.error();
    EXPECT_EQ(neutral.value().level, LogLevel::Unknown)
        << "the strategy must not walk kLevelLifts — that walk is canon's lift_level over the "
           "composed rows; got "
        << insight::to_string(neutral.value().level);
}

// ── Unmarked body: falls back to canon's failure-cue inference (level-escaping crash recovery) ──
TEST(GithubStrategy, InfersErrorFromBodyCueWhenUnmarked)
{
    ArenaAllocator arena{4096};
    const auto strategy{insight::semantic::github::make_strategy()};

    const auto crash{strategy->parse(kGHASegfault, arena)};
    ASSERT_TRUE(crash.has_value()) << crash.error();
    EXPECT_EQ(crash.value().level, LogLevel::Error)
        << "bare 'Segmentation fault (core dumped)' escalates via the lexicon";
    EXPECT_EQ(crash.value().content, "Segmentation fault (core dumped)");

    const auto seg_pipe{strategy->parse(kGHASegPipe, arena)};
    ASSERT_TRUE(seg_pipe.has_value()) << seg_pipe.error();
    EXPECT_EQ(seg_pipe.value().level, LogLevel::Unknown)
        << "'image segmentation pipeline complete' — 'segmentation' not adjacent to 'fault'";

    const auto page_fault{strategy->parse(kGHAPageFault, arena)};
    ASSERT_TRUE(page_fault.has_value()) << page_fault.error();
    EXPECT_EQ(page_fault.value().level, LogLevel::Unknown)
        << "'page fault handler registered' — bare 'fault' is not the cue phrase";
}

TEST(GithubStrategy, DeclinesTimestampOnlyBlankLines)
{
    ArenaAllocator arena{4096};
    const auto strategy{insight::semantic::github::make_strategy()};
    EXPECT_FALSE(strategy->parse(kGHABlankWithSpace, arena).has_value());
    EXPECT_FALSE(strategy->parse(kGHABlankNoSpace, arena).has_value());
}

TEST(GithubStrategy, ConfidenceHighForGHAShape)
{
    const auto strategy{insight::semantic::github::make_strategy()};
    EXPECT_GT(strategy->confidence(kGHALine), 0.85);
}

// ── Confidence is ZERO on non-GHA shapes (a strict subset of RFC3339 — 7 fractional digits + 'Z')
// ──
TEST(GithubStrategy, ConfidenceZeroForNonGhaShapes)
{
    const auto strategy{insight::semantic::github::make_strategy()};
    EXPECT_EQ(strategy->confidence(kRFC3339Line), 0.0) << "whole-second RFC3339 is NOT GHA";
    EXPECT_EQ(strategy->confidence(kJSONLine), 0.0) << "JSON is NOT GHA";
}

// ── The GHA prefix is a STRICT subset of RFC3339: exactly 7 fractional digits + 'Z', whole-line or
// space-terminated. Migrated from canon test_fast_gates (is_github_actions_prefix moved here, now
// package-private) and asserted through the public confidence() boundary. ──
TEST(GithubStrategy, ConfidenceRespectsGhaPrefixSubset)
{
    const auto strategy{insight::semantic::github::make_strategy()};
    EXPECT_GT(strategy->confidence("2024-04-27T10:15:00.1234567Z ##[group]Run actions/checkout"),
              0.0)
        << "exactly 7 fractional digits + 'Z' is the GHA shape";
    EXPECT_GT(strategy->confidence("2024-04-27T10:15:00.1234567Z"), 0.0)
        << "a blank GHA line is exactly the 28-char timestamp";
    EXPECT_EQ(strategy->confidence("2024-04-27T10:15:00.123456Z six fractional digits"), 0.0)
        << "6 fractional digits is NOT the GHA subset (100-ns ticks = exactly 7)";
    EXPECT_EQ(strategy->confidence("2024-04-27T10:15:00.1234567+00:00 not Z"), 0.0)
        << "must end in 'Z'";
    EXPECT_EQ(strategy->confidence("2024-04-27T10:15:00.1234567Zx"), 0.0)
        << "the timestamp must be the whole line or followed by a space";
}

TEST(GithubStrategy, RawLinePreserved)
{
    ArenaAllocator arena{4096};
    const auto strategy{insight::semantic::github::make_strategy()};
    const auto result{strategy->parse(kGHALine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result.value().raw_line, kGHALine);
}

// ── Composed detection: the GHA strategy OUTRANKS the RFC3339-claiming representation strategies
// on a genuine GHA line — the composed FormatDetector routes it to GitHubActions. (Pre-split this
// was a raw confidence() vs SyslogStrategy comparison; post-split SyslogStrategy is sealed, so the
// composed FormatDetector outcome — event.format — is the honest, public-surface assertion of the
// same property.)
TEST(GithubStrategy, DetectionRoutesGhaLineToGitHubActions)
{
    ArenaAllocator arena{64U * 1024U};
    const ComposedSemantics gh{github_only()};
    Tokenizer tokenizer{arena, MaskConfig{}, gh};
    const auto event{tokenizer.process_line(kGHALine)};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->format, LogFormat::GitHubActions)
        << "the composed GHA strategy must win over the RFC3339 representation strategies on a GHA "
           "line";
}

// ── …but the GHA strategy does NOT steal a whole-second RFC3339 syslog line (no 7-digit fraction)
// ──
TEST(GithubStrategy, DetectionLeavesWholeSecondRfc3339AsSyslog)
{
    ArenaAllocator arena{64U * 1024U};
    const ComposedSemantics gh{github_only()};
    Tokenizer tokenizer{arena, MaskConfig{}, gh};
    const auto event{tokenizer.process_line(kRFC3339Line)};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->format, LogFormat::Syslog)
        << "whole-second RFC3339 is real syslog — the GHA strategy must not claim it";
}
// NOLINTEND
