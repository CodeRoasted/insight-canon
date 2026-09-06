#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

// invariant: provenance survives the copy from the parsed line into the canonical event.
// invariant: HOMED at the TOKENIZER grain, and that is a correction to an earlier call — the
// property is not about the parsed line, where the event-time type makes it hard to get wrong.
// invariant: it is about the COPY, which represents the same fact as two fields, a timestamp and a
// boolean, because ABSENCE changes representation across that boundary.
// invariant: the optional becomes the zero sentinel that event-time resolution keys on.
// invariant: that asymmetry is a DELIBERATE boundary and not a hole — pushing the richer type
// across would drag the optional into every downstream consumer and retire the sentinel.
// invariant: the alternative leaves the type with TWO representations of absence, which is worse
// than two types with one each.
// invariant: so the arm belongs where the copy happens, and it does NOT belong in the seam suite.
// invariant: the test for a seam is whether the property needs a fact only the other instrument can
// supply, NEVER whether it crosses a type boundary.
// invariant: THE PAIR IS THE PROPERTY, NEVER THE FLAG — asserting the flag alone passes with a
// WRONG timestamp sitting beside it.
// invariant: the two fields are exactly what can drift apart, so an arm reading only one is blind
// to the drift it exists to catch; every case below asserts the flag AND the value together.
// invariant: determinism — literal lines, one arena per case, no wall clock, no RNG.
// refs: DN-29.D18, MEM:synthetic-gate-vacuity-vs-judgment
namespace
{

constexpr std::int64_t kDeclaredUnixSeconds{1'777'024'800};
constexpr std::int64_t kParsedUnixSeconds{1'777'039'200};

[[nodiscard]] Timestamp stamp_at(std::int64_t unix_seconds)
{
    return Timestamp{} + std::chrono::seconds{unix_seconds};
}

// invariant: an OTLP LOG RECORD, deliberately NOT a span — a rule that re-derived provenance from
// the span predicate would leave the span case green and red only here.
// invariant: that red-there and green-here signature NAMES the defect instead of merely reporting
// one.
[[nodiscard]] std::string otel_log_record(std::int64_t unix_seconds)
{
    const std::int64_t nanos{unix_seconds * 1'000'000'000LL};
    return R"({"timeUnixNano":")" + std::to_string(nanos) +
           R"(","severityNumber":17,"severityText":"ERROR",)"
           R"("body":{"stringValue":"connection pool exhausted"}})";
}

// invariant: a flat OTLP SPAN carrying the same declared time, so the two can be compared directly.
[[nodiscard]] std::string otel_span(std::int64_t unix_seconds)
{
    const std::int64_t nanos{unix_seconds * 1'000'000'000LL};
    return R"({"traceId":"aabb","spanId":"0001","name":"checkout","kind":"SPAN_KIND_INTERNAL",)"
           R"("startTimeUnixNano":")" +
           std::to_string(nanos) + R"(","endTimeUnixNano":")" + std::to_string(nanos + 500) +
           R"(","status":{"code":"STATUS_CODE_UNSET"},"attributes":[]})";
}

// invariant: the degenerate zero-package composition, because the universal formats are
// semantic-unaware.
// invariant: the composition must OUTLIVE the const-ref the tokenizer holds, so both live in the
// caller's scope through this fixture.
struct TokenizerFixture
{
    static constexpr std::size_t kArenaSize{1U << 20U};
    ArenaAllocator arena{kArenaSize};
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed};
};

} // namespace

TEST(DeclaredTimeCopy, AnOtelLogRecordIsMarkedDeclaredAndKeepsItsDeclaredValue)
{
    TokenizerFixture fx;
    const auto event{fx.tokenizer.process_line(otel_log_record(kDeclaredUnixSeconds))};
    ASSERT_TRUE(event.has_value()) << "the OTLP log record did not parse at all";

    // invariant: THE PAIR — either half alone is satisfiable while the other is wrong.
    EXPECT_TRUE(event->declared_timestamp)
        << "an OTLP log record crossed into CanonicalEvent WITHOUT its declared provenance. "
           "`timeUnixNano` is the schema's statement of when the event happened, so the ladder "
           "must rank it above a transport stamp — unmarked, it silently drops to rung 3 and the "
           "delivery stamp overwrites it (DN-29.D12).";
    EXPECT_EQ(event->timestamp, stamp_at(kDeclaredUnixSeconds))
        << "marked declared, but the VALUE that crossed is not the declared one. The flag and the "
           "time are separate fields on CanonicalEvent and this is exactly the drift that makes "
           "them separable — a flag asserted alone would have passed here.";
}

TEST(DeclaredTimeCopy, ASpanAndALogRecordAgreeSoProvenanceIsNotDerivedFromIsSpan)
{
    TokenizerFixture fx;
    const auto record{fx.tokenizer.process_line(otel_log_record(kDeclaredUnixSeconds))};
    const auto span{fx.tokenizer.process_line(otel_span(kDeclaredUnixSeconds))};
    ASSERT_TRUE(record.has_value());
    ASSERT_TRUE(span.has_value());

    // invariant: stated as AGREEMENT between the two shapes rather than as two separate constants
    // — the property is that the span predicate does not decide provenance.
    // invariant: an equality cannot drift the way two hand-written expectations can.
    EXPECT_EQ(record->declared_timestamp, span->declared_timestamp)
        << "a span and a log record carrying the SAME declared time disagree about provenance "
           "(span="
        << span->declared_timestamp << ", record=" << record->declared_timestamp
        << "). Provenance is being re-derived from `is_span` rather than carried from the parse — "
           "which makes every non-span declared time silently parsed.";
    EXPECT_EQ(record->timestamp, span->timestamp)
        << "the two shapes resolved different times from the same declared value";
}

// invariant: the OPPOSITE failure — provenance set TOO EAGERLY.
// invariant: the arm above catches a site that FORGETS the flag; this catches one that sets it
// always, which is invisible there because that arm only looks at inputs that SHOULD be declared.
// invariant: a blanket assignment would pass every case above and silently promote a printf stamp
// to the top rung, where it would outrank the delivery stamp that must win.
// refs: ADR-23
TEST(DeclaredTimeCopy, AParsedTimestampCrossesAsPARSEDAndStillCarriesItsTime)
{
    TokenizerFixture fx;
    const std::string line{R"({"ts":"2026-04-24T12:00:00Z","level":"INFO",)"
                           R"("component":"payments","message":"GET /health -> 200"})"};

    const auto event{fx.tokenizer.process_line(line)};
    ASSERT_TRUE(event.has_value()) << "the plain JSON record did not parse";

    EXPECT_FALSE(event->declared_timestamp)
        << "a STRATEGY-PARSED timestamp crossed into CanonicalEvent marked DECLARED. `ts` is "
           "applicative content of ambiguous authorship — exactly what ADR-23 ranks BELOW the "
           "delivery stamp. Marked declared it climbs to rung 1 and outranks the transport stamp, "
           "inverting the rule DN-29.D12 explicitly did NOT overturn.\n"
           "    A blanket `declared_timestamp = true` passes every declared-input case; this is "
           "the only arm that sees it.";

    // invariant: the pair again, from the other side — NOT DECLARED must not be achieved by
    // losing the time.
    // invariant: the sentinel is load-bearing, because event-time resolution keys on a non-zero
    // value, so a zero here would silently drop this line to forward-fill.
    EXPECT_NE(event->timestamp, Timestamp{})
        << "the parsed time crossed as the ZERO sentinel, so the record reads as having no "
           "parseable time at all and falls to forward-fill — the value was lost in the copy even "
           "though the provenance was right";
}
