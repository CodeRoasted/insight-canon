
// invariant: the ingest-normalization measurement moved 436 quanta to-failing and one quantum
// to-PASSING on the control dialect.
// invariant: a to-passing flip is a possible RECALL LOSS — a real failure the change would
// suppress — so it was carried as owed rather than assumed benign.
// invariant: RULED BENIGN at the bytes: every to-passing flip is ONE producer shape, nine instances
// across nine job logs, and it is the dialect's own after-script warning.
// invariant: the line says in the producer's own words that the failure does not affect the job,
// and the producer marks it warning-severity twice — the literal token and the colour.
// invariant: so the RAW reading of Error is the wrong one, and normalization removes a false
// positive rather than suppressing a real failure.
// invariant: SUPERSEDED on the level channel — the leading scan's budget is now a TOKEN count and
// an escape run is a delimiter, so the warning word is in reach on the RAW bytes too.
// invariant: the flip this file defends therefore has no line left to flip on; what stage 1 still
// changes is the TEMPLATE and the byte position of a later cue against its own head.
// invariant: the raw side is now pinned at the SAME verdict, which is the boundary assertion that
// reds if a byte budget ever returns to stage 1.
// invariant: THE GRAIN DIFFERS FROM THE HEADLINE and that is not a discrepancy — the measurement
// counted ONE QUANTUM and this counts NINE LINES.
// invariant: the failing-line count fires only on UNMATCHED structural nodes, so eight of the nine
// sit inside matched quanta and never reach the quantum grain.
// invariant: a literal rather than a corpus gate, because the property is a single-component claim
// about one classifier on one known-hazardous input.
// refs: ADR-16.D7, ADR-21
#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogLevel;
using insight::utils::infer_leading_log_level;

namespace
{
// invariant: byte-exact from the marker corpus, with the control bytes spelled out — the carriage
// return is the producer's own, and the escapes are its erase and its yellow.
constexpr std::string_view kRawAfterScriptWarning{
    "section_end:1737226867:after_script\r\x1b[0K\x1b[0;33mWARNING: after_script failed, but job "
    "will continue unaffected: exit code 1\x1b[0;m"};

// invariant: the same line with ONLY the bare carriage return removed — a FIXTURE, not a corpus
// line, because no producer emits this, and that is the point.
// invariant: it exists so the carriage-return clause is a checked assertion rather than a story.
constexpr std::string_view kRawWithoutCarriageReturn{
    "section_end:1737226867:after_script\x1b[0K\x1b[0;33mWARNING: after_script failed, but job "
    "will continue unaffected: exit code 1\x1b[0;m"};

[[nodiscard]] bool is_failing(LogLevel level)
{
    return level == LogLevel::Error || level == LogLevel::Fatal;
}
} // namespace

TEST(IngestNormalizationLevelFlip, TheAfterScriptWarningReadsWarnOnceNormalized)
{
    std::string scratch;
    const std::string_view normalized{
        insight::tokenization::normalize(kRawAfterScriptWarning, scratch).bytes()};

    // invariant: the RAW line is pinned at the SAME verdict and not merely at not-failing, so a
    // classifier that stopped reading levels still reds here.
    // invariant: so does a byte budget returning to stage 1 — the escape run would push the
    // warning word out of a head, which is the state the flip moved AWAY from.
    EXPECT_EQ(infer_leading_log_level(kRawAfterScriptWarning), LogLevel::Warn)
        << "raw reads " << to_string(infer_leading_log_level(kRawAfterScriptWarning).value())
        << " — with a token budget the escape runs are delimiters and WARNING is token 3, so the "
           "raw line reads the producer's own severity: Error here is a byte budget back in Stage "
           "1, Unknown a dead classifier";

    // invariant: the NORMALIZED line agrees with the producer, which marked it warning in its text
    // and yellow in its colour.
    const LogLevel normalized_level{infer_leading_log_level(normalized).value()};
    EXPECT_FALSE(is_failing(normalized_level))
        << "normalized reads " << to_string(normalized_level)
        << " — a failing verdict here would re-introduce the false positive the flip removed. The "
           "line states the after_script failure leaves the job unaffected.";
    EXPECT_EQ(normalized_level, LogLevel::Warn)
        << "expected the producer's own severity (it wrote WARNING: and coloured it 0;33), got "
        << to_string(normalized_level);
}

// invariant: THE BARE CARRIAGE RETURN IS LOAD-BEARING — it is CONTENT in these logs, not a
// delimiter, and this row is the cheapest live proof of that rule in the tree.
// invariant: stage 1 removes the escape run and correctly LEAVES the carriage return, which keeps
// the warning word a separate token.
// invariant: delete that one byte and the section name fuses with it, the warning token is gone,
// the classifier falls through to the failure word, and the line reads Error again.
// invariant: so a carriage-return-folding reader would silently re-manufacture the very false
// positive this flip removed.
TEST(IngestNormalizationLevelFlip, FoldingTheBareCarriageReturnReManufacturesTheFalsePositive)
{
    std::string scratch;
    const std::string_view normalized{
        insight::tokenization::normalize(kRawWithoutCarriageReturn, scratch).bytes()};
    EXPECT_TRUE(is_failing(infer_leading_log_level(normalized).value()))
        << "without the \\r this line reads "
        << to_string(infer_leading_log_level(normalized).value())
        << " — if this ever stops being a failing read, the \\r is no longer what separates the "
           "WARNING token and this file's clause-6 argument needs re-deriving, not updating";
}

// invariant: two-sidedness — without this the row above would pass a classifier that had simply
// stopped reporting failures on normalized content.
// invariant: that is the failure mode that would turn a precision gain into a real recall loss,
// silently, and it is exactly what the to-passing flip was feared to be.
TEST(IngestNormalizationLevelFlip, NormalizationDoesNotDisarmGenuineFailureLines)
{
    std::string scratch;
    for (const std::string_view raw :
         {std::string_view{"\x1b[0;31mERROR: Job failed: exit code 1\x1b[0;m"},
          std::string_view{"\x1b[0K\x1b[31;1merror: cannot find crate 'serde'\x1b[0;m"},
          std::string_view{"FATAL: unrecoverable"}})
    {
        const std::string_view normalized{insight::tokenization::normalize(raw, scratch).bytes()};
        EXPECT_TRUE(is_failing(infer_leading_log_level(normalized).value()))
            << "a genuine failure line must stay failing after normalization; '" << normalized
            << "' read " << to_string(infer_leading_log_level(normalized).value());
    }
}
