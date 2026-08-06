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
// root (no parent), NO links. Span 2: INT kind/status (real-collector form), a parent, empty
// attributes, and TWO cross-trace links.
//
// Span 2 carries the links deliberately, and its `links[]` sits between `parentSpanId` and `name`
// — the lab's own field order (log_formatter_cloud.cpp `fmt_otel_span`). That is the
// ordering-sensitive position, so a re-serializer that carries links but appends them in the wrong
// place still fails this golden. Span 1 stays link-free in the SAME document so the
// empty ⇒ nothing-written property (a non-linking span is byte-identical to pre-links) keeps its
// coverage here rather than needing a second fixture.
//
// WHY THE LINKS ARE HERE AT ALL (DN-29.D7). Before this, the fixture had no links and the
// byte-equivalence golden went green while `append_canonical_span` silently dropped every Span
// Link — `MEM:synthetic-gate-vacuity-vs-judgment`'s green-BLIND row: the gate could not fail on
// the one field the whole OTEL subject was minted for (ADR-29.D1/D2 — the Span Link is the
// declared cross-trace Régime-B edge). A fixture that omits the load-bearing field turns an
// equivalence assertion into a tautology.
constexpr std::string_view kDocument{
    R"({"resourceSpans":[{"resource":{"attributes":[{"key":"service.name","value":{"stringValue":"checkout-svc"}}]},)"
    R"("scopeSpans":[{"spans":[)"
    R"({"traceId":"aabb","spanId":"0001","name":"checkout","kind":"SPAN_KIND_INTERNAL",)"
    R"("startTimeUnixNano":"1000","endTimeUnixNano":"1500","status":{"code":"STATUS_CODE_UNSET"},)"
    R"("attributes":[{"key":"http.method","value":{"stringValue":"GET"}}]},)"
    R"({"traceId":"aabb","spanId":"0002","parentSpanId":"0001",)"
    R"("links":[{"traceId":"ccdd","spanId":"00a1"},{"traceId":"eeff","spanId":"00b2"}],)"
    R"("name":"db_query","kind":2,)"
    R"("startTimeUnixNano":"1100","endTimeUnixNano":"1400","status":{"code":2},"attributes":[]})"
    R"(]}]}]})"};

// The canonical flat-span records the unpack MUST emit — the exact bytes the LogCraft lab emits
// for the same spans (fixed field order; service.name injected first, then span attributes
// verbatim; int kind/status normalized). Span 1 (root) omits parentSpanId and links; span 2
// carries both, in the lab's order: parentSpanId, links, name.
constexpr std::string_view kExpectedSpan0{
    R"({"traceId":"aabb","spanId":"0001","name":"checkout","kind":"SPAN_KIND_INTERNAL",)"
    R"("startTimeUnixNano":"1000","endTimeUnixNano":"1500","status":{"code":"STATUS_CODE_UNSET"},)"
    R"("attributes":[{"key":"service.name","value":{"stringValue":"checkout-svc"}},)"
    R"({"key":"http.method","value":{"stringValue":"GET"}}]})"};

constexpr std::string_view kExpectedSpan1{
    R"({"traceId":"aabb","spanId":"0002","parentSpanId":"0001",)"
    R"("links":[{"traceId":"ccdd","spanId":"00a1"},{"traceId":"eeff","spanId":"00b2"}],)"
    R"("name":"db_query","kind":"SPAN_KIND_SERVER",)"
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

// ── The probe's SHAPE, not just its verdict (ADR-29.D7 / DN-29.D9) ───────────────────────────
// The bar is two-part — first-non-whitespace `{`, then a BOUNDED prefix key probe — and it is a
// shipping condition, because this probe is charged to lines of streams that are not OTEL at all.
// A verdict-only test is satisfied by the unbounded whole-line scan the bar forbids, so the gate
// and the bound each get an assertion that the forbidden form would fail.

TEST(SpanUnpack, ProbeIsGatedOnTheJsonLayoutSoNonJsonLinesAreNeverScanned)
{
    // Leading whitespace is skipped, exactly as JsonStrategy::confidence skips it.
    EXPECT_TRUE(is_otel_span_document("  \t" + std::string{kDocument}));

    // The gate: a line that is not a JSON object is rejected on its FIRST byte, whatever it goes
    // on to contain. An unbounded `.contains("resourceSpans")` says true here.
    EXPECT_FALSE(is_otel_span_document(R"(2026-01-01 INFO exporting "resourceSpans" to disk)"))
        << "a plain log line mentioning the key was read as an OTLP export — the `{` gate is not "
           "being applied, so every non-JSON stream pays a whole-line scan";
    EXPECT_FALSE(is_otel_span_document(R"([{"resourceSpans":[]}])"))
        << "a top-level ARRAY is not the ExportTraceServiceRequest object";
    EXPECT_FALSE(is_otel_span_document(""));
    EXPECT_FALSE(is_otel_span_document("   "));
}

TEST(SpanUnpack, ProbeComparesTheFirstKeyInsteadOfSearchingForIt)
{
    // OTLP's ExportTraceServiceRequest has exactly one top-level field, so in a real export
    // `"resourceSpans"` IS the root object's first key. The probe compares it there; it does not
    // search. A payload that puts the key anywhere else is NOT claimed — a positive statement of
    // the declared boundary, not a defect, and the assertion any searching form fails.
    const std::string buried{R"({"pad":")" + std::string(4096, 'x') +
                             R"(","resourceSpans":[{"resource":{},"scopeSpans":[]}]})"};
    EXPECT_FALSE(is_otel_span_document(buried))
        << "the key was found behind a 4 KiB prefix — the probe is searching the line rather than "
           "reading its first key, which is the cost ADR-29.D7 refuses";
    EXPECT_FALSE(is_otel_span_document(R"({"schemaUrl":"x","resourceSpans":[]})"))
        << "a second-position key is outside the declared shape";

    // ...and the shapes that DO occur are still claimed: a pretty-printed export whose first key
    // sits behind a newline and an indent.
    EXPECT_TRUE(is_otel_span_document("{\n    \"resourceSpans\": [\n    ]\n}"))
        << "a pretty-printed collector export must still be claimed";
    EXPECT_TRUE(is_otel_span_document(R"({ "resourceSpans": [] })"));
}

TEST(SpanUnpack, UnpacksDocumentToByteIdenticalCanonicalRecords)
{
    std::vector<std::string> records;
    const std::size_t count{unpack_otel_spans(kDocument, records)};
    ASSERT_EQ(count, 2U);
    ASSERT_EQ(records.size(), 2U);
    // Byte-identical to the lab's flat-span emission (the shape-1 ≡ shape-2 property,
    // SRC-D-OTEL-18a).
    EXPECT_EQ(records[0], kExpectedSpan0) << "got: " << records[0];
    EXPECT_EQ(records[1], kExpectedSpan1) << "got: " << records[1];
}

// ── DN-29.D15: recognition and refusal must NOT share one predicate ──────────────────────────
//
// READ THIS BEFORE "FIXING" THE FIXTURE. `kNonCanonicalKeyOrderDocument` is a **conformant** OTLP
// export. JSON object key order is not semantically significant (RFC 8259 §4), so a document that
// emits `schemaUrl` before `resourceSpans` is exactly as valid as the canonical one, and a
// conformant producer is entitled to emit it. Reordering these keys into canonical order does not
// fix a broken fixture — it deletes the only test in the suite that can see the defect below.
//
// THE DEFECT THE ARM EXISTS FOR. Today ONE predicate (`is_otel_span_document`, an O(1) first-key
// compare) serves three callers: the record-path REFUSAL (json.cpp), the acquisition-path
// RECOGNITION (tokenizer_engine.cpp) and the unpack guard (span_unpack.cpp). Because refusal and
// recognition are keyed on the SAME predicate, a document the probe stops recognising is also a
// document the refusal stops refusing — so it falls through to the generic record route and yields
// ONE plausible event for an export carrying N spans. No error, no diagnostic. That is precisely
// the silent-wrong-answer class ADR-29.D5 says this product may not ship, arrived at through the
// door built to prevent it. The O(1) bound on the record path is correct and stays (ADR-29.D7);
// the acquisition entry holds the whole input and is not hot, so it takes a broad, deliberately
// over-triggering check instead. That is the split.
//
// WHAT THIS TEST'S GREEN IS WORTH — a reader must not have to guess. It asserts the two halves
// SEPARATELY: the record path refuses (bounded, first-key) and the acquisition path still unpacks
// (broad). A single shared predicate cannot satisfy both at once for this input, so a green here
// means the predicates are genuinely split. If someone later re-merges them for tidiness, this is
// the arm that goes red; nothing else in the suite would notice.
constexpr std::string_view kNonCanonicalKeyOrderDocument{
    R"({"schemaUrl":"https://opentelemetry.io/schemas/1.21.0",)"
    R"("resourceSpans":[{"resource":{"attributes":[{"key":"service.name","value":{"stringValue":"checkout-svc"}}]},)"
    R"("scopeSpans":[{"spans":[)"
    R"({"traceId":"aabb","spanId":"0001","name":"checkout","kind":"SPAN_KIND_INTERNAL",)"
    R"("startTimeUnixNano":"1000","endTimeUnixNano":"1500","status":{"code":"STATUS_CODE_UNSET"},)"
    R"("attributes":[]},)"
    R"({"traceId":"aabb","spanId":"0002","parentSpanId":"0001","name":"db_query","kind":2,)"
    R"("startTimeUnixNano":"1100","endTimeUnixNano":"1400","status":{"code":2},"attributes":[]})"
    R"(]}]}]})"};

// L2, the independent backstop. The verb is DIAGNOSES, not refuses — and the distinction is the
// whole ruling. L1 (the O(1) first-key probe) does NOT promise to RECOGNISE a non-canonical export;
// refusal ⊆ recognition, so on one path it cannot. What the record path promises is narrower and
// achievable: **it is never SILENT about input it understood nothing of.**
//
// L2 is schema-blind by construction — "zero recognized roles" is already computed by the end of
// parse(), so it costs no needle, no window constant and no second pass, and it cannot be defeated
// by the same schema change that defeats L1. It therefore also catches a `resourceLogs`, and every
// future OTLP shape, while knowing nothing about OTLP.
//
// WHAT A FUTURE GREEN HERE IS WORTH: this arm goes red again only if someone collapses L1 and L2
// back into a single predicate. That is its entire job. It is not a parser test.
TEST(SpanUnpack, RecordPathDiagnosesAConformantExportWhoseKeysAreNotInCanonicalOrder)
{
    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto parsed{strategy.parse(kNonCanonicalKeyOrderDocument, arena)};

    // The forbidden outcome is a CLEAN-LOOKING event: a caller receiving this cannot doubt it, and
    // one plausible event for a 2-span export is the precision-first defect ADR-29.D5 names. Either
    // L1 refused it (canonical order) or L2 must mark it — never a confident, unmarked event.
    if (parsed.has_value())
    {
        EXPECT_FALSE(parsed->no_role_witness_key.empty())
            << "the record path returned a CONFIDENT, UNMARKED event for an OTLP export carrying 2 "
               "spans. Nothing in the line was understood — no timestamp, level, component or "
               "message role matched — yet the result is indistinguishable to a caller from a "
               "well-parsed record. That is the silent-wrong-answer class, reached through the "
               "door built to stop it (DN-29.D15 L2).";
        if (!parsed->no_role_witness_key.empty())
            EXPECT_EQ(parsed->no_role_witness_key, "schemaUrl")
                << "marked, but the witness is not a key that was actually present on the line, so "
                   "a reader cannot tell WHAT arrived. Got: "
                << parsed->no_role_witness_key;
    }
}

// The marker is UNCONDITIONAL; the rate limit governs the LOG ONLY.
//
// WHY THIS ARM EXISTS WHEN THE IMPLEMENTATION IS ALREADY CORRECT. The assignment sits above the
// rate-limit block, so every role-less record is marked today. But every other test in this suite
// feeds ONE role-less line, which lands at the head of a rate-limit period — the exact index where
// a sampled marker and an unconditional one are INDISTINGUISHABLE. Nothing in the tree could tell
// the difference, so the invariant comment at the site is one edit away from being silently false:
// 999 of every 1000 role-less records would return an empty marker, i.e. indistinguishable from a
// well-parsed record. That is the state DN-29.D16 was ruled to end — the console desilenced, the
// contract still mute.
//
// Crossing a full period is the only thing that separates the two designs. The assertion is on
// EVERY iteration rather than on the last, so it holds whatever phase the shared thread_local
// counter is in when this test runs: test ORDER must never decide whether a guarantee is checked.
//
// It asserts on the MARKER and never on log volume — the counter is thread_local, so the number of
// WARN lines varies with worker count and is not a property any test may pin.
TEST(SpanUnpack, TheNoRoleMarkerIsSetOnEveryRecordNotOncePerRateLimitPeriod)
{
    // One full period plus one, so a boundary is crossed wherever the shared counter starts.
    // Mirrors kWarnEveryNRoleless in json.cpp: if that constant grows this arm gets slower, never
    // wrong.
    constexpr int kRolelessLinesToFeed{1001};

    JsonStrategy strategy;
    ArenaAllocator arena{1U << 20U};

    int unmarked{0};
    int first_unmarked_index{-1};
    for (int index{0}; index < kRolelessLinesToFeed; ++index)
    {
        // Role-less but perfectly well-formed: no timestamp, level, component or message key.
        // Distinct per iteration so nothing can be served from a per-line cache.
        const std::string line{R"({"warehouseSpans":[{"unit":)" + std::to_string(index) + "}]}"};
        const auto parsed{strategy.parse(line, arena)};
        if (parsed.has_value() && parsed->no_role_witness_key.empty())
        {
            ++unmarked;
            if (first_unmarked_index < 0)
                first_unmarked_index = index;
        }
    }

    EXPECT_EQ(unmarked, 0)
        << unmarked << " of " << kRolelessLinesToFeed
        << " role-less records came back with an EMPTY witness marker (first at index "
        << first_unmarked_index
        << "). The marker has been tied to the WARN rate limit, so all but one record per period "
           "reaches the caller indistinguishable from a well-parsed record.\n"
           "    The rate limit is correct for the CONSOLE and must stay. It must never reach the "
           "record: the marker is set on EVERY role-less record, unconditionally (DN-29.D16).";
}

TEST(SpanUnpack, AcquisitionPathStillUnpacksAConformantNonCanonicalKeyOrderExport)
{
    std::vector<std::string> records;
    const std::size_t count{unpack_otel_spans(kNonCanonicalKeyOrderDocument, records)};

    EXPECT_EQ(count, 2U)
        << "the acquisition path yielded " << count
        << " records for a CONFORMANT 2-span export whose only unusual property is that "
           "`resourceSpans` is not its first key — which JSON does not make significant.\n"
           "    The acquisition entry is not the hot record path: it holds the whole input "
           "already, "
           "so it must use a BROAD, deliberately over-triggering check rather than the O(1) "
           "first-key compare the record path is bound to (DN-29.D15).\n"
           "    Sharing one predicate between the two is what makes a conformant export "
           "simultaneously unrecognised and unrefused.";
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

// ── Losslessness stated as OUTPUT EQUALITY, which is the form the ruling actually takes ──────
//
// DN-29.D7 is normative that the unpack is "lossless with respect to every field the flat-span
// parser consumes", and explicit that the property is "equality of the two shapes' parsed output,
// never a field list to keep in step by hand". The byte golden above is exactly such a hand-kept
// list wearing an equivalence's clothes: it is only as complete as whoever last edited the fixture
// remembered to include, which is precisely how links went missing from it in the first place.
//
// So this is the arm that reads the PARSER rather than a literal. It takes the document (shape 1),
// unpacks it, parses the result — and compares against the SAME span parsed directly as a flat
// record (shape 2). No expected-bytes constant participates. A field the unpack drops makes the
// two parsed events differ no matter what anyone remembered to write down.
constexpr std::string_view kLinkedSpanFlat{
    R"({"traceId":"aabb","spanId":"0002","parentSpanId":"0001",)"
    R"("links":[{"traceId":"ccdd","spanId":"00a1"},{"traceId":"eeff","spanId":"00b2"}],)"
    R"("name":"db_query","kind":"SPAN_KIND_SERVER",)"
    R"("startTimeUnixNano":"1100","endTimeUnixNano":"1400","status":{"code":"STATUS_CODE_ERROR"},)"
    R"("attributes":[{"key":"service.name","value":{"stringValue":"checkout-svc"}}]})"};

TEST(SpanUnpack, DocumentPathAndFlatPathAgreeOnLinkedSpanIds)
{
    std::vector<std::string> records;
    ASSERT_EQ(unpack_otel_spans(kDocument, records), 2U);

    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto via_document{strategy.parse(records[1], arena)};
    const auto via_flat{strategy.parse(kLinkedSpanFlat, arena)};
    ASSERT_TRUE(via_document.has_value());
    ASSERT_TRUE(via_flat.has_value());

    ASSERT_EQ(via_flat->linked_span_ids.size(), 2U)
        << "the flat-path control lost its own links — this arm's ORACLE is broken, so a match "
           "below would prove nothing (both sides empty compares equal)";

    ASSERT_EQ(via_document->linked_span_ids.size(), via_flat->linked_span_ids.size())
        << "shape-1 (document, unpacked) yielded " << via_document->linked_span_ids.size()
        << " link target(s); shape-2 (the same span, flat) yielded "
        << via_flat->linked_span_ids.size()
        << ". The document path is LOSSY w.r.t. links[] — every Span Link in an export is "
           "destroyed "
           "before the flat-span parser sees it, which deletes the cross-trace Régime-B edge the "
           "OTEL subject exists for (DN-29.D7, ADR-29.D1/D2)";

    for (std::size_t index{0}; index < via_flat->linked_span_ids.size(); ++index)
        EXPECT_EQ(via_document->linked_span_ids[index], via_flat->linked_span_ids[index])
            << "link #" << index
            << " differs between the document and flat paths — order or "
               "identity is not preserved through the unpack";

    // The rest of the parsed event must agree too, so this arm covers losslessness beyond links
    // without naming a field list: same content, component, level, trace context, ordinals.
    EXPECT_EQ(via_document->content, via_flat->content);
    EXPECT_EQ(via_document->component, via_flat->component);
    EXPECT_EQ(via_document->level, via_flat->level);
    EXPECT_EQ(via_document->trace.span_id, via_flat->trace.span_id);
    EXPECT_EQ(via_document->trace.has_parent, via_flat->trace.has_parent);
    EXPECT_EQ(via_document->trace.parent_span_id, via_flat->trace.parent_span_id);
    ASSERT_EQ(via_document->ordinals.size(), via_flat->ordinals.size());
    for (std::size_t index{0}; index < via_flat->ordinals.size(); ++index)
        EXPECT_EQ(via_document->ordinals[index].value, via_flat->ordinals[index].value);
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
