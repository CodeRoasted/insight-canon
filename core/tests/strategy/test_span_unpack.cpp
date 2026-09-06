#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

// invariant: OTEL span-export DOCUMENT unpack — one OTLP export of shape 1 is unpacked into N
// canonical flat-span records of shape 2.
// invariant: the records are byte-form-identical to the lab's emission, so the flat-span parser is
// authored ONCE.
// invariant: the two shapes' equivalence is golden-tested, which kills the two-paths-drift bug
// class.
// invariant: the resource service name is injected, and int-form kind and status are normalized to
// the protojson string enum.
// refs: SRC-D-OTEL-18, SRC-D-OTEL-18a
namespace
{

// invariant: a two-span export under one resource — span 1 has string kind and status, a real
// span attribute, no parent and NO links.
// invariant: span 2 has INT kind and status, which is the real-collector form, a parent, empty
// attributes and TWO cross-trace links.
// invariant: span 2's links sit between the parent id and the name, which is the lab's own field
// ORDER.
// invariant: a re-serializer that carries links but appends them in the wrong place still fails
// this golden.
// invariant: span 1 stays link-free in the SAME document so the empty-in-implies-nothing-written
// property keeps its coverage here rather than needing a second fixture.
// invariant: WHY THE LINKS ARE HERE AT ALL — before this the fixture had no links and the
// byte-equivalence golden went green while the unpack silently dropped every span link.
// invariant: that is the green-BLIND shape: the gate could not fail on the one field the whole OTEL
// subject was minted for, the span link being the declared cross-trace edge.
// invariant: A FIXTURE THAT OMITS THE LOAD-BEARING FIELD TURNS AN EQUIVALENCE ASSERTION INTO A
// TAUTOLOGY.
// refs: ADR-29.D1, ADR-29.D2, DN-29.D7, MEM:synthetic-gate-vacuity-vs-judgment
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

// invariant: the exact bytes the lab emits for the same spans — fixed field order, service name
// injected first, then span attributes verbatim, int kind and status normalized.
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
    // invariant: a flat span is NOT a document and takes the direct parser path.
    EXPECT_FALSE(is_otel_span_document(
        R"({"traceId":"aa","spanId":"bb","name":"x","startTimeUnixNano":"1"})"));
    EXPECT_FALSE(is_otel_span_document(R"({"level":"info","message":"hi"})"));
}

// invariant: the probe's SHAPE and not just its verdict — the bar is two-part, a first
// non-whitespace brace and then a BOUNDED prefix key probe.
// invariant: it is a shipping condition because this probe is charged to lines of streams that are
// not OTEL at all.
// invariant: a verdict-only test is satisfied by the unbounded whole-line scan the bar forbids, so
// the gate and the bound EACH get an assertion that the forbidden form would fail.
// refs: ADR-29.D7, DN-29.D9
TEST(SpanUnpack, ProbeIsGatedOnTheJsonLayoutSoNonJsonLinesAreNeverScanned)
{
    // invariant: leading whitespace is skipped, exactly as the JSON strategy's confidence skips it.
    EXPECT_TRUE(is_otel_span_document("  \t" + std::string{kDocument}));

    // invariant: a line that is not a JSON object is rejected on its FIRST byte, whatever it goes
    // on to contain — an unbounded whole-line search says true here.
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
    // invariant: OTLP's export request has exactly one top-level field, so in a real export the key
    // IS the root object's first key; the probe compares it there and does not search.
    // invariant: a payload that puts the key anywhere else is NOT claimed — a positive statement
    // of the declared boundary, not a defect, and the assertion any searching form fails.
    const std::string buried{R"({"pad":")" + std::string(4096, 'x') +
                             R"(","resourceSpans":[{"resource":{},"scopeSpans":[]}]})"};
    EXPECT_FALSE(is_otel_span_document(buried))
        << "the key was found behind a 4 KiB prefix — the probe is searching the line rather than "
           "reading its first key, which is the cost ADR-29.D7 refuses";
    EXPECT_FALSE(is_otel_span_document(R"({"schemaUrl":"x","resourceSpans":[]})"))
        << "a second-position key is outside the declared shape";

    // invariant: the shapes that DO occur are still claimed — a pretty-printed export whose first
    // key sits behind a newline and an indent.
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
    // invariant: byte-identical to the lab's flat-span emission, which is the two-shapes
    // equivalence property itself.
    // refs: SRC-D-OTEL-18a
    EXPECT_EQ(records[0], kExpectedSpan0) << "got: " << records[0];
    EXPECT_EQ(records[1], kExpectedSpan1) << "got: " << records[1];
}

// invariant: RECOGNITION AND REFUSAL MUST NOT SHARE ONE PREDICATE — read this before fixing the
// fixture.
// invariant: the non-canonical-key-order document is a CONFORMANT export: JSON object key order is
// not semantically significant, so emitting the schema URL first is exactly as valid.
// invariant: reordering these keys into canonical order does not fix a broken fixture — it
// DELETES the only test in the suite that can see the defect below.
// invariant: THE DEFECT: one O(1) first-key predicate serves three callers — the record-path
// REFUSAL, the acquisition-path RECOGNITION and the unpack guard.
// invariant: because refusal and recognition key on the SAME predicate, a document the probe stops
// recognising is one the refusal stops refusing.
// invariant: so it falls through to the generic record route and yields ONE plausible event for an
// export carrying N spans.
// invariant: no error and no diagnostic — the silent-wrong-answer class this product may not
// ship, arrived at through the door built to prevent it.
// invariant: the O(1) bound on the record path is correct and STAYS; the acquisition entry holds
// the whole input and is not hot, so it takes a broad, deliberately over-triggering check.
// invariant: this test asserts the two halves SEPARATELY — the record path refuses and the
// acquisition path still unpacks.
// invariant: a single shared predicate cannot satisfy both for this input, so a green here means
// the predicates are genuinely split.
// invariant: if someone later re-merges them for tidiness this is the arm that goes red, and
// nothing else in the suite would notice.
// refs: ADR-29.D5, ADR-29.D7, DN-29.D15
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

// invariant: the independent backstop, and the verb is DIAGNOSES rather than refuses — the
// distinction is the whole ruling.
// invariant: the O(1) first-key probe does NOT promise to RECOGNISE a non-canonical export, since
// refusal is a subset of recognition and on one path it cannot.
// invariant: what the record path promises is narrower and achievable — IT IS NEVER SILENT ABOUT
// INPUT IT UNDERSTOOD NOTHING OF.
// invariant: the backstop is schema-blind by construction: zero recognized roles is already
// computed by the end of parse, so it costs no needle, no window constant and no second pass.
// invariant: it therefore cannot be defeated by the same schema change that defeats the probe, and
// it also catches every future OTLP shape while knowing nothing about OTLP.
// invariant: this arm goes red again only if someone collapses the two layers back into a single
// predicate; that is its entire job, and it is not a parser test.
TEST(SpanUnpack, RecordPathDiagnosesAConformantExportWhoseKeysAreNotInCanonicalOrder)
{
    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto parsed{strategy.parse(kNonCanonicalKeyOrderDocument, arena)};

    // invariant: the forbidden outcome is a CLEAN-LOOKING event — a caller receiving this cannot
    // doubt it, and one plausible event for a two-span export is the precision-first defect.
    // invariant: either the first layer refused it or the second must mark it, never a confident
    // unmarked event.
    // refs: ADR-29.D5
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

// invariant: the marker is UNCONDITIONAL and the rate limit governs the LOG ONLY.
// invariant: WHY THIS ARM EXISTS WHEN THE IMPLEMENTATION IS ALREADY CORRECT — the assignment sits
// above the rate-limit block, so every role-less record is marked today.
// invariant: every other test in this suite feeds ONE role-less line, which lands at the HEAD of a
// rate-limit period.
// invariant: that is the exact index where a sampled marker and an unconditional one are
// INDISTINGUISHABLE.
// invariant: so nothing in the tree could tell the difference and the invariant at the site is one
// edit away from being silently false.
// invariant: under a sampled marker, 999 of every 1000 role-less records would return an empty one.
// invariant: crossing a full period is the only thing that separates the two designs.
// invariant: the assertion is on EVERY iteration rather than on the last, so it holds whatever
// phase the shared thread-local counter is in.
// invariant: test ORDER must never decide whether a guarantee is checked.
// invariant: it asserts on the MARKER and never on log volume, because the counter is thread-local
// so the number of warning lines varies with worker count and is not a property any test may pin.
// refs: DN-29.D16
TEST(SpanUnpack, TheNoRoleMarkerIsSetOnEveryRecordNotOncePerRateLimitPeriod)
{
    // invariant: one full period plus one, so a boundary is crossed wherever the shared counter
    // starts; if that constant grows this arm gets slower, never wrong.
    constexpr int kRolelessLinesToFeed{1001};

    JsonStrategy strategy;
    ArenaAllocator arena{1U << 20U};

    int unmarked{0};
    int first_unmarked_index{-1};
    for (int index{0}; index < kRolelessLinesToFeed; ++index)
    {
        // invariant: role-less but perfectly well-formed — no timestamp, level, component or
        // message key.
        // invariant: distinct per iteration so nothing can be served from a per-line cache.
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

// invariant: a crafted flat OTLP span declaring two cross-trace link targets — the canon-grain
// unit guard for the lab's links emission.
// invariant: canon collects each link's span id IN ORDER.
// invariant: the link's trace id and any link attributes are consumed-not-retained, since only the
// span id feeds the cross-trace distillation downstream.
// refs: SRC-D-OTEL-9, SRC-D-OTEL-23
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
    // invariant: the link's trace id does not disturb the span's own trace context.
    EXPECT_EQ(span->trace.span_id, span_id_from_hex("0001"));
}

// invariant: a span without a links array carries no cross-trace edges — empty in implies empty
// out, allocating nothing, so a non-linking span stays byte-identical to pre-links.
TEST(SpanUnpack, FlatSpanWithoutLinksHasEmptyLinkedSpanIds)
{
    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto span{strategy.parse(kExpectedSpan0, arena)};
    ASSERT_TRUE(span.has_value());
    EXPECT_TRUE(span->linked_span_ids.empty());
}

// invariant: losslessness stated as OUTPUT EQUALITY, which is the form the ruling actually takes
// — equality of the two shapes' parsed output, never a field list to keep in step by hand.
// invariant: the byte golden above is exactly such a hand-kept list wearing an equivalence's
// clothes.
// invariant: it is only as complete as whoever last edited the fixture remembered, which is
// precisely how links went missing from it.
// invariant: so this arm reads the PARSER rather than a literal — it unpacks the document, parses
// the result, and compares against the SAME span parsed directly as a flat record.
// invariant: no expected-bytes constant participates, so a field the unpack drops makes the two
// parsed events differ no matter what anyone remembered to write down.
// refs: DN-29.D7
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

    // invariant: the rest of the parsed event must agree too, so this arm covers losslessness
    // beyond links without naming a field list.
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

// invariant: the de-risk — every unpacked record parses through the SAME flat-span parser into
// the event it should, closing the loop from document to flat record to event.
TEST(SpanUnpack, UnpackedRecordsRoundTripThroughTheFlatSpanParser)
{
    std::vector<std::string> records;
    ASSERT_EQ(unpack_otel_spans(kDocument, records), 2U);

    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto span0{strategy.parse(records[0], arena)};
    ASSERT_TRUE(span0.has_value());
    EXPECT_EQ(span0->content, "checkout");
    EXPECT_EQ(span0->component, "checkout-svc");
    EXPECT_EQ(span0->level, LogLevel::Info);
    EXPECT_FALSE(span0->trace.has_parent);
    ASSERT_EQ(span0->ordinals.size(), 1U);
    EXPECT_EQ(span0->ordinals[0].value, 500);

    const auto span1{strategy.parse(records[1], arena)};
    ASSERT_TRUE(span1.has_value());
    EXPECT_EQ(span1->content, "db_query");
    EXPECT_EQ(span1->component, "checkout-svc");
    EXPECT_EQ(span1->level, LogLevel::Error);
    EXPECT_TRUE(span1->trace.has_parent);
    EXPECT_EQ(span1->trace.parent_span_id, span_id_from_hex("0001"));
    ASSERT_EQ(span1->ordinals.size(), 1U);
    EXPECT_EQ(span1->ordinals[0].value, 300);
}
