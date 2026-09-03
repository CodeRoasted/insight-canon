// Unit tests: allow short identifiers and test-specific patterns
// tests/utils/test_verdict_register_kind_slot.cpp
//
// SRC-D-OUT-4c — the verdict register is a POSITION claim, not an adjacency.
// The five numbered properties below are the whole property set for that claim.
//
// THE RULE. A trailing `:` anchors token `T` only when `T` occupies the line's KIND SLOT: every
// token preceding `T` is itself colon-terminated (`ld:`, `src/main.rs:`, `357:`) or bracket-
// enclosed (`[main]`, `(none)`, `<WORKSPACE>`). The reason a walk is needed at all is a byte
// compare: `error: connection refused` (a verdict), `error: string` (a struct field) and
// `err: &str` (a code-frame parameter) are byte-IDENTICAL in the ±1 neighbourhood of the token, so
// no widening of that neighbourhood discriminates — the information is positional.
//
// WHY THESE ROWS AND NOT FOUR FIXTURES DRAWN FROM THE OFFENDING SHAPES. Four fixtures copied from
// the false positives would pass a per-shape denylist just as happily as a position rule, and would
// prove nothing about either. Every row below is built to fail a rule that is nearly-right:
//   * KindSlotNegative is the SAME STRING as its positive with ONE non-prefix token inserted. Same
//     words, same colon, one token moved — it varies POSITION while holding vocabulary fixed.
//   * CapsAnchorSurvives re-runs every negative in CAPS. Without it, a fix that over-tightened and
//     killed anchor #1 would pass every other row and read green — blind, not correct.
//   * PrefixLengthInvariance is METAMORPHIC over ONE input — it sweeps a padding length and asserts
//     the verdict is constant — so no constant can be tuned to satisfy it. It pins the HEAD defect
//     (a byte budget deciding a claim) where the rows above pin the ANCHOR.

#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogLevel;
using insight::utils::contains_failure_cue;
using insight::utils::infer_leading_log_level;

namespace
{
// Each declared prefix class, as a positive line and the SAME line with one non-prefix token
// inserted immediately before the level word. The catalogue IS the row set: adding a prefix class
// means adding a row here, and that coupling is the point.
struct KindSlotRow
{
    std::string_view label;
    std::string_view in_slot;   ///< the level word occupies the kind slot -> Error
    std::string_view displaced; ///< one non-prefix token inserted before it -> Unknown
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

// The CAPS form of a row: the level word uppercased in place. Anchor #1 is a pure token test and
// must be untouched by the kind-slot precondition on anchor #2.
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

// ── Property 1 — one positive per DECLARED prefix class ────────────────────────
TEST(VerdictRegisterKindSlot, EveryDeclaredPrefixClassKeepsTheAnchor)
{
    for (const KindSlotRow& row : kRows)
        EXPECT_EQ(infer_leading_log_level(row.in_slot), LogLevel::Error)
            << "prefix class '" << row.label
            << "' must keep the verdict colon anchored\n  line: " << row.in_slot;

    // A kind slot is necessary, not sufficient, and the interaction is worth stating because the
    // declared prefix-class table lists this exact bazel line. `<WORKSPACE>:16: error:` IS in the
    // kind slot, and it still does not classify Error — the level word's immediate predecessor is
    // the bare integer `16`, so SRC-D-CNT-1's COUNT register reads it as an aggregate ("16 errors")
    // and caps the line at Warn. That register is checked independently of this one and is unmoved
    // by SRC-D-OUT-4c; the row above uses `:16:1:` so the `1`'s own predecessor is digit-leading,
    // which is exactly the numeric-chain guard SRC-D-CNT-1 already carries for timestamps.
    EXPECT_EQ(infer_leading_log_level("<WORKSPACE>:16: error: no comment on the exported symbol"),
              LogLevel::Warn)
        << "SRC-D-CNT-1 (count register), not SRC-D-OUT-4c: a bare integer immediately before the "
           "level "
           "word makes it a summary, and a summary caps at Warn";
}

// ── Property 2 — THE DISCRIMINATING ROW: one token, not one vocabulary ─────────
// Each line here is its positive above with a single non-prefix token inserted before the level
// word. Same words, same colon, one token moved. A rule that keyed on vocabulary (a denylist of
// "error" contexts) would classify these identically to their positives; a POSITION rule cannot.
TEST(VerdictRegisterKindSlot, OneInsertedNonPrefixTokenDisplacesTheKindSlot)
{
    for (const KindSlotRow& row : kRows)
        EXPECT_EQ(infer_leading_log_level(row.displaced), LogLevel::Unknown)
            << "prefix class '" << row.label
            << "': one non-prefix token before the level word must displace the kind slot\n"
               "  in slot:   "
            << row.in_slot << "\n  displaced: " << row.displaced;
}

// ── Property 3 — the green-but-BLIND guard: anchor #1 is independent ───────────
// Every negative above, in CAPS, must still classify Error. Without this row a fix that
// over-tightened and took anchor #1 down with anchor #2 would pass properties 1 and 2, and read
// green.
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

// ── Property 4 — the property whose ABSENCE is the 436: byte-length invariance ─
// METAMORPHIC OVER ONE INPUT, which is what makes it untunable: it sweeps a bracket-enclosed
// padding run from 1 to 60 bytes — across the 40-byte head Stage 1 scanned until ADR-16.D7 made
// its budget a token count (kLeadingScanTokens) — and asserts the verdict never moves. No choice
// of a byte constant satisfies it; only removing bytes from the CLAIM does. The padding is ONE
// token whatever its length, so under the token budget the property holds by construction, and
// the row stays as the falsifier that reds if a byte budget ever returns.
//
// The shape is a real corpus family (GHA `revert/v1/full`): `Failed to resolve action download
// info. Error: …`. `info.` is an unanchored level word, and whether the `Error` after it was seen
// depended on whether it STARTED within 40 bytes — so the line read as Info (a bare terminal
// status) at one padding length and Error at another. Terminality is a property of the LINE;
// deriving it inside a head made a cost bound decide a verdict.
TEST(VerdictRegisterKindSlot, VerdictIsInvariantUnderPrefixLengthAcrossTheScanHead)
{
    constexpr std::string_view kBody{"Failed to resolve action download info. Error: boom"};
    constexpr std::size_t kMinPad{1};
    constexpr std::size_t kMaxPad{
        60}; // comfortably past the 40-byte head the token budget replaced

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

// ── Property 5 — monotone-demoting, and the ONE surface it does not carry ──────
// SRC-D-OUT-4c only ever REMOVES an anchor, so on the CUE surface (contains_failure_cue) no line
// that did not fire can start firing: the anchors confirm an already-matched failure word and there
// is no branch in which losing one creates a cue. That is the precision-first direction the ruling
// rests on — a degradation may drop signal, never fabricate one — and these rows pin it.
TEST(VerdictRegisterKindSlot, TheCueSurfaceOnlyEverLosesAnchors)
{
    EXPECT_FALSE(contains_failure_cue("Writing tsc-error-report.json"));
    EXPECT_FALSE(contains_failure_cue("the error path is documented in the runbook"));
    EXPECT_FALSE(contains_failure_cue("timeout budget set to 30s for the slow shards"));
    EXPECT_FALSE(contains_failure_cue("request GET /api/orders 200 14ms"));
    // The kind-slot displacements above are the new members of this set — they lost an anchor and
    // gained nothing.
    for (const KindSlotRow& row : kRows)
        EXPECT_FALSE(contains_failure_cue(row.displaced))
            << "a displaced level word must not fire as a cue either: " << row.displaced;
}

// MEASURED COUNTER-EXAMPLE, pinned rather than hidden. Monotone-demoting is a property of the
// ANCHOR and it does NOT carry through to infer_leading_log_level, because that function FALLS
// THROUGH: taking Stage 1's authority away from a non-alerting leading level word hands the line to
// Stage 2's cue scan, which can classify it HIGHER. Measured on GHA `revert/v1/full`: 371 of
// 22 457 947 lines are promoted this way against 9 556 demoted (26:1 demoting), 3 quanta to-failing
// against 930 to-passing. The dominant family is below — `warning:` was authoritative Warn, and
// once displaced the self-anchoring `failed` fires. Kept as a row so the qualification is stated
// where the property is claimed, not left for the next reader to rediscover.
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

// ── DECLARED LIMITATION: a leading TIMESTAMP is not prefix material ────────────
// The declared prefix-class catalogue lists `2026-…T10:00:00.123Z error: msg` as firing, on the
// reading that
// "timestamp segments are colon-terminated". The LAST segment is not: under the shared canon
// tokenization `2026-05-29T10:00:00.123Z` is three tokens — `2026-05-29T10`, `00`, `00.123Z` — and
// the third is followed by a SPACE, so it is neither colon-terminated nor bracket-enclosed and the
// level word after it is displaced. The two forms `[2026-…Z]` and `2026-05-29 10:00:00,123` fail
// the same way.
//
// This is behaviour, not a bug this test may paper over, and it is pinned so it cannot shift
// silently. It costs nothing on the shipped paths — every production consumer hands
// infer_leading_log_level content whose timestamp a strategy or `strip_leading_timestamp` already
// removed — and it is measurable only on the RawTextStrategy path, which passes the whole line:
// over the same 22 457 947 GHA lines the raw-line probe moves 17 295 lines down (largely genuine
// compiler diagnostics behind a runner stamp) against 1 031 up. The disposition is Eqya's and the
// prefix catalogue is Daidalos's; this row states what the code does today.
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
    // Two-sidedness — without these the rows above would pass a classifier that had simply stopped
    // working on timestamped lines.
    EXPECT_EQ(infer_leading_log_level("2026-05-29T10:00:00.123Z ERROR: cannot find crate 'serde'"),
              LogLevel::Error)
        << "the CAPS form behind the same timestamp still fires — anchor #1 needs no kind slot";
    EXPECT_EQ(infer_leading_log_level("error: cannot find crate 'serde'"), LogLevel::Error)
        << "and the same line with the timestamp removed fires — the timestamp is the whole "
           "difference";
}
