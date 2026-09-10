// refs: F-SRC-insight-canon:test_transport_peel_equivalence_gate.cpp
// invariant: peel EQUIVALENCE is not re-asserted here — that frozen gate owns it; this file
// asserts only that the package's declared vocabulary reaches a decision on that path.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::LogLevel;
using insight::semantic::ComposedSemantics;
using insight::semantic::ResolvedStream;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;
using insight::transport::IngestDeclaration;
using insight::transport::RawPeeledLine;

namespace
{
// note: GitHub's API stamps every line it serves; a caller that fetched a job log declares it
constexpr std::array<std::string_view, 1> kGhaStack{{"api-rfc3339-line-prefix"}};

[[nodiscard]] ComposedSemantics github_composition()
{
    return insight::semantic::compose(std::array{insight::semantic::github::kManifest});
}

[[nodiscard]] ResolvedStream gha_stream(const ComposedSemantics& composed)
{
    return insight::semantic::resolve_stream(
        composed, IngestDeclaration{.stack = kGhaStack,
                                    .dialect = insight::semantic::github::kDialect,
                                    .channel = insight::semantic::github::kChannelAnnotated});
}

constexpr std::string_view kGHALine{
    "2026-05-27T15:26:41.7842152Z   CODEROAST_IPC_REPO: CodeRoasted/coderoast-ipc"};
constexpr std::string_view kGHASegfault{
    "2026-05-27T15:26:41.7842152Z Segmentation fault (core dumped)"};
constexpr std::string_view kGHASegPipe{
    "2026-05-27T15:26:41.7842152Z image segmentation pipeline complete"};
constexpr std::string_view kGHAPageFault{
    "2026-05-27T15:26:41.7842152Z page fault handler registered"};
// assert: a stamp with no body peels to EMPTY, and empty means DROP — never an empty template.
constexpr std::string_view kGHABlankWithSpace{"2026-05-27T15:26:41.7842152Z "};
constexpr std::string_view kGHABlankNoSpace{"2026-05-27T15:26:41.7842152Z"};
} // namespace

TEST(GithubDeclaredIngest, DeclaredPeelStripsStampAndIndentation)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{gha_stream(composed)};
    const RawPeeledLine peeled{stream.transport.peel_raw(kGHALine)};
    EXPECT_EQ(peeled.content, "CODEROAST_IPC_REPO: CodeRoasted/coderoast-ipc")
        << "the whole message must survive, leading GHA indentation stripped";
    EXPECT_TRUE(peeled.observation_time.has_value())
        << "a declared LinePrefixTimestamp extracts an OBSERVATION time for the caller to inject "
           "(never an ordering key, never a replay input)";
}

TEST(GithubDeclaredIngest, StampOnlyLinePeelsToBlank)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{gha_stream(composed)};
    EXPECT_TRUE(stream.transport.peel_raw(kGHABlankWithSpace).is_blank());
    EXPECT_TRUE(stream.transport.peel_raw(kGHABlankNoSpace).is_blank());
}

// invariant: the rows are this package's DATA and the walk is canon's, so the composed pipeline end
// to end is the only place a declared row reaches a decision.
// assert: all EIGHT rows, and each asserts the SPECIES as well as the value — canon's own word
// lexicon now knows `notice`, so a value check alone cannot see a lift that stopped firing.
// invariant: half this vocabulary is corpus-unfalsifiable and is falsified only here: over the D11
// slice (4 082 logs, 22 490 937 lines) the five forms lead 41 lines, `::notice::` none.
TEST(GithubDeclaredIngest, LiftsDeclaredLevelsFromWorkflowCommands)
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

    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{gha_stream(composed)};
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics};
    for (const LiftCase& probe : kCases)
    {
        // assert: the body carries no level token and no failure cue, so a lift that stopped firing
        // would read Unknown here rather than the right answer by accident.
        const std::string line{std::string{"2026-05-27T15:26:41.7842152Z "} +
                               std::string{probe.marker} + "the quick brown fox"};
        const RawPeeledLine peeled{stream.transport.peel_raw(line)};
        ASSERT_FALSE(peeled.is_blank()) << "marker=" << probe.marker;
        const auto event{tokenizer.process_line(peeled.content)};
        ASSERT_TRUE(event.has_value()) << "marker=" << probe.marker << " line=\"" << line
                                       << "\" parse failed: " << event.error();
        EXPECT_EQ(event->level, probe.expected)
            << "marker=" << probe.marker << " expected " << insight::to_string(probe.expected)
            << ", got " << insight::to_string(event->level) << " (template=\""
            << event->template_str << "\")";
        EXPECT_TRUE(event->declared_level)
            << "marker=" << probe.marker
            << ": the level must be DECLARED by the row, not inferred from the marker word by "
               "canon's own lexicon — for `notice` the two species carry the SAME value, so the "
               "value check above cannot tell them apart";
        EXPECT_TRUE(event->template_str.starts_with(probe.marker))
            << "the marker stays in the templated content; template=\"" << event->template_str
            << "\"";
    }
}

// assert: this is the arm that catches a stream filter which silently kept everything.
TEST(GithubDeclaredIngest, AnUndeclaredStreamGetsNoDeclaredLift)
{
    const ComposedSemantics composed{github_composition()};
    // assert: transport stays declared so the BYTES are identical; only the dialect is withheld.
    const ResolvedStream stream{insight::semantic::resolve_stream(
        composed, IngestDeclaration{.stack = kGhaStack,
                                    .dialect = insight::semantic::kAnyDialect,
                                    .channel = insight::semantic::github::kChannelAnnotated})};
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics};
    const auto event{tokenizer.process_line(
        stream.transport.peel_raw("2026-05-27T15:26:41.7842152Z ##[notice]the quick brown fox")
            .content)};
    ASSERT_TRUE(event.has_value()) << event.error();
    // assert: the discriminator is the SPECIES, never the value — canon's word lexicon learned
    // `notice`, so the generic inference now reads Info from this marker on its own.
    // assert: Unknown was a PROXY for "no lift fired", and the proxy stopped being sound the day
    // the two species began to agree on the value.
    // refs: ADR-20.D16
    EXPECT_FALSE(event->declared_level)
        << "a dialect-gated level lift fired on a stream that declared NO dialect — fail-closed on "
           "depth is not optional; got "
        << insight::to_string(event->level);
    EXPECT_EQ(event->level, LogLevel::Info)
        << "the undeclared stream still gets the NARROWER reading: canon infers Info from the "
           "word, "
           "and declaring the dialect is what upgrades it to a declared fact; got "
        << insight::to_string(event->level);
}

// invariant: `LogParser` applies the declared lift AFTER the body inference has run, so an
// implementation that let the inference stand passes every other case and fails only here.
TEST(GithubDeclaredIngest, DeclaredLiftOutranksBodyInference)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{gha_stream(composed)};
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics};

    const auto lifted{tokenizer.process_line(
        stream.transport
            .peel_raw("2026-05-27T15:26:41.7842152Z ##[notice]ERROR the deploy step was skipped")
            .content)};
    ASSERT_TRUE(lifted.has_value()) << lifted.error();
    EXPECT_EQ(lifted->level, LogLevel::Info)
        << "the declared ##[notice] row must outrank the leading ERROR token the body inference "
           "would read; got "
        << insight::to_string(lifted->level);

    const auto unlifted{tokenizer.process_line(
        stream.transport.peel_raw("2026-05-27T15:26:41.7842152Z ERROR the deploy step was skipped")
            .content)};
    ASSERT_TRUE(unlifted.has_value()) << unlifted.error();
    EXPECT_EQ(unlifted->level, LogLevel::Error)
        << "control: the unmarked body must infer Error, else the contest above is vacuous; got "
        << insight::to_string(unlifted->level);
}

TEST(GithubDeclaredIngest, InfersErrorFromBodyCueWhenUnmarked)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{gha_stream(composed)};
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics};

    const auto crash{tokenizer.process_line(stream.transport.peel_raw(kGHASegfault).content)};
    ASSERT_TRUE(crash.has_value()) << crash.error();
    EXPECT_EQ(crash->level, LogLevel::Error)
        << "bare 'Segmentation fault (core dumped)' escalates via the lexicon";
    EXPECT_EQ(crash->template_str, "Segmentation fault (core dumped)");

    const auto seg_pipe{tokenizer.process_line(stream.transport.peel_raw(kGHASegPipe).content)};
    ASSERT_TRUE(seg_pipe.has_value()) << seg_pipe.error();
    EXPECT_EQ(seg_pipe->level, LogLevel::Unknown)
        << "'image segmentation pipeline complete' — 'segmentation' not adjacent to 'fault'";

    const auto page_fault{tokenizer.process_line(stream.transport.peel_raw(kGHAPageFault).content)};
    ASSERT_TRUE(page_fault.has_value()) << page_fault.error();
    EXPECT_EQ(page_fault->level, LogLevel::Unknown)
        << "'page fault handler registered' — bare 'fault' is not the cue phrase";
}

// refs: ADR-22
TEST(GithubDeclaredIngest, ThePackageShipsNoStrategyAndOneProvenanceHook)
{
    const ComposedSemantics composed{github_composition()};
    ASSERT_EQ(composed.packages().size(), 1U);
    EXPECT_FALSE(composed.packages()[0].has_strategy)
        << "the github package must ship NO format strategy after T4 — a dialect is a vocabulary "
           "over a host format, not a parser";
    EXPECT_TRUE(composed.packages()[0].has_echoed_source)
        << "the echoed-source provenance hook is the code tier that remains";
}
