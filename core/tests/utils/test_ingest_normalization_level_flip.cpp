// NOLINTBEGIN
// Unit tests: allow short identifiers and test-specific patterns
// tests/utils/test_ingest_normalization_level_flip.cpp
//
// THE ONE TO-PASSING FLIP, pinned as a literal (architecture/insight_ingest_normalization_contract.md
// § 6.1). The ingest-normalization measurement moved 436 GHA quanta to-FAILING and — on the GitLab
// control — a single quantum to-PASSING. A to-passing flip is a possible RECALL LOSS: a real failure
// the change would suppress. It was the only measurement pointing that way, so it was carried as
// owed rather than assumed benign.
//
// RULED BENIGN, and the bytes are the argument (Kleio, 2026-07-29). Measured with the SHIPPED
// classifier over the 627 traces of marker_corpus_v1 (5 956 626 lines): every to-passing flip is
// ONE producer shape, nine instances across nine job logs, and the shape is GitLab's own
// `after_script` warning:
//
//   section_end:<epoch>:after_script CR ESC[0K ESC[0;33m
//   WARNING: after_script failed, but job will continue unaffected: exit code 1 ESC[0;m
//
// **The line says, in GitLab's own words, that the failure does not affect the job**, and the
// producer marks it warning-severity twice over — the literal `WARNING:` token and the ANSI colour
// `0;33` (yellow). So the RAW reading of `Error` is the wrong one: it fires on `failed` in a line
// whose whole content is that the failure was inconsequential. Normalization does not suppress a
// real failure here; it removes a false positive, and the normalized verdict is the one that AGREES
// with the producer's declared severity.
//
// ⚠ THE BARE `\r` IS LOAD-BEARING, and this row is the cheapest live proof of clause 6 in the tree
// (technical_docs/adr/008-corpus-gates-oracles-and-measurement.md D2 — *a `\r` is content in CI logs, not a delimiter*). Stage 1
// removes the escape run and CORRECTLY leaves the `\r`, which keeps `WARNING` a separate token and
// is why the normalized line reads Warn. Trim that one byte — as a `\r`-stripping read path would —
// and `after_script` fuses with `WARNING` into `after_scriptWARNING`; the warning token is gone, the
// classifier falls through to `failed`, and the line reads **Error** again. So a `\r`-folding reader
// would silently re-manufacture the very false positive this flip removed. Verified by running this
// row's literal with the `\r` deleted: Warn → Error.
//
// ⚠ THE GRAIN DIFFERS FROM THE HEADLINE, and that is not a discrepancy to reconcile away. The
// measurement counted ONE QUANTUM; this counts NINE LINES. `lines_failed` fires only on UNMATCHED
// structural nodes, so eight of the nine sit inside matched quanta and never reach the quantum-grain
// count. Nine lines and one quantum are consistent readings of the same corpus at two grains.
//
// WHY A LITERAL AND NOT A CORPUS GATE. Same homing as the 436's follow-up: the property is a
// single-component claim about one classifier on one known-hazardous input, which is what a fixture
// is good at ([[test-homing-integration-vs-single-component]]). The corpus was needed to FIND the
// shape; it is not needed to pin it, and a corpus gate here would buy nothing the literal does not.

#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogLevel;
using insight::utils::infer_leading_log_level;

namespace
{
// The offending line, byte-exact from marker_corpus_v1
// (gitlab.haskell.org/1Jajen1__ghc/pipeline_105803/job_2092177.log and eight siblings), with the
// control bytes spelled out. The `\r` after `after_script` is GitLab's own; `\x1b[0K` is the
// erase-to-end-of-line it emits before a coloured runner message; `\x1b[0;33m` is the yellow that
// makes this a warning on a terminal.
constexpr std::string_view kRawAfterScriptWarning{
    "section_end:1737226867:after_script\r\x1b[0K\x1b[0;33mWARNING: after_script failed, but job "
    "will continue unaffected: exit code 1\x1b[0;m"};

// The same line with ONLY the bare `\r` removed — the byte a `\r`-folding read path would eat. It
// is a fixture, not a corpus line: no producer emits this, and that is the point. It exists so the
// clause-6 claim in this file's header is a checked assertion rather than a story.
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

    // The RAW line reads as failing — recorded, because it is the state the flip moved AWAY from and
    // a test that only pinned the destination could not tell a fix from a classifier that had
    // stopped working.
    EXPECT_TRUE(is_failing(infer_leading_log_level(kRawAfterScriptWarning)))
        << "raw reads " << to_string(infer_leading_log_level(kRawAfterScriptWarning))
        << " — the escape run makes the line's head parse as a failure";

    // And the NORMALIZED line does not. This is the pin: it agrees with the producer, which marked
    // the line WARNING in its text and yellow in its colour.
    const LogLevel normalized_level{infer_leading_log_level(normalized)};
    EXPECT_FALSE(is_failing(normalized_level))
        << "normalized reads " << to_string(normalized_level)
        << " — a failing verdict here would re-introduce the false positive the flip removed. The "
           "line states the after_script failure leaves the job unaffected.";
    EXPECT_EQ(normalized_level, LogLevel::Warn)
        << "expected the producer's own severity (it wrote WARNING: and coloured it 0;33), got "
        << to_string(normalized_level);
}

// Clause 6, made checkable on the one line that proves it costs something. Delete the bare `\r` —
// the single byte a `\r`-folding read path eats — and the warning token fuses into its neighbour,
// the classifier falls through to `failed`, and the false positive comes back.
TEST(IngestNormalizationLevelFlip, FoldingTheBareCarriageReturnReManufacturesTheFalsePositive)
{
    std::string scratch;
    const std::string_view normalized{
        insight::tokenization::normalize(kRawWithoutCarriageReturn, scratch).bytes()};
    EXPECT_TRUE(is_failing(infer_leading_log_level(normalized)))
        << "without the \\r this line reads " << to_string(infer_leading_log_level(normalized))
        << " — if this ever stops being a failing read, the \\r is no longer what separates the "
           "WARNING token and this file's clause-6 argument needs re-deriving, not updating";
}

// Two-sidedness: without this the row above would pass a classifier that had simply stopped
// reporting failures on normalized content — which is the failure mode that would turn a precision
// gain into a real recall loss, silently, and is exactly what the to-passing flip was feared to be.
TEST(IngestNormalizationLevelFlip, NormalizationDoesNotDisarmGenuineFailureLines)
{
    std::string scratch;
    for (const std::string_view raw :
         {std::string_view{"\x1b[0;31mERROR: Job failed: exit code 1\x1b[0;m"},
          std::string_view{"\x1b[0K\x1b[31;1merror: cannot find crate 'serde'\x1b[0;m"},
          std::string_view{"FATAL: unrecoverable"}})
    {
        const std::string_view normalized{insight::tokenization::normalize(raw, scratch).bytes()};
        EXPECT_TRUE(is_failing(infer_leading_log_level(normalized)))
            << "a genuine failure line must stay failing after normalization; '" << normalized
            << "' read " << to_string(infer_leading_log_level(normalized));
    }
}
// NOLINTEND
