// NOLINTBEGIN — unit test: short identifiers and literals are fine.
// test_template_id.cpp — the D-TIR-1 invariants for canon's TemplateId
// (insight_perf_template_id.md §2). These are the golden-preserving guards the whole
// perf refactor rests on: byte-lexicographic order == "h:"+hex order, render/parse
// round-trip, content determinism, and a working std::hash specialization.

#include <gtest/gtest.h>

import insight.canon.test;

using insight::ngram_id_of;
using insight::NgramId;
using insight::parse_template_id;
using insight::render;
using insight::template_id_of;
using insight::TemplateId;

namespace
{

// A spread of representative masked templates (the kind the masker emits).
const std::vector<std::string> kSamples{
    "GET <*> -> <*>",
    "user <*> logged in",
    "charge order total",
    "verify session token",
    "took alternate cache path",
    "ERROR db connect failed code=<*>",
    "",
    "a",
    "the quick brown fox jumps",
};

} // namespace

TEST(TemplateIdInvariants, RenderShapeIsHColonThirtyTwoHex)
{
    for (const auto& sample : kSamples)
    {
        const std::string rendered{render(template_id_of(sample))};
        ASSERT_EQ(rendered.size(), 34U) << "render must be \"h:\" + 32 hex for: " << sample;
        EXPECT_EQ(rendered.substr(0, 2), "h:");
        for (const char chr : rendered.substr(2))
            EXPECT_TRUE((chr >= '0' && chr <= '9') || (chr >= 'a' && chr <= 'f'))
                << "non-lowercase-hex char in " << rendered;
    }
}

TEST(TemplateIdInvariants, ContentDeterministic)
{
    for (const auto& sample : kSamples)
        EXPECT_EQ(template_id_of(sample), template_id_of(sample));
}

TEST(TemplateIdInvariants, ParseIsInverseOfRender)
{
    for (const auto& sample : kSamples)
    {
        const TemplateId id{template_id_of(sample)};
        EXPECT_EQ(parse_template_id(render(id)), id) << "round-trip failed for: " << sample;
    }
}

// Invariant 1 (the golden-preserver): byte-lexicographic order over `bytes` reproduces
// the old "h:"+hex string order EXACTLY, so every vector<TemplateId> sort is byte-identical
// to the old vector<string> sort.
TEST(TemplateIdInvariants, ByteOrderMatchesHexStringOrder)
{
    for (const auto& lhs : kSamples)
        for (const auto& rhs : kSamples)
        {
            const TemplateId lhs_id{template_id_of(lhs)};
            const TemplateId rhs_id{template_id_of(rhs)};
            EXPECT_EQ(lhs_id < rhs_id, render(lhs_id) < render(rhs_id))
                << "POD order diverged from hex-string order: \"" << lhs << "\" vs \"" << rhs
                << '"';
        }
}

// std::hash<TemplateId> must be reachable + usable (the whole point of carrying a POD).
TEST(TemplateIdInvariants, StdHashUsableInUnorderedContainer)
{
    std::unordered_map<TemplateId, std::string> by_id;
    for (const auto& sample : kSamples)
        by_id[template_id_of(sample)] = sample;
    for (const auto& sample : kSamples)
    {
        const auto iter{by_id.find(template_id_of(sample))};
        ASSERT_NE(iter, by_id.end()) << "lookup miss for: " << sample;
    }
    // distinct non-empty samples → distinct ids (no accidental collision in the set)
    EXPECT_GE(by_id.size(), kSamples.size() - 1U); // -1 tolerates any dup in the literal list
}

// ── NgramId (D-TIR-4(2)): the scalar key for n-gram SEQUENCES ──
// Transient (never serialized), so the contract is in-memory keying only: deterministic,
// order-sensitive, distinct-per-distinct-sequence, and usable in an unordered_map.
namespace
{
[[nodiscard]] std::vector<TemplateId> seq(std::initializer_list<std::string_view> templates)
{
    std::vector<TemplateId> out;
    out.reserve(templates.size());
    for (const auto& tmpl : templates)
        out.push_back(template_id_of(tmpl));
    return out;
}
} // namespace

TEST(NgramIdInvariants, Deterministic)
{
    const auto sequence{seq({"GET <*> -> <*>", "user <*> logged in", "charge order total"})};
    EXPECT_EQ(ngram_id_of(sequence), ngram_id_of(sequence));
}

TEST(NgramIdInvariants, OrderSensitive)
{
    // [a,b] and [b,a] are DISTINCT n-grams → distinct ids (transition order is significant).
    const auto forward{seq({"user <*> logged in", "charge order total"})};
    const auto reversed{seq({"charge order total", "user <*> logged in"})};
    EXPECT_NE(ngram_id_of(forward), ngram_id_of(reversed));
}

TEST(NgramIdInvariants, DistinctSequencesGiveDistinctIds)
{
    const std::vector<std::vector<TemplateId>> sequences{
        seq({"a"}),
        seq({"a", "the quick brown fox jumps"}),
        seq({"the quick brown fox jumps", "a"}),
        seq({"GET <*> -> <*>", "user <*> logged in", "charge order total"}),
        seq({"GET <*> -> <*>", "user <*> logged in"}),
        seq({}), // empty sequence is well-defined (the basis), distinct from any non-empty
    };
    std::unordered_map<NgramId, std::size_t> by_id;
    for (std::size_t i{0}; i < sequences.size(); ++i)
        by_id[ngram_id_of(sequences[i])] = i;
    EXPECT_EQ(by_id.size(), sequences.size()) << "ngram_id collision across distinct sequences";
}

// NOLINTEND
