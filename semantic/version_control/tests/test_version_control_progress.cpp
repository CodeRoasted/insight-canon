// invariant: git's checkout gauge is this package's vocabulary, so the arms proving which lines
// take the Progress role, and which lines sharing its bytes stay content, home with the rows.
// invariant: every row is dialect-independent, so each arm holds on a stream declaring nothing and
// on one declaring this package's own name alike.
// invariant: an arm behind a CI runner's stamp or stream prefix needs that runner's peel, so it
// homes with the shipped composition, in insight-eidos's composition suite.
// refs: ADR-17.D14
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.version_control;

using insight::semantic::ComposedSemantics;
using insight::tokenization::classify;

namespace
{
// invariant: git prints the percent right-aligned in three columns: one line of each width, and
// the closing line ending in a carriage return the line ending removes.
constexpr std::array<std::string_view, 4> kGaugeLines{
    "Updating files:   7% (70/1000)", "Updating files:  45% (450/1000) ",
    "Updating files: 100% (1000/1000)", "Updating files: 100% (1000/1000), done.\r"};

[[nodiscard]] insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

[[nodiscard]] ComposedSemantics view_of(std::string_view dialect)
{
    const std::array manifests{insight::semantic::version_control::kManifest};
    return insight::semantic::compose(manifests).for_stream(dialect, {});
}

// post: the template and role canon gives `line` under `composed`, read through the public
// tokenizer; the role reads "no event" when the line yields none.
[[nodiscard]] std::pair<std::string, std::string> event_of(const ComposedSemantics& composed,
                                                           std::string_view line)
{
    insight::tokenization::ArenaAllocator arena{std::size_t{64} * 1024};
    insight::tokenization::Tokenizer tokenizer{
        arena, insight::tokenization::MaskConfig{}, composed, {}};
    const auto event{tokenizer.process_line(line)};
    if (!event.has_value())
        return {"", "no event"};
    return {std::string{event->template_str},
            std::string{insight::to_string(event->structural_role)}};
}
} // namespace

TEST(VersionControlProgress, EachGaugeShapeTakesTheRoleOnAnyStream)
{
    for (const std::string_view dialect :
         {insight::semantic::kAnyDialect, insight::semantic::version_control::kDialect})
    {
        const ComposedSemantics view{view_of(dialect)};
        for (const std::string_view line : kGaugeLines)
            EXPECT_EQ(insight::to_string(classify(norm_probe(line), view)),
                      std::string_view{"Progress"})
                << "the gauge line \"" << line << "\" took no role on a stream declaring \""
                << dialect << '"';
    }
}

// invariant: each line opens with the gauge's bytes and is content: git's words with no count, a
// sample with text after it, a padding no column width gives, and a truncated sample.
// invariant: so is the last sample joined to the closing line by a carriage return on one line.
TEST(VersionControlProgress, LinesSharingTheGaugeBytesStayContent)
{
    const ComposedSemantics view{view_of(insight::semantic::kAnyDialect)};
    for (const std::string_view content :
         {"Updating files: done", "Updating files:  45% (450/1000) extra",
          "Updating files:    5% (50/1000)", "Updating files:  45%",
          "Updating files: 100% (1000/1000)\rUpdating files: 100% (1000/1000), done."})
        EXPECT_EQ(insight::to_string(classify(norm_probe(content), view)), std::string_view{"None"})
            << "content took a role: \"" << content << '"';
}

// invariant: the package names a role and moves nothing else: with it not composed, a gauge line's
// event is the one canon gave before the package existed, its template byte for byte.
TEST(VersionControlProgress, NotComposedTheEventIsUnchangedSaveTheRole)
{
    const std::array<insight::semantic::SemanticPackageManifest, 0> none{};
    const ComposedSemantics without{insight::semantic::compose(none)};
    const ComposedSemantics with{view_of(insight::semantic::kAnyDialect)};
    for (const std::string_view line : kGaugeLines)
    {
        const auto [template_without, role_without]{event_of(without, line)};
        const auto [template_with, role_with]{event_of(with, line)};
        EXPECT_EQ(role_without, "None") << "line \"" << line << '"';
        EXPECT_EQ(role_with, "Progress") << "line \"" << line << '"';
        EXPECT_EQ(template_with, template_without)
            << "composing the package moved the template of \"" << line << '"';
    }
}
