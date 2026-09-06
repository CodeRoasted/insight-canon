
// invariant: the verdict register is a POSITION claim, not an adjacency, and the five properties
// below are the whole property set for that claim.
// invariant: a trailing colon anchors a token only when every token before it is itself
// colon-terminated or bracket-enclosed — the KIND SLOT.
// invariant: a walk is needed at all because three real shapes are byte-IDENTICAL in the token's
// immediate neighbourhood, so no widening of that neighbourhood discriminates.
// invariant: the information is POSITIONAL.
// invariant: the rows are built to fail a rule that is NEARLY right, not copied from the offending
// shapes — four such fixtures would pass a per-shape denylist just as happily.
// invariant: the negative is the SAME STRING as its positive with ONE non-prefix token inserted, so
// it varies POSITION while holding vocabulary fixed.
// invariant: the caps row re-runs every negative in upper case, because a fix that over-tightened
// and killed the pure token anchor would pass every other row and read green.
// invariant: the invariance row is METAMORPHIC over ONE input, so no constant can be tuned to
// satisfy it — it pins the HEAD defect where the rows above pin the ANCHOR.
// refs: SRC-D-OUT-4c
#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogLevel;
using insight::utils::contains_failure_cue;
using insight::utils::infer_leading_log_level;

namespace
{
// invariant: each declared prefix class appears as a positive and as the SAME line with one
// non-prefix token inserted before the level word.
// invariant: the catalogue IS the row set — adding a prefix class means adding a row here, and
// that coupling is the point.
struct KindSlotRow
{
    std::string_view label;
    std::string_view in_slot;
    std::string_view displaced;
};

constexpr std::array<KindSlotRow, 7U> kRows{{
    {.label = "index 0 (vacuously in slot)",
     .in_slot = "error: cannot find crate 'serde'",
     .displaced = "boot error: cannot find crate 'serde'"},
    {.label = "a run of colon-terminated tokens (the compiler frame)",
     .in_slot = "src/shell/shell.zig:357:14: error: expected primary type expression",
     .displaced = "at src/shell/shell.zig:357:14: error: expected primary type expression"},
    {.label = "a colon-terminated tool prefix",
     .in_slot = "ld: error: cannot find crate 'serde'",
     .displaced = "ld: linking error: cannot find crate 'serde'"},
    {.label = "bracket-enclosed [..]",
     .in_slot = "[main] error: cannot find crate 'serde'",
     .displaced = "[main] boot error: cannot find crate 'serde'"},
    {.label = "bracket-enclosed (..)",
     .in_slot = "(none) error: cannot find crate 'serde'",
     .displaced = "(none) boot error: cannot find crate 'serde'"},
    {.label = "bracket-enclosed <..>",
     .in_slot = "<toolchain> error: cannot find crate 'serde'",
     .displaced = "<toolchain> boot error: cannot find crate 'serde'"},
    {.label = "bracket-enclosed <..> then a colon-terminated run",
     .in_slot = "<WORKSPACE>:16:1: error: no comment on the exported symbol",
     .displaced = "<WORKSPACE>:16:1: pedantic error: no comment on the exported symbol"},
}};

// invariant: the caps form uppercases the level word IN PLACE, because the pure token anchor must
// be untouched by the kind-slot precondition on the colon anchor.
[[nodiscard]] std::string shout(std::string_view line)
{
    std::string out{line};
    const std::size_t at{out.find("error:")};
    EXPECT_NE(at, std::string::npos) << "row has no lowercase 'error:' to uppercase: " << line;
    if (at != std::string::npos)
        out.replace(at, 5U, "ERROR");
    return out;
}
} // namespace

TEST(VerdictRegisterKindSlot, EveryDeclaredPrefixClassKeepsTheAnchor)
{
    for (const KindSlotRow& row : kRows)
        EXPECT_EQ(infer_leading_log_level(row.in_slot), LogLevel::Error)
            << "prefix class '" << row.label
            << "' must keep the verdict colon anchored\n  line: " << row.in_slot;

    // invariant: a kind slot is NECESSARY, not sufficient — the count register reads a bare
    // integer predecessor as an aggregate and caps the line, independently of this register.
    // invariant: the positive row uses a second numeric field so the integer's own predecessor is
    // digit-leading, which is the numeric-chain guard the count register already carries.
    // refs: SRC-D-CNT-1
    EXPECT_EQ(infer_leading_log_level("<WORKSPACE>:16: error: no comment on the exported symbol"),
              LogLevel::Warn)
        << "SRC-D-CNT-1 (count register), not SRC-D-OUT-4c: a bare integer immediately before the "
           "level "
           "word makes it a summary, and a summary caps at Warn";
}

// invariant: THE DISCRIMINATING ROW — same words, same colon, ONE token moved.
// invariant: a rule that keyed on vocabulary would classify these identically to their positives; a
// POSITION rule cannot.
TEST(VerdictRegisterKindSlot, OneInsertedNonPrefixTokenDisplacesTheKindSlot)
{
    for (const KindSlotRow& row : kRows)
        EXPECT_EQ(infer_leading_log_level(row.displaced), LogLevel::Unknown)
            << "prefix class '" << row.label
            << "': one non-prefix token before the level word must displace the kind slot\n"
               "  in slot:   "
            << row.in_slot << "\n  displaced: " << row.displaced;
}

// invariant: the green-but-BLIND guard — every negative in caps must still classify, or a fix
// that took the pure token anchor down with the colon one would pass the first two properties.
TEST(VerdictRegisterKindSlot, CapsRegisterStillFiresOnEveryDisplacedRow)
{
    for (const KindSlotRow& row : kRows)
    {
        const std::string caps{shout(row.displaced)};
        EXPECT_EQ(infer_leading_log_level(caps), LogLevel::Error)
            << "the CAPS form of a DISPLACED row must still fire — anchor #1 is a pure token test "
               "and the kind slot does not gate it\n  line: "
            << caps;
    }
}

// invariant: METAMORPHIC over one input, which is what makes it untunable — it sweeps a padding
// run across the head the old byte budget scanned and asserts the verdict never moves.
// invariant: no choice of a byte constant satisfies it; only removing bytes from the CLAIM does.
// invariant: the padding is ONE token whatever its length, so under the token budget the property
// holds by construction and the row stays as the falsifier if a byte budget ever returns.
// invariant: the shape is a real corpus family whose unanchored level word made the verdict depend
// on whether the next word STARTED within the head — a cost bound deciding a verdict.
// refs: ADR-16.D7
TEST(VerdictRegisterKindSlot, VerdictIsInvariantUnderPrefixLengthAcrossTheScanHead)
{
    constexpr std::string_view kBody{"Failed to resolve action download info. Error: boom"};
    constexpr std::size_t kMinPad{1};
    constexpr std::size_t kMaxPad{60};

    const auto padded{[&](std::size_t pad)
                      { return "[" + std::string(pad, 'a') + "] " + std::string{kBody}; }};

    const LogLevel reference{infer_leading_log_level(padded(kMinPad)).value()};
    EXPECT_EQ(reference, LogLevel::Error)
        << "the un-padded control must be Error — `Failed` is a self-anchoring verdict, so a "
           "vacuous 'everything is Unknown' invariance cannot pass this row";
    for (std::size_t pad{kMinPad}; pad <= kMaxPad; ++pad)
    {
        const std::string line{padded(pad)};
        EXPECT_EQ(infer_leading_log_level(line), reference)
            << "the verdict moved at padding length " << pad << " (the level word starts at byte "
            << line.find("info")
            << ") — a register decision must not depend on a byte budget\n  line: " << line;
    }
}

// invariant: the rule only ever REMOVES an anchor, so on the CUE surface no line that did not fire
// can start firing — the precision-first direction the ruling rests on.
// invariant: a degradation may drop signal, never fabricate one.
TEST(VerdictRegisterKindSlot, TheCueSurfaceOnlyEverLosesAnchors)
{
    EXPECT_FALSE(contains_failure_cue("Writing tsc-error-report.json"));
    EXPECT_FALSE(contains_failure_cue("the error path is documented in the runbook"));
    EXPECT_FALSE(contains_failure_cue("timeout budget set to 30s for the slow shards"));
    EXPECT_FALSE(contains_failure_cue("request GET /api/orders 200 14ms"));
    // invariant: the kind-slot displacements are the new members of that set — they lost an
    // anchor and gained nothing.
    for (const KindSlotRow& row : kRows)
        EXPECT_FALSE(contains_failure_cue(row.displaced))
            << "a displaced level word must not fire as a cue either: " << row.displaced;
}

// invariant: MEASURED COUNTER-EXAMPLE, pinned rather than hidden — monotonicity is a property of
// the ANCHOR and does NOT carry through to the level inference, which FALLS THROUGH.
// invariant: taking the explicit stage's authority away from a non-alerting leading level word
// hands the line to the cue scan, which can classify it HIGHER.
// invariant: measured at 371 promotions against 9 556 demotions on one corpus, and 3 quanta
// to-failing against 930 to-passing.
// invariant: kept as a row so the qualification is stated where the property is claimed, not left
// for the next reader to rediscover.
TEST(VerdictRegisterKindSlot, TheLevelSurfaceIsNotMonotoneAndThisIsTheMeasuredFamily)
{
    constexpr std::string_view kEchoedRetry{
        "  sudo apt-get clean || echo \"::warning::The command [sudo apt-get clean] failed to "
        "complete successfully. Proceeding...\""};
    EXPECT_EQ(infer_leading_log_level(kEchoedRetry), LogLevel::Error)
        << "the displaced `warning:` no longer caps this line at Warn, so the self-anchoring "
           "`failed` decides it — a PROMOTION produced by a demoting anchor rule";
    EXPECT_TRUE(contains_failure_cue(kEchoedRetry))
        << "and the cue surface is unchanged: `failed` self-anchors, it never needed the register";
}

// invariant: DECLARED LIMITATION — a leading TIMESTAMP is not prefix material, because its LAST
// segment is followed by a space rather than being colon-terminated or bracket-enclosed.
// invariant: so the level word after it is displaced, and the bracketed and comma-millisecond
// spellings fail the same way.
// invariant: this is BEHAVIOUR and not a bug this test may paper over, pinned so it cannot shift
// silently.
// invariant: it costs nothing on the shipped paths, because every production consumer hands the
// inference content whose timestamp a strategy or a strip has already removed.
// invariant: it is measurable only on the raw-text path, where the whole line is passed — 17 295
// lines move down against 1 031 up over one corpus.
// invariant: the disposition belongs to the claim boundary's owner and the prefix catalogue to the
// architect; this row states what the code does TODAY.
TEST(VerdictRegisterKindSlot, ALeadingTimestampIsNotPrefixMaterialDeclaredLimitation)
{
    EXPECT_EQ(infer_leading_log_level("2026-05-29T10:00:00.123Z error: cannot find crate 'serde'"),
              LogLevel::Unknown)
        << "the last timestamp segment `00.123Z` is space-terminated, so it is not prefix material "
           "and the level word is displaced";
    EXPECT_EQ(infer_leading_log_level("[2026-05-29T10:00:00Z] error: cannot find crate 'serde'"),
              LogLevel::Unknown)
        << "the bracketed form fails identically — `00Z` is bracket-CLOSED but not bracket-"
           "ENCLOSED (its left neighbour is a colon, not the opening bracket)";
    EXPECT_EQ(infer_leading_log_level("2026-05-29 10:00:00,123 error: cannot find crate 'serde'"),
              LogLevel::Unknown)
        << "the space-separated date+time form fails at its FIRST token: `2026-05-29` is followed "
           "by a space";
    // invariant: two-sidedness — without these the rows above would pass a classifier that had
    // simply stopped working on timestamped lines.
    EXPECT_EQ(infer_leading_log_level("2026-05-29T10:00:00.123Z ERROR: cannot find crate 'serde'"),
              LogLevel::Error)
        << "the CAPS form behind the same timestamp still fires — anchor #1 needs no kind slot";
    EXPECT_EQ(infer_leading_log_level("error: cannot find crate 'serde'"), LogLevel::Error)
        << "and the same line with the timestamp removed fires — the timestamp is the whole "
           "difference";
}
