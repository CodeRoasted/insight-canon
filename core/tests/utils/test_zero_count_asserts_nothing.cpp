// invariant: a failure word counted by exactly `0` asserts no failure, so the line goes on to the
// invariant: remaining stages as if the word were absent: a green test summary reads no cue.
// invariant: a non-zero count still caps the line at Warn, and a fail glyph still reads Error.
// invariant: the lines are synthetic, written in the producers' public summary shapes.
// refs: DN-163.D11, ADR-20.D5
#include <gtest/gtest.h>

import insight.canon.test;

using insight::utils::infer_leading_log_level;

namespace
{
// post: the level's name, so a failure prints `Unknown` against `Warn`, never two byte dumps.
[[nodiscard]] std::string_view level_of(std::string_view line)
{
    return insight::to_string(infer_leading_log_level(line).value());
}

struct LevelRow
{
    std::string_view line;
    std::string_view level;
};

} // namespace

TEST(ZeroCountAssertsNothing, AZeroFailureCountReadsNoCue)
{
    for (const LevelRow& row :
         {LevelRow{"[9/69] ok 3 pass / 0 fail a.test.ts", "Unknown"},
          LevelRow{"5 passed, 0 failed", "Unknown"}, LevelRow{"0 fail", "Unknown"}})
        EXPECT_EQ(level_of(row.line), row.level)
            << "a zero count summarizes no failure (DN-163.D11).\n  line: " << row.line;
}

TEST(ZeroCountAssertsNothing, ANonZeroCountAndAFailGlyphKeepTheirLevel)
{
    for (const LevelRow& row :
         {LevelRow{"0 passed, 5 failed", "Warn"}, LevelRow{"10 failed", "Warn"},
          LevelRow{"\xe2\x9c\x97 0 failed", "Error"}})
        EXPECT_EQ(level_of(row.line), row.level)
            << "a counted failure stays a summary, a leading fail glyph an error.\n  line: "
            << row.line;
}
