// NOLINTBEGIN — Unit tests: allow short identifiers and test-specific patterns
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

// EventTime provenance (DN-29.D14) — the one-site clause asserted as a TYPE property.
//
// The ruling is that provenance is part of the value, not a flag beside it, so a declared-but-
// absent time is not a state anyone can reach. That is a claim about what the type ADMITS, and the
// honest way to assert it is at compile time: a runtime check on a default-constructed value tests
// only that the default happens to be undeclared, which is a strictly weaker claim than "no such
// value can be built".
//
// These need no negative-compilation harness. `static_assert` over type traits states
// inexpressibility directly — if any of these ever becomes constructible, the file stops compiling,
// which is a louder failure than a red test and cannot be skipped or filtered.
namespace
{

// (1) THE HOLE THE RULING NAMES. An implicit conversion from `std::optional<Timestamp>` would make
// `timestamp = parse_unix_nano(...)` compile and silently mean PARSED. The type must refuse it.
static_assert(!std::is_convertible_v<std::optional<Timestamp>, EventTime>,
              "EventTime is implicitly constructible from optional<Timestamp> — an assignment can "
              "now silently mean PARSED without naming its provenance, which is the exact hole "
              "DN-29.D14 closes");

// (2) The same hole one step over: a bare Timestamp must not slide in either, or a declared-time
// field read could be assigned without saying so.
static_assert(!std::is_convertible_v<Timestamp, EventTime>,
              "EventTime is implicitly constructible from a bare Timestamp — provenance can be "
              "omitted at an assignment site");

// (3) PROVENANCE WITHOUT A TIME IS UNREACHABLE. `declared` takes a Timestamp BY VALUE, not an
// optional, so there is no way to declare an absent time. Asserting on the factory's signature is
// what makes that structural rather than conventional.
static_assert(!std::is_invocable_v<decltype(&EventTime::declared), std::nullopt_t>,
              "EventTime::declared accepts nullopt — a value can be marked declared while carrying "
              "no time, which is the state the ruling says must not exist");
static_assert(!std::is_invocable_v<decltype(&EventTime::declared), std::optional<Timestamp>>,
              "EventTime::declared accepts an optional — same defect, reached through the type the "
              "parsed factory takes");

// (4) The two factories ARE the only ways in, so the set of reachable states is exactly
// {empty-parsed, valued-parsed, valued-declared}. If a third entry point appears, this reads false.
static_assert(std::is_invocable_v<decltype(&EventTime::parsed), std::optional<Timestamp>>,
              "EventTime::parsed no longer accepts optional<Timestamp> — the parsed rung cannot "
              "express an absent time, which every strategy needs");
static_assert(std::is_invocable_v<decltype(&EventTime::declared), Timestamp>,
              "EventTime::declared no longer accepts a Timestamp");

} // namespace

// The runtime companion — deliberately NOT a substitute for the asserts above, and it says so.
// It fixes the DEFAULT, which the type traits cannot: a default-constructed EventTime must be both
// empty and undeclared, so a field that is simply never assigned cannot read as a declared time.
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

// The two rungs are distinguishable at the value, which is the property every ladder site reads.
// Stated over both factories rather than one, so a change that collapses them cannot pass.
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

// An absent parsed time stays absent AND undeclared — the empty rung, which the forward-fill and
// sentinel steps below the ladder depend on being honestly empty.
TEST(EventTimeProvenance, AnAbsentParsedTimeIsNeitherPresentNorDeclared)
{
    const EventTime none{EventTime::parsed(std::nullopt)};
    EXPECT_FALSE(none.has_value());
    EXPECT_FALSE(none.is_declared())
        << "an EMPTY event time claims declared provenance — rung 1 would then win with no value "
           "to contribute, and the transport stamp that should have applied is discarded";
}

// NOLINTEND
