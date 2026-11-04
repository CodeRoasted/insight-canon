// refs: ADR-20.D12, ADR-16.D13
// invariant: the arming control of `ADR-20.D12` gate 2 half one's render-delta census — the
// rendered BYTES, a pure function of the NAMES — over synthetic names, so it needs no mount.
// note: the bank arm is retired (N350 R6); its numbers: 1.10.2/gd_gate2_render_delta
// invariant: no RNG, no thread, no wall clock, no float — every case is a literal
#include <gtest/gtest.h>

import std;
import insight.canon;

using insight::canonicalize_intent;
using insight::discriminant_of;
using insight::trimmed_intent_name;

namespace
{

// note: how many producer names already contain the separator is READABILITY, not correctness
constexpr std::string_view kPhaseSeparator{" ▸ "};

// invariant: each flag is a property of the RENDERED bytes, so a hazard the CLASS already carries
// is on the wire today and is reported beside it rather than charged to the ruling.
struct DisplayRisk
{
    bool non_ascii{false};
    bool interior_control{false};
    bool carries_separator{false};
    bool carries_quote{false};

    [[nodiscard]] bool any() const noexcept
    {
        return non_ascii || interior_control || carries_separator || carries_quote;
    }
};

[[nodiscard]] DisplayRisk risk_of(std::string_view rendered)
{
    DisplayRisk out;
    for (const char byte : rendered)
    {
        const auto raw{static_cast<unsigned char>(byte)};
        if (raw >= 0x80U)
            out.non_ascii = true;
        if (raw < 0x20U || raw == 0x7FU)
            out.interior_control = true;
        if (raw == static_cast<unsigned char>('"'))
            out.carries_quote = true;
    }
    out.carries_separator = rendered.find(kPhaseSeparator) != std::string_view::npos;
    return out;
}

// post: every byte outside printable ASCII is escaped, so a control byte in a payload cannot
// corrupt the census output it is reported in.
[[nodiscard]] std::string escaped(std::string_view text)
{
    static constexpr std::string_view kHex{"0123456789abcdef"};
    std::string out;
    for (const char byte : text)
    {
        const auto raw{static_cast<unsigned char>(byte)};
        if (raw >= 0x20U && raw < 0x7FU)
        {
            out.push_back(byte);
            continue;
        }
        out += "\\x";
        out.push_back(kHex[raw >> 4U]);
        out.push_back(kHex[raw & 0x0FU]);
    }
    return out;
}

[[nodiscard]] bool risk_introduced(std::string_view name)
{
    return risk_of(trimmed_intent_name(name)).any() && !risk_of(canonicalize_intent(name)).any();
}

} // namespace

// refs: MEM:synthetic-gate-vacuity-vs-judgment
// invariant: both detectors are exercised in BOTH directions here — a case that must fire and a
// case that must not — because a census whose detectors cannot fire reports meaningless zeros.
TEST(MaskedSpanCensusControl, TheRenderDeltaPredicateAndTheRiskDetectorBothFireAndBothStaySilent)
{
    struct DeltaCase
    {
        std::string_view name;
        bool moves;
        std::string_view why;
    };
    static constexpr std::array<DeltaCase, 6> kDeltaCases{{
        {"macos-14 (15.3)", true, "a runner-matrix cell: two masked spans, both rewritten"},
        {"ESLint v6", true, "one masked span, rewritten"},
        {"Lint", false, "no masked span at all — nothing to rewrite"},
        {"Build", false, "no masked span at all — nothing to rewrite"},
        {"Test (M)", false, "carries a masked span whose mask reproduces its own bytes"},
        {"  Lint\t", false, "differs from its class only by the trim, which clause 1 also applies"},
    }};
    for (const auto& [name, moves, why] : kDeltaCases)
    {
        const bool observed{trimmed_intent_name(name) != canonicalize_intent(name)};
        EXPECT_EQ(observed, moves)
            << "the render-delta predicate answered " << (observed ? "MOVES" : "unchanged")
            << " for \"" << name << "\" and the case says " << (moves ? "MOVES" : "unchanged")
            << " — " << why << ".\n  trimmed name : \"" << escaped(trimmed_intent_name(name))
            << "\"\n  class today  : \"" << escaped(canonicalize_intent(name)) << "\"";
    }
    EXPECT_FALSE(discriminant_of("Test (M)").empty())
        << "\"Test (M)\" no longer carries a masked span, so it is no longer the case that "
           "separates the render-delta predicate from a span count, and the comment above it is "
           "false.";

    EXPECT_FALSE(risk_of("Build and test (ubuntu-latest)").any())
        << "the risk detector fires on an ordinary ASCII job name, so every at-risk count below "
           "is noise.";
    EXPECT_TRUE(risk_of("Tests \xE2\x9C\x93").non_ascii) << "the non-ASCII flag does not fire";
    EXPECT_TRUE(risk_of("build\rtest").interior_control)
        << "the interior-control flag does not fire on a CR between two words — a CR a name holds "
           "is content, since canon removes a line's ending before any name exists (ADR-21.D6).";
    EXPECT_FALSE(risk_of(trimmed_intent_name("build \t")).interior_control)
        << "the interior-control flag fires on a tab the trim already removed, so it would charge "
           "`ADR-20.D12` clause 1 for a byte clause 1 never renders.";
    EXPECT_TRUE(
        risk_of(std::string{"build"} + std::string{kPhaseSeparator} + "test").carries_separator)
        << "the separator flag does not fire";
    EXPECT_TRUE(risk_of("run \"the suite\"").carries_quote) << "the quote flag does not fire";

    EXPECT_FALSE(risk_introduced("run \"the suite\""))
        << "a double quote that survives masking is on the wire today — the class is \""
        << canonicalize_intent("run \"the suite\"")
        << "\" — so clause 1 introduces nothing and must not be charged for it.";
    EXPECT_TRUE(risk_introduced("deploy (\"eu-west-1\")"))
        << "a quote that the paren mask REMOVES from the class — class \""
        << canonicalize_intent("deploy (\"eu-west-1\")")
        << "\" — is a hazard clause 1 puts back in front of a reader, and the split must see it.";
}
