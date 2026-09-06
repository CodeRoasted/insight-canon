// refs: SRC-II-6, STU-12
// invariant: every byte form here is verbatim from marker_corpus_v1, never invented — the
// vocabulary was measured on real traces and then graduated into rows.
// note: determinism — byte-only recognition over the composed rows, no RNG, clock or float
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.gitlab;

using insight::semantic::ComposedSemantics;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

// pre: the walkers take `NormalizedContent`, a precondition carried by a type unforgeable outside
// canon; every probe here is escape-free, so `normalize` is the zero-copy fixed point.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

namespace
{
// invariant: a concretely-gated row is reachable only through a DECLARATION of this dialect, never
// through per-line format detection — which is why the fail-closed arm is a second helper.
[[nodiscard]] ComposedSemantics gitlab_only()
{
    const std::array manifests{insight::semantic::gitlab::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::gitlab::kDialect,
                                                            {});
}

[[nodiscard]] ComposedSemantics undeclared_stream()
{
    const std::array manifests{insight::semantic::gitlab::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::kAnyDialect, {});
}
} // namespace

TEST(GitLabMarkers, SectionStartIsAnOrderedStep)
{
    const ComposedSemantics composed{gitlab_only()};
    const auto marker{recognize(norm_probe("section_start:1784657178:prepare_executor"), composed)};
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
    // invariant: `IntentMarker::name` is the raw payload `compare_skeletons` keys on, so two runs
    // of one phase carry different epochs and must still carry the SAME name.
    EXPECT_EQ(recognize(norm_probe("section_start:1784657178:get_sources"), composed).name,
              "get_sources");
    EXPECT_EQ(recognize(norm_probe("section_start:1784999999:get_sources"), composed).name,
              "get_sources");
}

TEST(GitLabMarkers, TheCarriageReturnTerminatesTheNameAndTheOptionGroupIsDropped)
{
    const ComposedSemantics composed{gitlab_only()};
    // refs: SRC-D-TID-11
    // assert: GitLab closes a marker with `\r` plus an erase-line escape — the escape dies at
    // ingest normalization and the CR survives, so the CR is what terminates the name.
    EXPECT_EQ(recognize(norm_probe("section_start:1784657178:prepare_executor\r"), composed).name,
              "prepare_executor");
    EXPECT_EQ(
        recognize(norm_probe("section_start:1784657178:build_tools_section\rTools build"), composed)
            .name,
        "build_tools_section");
    EXPECT_EQ(
        recognize(norm_probe(
                      "section_start:1784657178:log_disk_usage[collapsed=true]\rDisk usage detail"),
                  composed)
            .name,
        "log_disk_usage")
        << "without the option-group drop, a producer toggling `collapsed` RENAMES the section";
    EXPECT_EQ(
        recognize(norm_probe("section_start:1784657178:build[hide_duration=true,collapsed=true]\r"),
                  composed)
            .name,
        "build");
}

TEST(GitLabMarkers, TheMalformedProducerMarkerIsDeclinedNotMisParsed)
{
    const ComposedSemantics composed{gitlab_only()};
    // invariant: the wireshark class is a DECLARED limitation, not a filter: 60 `section_start:`
    // occurrences across 21 traces carry an unexpanded `%s` where the stamp belongs.
    // note: structure present, stamp absent: a section must not be named after a shell expression
    EXPECT_EQ(recognize(norm_probe("section_start:%s:build"), composed).kind,
              IntentMarkerKind::None);
    EXPECT_EQ(recognize(norm_probe("section_start:$(date +%s):build"), composed).kind,
              IntentMarkerKind::None);
    EXPECT_EQ(recognize(norm_probe("section_start:1784657178:"), composed).kind,
              IntentMarkerKind::None);
    EXPECT_EQ(recognize(norm_probe("section_start:1784657178:\rheader only"), composed).kind,
              IntentMarkerKind::None);
}

TEST(GitLabMarkers, TheEchoedMarkerIsRejectedByPositionNotByTheStamp)
{
    const ComposedSemantics composed{gitlab_only()};
    // assert: bash `set -x` echoes the script's own marker with a fully expanded, strictly VALID
    // stamp, so a stamp-shape guard admits it and only ANCHORING rejects it.
    EXPECT_EQ(recognize(norm_probe("++ echo -e '\\e[0Ksection_start:1784657178:build\\r\\e[0K'"),
                        composed)
                  .kind,
              IntentMarkerKind::None)
        << "an echoed marker is not a section open — 23 of the 59 non-leading corpus markers are "
           "exactly this class";
    EXPECT_EQ(recognize(norm_probe("$ section_start:1784657178:build"), composed).kind,
              IntentMarkerKind::None);
}

TEST(GitLabMarkers, SectionEndIsNotARow)
{
    const ComposedSemantics composed{gitlab_only()};
    // invariant: `IntentMarkerKind` has no close kind and the fold is open-marker driven, so a
    // section's quantum runs until the next section opens; bounding a span is step_duration work.
    EXPECT_EQ(recognize(norm_probe("section_end:1784657178:prepare_executor\r"), composed).kind,
              IntentMarkerKind::None);
}

TEST(GitLabMarkers, RowsAreDialectGatedAndFailClosedWhenUndeclared)
{
    EXPECT_EQ(recognize(norm_probe("section_start:1784657178:build"), undeclared_stream()).kind,
              IntentMarkerKind::None)
        << "an UNDECLARED stream withholds every concretely-gated row (fail-closed on depth)";
}
