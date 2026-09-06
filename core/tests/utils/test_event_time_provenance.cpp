#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

// invariant: provenance is part of the VALUE, not a flag beside it, so a declared-but-absent time
// is not a state anyone can reach.
// invariant: that is a claim about what the type ADMITS, so it is asserted at COMPILE time — a
// runtime check on a default-constructed value tests only what the default happens to be.
// invariant: no negative-compilation harness is needed: a type-trait assertion states
// inexpressibility directly, and it cannot be skipped or filtered.
// refs: DN-29.D14
namespace
{

// invariant: an implicit conversion from an optional timestamp would make an assignment compile and
// silently mean PARSED, which is the hole the ruling names.
static_assert(!std::is_convertible_v<std::optional<Timestamp>, EventTime>,
              "EventTime is implicitly constructible from optional<Timestamp> — an assignment can "
              "now silently mean PARSED without naming its provenance, which is the exact hole "
              "DN-29.D14 closes");

// invariant: the same hole one step over — a bare timestamp must not slide in either, or a
// declared-time field read could be assigned without saying so.
static_assert(!std::is_convertible_v<Timestamp, EventTime>,
              "EventTime is implicitly constructible from a bare Timestamp — provenance can be "
              "omitted at an assignment site");

// invariant: PROVENANCE WITHOUT A TIME IS UNREACHABLE — the declared factory takes a timestamp BY
// VALUE, so there is no way to declare an absent one.
// invariant: asserting on the factory's SIGNATURE is what makes that structural rather than
// conventional.
static_assert(!std::is_invocable_v<decltype(&EventTime::declared), std::nullopt_t>,
              "EventTime::declared accepts nullopt — a value can be marked declared while carrying "
              "no time, which is the state the ruling says must not exist");
static_assert(!std::is_invocable_v<decltype(&EventTime::declared), std::optional<Timestamp>>,
              "EventTime::declared accepts an optional — same defect, reached through the type the "
              "parsed factory takes");

// invariant: the two factories ARE the only ways in, so the reachable state set is exactly
// empty-parsed, valued-parsed and valued-declared.
// invariant: if a third entry point appears, this assertion reads FALSE.
static_assert(std::is_invocable_v<decltype(&EventTime::parsed), std::optional<Timestamp>>,
              "EventTime::parsed no longer accepts optional<Timestamp> — the parsed rung cannot "
              "express an absent time, which every strategy needs");
static_assert(std::is_invocable_v<decltype(&EventTime::declared), Timestamp>,
              "EventTime::declared no longer accepts a Timestamp");

} // namespace

// invariant: the runtime companion fixes the DEFAULT, which the type traits cannot — a field that
// is simply never assigned must not read as a declared time.
// note: it is deliberately NOT a substitute for the assertions above.
TEST(EventTimeProvenance, ADefaultConstructedValueIsEmptyAndUndeclared)
{
    const EventTime unset;
    EXPECT_FALSE(unset.has_value())
        << "a default-constructed EventTime carries a time — an unassigned field would supply one";
    EXPECT_FALSE(unset.is_declared())
        << "a default-constructed EventTime reports DECLARED provenance. Every site that forgets "
           "to assign would then outrank a transport stamp on the DN-29.D12 ladder, which inverts "
           "the ruling exactly where it is least visible";
}

// invariant: the two rungs are distinguishable AT THE VALUE, which is the property every ladder
// site reads; stated over both factories so a change collapsing them cannot pass.
TEST(EventTimeProvenance, TheTwoRungsAreDistinguishableAndCarryTheSameTime)
{
    const auto when{Timestamp{} + std::chrono::seconds{1'777'024'800}};

    const EventTime declared{EventTime::declared(when)};
    const EventTime parsed{EventTime::parsed(when)};

    ASSERT_TRUE(declared.has_value());
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*declared, *parsed) << "the two rungs disagree about the TIME; they must differ only "
                                     "in provenance, or a ladder site changes the value it "
                                     "resolves as a side effect of ranking it";
    EXPECT_TRUE(declared.is_declared());
    EXPECT_FALSE(parsed.is_declared())
        << "a parsed time reports declared provenance — it would outrank a transport stamp, which "
           "is precisely the ADR-23 rule DN-29.D12 did NOT overturn";
}

// invariant: the empty rung stays empty AND undeclared, which the forward-fill and sentinel steps
// below the ladder depend on.
TEST(EventTimeProvenance, AnAbsentParsedTimeIsNeitherPresentNorDeclared)
{
    const EventTime none{EventTime::parsed(std::nullopt)};
    EXPECT_FALSE(none.has_value());
    EXPECT_FALSE(none.is_declared())
        << "an EMPTY event time claims declared provenance — rung 1 would then win with no value "
           "to contribute, and the transport stamp that should have applied is discarded";
}
