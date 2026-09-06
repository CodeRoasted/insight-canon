
// invariant: the full ingest pipeline end to end — a raw log line through the tokenizer to a
// canonical event.
// invariant: it covers end-to-end correctness for the universal formats, event identity, template
// grouping, param extraction and batch processing.
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

class TokenizerTest : public ::testing::Test
{
  protected:
    static constexpr std::size_t kArenaSize{1u << 20};

    ArenaAllocator arena{kArenaSize};
    // invariant: SEMANTIC-UNAWARE — the universal formats tokenize with a degenerate zero-package
    // composition.
    // invariant: the composition is declared BEFORE the tokenizer so it outlives the const-ref the
    // tokenizer holds.
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed};
};

// invariant: the level assertion here USED to read Unknown, on a body carrying neither a level word
// nor a lexicon cue.
// invariant: so it held BOTH before and after the branch started reading levels at all — a
// can't-FAIL arm, and nine generations of green said nothing about this path.
// invariant: the prose it carried was also FALSE as a general claim, which is why the repair is a
// line whose body DOES carry a level rather than a re-assertion of the old one.
// refs: DN-43.D5
TEST_F(TokenizerTest, ProcessesBSDSyslogLine)
{
    constexpr std::string_view line{
        "Jan 15 08:03:22 myhost sshd[1]: error: PAM authentication failure for alice"};
    auto result{tokenizer.process_line(line)};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_EQ(ev.component, "sshd");
    EXPECT_EQ(ev.level, LogLevel::Error) << "level = " << to_string(ev.level);
    EXPECT_FALSE(ev.declared_level) << "a level read out of the body is INFERRED, never declared";
    EXPECT_FALSE(ev.template_str.empty());
}

// invariant: a body with no level and no cue still yields NO level — the boundary the arm above
// needs to stay honest.
// invariant: split out so each arm can fail for exactly ONE reason.
TEST_F(TokenizerTest, ProcessesBSDSyslogLineWithNoLevelInItsBody)
{
    constexpr std::string_view line{"Jan 15 08:03:22 myhost sshd[1]: Accepted password for alice"};
    auto result{tokenizer.process_line(line)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Unknown);
}

TEST_F(TokenizerTest, ProcessesJSONLine)
{
    constexpr std::string_view line{
        R"({"ts":"2024-01-15T10:30:00Z","level":"ERROR","component":"db","message":"Query timeout"})"};
    auto result{tokenizer.process_line(line)};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_EQ(ev.level, LogLevel::Error);
    EXPECT_EQ(ev.component, "db");
    EXPECT_FALSE(ev.template_str.empty());
}

TEST_F(TokenizerTest, ProcessesKVLine)
{
    constexpr std::string_view line = "ts=2024-01-15T10:30:00Z level=WARN component=cache "
                                      "msg=eviction_threshold_reached";
    auto result{tokenizer.process_line(line)};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_EQ(ev.level, LogLevel::Warn);
    EXPECT_EQ(ev.component, "cache");
}

// invariant: the cube-dimension precondition for the key-value flow path — such a line tokenizes
// to a CLEAN level, component and role tuple.
// invariant: the component is the declared service and non-empty, the role is None because the
// message announces no structural marker, and the level is taken verbatim from its field.
// invariant: the cube's axes read exactly these three event fields, so this is their canon-level
// guarantee.
// invariant: RE-HOMED from a former end-to-end substrate precondition that asserted the same tuple
// on LIVE generated records.
// invariant: the wiring it guards is a SINGLE-COMPONENT canon property, so it is proven here on
// hand-built lines, decoupled from the generator.
// invariant: the collapse claim itself stays in the contract fixtures, which rely on this
// precondition.
TEST_F(TokenizerTest, KvFlowRecordsPopulateCubeDimsCleanly)
{
    struct KvCase
    {
        std::string_view line;
        LogLevel level;
        std::string_view component;
    };
    // invariant: representative BENIGN flow records — the declared services across the level
    // band, with plain prose messages announcing no structural marker.
    const KvCase cases[]{
        {R"(level=info component=gateway msg="accept settle request")", LogLevel::Info, "gateway"},
        {R"(level=info component=payments msg="settle order total")", LogLevel::Info, "payments"},
        {R"(level=error component=payments msg="settle order total")", LogLevel::Error, "payments"},
        {R"(level=info component=notifier msg="publish settlement event")", LogLevel::Info,
         "notifier"},
        {R"(level=error component=notifier msg="publish settlement event")", LogLevel::Error,
         "notifier"},
    };

    for (const auto& kase : cases)
    {
        const auto event{tokenizer.process_line(kase.line)};
        ASSERT_TRUE(event.has_value()) << "kv flow line failed to tokenize: " << kase.line;
        EXPECT_EQ(event->component, kase.component)
            << "component= field must populate the WHERE axis cleanly: " << kase.line;
        EXPECT_FALSE(event->component.empty()) << "empty component on: " << kase.line;
        EXPECT_EQ(event->level, kase.level)
            << "level= field must populate the SEVERITY axis verbatim: " << kase.line;
        EXPECT_EQ(event->structural_role, StructuralRole::None)
            << "a benign prose message announces no role (KIND axis = None): " << kase.line;
    }
}

TEST_F(TokenizerTest, ProcessesCLFLine)
{
    constexpr std::string_view line{
        R"(192.168.1.5 - bob [15/Jan/2024:10:30:00 +0000] "GET /api/health HTTP/1.1" 200 42)"};
    auto result{tokenizer.process_line(line)};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_EQ(ev.level, LogLevel::Info);
    // invariant: THE SEAM THAT MATTERS.
    // invariant: the component reaches the cube's WHERE axis UNMASKED, so a client address left
    // there was published RAW while the same octets in the content were masked.
    // invariant: it is a HOST, it goes in the host field, and it stays OFF the axis.
    // refs: DN-43.D8
    EXPECT_EQ(ev.host, "192.168.1.5");
    EXPECT_TRUE(ev.component.empty()) << "component = \"" << ev.component << "\"";
    EXPECT_NE(ev.template_str.find("GET"), std::string::npos);
}

// invariant: the through-pipeline property, homed HERE rather than on the strategy because the
// defect it guards is a JOINT one.
// invariant: the routing, the projection and the level inference all have to hold together for the
// identity that reaches the wire to be right.
// invariant: the timestamp assertion is NOT decoration — without it this passes on the rejected
// alternative, a bare rejection to raw text.
// invariant: that alternative sets no event time and would lose the clock the downstream windows
// are built on.
// refs: DN-43.D4, DN-43.D6
TEST_F(TokenizerTest, Rfc3339ApplicationLinesTemplateDistinctlyAndKeepTheirStamp)
{
    static constexpr std::array kLines{
        std::string_view{
            "2026-05-31T08:00:01Z INFO request method=GET path=/api/users/1000 status=200"},
        std::string_view{"2026-05-31T08:02:03Z DEBUG db query=select_orders duration_ms=15 rows=2"},
        std::string_view{"2026-05-31T08:03:04Z INFO cache key=session:1021 hit=true"},
        std::string_view{
            "2026-05-31T09:00:01Z ERROR upstream timeout service=payments after_ms=80"},
        std::string_view{"2026-05-31T09:01:02Z WARN slow request path=/api/report latency_ms=103"},
    };
    static constexpr std::array kExpected{LogLevel::Info, LogLevel::Debug, LogLevel::Info,
                                          LogLevel::Error, LogLevel::Warn};

    std::set<std::string> templates;
    for (std::size_t i = 0; i < kLines.size(); ++i)
    {
        auto result{tokenizer.process_line(kLines[i])};
        ASSERT_TRUE(result.has_value()) << "line " << i << ": " << kLines[i];
        const auto& ev{result.value()};

        // invariant: the projection is TOTAL — the whole remainder is content, so each line
        // templates to its own structure.
        // invariant: the defect published ONE template for the whole file, the digest prefix of the
        // empty string.
        EXPECT_FALSE(ev.template_str.empty()) << "line " << i << ": " << kLines[i];
        templates.insert(std::string{ev.template_str});

        // invariant: the level is READ and is what the body says, not a uniform default.
        EXPECT_EQ(ev.level, kExpected[i]) << "line " << i << " level=" << to_string(ev.level)
                                          << " expected=" << to_string(kExpected[i]);

        // invariant: the event time SURVIVES the split, and a missing timestamp lands as the epoch.
        EXPECT_NE(ev.timestamp, Timestamp{}) << "line " << i << " lost its event time";

        // invariant: the layout names no functional source, and inventing one would be a FABRICATED
        // cube axis.
        EXPECT_TRUE(ev.component.empty())
            << "line " << i << " component=\"" << ev.component << "\"";
    }
    EXPECT_EQ(templates.size(), kLines.size())
        << "distinct templates=" << templates.size() << " lines=" << kLines.size();
}

TEST_F(TokenizerTest, EventIDMonotonicallyIncreases)
{
    constexpr std::string_view line{R"({"level":"INFO","message":"tick"})"};
    auto r1{tokenizer.process_line(line)};
    auto r2{tokenizer.process_line(line)};
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_LT(r1.value().id, r2.value().id);
}

TEST_F(TokenizerTest, EventsProducedCounterIncrements)
{
    EXPECT_EQ(tokenizer.events_produced(), 0u);
    static_cast<void>(tokenizer.process_line(R"({"msg":"one"})"));
    EXPECT_EQ(tokenizer.events_produced(), 1u);
    static_cast<void>(tokenizer.process_line(R"({"msg":"two"})"));
    EXPECT_EQ(tokenizer.events_produced(), 2u);
}

TEST_F(TokenizerTest, SameStructuredLinesSameTemplateID)
{
    auto r1{tokenizer.process_line(R"({"msg":"User alice connected"})")};
    auto r2{tokenizer.process_line(R"({"msg":"User alice connected"})")};
    ASSERT_TRUE(r1.has_value() && r2.has_value());
    // invariant: identity is the content-deterministic template string, whose digest is the
    // template id, so the same line yields the same template STATELESSLY.
    EXPECT_EQ(r1.value().template_str, r2.value().template_str);
}

// invariant: the alert label must NOT enter template identity — the load-bearing property of
// claiming the labelled shape.
// invariant: the alert-class column is the corpus's ANSWER KEY, so an identity that varied with it
// would split ONE event class by CURATION rather than by structure.
// invariant: that class is flagged on 348 398 pinned-corpus lines and unflagged on 506 797.
// invariant: asserted at the PIPELINE grain, because template identity is what the wire carries and
// a per-field strategy assertion cannot see it.
// refs: DN-43.D14
TEST_F(TokenizerTest, AnAlertLabelledBGLLineHasTheSameTemplateAsItsUnlabelledTwin)
{
    constexpr std::string_view kLabelled{
        "KERNDTLB 1117838570 2005.06.03 R02-M1-N0-C:J12-U11 2005-06-03-15.42.50.675872 "
        "R02-M1-N0-C:J12-U11 RAS KERNEL FATAL data TLB error interrupt"};
    constexpr std::string_view kTwin{
        "- 1117838570 2005.06.03 R02-M1-N0-C:J12-U11 2005-06-03-15.42.50.675872 "
        "R02-M1-N0-C:J12-U11 RAS KERNEL FATAL data TLB error interrupt"};
    auto labelled{tokenizer.process_line(kLabelled)};
    auto twin{tokenizer.process_line(kTwin)};
    ASSERT_TRUE(labelled.has_value());
    ASSERT_TRUE(twin.has_value());
    EXPECT_EQ(labelled.value().template_str, twin.value().template_str)
        << "labelled = " << labelled.value().template_str
        << " | twin = " << twin.value().template_str;
    EXPECT_EQ(template_id_of(labelled.value().template_str),
              template_id_of(twin.value().template_str));
    // invariant: the twin pair agrees on every projected field, not just on identity.
    EXPECT_EQ(labelled.value().component, twin.value().component);
    EXPECT_EQ(labelled.value().level, twin.value().level);
    EXPECT_TRUE(labelled.value().declared_level)
        << "BGL declares its severity in a fixed column, so the level is read, never guessed";
}

TEST_F(TokenizerTest, VariablePartBecomesWildcardInTemplate)
{
    // invariant: the address token masks per-line and statelessly, while the kept literal anchors
    // the template.
    // invariant: letter-leading names are KEPT rather than masked — only syntactic
    // high-cardinality classes mask.
    // refs: SRC-D-TID-14
    auto r{tokenizer.process_line(R"({"msg":"User bob logged in from 10.0.0.2"})")};
    ASSERT_TRUE(r.has_value());
    EXPECT_NE(r.value().template_str.find("User"), std::string::npos);
    EXPECT_NE(r.value().template_str.find("<*>"), std::string::npos);
}

TEST_F(TokenizerTest, DifferentFormatLinesDifferentTemplates)
{
    auto rSyslog{tokenizer.process_line("Jan 15 08:03:22 host proc[1]: kernel startup completed")};
    auto rJSON{
        tokenizer.process_line(R"({"level":"INFO","message":"database connection established"})")};
    ASSERT_TRUE(rSyslog.has_value() && rJSON.has_value());
    // invariant: the two lines go through DIFFERENT strategies, so they have different content and
    // therefore different templates.
    EXPECT_NE(rSyslog.value().template_str, rJSON.value().template_str);
}

TEST_F(TokenizerTest, ParamsExtractedAfterTemplateStabilises)
{
    static_cast<void>(tokenizer.process_line(R"({"msg":"Retry attempt 1 of 3"})"));
    static_cast<void>(tokenizer.process_line(R"({"msg":"Retry attempt 2 of 3"})"));
    auto r{tokenizer.process_line(R"({"msg":"Retry attempt 5 of 10"})")};
    ASSERT_TRUE(r.has_value());
    EXPECT_GE(r.value().params.size(), 1u);
}

TEST_F(TokenizerTest, ParamsAreArenaOwned)
{
    static_cast<void>(tokenizer.process_line(R"({"msg":"connect from 10.0.0.1 ok"})"));
    auto r{tokenizer.process_line(R"({"msg":"connect from 10.0.0.2 ok"})")};
    ASSERT_TRUE(r.has_value());
    for (auto sv : r.value().params)
        EXPECT_TRUE(arena.owns(sv.data()));
}

// invariant: NOT one result per line — a batch of ordinary lines is one-to-one, but an OTLP
// export document is one-to-many.
// invariant: naming this test for the one-to-one case ALONE once made it read as a guarantee
// AGAINST the fan-out.
TEST_F(TokenizerTest, BatchReturnsOneResultPerNonDocumentLine)
{
    const std::vector<std::string_view> lines = {
        R"({"msg":"line one"})",
        R"({"msg":"line two"})",
        R"({"msg":"line three"})",
    };
    auto results{tokenizer.process_batch(lines)};
    EXPECT_EQ(results.size(), lines.size());
}

// invariant: an OTLP export is ONE input line carrying N spans, and the batch entry unpacks it into
// N canonical records before tokenizing each one-to-one.
// invariant: the span-unpack suite covers the unpacker in ISOLATION; this covers the SEAM, that the
// batch entry actually routes a document through it.
// invariant: without this, deleting the document branch from the batch entry left every test GREEN
// — the unpack tests never call the batch entry and the batch tests never fed it a document.
// invariant: worse, the surviving guard was NAMED for one result per line, so a green suite
// actively ENDORSED the deletion.
// refs: SRC-D-OTEL-18
namespace
{
// invariant: two spans under one resource, the same shape as the span-unpack fixture, kept LOCAL so
// this test states its own premise.
// invariant: the second span carries the int-form kind and status a real collector emits.
constexpr std::string_view kSpanDocument{
    R"({"resourceSpans":[{"resource":{"attributes":[{"key":"service.name","value":{"stringValue":"checkout-svc"}}]},)"
    R"("scopeSpans":[{"spans":[)"
    R"({"traceId":"aabb","spanId":"0001","name":"checkout","kind":"SPAN_KIND_INTERNAL",)"
    R"("startTimeUnixNano":"1000","endTimeUnixNano":"1500","status":{"code":"STATUS_CODE_UNSET"},)"
    R"("attributes":[{"key":"http.method","value":{"stringValue":"GET"}}]},)"
    R"({"traceId":"aabb","spanId":"0002","parentSpanId":"0001","name":"db_query","kind":2,)"
    R"("startTimeUnixNano":"1100","endTimeUnixNano":"1400","status":{"code":2},"attributes":[]})"
    R"(]}]}]})"};
} // namespace

TEST_F(TokenizerTest, BatchUnpacksAnOtelSpanDocumentIntoOneResultPerSpan)
{
    const std::vector<std::string_view> lines{kSpanDocument};
    const auto results{tokenizer.process_batch(lines)};

    ASSERT_EQ(results.size(), 2U)
        << "one resourceSpans line carrying 2 spans produced " << results.size()
        << " result(s) — process_batch is not routing the document through unpack_otel_spans, so "
           "an entire OTLP export collapses to a single event (or none)";

    // invariant: the COUNT alone is not the contract — two copies of one span would also be two.
    // invariant: each result must be the span it CAME FROM.
    ASSERT_TRUE(results[0].has_value()) << "span 0: " << results[0].error();
    ASSERT_TRUE(results[1].has_value()) << "span 1: " << results[1].error();
    EXPECT_EQ(results[0]->template_str, "checkout");
    EXPECT_EQ(results[1]->template_str, "db_query");
    EXPECT_EQ(results[0]->component, "checkout-svc") << "resource service.name must be injected";
    EXPECT_EQ(results[1]->component, "checkout-svc");
    // invariant: the second span's status arrives in INT form, so a collapsed fan-out that
    // re-emitted the first span twice would report the wrong level here.
    EXPECT_EQ(results[1]->level, LogLevel::Error);

    EXPECT_EQ(tokenizer.events_produced(), 2U)
        << "the produced counter must follow the fan-out, not the input line count";
}

TEST_F(TokenizerTest, BatchFanOutKeepsSurroundingLinesInPlace)
{
    // invariant: the fan-out is spliced IN POSITION, not appended.
    // invariant: a branch that pushed the unpacked spans after the rest of the batch would still
    // return the right COUNT and still pass a count-only check.
    // invariant: while silently reordering every downstream event id.
    const std::vector<std::string_view> lines{
        R"({"msg":"before"})",
        kSpanDocument,
        R"({"msg":"after"})",
    };
    const auto results{tokenizer.process_batch(lines)};

    ASSERT_EQ(results.size(), 4U) << "3 lines, one of them a 2-span document, must yield 4 results";
    for (std::size_t index{0}; index < results.size(); ++index)
        ASSERT_TRUE(results[index].has_value())
            << "result " << index << ": " << results[index].error();

    EXPECT_EQ(results[0]->template_str, "before");
    EXPECT_EQ(results[1]->template_str, "checkout");
    EXPECT_EQ(results[2]->template_str, "db_query");
    EXPECT_EQ(results[3]->template_str, "after");
}

// invariant: the RECORD entry REFUSES a document, and that entry is the one every live consumer
// uses.
// invariant: document mode is ACQUISITION-tier and does not live there, so the question is what the
// record entry does when a document arrives anyway.
// invariant: it used to answer SILENTLY AND WRONGLY — the span predicate excluded documents, so
// an export carrying N spans fell through to the generic route and produced ONE plausible event.
// invariant: nothing distinguished that event from a real one.
// invariant: this is the PAIRED assertion to the fan-out tests above — the same input, the two
// entries, two DIFFERENT and both-correct answers.
// refs: DN-29.D6
TEST_F(TokenizerTest, RecordEntryRefusesASpanDocumentInsteadOfCollapsingIt)
{
    const auto result{tokenizer.process_line(kSpanDocument)};

    ASSERT_FALSE(result.has_value())
        << "process_line accepted an OTLP export document and produced a single event with "
           "template \""
        << (result.has_value() ? result->template_str : std::string_view{})
        << "\" — a document carrying 2 spans collapsed to 1 plausible event, which is "
           "indistinguishable downstream from a genuine one-line stream";
    EXPECT_NE(result.error().find("resourceSpans"), std::string::npos)
        << "the refusal must name what was refused so a caller can act on it; got: "
        << result.error();
    EXPECT_EQ(tokenizer.events_produced(), 0U) << "a refused line must not count as produced";
}

TEST_F(TokenizerTest, RecordEntryStillAcceptsAFlatSpan)
{
    // invariant: the CONTROL arm — refusing the DOCUMENT must not refuse the shape the wire
    // actually carries.
    // invariant: a refusal that also swallowed flat spans would pass the test above and DELETE the
    // feature.
    const auto result{tokenizer.process_line(
        R"({"traceId":"aabb","spanId":"0001","name":"checkout","kind":"SPAN_KIND_SERVER",)"
        R"("startTimeUnixNano":"1000","endTimeUnixNano":"1500","status":{"code":"STATUS_CODE_UNSET"},)"
        R"("attributes":[{"key":"service.name","value":{"stringValue":"checkout-svc"}}]})")};

    ASSERT_TRUE(result.has_value()) << "a flat span was refused: " << result.error();
    EXPECT_EQ(result->template_str, "checkout");
    EXPECT_EQ(result->component, "checkout-svc");
}

TEST_F(TokenizerTest, BatchCountsEventsProduced)
{
    const std::vector<std::string_view> lines = {
        R"({"msg":"a"})",
        R"({"msg":"b"})",
        R"({"msg":"c"})",
    };
    static_cast<void>(tokenizer.process_batch(lines));
    EXPECT_EQ(tokenizer.events_produced(), lines.size());
}

TEST_F(TokenizerTest, BatchErrorLineDoeNotCountAsProduced)
{
    const std::vector<std::string_view> lines = {
        R"({"msg":"valid"})",
        "",
        R"({"msg":"also valid"})",
    };
    auto results{tokenizer.process_batch(lines)};
    ASSERT_EQ(results.size(), 3u);
    EXPECT_TRUE(results[0].has_value());
    EXPECT_FALSE(results[1].has_value());
    EXPECT_TRUE(results[2].has_value());
    EXPECT_EQ(tokenizer.events_produced(), 2u);
}

TEST_F(TokenizerTest, EmptyLineReturnsError)
{
    auto result{tokenizer.process_line("")};
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(tokenizer.events_produced(), 0u);
}

// invariant: the cluster-count accessor was RETIRED with the clustering it reported — the
// stateless masker has no cluster state.
// refs: SRC-D-TID-3
TEST_F(TokenizerTest, ReportsParsedLineCount)
{
    static_cast<void>(tokenizer.process_line(R"({"msg":"test"})"));
    EXPECT_GE(tokenizer.lines_parsed(), 1u);
}

TEST_F(TokenizerTest, ShortGarbageLineParsesAsRawText)
{
    // invariant: a short garbage line matches no structured strategy, so the raw-text fallback
    // templates it rather than DROPPING it.
    // invariant: that is what lets the wedge ingest unstructured CI logs.
    auto result{tokenizer.process_line("xyz")};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_EQ(ev.level, LogLevel::Unknown);
    EXPECT_FALSE(ev.template_str.empty());
}

TEST_F(TokenizerTest, WhitespaceOnlyLineReturnsError)
{
    auto result{tokenizer.process_line("   ")};
    EXPECT_FALSE(result.has_value());
}

TEST_F(TokenizerTest, UTF8ContentEndToEnd)
{
    // invariant: multi-byte text in a message field must flow through the parser and the masker
    // without corruption and without a crash.
    auto result{tokenizer.process_line(
        R"({"level":"INFO","message":"connexion \u00e9tablie avec succ\u00e8s"})")};
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().template_str.empty());
}

TEST_F(TokenizerTest, BatchAllFourFormatsAllSucceed)
{
    const std::vector<std::string_view> lines = {
        "Jan 15 08:03:22 myhost sshd[1]: Accepted password for user",
        R"({"level":"INFO","component":"api","message":"request served"})",
        "level=WARN component=cache msg=eviction_triggered",
        R"(10.0.0.1 - - [15/Jan/2024:10:30:00 +0000] "GET /status HTTP/1.1" 200 64)",
    };
    auto results{tokenizer.process_batch(lines)};
    ASSERT_EQ(results.size(), 4u);
    for (std::size_t i{0}; i < results.size(); ++i)
    {
        EXPECT_TRUE(results[i].has_value())
            << "Line " << i << " failed: " << (!results[i].has_value() ? results[i].error() : "");
    }
    EXPECT_EQ(tokenizer.events_produced(), 4u);
}

TEST_F(TokenizerTest, InterleaveErrorsDoNotCorruptTemplateIds)
{
    // invariant: a valid line, an error, then a structurally identical valid line — the third
    // line's template must equal the first's.
    // invariant: the intervening error does not perturb the STATELESS masker.
    auto r1{tokenizer.process_line(R"({"msg":"worker job started"})")};
    ASSERT_TRUE(r1.has_value());

    auto rErr{tokenizer.process_line("")};
    EXPECT_FALSE(rErr.has_value());

    auto r3{tokenizer.process_line(R"({"msg":"worker job started"})")};
    ASSERT_TRUE(r3.has_value());
    EXPECT_EQ(r1.value().template_str, r3.value().template_str);
}

TEST_F(TokenizerTest, JSONWithKVContentMaskedStatelessly)
{
    // invariant: the strategy extracts the message and the masker classifies each token by its OWN
    // content.
    // invariant: a key-value pair is a single letter-leading token and is KEPT literal, because a
    // varying value-WORD is not a syntactic high-cardinality class.
    // invariant: masking it needs the unbuilt semantic class registry, so two lines differing only
    // in a value-word are DISTINCT templates.
    // invariant: that is the accepted stateless OVER-SPLIT, and NOT the old cross-line wildcard.
    // refs: SRC-D-TID-14
    auto ra{tokenizer.process_line(R"({"msg":"action=login user=alice status=ok"})")};
    auto rb{tokenizer.process_line(R"({"msg":"action=login user=bob status=ok"})")};
    ASSERT_TRUE(ra.has_value() && rb.has_value());
    EXPECT_NE(ra.value().template_str.find("action=login"), std::string::npos);
    EXPECT_NE(ra.value().template_str, rb.value().template_str)
        << "a varying KV value-word stays literal (no cross-line wildcard): "
        << ra.value().template_str << " vs " << rb.value().template_str;
    // invariant: a digit-leading value as its own token still masks per-line.
    auto rn{tokenizer.process_line(R"({"msg":"served 500 status=ok"})")};
    ASSERT_TRUE(rn.has_value());
    EXPECT_NE(rn.value().template_str.find("<*>"), std::string::npos);
}

TEST_F(TokenizerTest, HighVolumeTemplateStabilisesParams)
{
    // invariant: the numeric position is a VARIABLE count and not a status value, so it masks
    // per-line and a param is extracted.
    // invariant: a status value behind a status keyword would instead be KEPT distinct, and the
    // noun here is not a status keyword.
    const std::vector<std::string_view> lines = {
        R"({"msg":"fetched 200 rows returned"})", R"({"msg":"fetched 201 rows returned"})",
        R"({"msg":"fetched 400 rows returned"})", R"({"msg":"fetched 404 rows returned"})",
        R"({"msg":"fetched 500 rows returned"})", R"({"msg":"fetched 503 rows returned"})",
        R"({"msg":"fetched 301 rows returned"})", R"({"msg":"fetched 204 rows returned"})",
    };
    auto results{tokenizer.process_batch(lines)};
    ASSERT_EQ(results.size(), 8u);

    bool any_param_found{false};
    for (auto& r : results)
    {
        if (r.has_value() && !r.value().params.empty())
        {
            any_param_found = true;
            break;
        }
    }
    EXPECT_TRUE(any_param_found);
}

TEST_F(TokenizerTest, NonLatinUnicodeEndToEnd)
{
    // invariant: non-Latin codepoints are valid multi-byte text and must pass through the parser
    // and the masker without corruption or crash.
    auto r1{tokenizer.process_line(
        R"({"level":"INFO","message":"\u062a\u0633\u062c\u064a\u0644 \u0645\u0633\u062a\u062e\u062f\u0645"})")};
    ASSERT_TRUE(r1.has_value());
    EXPECT_FALSE(r1.value().template_str.empty());

    auto r2{tokenizer.process_line(
        R"({"level":"INFO","message":"\u0432\u0445\u043e\u0434 \u043f\u043e\u043b\u044c\u0437\u043e\u0432\u0430\u0442\u0435\u043b\u044f"})")};
    ASSERT_TRUE(r2.has_value());
    EXPECT_FALSE(r2.value().template_str.empty());

    auto r3{tokenizer.process_line(R"({"level":"INFO","message":"\u767b\u5f55\u6210\u529f"})")};
    ASSERT_TRUE(r3.has_value());
    EXPECT_FALSE(r3.value().template_str.empty());
}

TEST_F(TokenizerTest, EmojiContentEndToEnd)
{
    // invariant: emoji codepoints, whether encoded as surrogate pairs or directly, must survive the
    // parser and the masker without crashing.
    auto r1{tokenizer.process_line(
        R"({"level":"INFO","message":"deploy \ud83d\ude80 succeeded \u2705"})")};
    ASSERT_TRUE(r1.has_value());
    EXPECT_FALSE(r1.value().template_str.empty());

    // invariant: two structurally identical emoji lines must receive the SAME template.
    auto r2{tokenizer.process_line(
        R"({"level":"INFO","message":"deploy \ud83d\ude80 succeeded \u2705"})")};
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r1.value().template_str, r2.value().template_str);
}
