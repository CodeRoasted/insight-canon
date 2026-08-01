// NOLINTBEGIN — Unit tests: allow short identifiers and test-specific patterns
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

// OTEL span-export DOCUMENT unpack (SRC-D-OTEL-18 / SRC-D-OTEL-18a): one OTLP
// `resourceSpans` export (shape 1) is unpacked into N CANONICAL flat-span records (shape 2) —
// byte-form-identical to the lab's emission, so the flat-span parser is authored once and
// shape-1 ≡ shape-2 is golden-tested (the two-paths-drift bug class dies here). resource
// service.name is injected; int-form kind/status are normalized to the protojson string enum.
namespace
{

// A two-span export under one resource. Span 1: string kind/status, a real span attribute,
// root (no parent). Span 2: INT kind/status (real-collector form), a parent, empty attributes.
constexpr std::string_view kDocument{
    R"({"resourceSpans":[{"resource":{"attributes":[{"key":"service.name","value":{"stringValue":"checkout-svc"}}]},)"
    R"("scopeSpans":[{"spans":[)"
    R"({"traceId":"aabb","spanId":"0001","name":"checkout","kind":"SPAN_KIND_INTERNAL",)"
    R"("startTimeUnixNano":"1000","endTimeUnixNano":"1500","status":{"code":"STATUS_CODE_UNSET"},)"
    R"("attributes":[{"key":"http.method","value":{"stringValue":"GET"}}]},)"
    R"({"traceId":"aabb","spanId":"0002","parentSpanId":"0001","name":"db_query","kind":2,)"
    R"("startTimeUnixNano":"1100","endTimeUnixNano":"1400","status":{"code":2},"attributes":[]})"
    R"(]}]}]})"};

// The canonical flat-span records the unpack MUST emit — the exact bytes the LogCraft lab emits
// for the same spans (fixed field order; service.name injected first, then span attributes
// verbatim; int kind/status normalized). Span 1 (root) omits parentSpanId; span 2 carries it.
constexpr std::string_view kExpectedSpan0{
    R"({"traceId":"aabb","spanId":"0001","name":"checkout","kind":"SPAN_KIND_INTERNAL",)"
    R"("startTimeUnixNano":"1000","endTimeUnixNano":"1500","status":{"code":"STATUS_CODE_UNSET"},)"
    R"("attributes":[{"key":"service.name","value":{"stringValue":"checkout-svc"}},)"
    R"({"key":"http.method","value":{"stringValue":"GET"}}]})"};

constexpr std::string_view kExpectedSpan1{
    R"({"traceId":"aabb","spanId":"0002","parentSpanId":"0001","name":"db_query","kind":"SPAN_KIND_SERVER",)"
    R"("startTimeUnixNano":"1100","endTimeUnixNano":"1400","status":{"code":"STATUS_CODE_ERROR"},)"
    R"("attributes":[{"key":"service.name","value":{"stringValue":"checkout-svc"}}]})"};

} // namespace

TEST(SpanUnpack, DetectsDocumentNotFlatSpan)
{
    EXPECT_TRUE(is_otel_span_document(kDocument));
    // A flat span (shape 2) is NOT a document — it takes the direct parser path.
    EXPECT_FALSE(is_otel_span_document(
        R"({"traceId":"aa","spanId":"bb","name":"x","startTimeUnixNano":"1"})"));
    EXPECT_FALSE(is_otel_span_document(R"({"level":"info","message":"hi"})"));
}

TEST(SpanUnpack, UnpacksDocumentToByteIdenticalCanonicalRecords)
{
    std::vector<std::string> records;
    const std::size_t count{unpack_otel_spans(kDocument, records)};
    ASSERT_EQ(count, 2U);
    ASSERT_EQ(records.size(), 2U);
    // Byte-identical to the lab's flat-span emission (the shape-1 ≡ shape-2 property, SRC-D-OTEL-18a).
    EXPECT_EQ(records[0], kExpectedSpan0) << "got: " << records[0];
    EXPECT_EQ(records[1], kExpectedSpan1) << "got: " << records[1];
}

TEST(SpanUnpack, NonDocumentYieldsNothing)
{
    std::vector<std::string> records;
    EXPECT_EQ(unpack_otel_spans(R"({"level":"info","message":"hi"})", records), 0U);
    EXPECT_TRUE(records.empty());
}

// O4b Span Links (SRC-D-OTEL-9 / SRC-D-OTEL-23): a crafted flat OTLP span declaring two cross-trace
// link targets — the canon-grain unit guard for the lab's links[] emission. Canon collects each
// link's spanId into linked_span_ids in order; the link's traceId (and any link attributes) are
// consumed-not-retained (only the span_id feeds metalog's cross-trace service distillation).
constexpr std::string_view kSpanWithLinks{
    R"({"traceId":"aabb","spanId":"0001","name":"consumer","kind":"SPAN_KIND_INTERNAL",)"
    R"("startTimeUnixNano":"1000","endTimeUnixNano":"1500","status":{"code":"STATUS_CODE_UNSET"},)"
    R"("links":[{"traceId":"ccdd","spanId":"00a1"},{"traceId":"eeff","spanId":"00b2"}],)"
    R"("attributes":[{"key":"service.name","value":{"stringValue":"consumer-svc"}}]})"};

TEST(SpanUnpack, FlatSpanLinksPopulateLinkedSpanIdsInOrder)
{
    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto span{strategy.parse(kSpanWithLinks, arena)};
    ASSERT_TRUE(span.has_value());
    ASSERT_EQ(span->linked_span_ids.size(), 2U)
        << "expected 2 link targets, got " << span->linked_span_ids.size();
    EXPECT_EQ(span->linked_span_ids[0], span_id_from_hex("00a1"));
    EXPECT_EQ(span->linked_span_ids[1], span_id_from_hex("00b2"));
    // The link's traceId does not disturb the span's own trace context (own spanId retained).
    EXPECT_EQ(span->trace.span_id, span_id_from_hex("0001"));
}

// A span without a links[] array carries no cross-trace edges — empty in ⇒ empty out
// (store_span_ids allocates nothing), so a non-linking span stays byte-identical to pre-links.
TEST(SpanUnpack, FlatSpanWithoutLinksHasEmptyLinkedSpanIds)
{
    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto span{strategy.parse(kExpectedSpan0, arena)};
    ASSERT_TRUE(span.has_value());
    EXPECT_TRUE(span->linked_span_ids.empty());
}

// The de-risk: every unpacked record parses through the SAME flat-span parser (2a) into the
// CanonicalEvent it should — closing the loop shape-1 → shape-2 → event.
TEST(SpanUnpack, UnpackedRecordsRoundTripThroughTheFlatSpanParser)
{
    std::vector<std::string> records;
    ASSERT_EQ(unpack_otel_spans(kDocument, records), 2U);

    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto span0{strategy.parse(records[0], arena)};
    ASSERT_TRUE(span0.has_value());
    EXPECT_EQ(span0->content, "checkout");
    EXPECT_EQ(span0->component, "checkout-svc"); // resource service.name → component
    EXPECT_EQ(span0->level, LogLevel::Info);     // STATUS_CODE_UNSET → Info
    EXPECT_FALSE(span0->trace.has_parent);
    ASSERT_EQ(span0->ordinals.size(), 1U);
    EXPECT_EQ(span0->ordinals[0].value, 500); // 1500 − 1000

    const auto span1{strategy.parse(records[1], arena)};
    ASSERT_TRUE(span1.has_value());
    EXPECT_EQ(span1->content, "db_query");
    EXPECT_EQ(span1->component, "checkout-svc");
    EXPECT_EQ(span1->level, LogLevel::Error); // int status 2 normalized → ERROR → Error
    EXPECT_TRUE(span1->trace.has_parent);
    EXPECT_EQ(span1->trace.parent_span_id, span_id_from_hex("0001"));
    ASSERT_EQ(span1->ordinals.size(), 1U);
    EXPECT_EQ(span1->ordinals[0].value, 300); // 1400 − 1100
}

// NOLINTEND
