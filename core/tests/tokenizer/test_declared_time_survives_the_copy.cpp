// NOLINTBEGIN — Unit tests: allow short identifiers and test-specific patterns
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

// Provenance survives the ParsedLine → CanonicalEvent copy (DN-29.D18).
//
// HOMING — the TOKENIZER grain, and this is a correction to my own earlier call. The property is
// not about `ParsedLine`, where `EventTime` already makes it hard to get wrong; it is about the
// copy into `CanonicalEvent`, which represents the same fact as two fields (`Timestamp timestamp` +
// `bool declared_timestamp`) because absence changes representation across that boundary — the
// optional becomes the `Timestamp{}` sentinel that `resolve_event_time` keys on. That asymmetry is
// a DELIBERATE boundary, not a hole: pushing `EventTime` across would drag the optional into every
// downstream consumer and retire the sentinel contract, or leave `EventTime` with two
// representations of absence, which is worse than two types with one each.
//
// So the arm belongs where the copy happens: `Tokenizer::process_line`, which returns the
// CanonicalEvent. It does NOT belong in the seam suite — LogCraft supplies no fact these tests
// cannot state themselves, and the test for a seam is "does the property need a fact only the
// other instrument can supply", never "does it cross a type boundary".
//
// THE PAIR IS THE PROPERTY, NEVER THE FLAG. Asserting `declared_timestamp == true` alone passes
// with a wrong timestamp sitting beside it — the two fields are exactly what can drift apart, so
// an arm reading only one of them is blind to the drift it exists to catch
// (MEM:synthetic-gate-vacuity-vs-judgment: ask what ELSE would make this pass). Every case below
// asserts the flag AND the value together.
//
// Determinism: literal lines, one arena per case, no wall clock, no RNG.
namespace
{

constexpr std::int64_t kDeclaredUnixSeconds{1'777'024'800};
constexpr std::int64_t kParsedUnixSeconds{1'777'039'200};

[[nodiscard]] Timestamp stamp_at(std::int64_t unix_seconds)
{
    return Timestamp{} + std::chrono::seconds{unix_seconds};
}

// An OTLP LOG RECORD — deliberately not a span. A rule that re-derived provenance from `is_span`
// would leave the span case green and red only here, which is the red-there/green-here signature
// that names the defect instead of merely reporting one.
[[nodiscard]] std::string otel_log_record(std::int64_t unix_seconds)
{
    const std::int64_t nanos{unix_seconds * 1'000'000'000LL};
    return R"({"timeUnixNano":")" + std::to_string(nanos) +
           R"(","severityNumber":17,"severityText":"ERROR",)"
           R"("body":{"stringValue":"connection pool exhausted"}})";
}

// A flat OTLP SPAN carrying the same declared time, so the two can be compared directly.
[[nodiscard]] std::string otel_span(std::int64_t unix_seconds)
{
    const std::int64_t nanos{unix_seconds * 1'000'000'000LL};
    return R"({"traceId":"aabb","spanId":"0001","name":"checkout","kind":"SPAN_KIND_INTERNAL",)"
           R"("startTimeUnixNano":")" +
           std::to_string(nanos) + R"(","endTimeUnixNano":")" + std::to_string(nanos + 500) +
           R"(","status":{"code":"STATUS_CODE_UNSET"},"attributes":[]})";
}

// Build a tokenizer with the degenerate (zero-package) composition — the universal formats are
// semantic-unaware, and `composed` must outlive the const-ref the Tokenizer holds, so both live in
// the caller's scope via this small fixture.
struct TokenizerFixture
{
    static constexpr std::size_t kArenaSize{1U << 20U};
    ArenaAllocator arena{kArenaSize};
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed};
};

} // namespace

// ── ARM 3 — a declared time crosses the copy AS declared, with its value intact ───────────────

TEST(DeclaredTimeCopy, AnOtelLogRecordIsMarkedDeclaredAndKeepsItsDeclaredValue)
{
    TokenizerFixture fx;
    const auto event{fx.tokenizer.process_line(otel_log_record(kDeclaredUnixSeconds))};
    ASSERT_TRUE(event.has_value()) << "the OTLP log record did not parse at all";

    // The PAIR. Either half alone is satisfiable while the other is wrong.
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

    // Stated as agreement between the two shapes rather than as two separate constants: the
    // property is that `is_span` does not decide provenance, and an equality cannot drift.
    EXPECT_EQ(record->declared_timestamp, span->declared_timestamp)
        << "a span and a log record carrying the SAME declared time disagree about provenance "
           "(span="
        << span->declared_timestamp << ", record=" << record->declared_timestamp
        << "). Provenance is being re-derived from `is_span` rather than carried from the parse — "
           "which makes every non-span declared time silently parsed.";
    EXPECT_EQ(record->timestamp, span->timestamp)
        << "the two shapes resolved different times from the same declared value";
}

// ── ARM 5 — the OPPOSITE failure: provenance set TOO EAGERLY ──────────────────────────────────
//
// Arm 3 catches a site that FORGETS the flag. This catches one that sets it always — which is
// invisible to arm 3, because arm 3 only ever looks at inputs that SHOULD be declared. A blanket
// `declared_timestamp = true` would pass every case above and silently promote a printf stamp to
// rung 1, where it would outrank the delivery stamp that ADR-23 says must win.
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

    // ...and the pair again, from the other side: "not declared" must not be achieved by losing
    // the time. The sentinel is load-bearing (resolve_event_time keys on != Timestamp{}), so a
    // zero here would silently drop this line to forward-fill.
    EXPECT_NE(event->timestamp, Timestamp{})
        << "the parsed time crossed as the ZERO sentinel, so the record reads as having no "
           "parseable time at all and falls to forward-fill — the value was lost in the copy even "
           "though the provenance was right";
}

// NOLINTEND
