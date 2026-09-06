// invariant: the classification MECHANISM is canon's longest-match algorithm and the VOCABULARY is
// this package's role rows, so the knowledge test homes with the vocabulary.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::StructuralRole;
using insight::to_string;
using insight::semantic::ComposedSemantics;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::classify;

// pre: `NormalizedContent` is canon's ingest-normalization proof, unforgeable outside canon; every
// probe here is escape-free, so `normalize` is the zero-copy fixed point.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

namespace
{
[[nodiscard]] ComposedSemantics github_only()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::github::kDialect,
                                                            {});
}

[[nodiscard]] ComposedSemantics undeclared_stream()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::kAnyDialect, {});
}
} // namespace

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

// assert: T4 changed the dialect gate's TYPE and never these rows' VALUE, so this is the arm that
// says the reading did not narrow when the type did.
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

// invariant: a Terminator is the ANNOUNCED bracketed marker, never the failure WORD.
TEST(GithubRoles, NoFalseRoleOnPlainContent)
{
    const ComposedSemantics gh{github_only()};
    EXPECT_EQ(classify(norm_probe("compiling tokenizer.cpp"), gh), StructuralRole::None);
    EXPECT_EQ(classify(norm_probe("error: undefined reference to foo"), gh), StructuralRole::None);
}

namespace
{
constexpr std::array<std::string_view, 1> kGhaStack{{"api-rfc3339-line-prefix"}};

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

// refs: DN-43.D12
// assert: an empty stack's peel is the identity, so the stamp survives into `content` and the role
// rows — which match strictly at offset 0 — cannot fire.
// invariant: a role assertion ALONE cannot separate `the stamp blocks the row` from `the projection
// was destroyed`, so it is paired with template and params evidence.
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
    EXPECT_TRUE(event->template_str.starts_with("<*> ##[error]"))
        << "the stamp must still be in `content` when the masker runs, as its own leading token; "
           "template_str = \""
        << event->template_str << "\"";
    ASSERT_FALSE(event->params.empty()) << "template_str = \"" << event->template_str << "\"";
    EXPECT_EQ(event->params.front(), "2026-05-27T15:42:03.4000004Z")
        << "the masked leading position must carry the stamp's own bytes; params[0] = \""
        << event->params.front() << "\"";
}
