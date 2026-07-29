// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_gitlab_markers.cpp — the GitLab section VOCABULARY (studies/012, graduated into grammar-5
// rows). What it guards:
//   SECTION = a line-anchored `section_start:<unix-ts>:<name>[<options>]` open, kind=Step (a GitLab
//             trace IS one job; its phases are steps under an implicit job), ORDERED (one runner,
//             one job, one phase at a time — a transposition IS a signal);
//   the payload is the NAME only — the epoch is skipped, the CR TERMINATES it, the option group is
//             dropped;
//   the malformed producer marker (`%s` / `$(date +%s)`) is DECLINED, never mis-parsed;
//   the ECHOED marker is rejected by POSITION — the xtrace form carries a fully-valid stamp, so a
//             stamp-shape guard would let it through and only anchoring does not;
//   `section_end:` is NOT a row;
//   II-6 = the rows are dialect-gated to gitlab and inert elsewhere.
// Every byte form here was taken from marker_corpus_v1, not invented. Determinism: byte-only
// recognition over the composed rows; no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.gitlab;

using insight::semantic::ComposedSemantics;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

namespace
{
// The RESOLVED view of a stream that declared this dialect (ADR 0065 clause 2) — after T4 the
// concretely-gated rows are reachable only through a declaration.
[[nodiscard]] ComposedSemantics gitlab_only()
{
    const std::array manifests{insight::semantic::gitlab::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::gitlab::kDialect, {});
}

// The same composition on a stream that declared NO dialect — the fail-closed arm.
[[nodiscard]] ComposedSemantics undeclared_stream()
{
    const std::array manifests{insight::semantic::gitlab::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::kAnyDialect, {});
}
} // namespace

TEST(GitLabMarkers, SectionStartIsAnOrderedStep)
{
    const ComposedSemantics composed{gitlab_only()};
    const auto marker{recognize("section_start:1784657178:prepare_executor", composed)};
    EXPECT_EQ(marker.kind, IntentMarkerKind::Step)
        << "a GitLab trace IS one job — mapping a section to Job would mint one 'job' per runner "
           "phase and leave every one of them step-less";
    EXPECT_EQ(marker.child_order, ChildOrder::Ordered)
        << "one runner, one job, one phase at a time — unlike GHA matrix jobs, GitLab phases never "
           "co-occur, so a transposition is a signal";
    EXPECT_EQ(marker.name, "prepare_executor");
}

TEST(GitLabMarkers, TheEpochIsSkippedNotFoldedIntoTheName)
{
    const ComposedSemantics composed{gitlab_only()};
    // The whole reason the extractor exists: `IntentMarker::name` is the raw payload
    // `compare_skeletons` keys on (adr/0045). Two runs of the same phase carry different epochs and
    // must still carry the SAME name.
    EXPECT_EQ(recognize("section_start:1784657178:get_sources", composed).name, "get_sources");
    EXPECT_EQ(recognize("section_start:1784999999:get_sources", composed).name, "get_sources");
}

TEST(GitLabMarkers, TheCarriageReturnTerminatesTheNameAndTheOptionGroupIsDropped)
{
    const ComposedSemantics composed{gitlab_only()};
    // The bytes canon sees after the transport peel and the D-TID-11 escape strip: GitLab closes the
    // marker with `\r\x1b[0K`, the escape dies, the CR survives.
    EXPECT_EQ(recognize("section_start:1784657178:prepare_executor\r", composed).name,
              "prepare_executor");
    // …and the producer may continue the SAME line with the section's human-readable header. Both
    // forms are verbatim from marker_corpus_v1.
    EXPECT_EQ(recognize("section_start:1784657178:build_tools_section\rTools build", composed).name,
              "build_tools_section");
    EXPECT_EQ(
        recognize("section_start:1784657178:log_disk_usage[collapsed=true]\rDisk usage detail",
                  composed)
            .name,
        "log_disk_usage")
        << "without the option-group drop, a producer toggling `collapsed` RENAMES the section";
    EXPECT_EQ(recognize("section_start:1784657178:build[hide_duration=true,collapsed=true]\r",
                        composed)
                  .name,
              "build");
}

TEST(GitLabMarkers, TheMalformedProducerMarkerIsDeclinedNotMisParsed)
{
    const ComposedSemantics composed{gitlab_only()};
    // The wireshark class studies/012 required be carried as declared: 95 markers in
    // marker_corpus_v1 carry an unexpanded `%s` / `$(date +%s)` where the stamp belongs. Structure
    // present, stamp absent — and a section must NOT end up named after a shell expression.
    EXPECT_EQ(recognize("section_start:%s:build", composed).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize("section_start:$(date +%s):build", composed).kind, IntentMarkerKind::None);
    // An empty name is not a quantum either.
    EXPECT_EQ(recognize("section_start:1784657178:", composed).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize("section_start:1784657178:\rheader only", composed).kind,
              IntentMarkerKind::None);
}

TEST(GitLabMarkers, TheEchoedMarkerIsRejectedByPositionNotByTheStamp)
{
    const ComposedSemantics composed{gitlab_only()};
    // bash `set -x` xtrace echoes the script's own marker with a FULLY EXPANDED, strictly-VALID
    // stamp — so a stamp-shape guard does not reject it and only anchoring does. `recognize` matches
    // with starts_with, so the guard is free. (The `\e` here is a literal two-character sequence in
    // the echoed text, not an escape byte — canon's ingest strip has nothing to remove.)
    EXPECT_EQ(recognize("++ echo -e '\\e[0Ksection_start:1784657178:build\\r\\e[0K'", composed).kind,
              IntentMarkerKind::None)
        << "an echoed marker is not a section open — 23 of the 59 non-leading corpus markers are "
           "exactly this class";
    // GitLab's own command echo, same argument.
    EXPECT_EQ(recognize("$ section_start:1784657178:build", composed).kind, IntentMarkerKind::None);
}

TEST(GitLabMarkers, SectionEndIsNotARow)
{
    const ComposedSemantics composed{gitlab_only()};
    // There is no close kind in IntentMarkerKind and the fold is open-marker driven: a section's
    // quantum runs until the next section opens. Recognizing a close is only useful to BOUND a span,
    // which is step_duration territory.
    EXPECT_EQ(recognize("section_end:1784657178:prepare_executor\r", composed).kind,
              IntentMarkerKind::None);
}

TEST(GitLabMarkers, RowsAreDialectGatedAndFailClosedWhenUndeclared)
{
    // II-6: the row is reachable only through a declaration of THIS dialect.
    EXPECT_EQ(recognize("section_start:1784657178:build", undeclared_stream()).kind,
              IntentMarkerKind::None)
        << "an UNDECLARED stream withholds every concretely-gated row (fail-closed on depth)";
}
// NOLINTEND
