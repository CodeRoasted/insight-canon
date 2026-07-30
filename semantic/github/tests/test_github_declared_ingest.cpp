// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_github_declared_ingest.cpp — the GitHub-Actions dialect over the DECLARED ingest path
// (ADR 0044 §4/§6, ADR 0063, ADR 0065). This file replaces `test_github_strategy.cpp`, whose whole
// subject — `GitHubActionsStrategy`: `format()`, `confidence()`, the detection race against Syslog,
// the in-`parse()` timestamp strip — was DELETED by T4.
//
// WHAT REPLACED IT, and why the tests could not simply be ported. The GHA per-line RFC 3339 stamp
// is a property of GitHub's *delivery*, not of the GHA *dialect* (ADR 0044 §3): the host format of
// a GHA job log is RawText and always was, and the dialect is the workflow-command VOCABULARY over
// it (ADR 0064 clause 1). So there is nothing left to DETECT — the caller DECLARES the transform
// and the dialect, canon verifies both, `TransportStack::peel` unwinds the stamp, and only
// `RawPeeledLine::content` crosses into the Tokenizer. A "does the strategy claim this line" test
// has no subject any more; a "does the declared path read this line" test does.
//
// The equivalence between the two — that the declared peel produces the same bytes the deleted
// detector did — is NOT re-asserted here. It is G1-PEEL's, scored against the frozen oracle over
// 4 082 logs / 22 490 937 lines in `test_transport_peel_equivalence_gate.cpp` (ADR 0062). This file
// asserts what the PACKAGE owns: that its declared vocabulary reaches a decision on that path.
//
// Determinism: byte-only peel + byte-only walks; no RNG, no clock, no float.
#include <gtest/gtest.h>

import std;
import insight.canon;           // compose / resolve_stream / transport / Tokenizer + enums
import insight.semantic.github; // kManifest / kDialect / kChannelAnnotated

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
// GitHub's serving API stamps every line it returns; a caller that fetched a job log DECLARES it.
constexpr std::array<std::string_view, 1> kGhaStack{{"api-rfc3339-line-prefix"}};

[[nodiscard]] ComposedSemantics github_composition()
{
    return insight::semantic::compose(std::array{insight::semantic::github::kManifest});
}

// The ONE call a caller makes at stream open: both semantic coordinates verified and filtered into
// the view, and the transport stack resolved — all before the first line.
[[nodiscard]] ResolvedStream gha_stream(const ComposedSemantics& composed)
{
    return insight::semantic::resolve_stream(
        composed, IngestDeclaration{.stack = kGhaStack,
                                    .dialect = insight::semantic::github::kDialect,
                                    .channel = insight::semantic::github::kChannelAnnotated});
}

constexpr std::string_view kGHALine{
    "2026-05-27T15:26:41.7842152Z   CODEROAST_IPC_REPO: CodeRoasted/coderoast-ipc"};
// Bare, UNMARKED bodies — the level falls back to canon's failure-cue inference. Only the 2-token
// "segmentation fault" adjacency is a cue; the distractors carry the words non-adjacently and stay
// benign.
constexpr std::string_view kGHASegfault{
    "2026-05-27T15:26:41.7842152Z Segmentation fault (core dumped)"};
constexpr std::string_view kGHASegPipe{
    "2026-05-27T15:26:41.7842152Z image segmentation pipeline complete"};
constexpr std::string_view kGHAPageFault{
    "2026-05-27T15:26:41.7842152Z page fault handler registered"};
// A stamp with no body is a blank line — it peels to EMPTY, and empty means DROP (never an empty ""
// template). This is the shipped detector's "timestamp-only line is a blank line: decline it"
// behavior, expressed on the declared side as `RawPeeledLine::is_blank()`.
constexpr std::string_view kGHABlankWithSpace{"2026-05-27T15:26:41.7842152Z "};
constexpr std::string_view kGHABlankNoSpace{"2026-05-27T15:26:41.7842152Z"};
} // namespace

// ── The declared peel: the stamp AND the GHA indentation leave, an observation time is extracted ──
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

// ── A stamp-only line peels to EMPTY, and empty means DROP ──
TEST(GithubDeclaredIngest, StampOnlyLinePeelsToBlank)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{gha_stream(composed)};
    EXPECT_TRUE(stream.transport.peel_raw(kGHABlankWithSpace).is_blank());
    EXPECT_TRUE(stream.transport.peel_raw(kGHABlankNoSpace).is_blank());
}

// ── Level lift: the workflow-command vocabulary (kLevelLifts) → LogLevel, over the PRODUCTION path
// ──
// The rows are this package's DATA; the walk is canon's (`insight::tokenization::lift_level` over
// the resolved view's table, applied by LogParser — ADR 0063 clause 2). So the gate drives the
// composed pipeline end to end: that is the only place the declared rows reach a decision, and it
// is the path a product binary takes.
//
// All EIGHT rows, because the `::…::` half and `##[notice]` are the ones with no safety net — a
// line that loses its lift falls through to `infer_leading_log_level`, whose vocabulary has no
// `notice` at all, so `##[notice]`/`::notice::` → Info is unrecoverable by inference.
//
// ⚠ CORPUS COVERAGE IS NOT SYMMETRIC ACROSS THESE EIGHT, and the difference matters when reading a
// green run elsewhere. Across the D11 full slice (4 082 logs / 22 490 937 lines) the `::…::` forms
// plus `##[notice]` lead a line 41 times in total, and `::notice::` occurs NOWHERE AT ALL. Half
// this vocabulary is corpus-unfalsifiable and is exercised HERE and only here; no corpus-scale green
// may be cited as coverage for it.
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
        // A neutral body: no level token, no failure cue — so the level can ONLY come from the
        // declared lift. If the lift stops firing this reads Unknown, never the right answer by
        // accident.
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
        EXPECT_TRUE(event->template_str.starts_with(probe.marker))
            << "the marker stays in the templated content; template=\"" << event->template_str
            << "\"";
    }
}

// ── The lift is reachable ONLY through the declaration — the fail-closed half ──
// The same eight rows on a stream that declared no dialect: every one is absent from the view, so
// the level falls back to inference and a `##[notice]` body reads Unknown. This is the leg that
// would catch a filter that silently kept everything, and it is the FIRST time this package's rows
// have had such a leg at all — before T4 the gate was `LogParser::routed_format()`, a per-line
// detector output, so "did the row fire" was a question about the line's own bytes.
TEST(GithubDeclaredIngest, AnUndeclaredStreamGetsNoDeclaredLift)
{
    const ComposedSemantics composed{github_composition()};
    // Transport still declared (so the bytes are the same), dialect deliberately NOT.
    const ResolvedStream stream{insight::semantic::resolve_stream(
        composed,
        IngestDeclaration{.stack = kGhaStack, .dialect = insight::semantic::kAnyDialect,
                          .channel = insight::semantic::github::kChannelAnnotated})};
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics};
    const auto event{tokenizer.process_line(
        stream.transport.peel_raw("2026-05-27T15:26:41.7842152Z ##[notice]the quick brown fox")
            .content)};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->level, LogLevel::Unknown)
        << "a dialect-gated level lift fired on a stream that declared NO dialect — fail-closed on "
           "depth is not optional; got "
        << insight::to_string(event->level);
}

// ── …and the LIFT beats the body inference, exactly as it did when the walk lived in parse() ──
// `##[notice]` on a body whose leading token would infer Error: the declared row must win. This is
// the precedence the relocation had to preserve — LogParser applies the lift AFTER the strategy has
// already inferred, so an implementation that let the inference stand would pass every case above
// and fail only here.
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

    // The control: the SAME body without the marker does infer Error — so the case above is a
    // genuine contest between the two sources, not a body the inference ignores anyway.
    const auto unlifted{tokenizer.process_line(
        stream.transport.peel_raw("2026-05-27T15:26:41.7842152Z ERROR the deploy step was skipped")
            .content)};
    ASSERT_TRUE(unlifted.has_value()) << unlifted.error();
    EXPECT_EQ(unlifted->level, LogLevel::Error)
        << "control: the unmarked body must infer Error, else the contest above is vacuous; got "
        << insight::to_string(unlifted->level);
}

// ── Unmarked body: falls back to canon's failure-cue inference (level-escaping crash recovery) ──
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

// ── The package's code tier is now ONE hook, and the composed report says so ──
// ADR 0065 clause 5 item 2: deleting the strategy removes the only hand-written PARSER in a dialect
// package, leaving `echoed_source` — a provenance hook, not a grammar. That is what makes this
// dialect DATA-ONLY, which is the door the LogCraft-generated dialect parser would come through.
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
// NOLINTEND
