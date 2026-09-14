
// invariant: the invariants for canon's template-id type — the golden-preserving guards the whole
// performance refactor rests on.
// invariant: byte-lexicographic order matching the old rendered-string order, the render and parse
// round trip, content determinism, and a working hash specialization.
// refs: F-SRC-insight-canon:canon.api.cppm:TemplateId
#include <gtest/gtest.h>

import insight.canon.test;

using insight::ngram_id_of;
using insight::NgramId;
using insight::parse_template_id;
using insight::render;
using insight::template_id_of;
using insight::template_id_of_label;
using insight::TemplateId;
using insight::TemplateIdParseError;

namespace
{

// invariant: a spread of representative MASKED templates, of the kind the masker actually emits.
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
        const auto parsed{parse_template_id(render(id))};
        ASSERT_TRUE(parsed.has_value())
            << "the rendered form '" << render(id) << "' was refused, for: " << sample;
        EXPECT_EQ(*parsed, id) << "round-trip failed for: " << sample;
    }
}

// refs: ADR-16.D3
// invariant: the parser FAILS CLOSED — every input that is not "h:" plus exactly 32 lowercase hex
// digits is refused, with the reason, and never answered with a partial or defaulted id.
TEST(TemplateIdInvariants, ParseRefusesEveryMalformedForm)
{
    const std::string valid_hex{render(template_id_of("GET <*> -> <*>")).substr(2)};
    constexpr char kCaseOffset{'a' - 'A'};
    std::string upper_hex{valid_hex};
    std::ranges::transform(
        upper_hex, upper_hex.begin(), [](char chr)
        { return chr >= 'a' && chr <= 'f' ? static_cast<char>(chr - kCaseOffset) : chr; });
    ASSERT_NE(upper_hex, valid_hex) << "the sample carries no hex letter to upper-case";
    const std::vector<std::pair<std::string, TemplateIdParseError>> malformed{
        {"", TemplateIdParseError::MissingPrefix},
        {valid_hex, TemplateIdParseError::MissingPrefix},
        {"H:" + valid_hex, TemplateIdParseError::MissingPrefix},
        {" h:" + valid_hex, TemplateIdParseError::MissingPrefix},
        {"h:", TemplateIdParseError::WrongLength},
        {"h:abc", TemplateIdParseError::WrongLength},
        {"h:" + valid_hex.substr(1), TemplateIdParseError::WrongLength},
        {"h:" + valid_hex + "0", TemplateIdParseError::WrongLength},
        {"h:" + upper_hex, TemplateIdParseError::NotLowercaseHex},
        {"h:" + valid_hex.substr(0, 31) + "g", TemplateIdParseError::NotLowercaseHex},
        {"h:" + valid_hex.substr(0, 31) + " ", TemplateIdParseError::NotLowercaseHex},
    };
    for (const auto& [input, reason] : malformed)
    {
        const auto parsed{parse_template_id(input)};
        EXPECT_FALSE(parsed.has_value())
            << "parse_template_id accepted the malformed input '" << input << "' as the id '"
            << (parsed.has_value() ? render(*parsed) : std::string{}) << "'";
        if (!parsed.has_value())
        {
            EXPECT_EQ(parsed.error(), reason)
                << "'" << input << "' was refused as reason " << static_cast<int>(parsed.error())
                << ", expected " << static_cast<int>(reason);
        }
    }
}

// refs: ADR-16.D3
// invariant: the declared label constructor gives distinct short labels distinct ids — the fixture
// tolerance lives here, where a label that is not a rendered id is never parsed.
TEST(TemplateIdInvariants, LabelConstructorSeparatesShortLabels)
{
    const std::vector<std::string_view> labels{"h:abc", "h:abd", "h:ab", "h:", "abc"};
    std::unordered_map<TemplateId, std::string_view> by_id;
    for (const std::string_view label : labels)
    {
        EXPECT_EQ(template_id_of_label(label), template_id_of_label(label))
            << "the label constructor is not deterministic for '" << label << "'";
        const auto [slot, inserted]{by_id.try_emplace(template_id_of_label(label), label)};
        EXPECT_TRUE(inserted) << "the labels '" << slot->second << "' and '" << label
                              << "' collide on the id '" << render(slot->first) << "'";
    }
}

// invariant: THE GOLDEN-PRESERVER — byte-lexicographic order over the raw bytes reproduces the
// old rendered-string order EXACTLY.
// invariant: so every sort of these ids is byte-identical to the old sort of the rendered strings.
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

// invariant: the standard hash specialization must be reachable AND usable, which is the whole
// point of carrying a plain-data type.
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
    // invariant: distinct non-empty samples yield distinct ids, with the bound written to TOLERATE
    // any duplicate in the literal list rather than to assume there is none.
    EXPECT_GE(by_id.size(), kSamples.size() - 1U);
}

// invariant: the scalar key for n-gram SEQUENCES is TRANSIENT and never serialized, so its contract
// is in-memory keying only.
// invariant: deterministic, ORDER-SENSITIVE, distinct per distinct sequence, and usable in a hash
// container.
// refs: F-SRC-insight-canon:canon.api.cppm:NgramId
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
    // invariant: a sequence and its reverse are DISTINCT n-grams and take distinct ids, because
    // transition ORDER is significant.
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
        // invariant: the EMPTY sequence is well-defined — it is the basis, and it is distinct
        // from any non-empty one.
        seq({}),
    };
    std::unordered_map<NgramId, std::size_t> by_id;
    for (std::size_t i{0}; i < sequences.size(); ++i)
        by_id[ngram_id_of(sequences[i])] = i;
    EXPECT_EQ(by_id.size(), sequences.size()) << "ngram_id collision across distinct sequences";
}
