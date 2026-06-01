// NOLINTBEGIN — unit test: short identifiers and test-specific patterns are fine.
// test_structural_role.cpp — StructuralRoleRegistry (announced line-roles).
//
// A line's structural role (what it DOES in the sequence) is a separate ontology
// from the semantic class of tokens inside it (what a value MEANS). This verifies
// the announced-marker recognition and that the role is carried on CanonicalEvent.

#include <gtest/gtest.h>

#include "insight/core/types.hpp"
#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/structural_role_registry.hpp"
#include "insight/tokenization/tokenizer_engine.hpp"

using insight::StructuralRole;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::StructuralRoleRegistry;
using insight::tokenization::Tokenizer;

TEST(StructuralRoleRegistry, RecognizesAnnouncedMarkers)
{
    const StructuralRoleRegistry registry;
    EXPECT_EQ(registry.classify("##[group]Run cmake --build ."), StructuralRole::GroupBegin);
    EXPECT_EQ(registry.classify("::group::Build"), StructuralRole::GroupBegin);
    EXPECT_EQ(registry.classify("##[endgroup]"), StructuralRole::GroupEnd);
    EXPECT_EQ(registry.classify("##[error]Process completed with exit code 2."),
              StructuralRole::Terminator);
    EXPECT_EQ(registry.classify("::error::file=x.cpp::boom"), StructuralRole::Terminator);
    // A plain line declares no role — and an error-LOOKING line without an
    // announced marker is NOT a structural terminator (announced-only discipline).
    EXPECT_EQ(registry.classify("compiling tokenizer.cpp"), StructuralRole::None);
    EXPECT_EQ(registry.classify("error: undefined reference to foo"), StructuralRole::None);
}

// End-to-end: the GHA strategy keeps the marker in `content`, so the tokenizer
// tags the role on the CanonicalEvent.
TEST(StructuralRole, TokenizerTagsTerminatorOnGhaError)
{
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena};
    const auto event{tokenizer.process_line(
        "2026-05-27T15:42:03.4000004Z ##[error]Process completed with exit code 2.")};
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->structural_role, StructuralRole::Terminator);
}

TEST(StructuralRole, TokenizerTagsGroupBoundary)
{
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena};
    const auto event{
        tokenizer.process_line("2026-05-27T15:42:03.4000004Z ##[group]Run cmake --build .")};
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->structural_role, StructuralRole::GroupBegin);
}

TEST(StructuralRole, PlainLineHasNoRole)
{
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena};
    const auto event{
        tokenizer.process_line("2026-05-27T15:42:03.4000004Z compiling tokenizer.cpp object")};
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->structural_role, StructuralRole::None);
}

// NOLINTEND — unit test
