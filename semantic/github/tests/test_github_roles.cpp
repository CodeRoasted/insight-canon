// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_github_roles.cpp — the GitHub-Actions STRUCTURAL-ROLE vocabulary. Migrated from
// canon tests/utils/test_structural_role.cpp: the classification MECHANISM
// (insight::tokenization::classify, longest-match over the composed role rows) is CANON's; the
// VOCABULARY — `##[group]`/`::group::` → GroupBegin, `##[endgroup]`/`::endgroup::` → GroupEnd,
// `##[error]`/`::error::` → Terminator — is THIS package's kRoles rows, so the knowledge test homes
// here. The rows use kAnyDialect (the pre-split UNGATED behavior): a role fires whatever the caller
// declared. Determinism: byte-only, no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;           // compose / ComposedSemantics / classify / Tokenizer + enums
import insight.semantic.github; // kManifest

using insight::StructuralRole;
using insight::to_string;
using insight::semantic::ComposedSemantics;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::classify;

// The walkers take NormalizedContent — canon's ingest-normalization precondition carried by a type
// unforgeable outside canon; every probe here is an escape-free literal, so normalize() is the
// zero-copy fixed point over a shared scratch.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

namespace
{
// The RESOLVED view of a stream declaring this dialect.
[[nodiscard]] ComposedSemantics github_only()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::github::kDialect,
                                                            {});
}

// The same composition with NO dialect declared. Every role row is kAnyDialect, so this view must
// classify EXACTLY as the declared one — which is the assertion, not an accident.
[[nodiscard]] ComposedSemantics undeclared_stream()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::kAnyDialect, {});
}
} // namespace

// ── The announced markers classify to their role ──
TEST(GithubRoles, RecognizesAnnouncedMarkers)
{
    const ComposedSemantics gh{github_only()};
    EXPECT_EQ(classify(norm_probe("##[group]Run cmake --build ."), gh), StructuralRole::GroupBegin);
    EXPECT_EQ(classify(norm_probe("::group::Build"), gh), StructuralRole::GroupBegin);
    EXPECT_EQ(classify(norm_probe("##[endgroup]"), gh), StructuralRole::GroupEnd);
    EXPECT_EQ(classify(norm_probe("::endgroup::"), gh), StructuralRole::GroupEnd);
    EXPECT_EQ(classify(norm_probe("##[error]Process completed with exit code 2."), gh),
              StructuralRole::Terminator);
    EXPECT_EQ(classify(norm_probe("::error::file=x.cpp::boom"), gh), StructuralRole::Terminator);
}

// ── kAnyDialect: the role rows fire whatever the caller declared (the pre-split ungated behavior)
// ── This is the key vocabulary property of kRoles (dialect_gate = kAnyDialect): a `##[group]` on a
// stream that declared nothing at all still classifies. T4 changed the gate's TYPE, never these six
// rows' VALUE, so this is the assertion that the reading did not narrow with the type.
TEST(GithubRoles, FireWhateverTheStreamDeclared)
{
    const ComposedSemantics declared{github_only()};
    const ComposedSemantics undeclared{undeclared_stream()};
    for (const auto& [view, label] :
         {std::pair{std::cref(declared), std::string_view{"the declared github stream"}},
          std::pair{std::cref(undeclared), std::string_view{"an UNDECLARED stream"}}})
    {
        EXPECT_EQ(classify(norm_probe("##[group]x"), view.get()), StructuralRole::GroupBegin)
            << "kAnyDialect role failed to fire under " << label;
        EXPECT_EQ(classify(norm_probe("##[error]boom"), view.get()), StructuralRole::Terminator)
            << "kAnyDialect role failed to fire under " << label;
    }
}

// ── No false role: ordinary content and a bare `error:` prose line announce nothing ──
// The vocabulary boundary: only the bracketed `##[…]`/`::…::` tokens announce; a plain `error:`
// message is content, not a role (the Terminator is the ANNOUNCED marker, never the failure WORD).
TEST(GithubRoles, NoFalseRoleOnPlainContent)
{
    const ComposedSemantics gh{github_only()};
    EXPECT_EQ(classify(norm_probe("compiling tokenizer.cpp"), gh), StructuralRole::None);
    EXPECT_EQ(classify(norm_probe("error: undefined reference to foo"), gh), StructuralRole::None);
}

// ── End-to-end, over the DECLARED path: the caller resolves a stream, peels
// the transport, and hands only `RawPeeledLine::content` to the Tokenizer, which then tags the role
// on CanonicalEvent.
//
// ⚠ THE PEEL IS THE CALLER'S, and these tests are the smallest place that says so. These same
// lines once worked because `GitHubActionsStrategy` DETECTED the stamp and stripped it inside
// `parse()`. That detection is gone; a caller that hands canon a stamped line without declaring
// `api-rfc3339-line-prefix` gets the stamp in its template and no role — which is not a defect, it
// is the declaration contract, and `TokenizerSeesTheStampWithoutADeclaration` below pins it.
namespace
{
constexpr std::array<std::string_view, 1> kGhaStack{{"api-rfc3339-line-prefix"}};

// The one call a caller makes at stream open, with GitHub's delivery stamp declared.
[[nodiscard]] insight::semantic::ResolvedStream gha_stream(const ComposedSemantics& composed)
{
    return insight::semantic::resolve_stream(
        composed, insight::transport::IngestDeclaration{
                      .stack = kGhaStack,
                      .dialect = insight::semantic::github::kDialect,
                      .channel = insight::semantic::github::kChannelAnnotated});
}
} // namespace

TEST(GithubRoles, DeclaredPeelThenTokenizerTagsTerminatorOnGhaError)
{
    const ComposedSemantics composed{
        insight::semantic::compose(std::array{insight::semantic::github::kManifest})};
    const insight::semantic::ResolvedStream stream{gha_stream(composed)};
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics};

    const insight::transport::RawPeeledLine peeled{stream.transport.peel_raw(
        "2026-05-27T15:42:03.4000004Z ##[error]Process completed with exit code 2.")};
    ASSERT_FALSE(peeled.is_blank());
    EXPECT_EQ(peeled.content, "##[error]Process completed with exit code 2.")
        << "the declared peel must remove the stamp AND the separator space";
    ASSERT_TRUE(peeled.observation_time.has_value())
        << "the declared LinePrefixTimestamp extracts an OBSERVATION time for the caller to inject";

    const auto event{tokenizer.process_line(peeled.content)};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->structural_role, StructuralRole::Terminator);
    EXPECT_EQ(event->level, insight::LogLevel::Error)
        << "the declared level lift (##[error] -> Error) fires off the DECLARED dialect's rows";
}

TEST(GithubRoles, DeclaredPeelThenTokenizerTagsGroupBoundary)
{
    const ComposedSemantics composed{
        insight::semantic::compose(std::array{insight::semantic::github::kManifest})};
    const insight::semantic::ResolvedStream stream{gha_stream(composed)};
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics};
    const auto event{tokenizer.process_line(
        stream.transport.peel_raw("2026-05-27T15:42:03.4000004Z ##[group]Run cmake --build .")
            .content)};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->structural_role, StructuralRole::GroupBegin);
}

TEST(GithubRoles, DeclaredPeelThenTokenizerPlainGhaLineHasNoRole)
{
    const ComposedSemantics composed{
        insight::semantic::compose(std::array{insight::semantic::github::kManifest})};
    const insight::semantic::ResolvedStream stream{gha_stream(composed)};
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics};
    const auto event{tokenizer.process_line(
        stream.transport.peel_raw("2026-05-27T15:42:03.4000004Z compiling tokenizer.cpp object")
            .content)};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->structural_role, StructuralRole::None);
}

// The COST of not declaring, pinned so nobody rediscovers it as a bug. An empty stack's peel is the
// identity, so the stamp survives into `content` and the line-anchored role rows do not match. This
// is fail-closed on DEPTH, not on the run — the line still tokenizes.
TEST(GithubRoles, TokenizerSeesTheStampWithoutADeclaration)
{
    const ComposedSemantics composed{
        insight::semantic::compose(std::array{insight::semantic::github::kManifest})};
    const insight::semantic::ResolvedStream stream{
        insight::semantic::resolve_stream(composed, insight::transport::IngestDeclaration{})};
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics};
    const auto event{tokenizer.process_line(
        stream.transport
            .peel_raw("2026-05-27T15:42:03.4000004Z ##[error]Process completed with exit code 2.")
            .content)};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->structural_role, StructuralRole::None)
        << "an undeclared transport leaves the delivery stamp at the head of the content, so a "
           "line-anchored role row cannot match — declaring is the path to depth";
}
// NOLINTEND
