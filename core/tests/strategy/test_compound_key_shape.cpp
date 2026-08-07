// NOLINTBEGIN — Unit tests: allow short identifiers and test-specific patterns
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

// Compound-key SHAPE resolution (DN-30) — the canon-grain half of the ECS gap.
//
// THE DEFECT, MEASURED BEFORE THE FIX. Canon's role allowlists are flat: `level`, `severity`,
// `component`, `service`, `message`… An ECS document spells its roles as COMPOUND keys — flat
// dotted (`log.level`, `service.name`) or nested (`log:{level}`, `service:{name}`) — so none of
// them matched. A 200-line ECS stream parsed with its message intact and its severity and service
// silently dropped: every record read as level Unknown with no component. A full production
// severity inversion of such a stream produced ZERO signal.
//
// WHY THIS FILE ASSERTS A SHAPE AND NOT A VOCABULARY — read this before adding a case.
//
// The obvious test is `{"log.level":"error"} → Error`. It is nearly worthless: it passes the
// moment someone appends the literal string "log.level" to the flat allowlist, which fixes ECS
// and nothing else, and leaves the next compound-key format (`data_stream.dataset`, a GELF or
// Bunyan namespace, an in-house convention) exactly as broken. A green built that way certifies a
// spelling, not a capability.
//
// So every case below uses a namespace that appears in NO specification and in no allowlist:
// `zzz`. Nothing can make these pass except a rule that reads the key's STRUCTURE — split the
// compound key, match its LAST segment against the role names canon already owns. If someone
// "fixes" this by adding field names, these tests stay red, which is the entire point of them.
//
// The ECS spellings themselves are asserted at the END, as the acceptance case — they must pass
// as a CONSEQUENCE of the shape rule, never as its cause.
//
// Determinism: literal inputs, one arena per case, no RNG, no clock, no shared state.
namespace
{

// A JSON record carrying the given role fragment, with a timestamp and message that always
// resolve — so a failure below is unambiguously the role under test and never a parse failure.
[[nodiscard]] std::string record_with(std::string_view role_fragment)
{
    return std::string{R"({"@timestamp":"2026-03-30T10:00:00.000Z",)"} +
           std::string{role_fragment} + R"(,"message":"connection pool exhausted"})";
}

} // namespace

// ── The LEVEL role, both compound shapes, under a namespace no spec defines ───────────────────

TEST(CompoundKeyShape, FlatDottedKeyResolvesItsLastSegmentToTheLevelRole)
{
    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto parsed{strategy.parse(record_with(R"("zzz.level":"error")"), arena)};
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->level, LogLevel::Error)
        << "a flat dotted compound key whose LAST segment is a known role name did not resolve. "
           "`zzz` is in no specification and no allowlist, so the only thing that can make this "
           "pass is a rule that reads the key's SHAPE — if this is red while an ECS-spelled case "
           "is green, a field NAME was added instead of a shape rule, and every other "
           "compound-key format is still broken";
}

TEST(CompoundKeyShape, NestedObjectResolvesItsLeafToTheLevelRole)
{
    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto parsed{strategy.parse(record_with(R"("zzz":{"level":"error"})"), arena)};
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->level, LogLevel::Error)
        << "a nested single-level object whose leaf key is a known role name did not resolve. The "
           "two wire shapes are equally legal ECS (libraries emit flat, collectors emit nested), "
           "so a fix that handles only one leaves half the ecosystem unreadable";
}

TEST(CompoundKeyShape, BothCompoundShapesAgreeWithEachOtherAndWithTheFlatSpelling)
{
    JsonStrategy strategy;
    ArenaAllocator arena{8192};

    const auto flat_key{strategy.parse(record_with(R"("zzz.level":"error")"), arena)};
    const auto nested{strategy.parse(record_with(R"("zzz":{"level":"error"})"), arena)};
    const auto plain{strategy.parse(record_with(R"("level":"error")"), arena)};
    ASSERT_TRUE(flat_key.has_value() && nested.has_value() && plain.has_value());

    // Stated as equality between the three shapes rather than as three separate constants: the
    // property is that the SHAPE does not change the meaning, and an equality cannot drift the way
    // three hand-written expectations can.
    EXPECT_EQ(flat_key->level, plain->level)
        << "the dotted shape disagrees with the plain spelling of the same role";
    EXPECT_EQ(nested->level, plain->level)
        << "the nested shape disagrees with the plain spelling of the same role";
}

// ── The COMPONENT role — a second role, so the rule cannot be level-special ───────────────────

TEST(CompoundKeyShape, TheShapeRuleAppliesToEveryRoleNotJustLevel)
{
    JsonStrategy strategy;
    ArenaAllocator arena{8192};

    const auto dotted{strategy.parse(record_with(R"("zzz.component":"payments")"), arena)};
    const auto nested{strategy.parse(record_with(R"("zzz":{"component":"payments"})"), arena)};
    ASSERT_TRUE(dotted.has_value() && nested.has_value());

    EXPECT_EQ(dotted->component, "payments")
        << "the shape rule resolved a compound LEVEL but not a compound COMPONENT — it was "
           "implemented per-role rather than over the role allowlists canon already owns";
    EXPECT_EQ(nested->component, "payments") << "same, for the nested shape";
}

// ── The BOUND — one level of descent, and no more ─────────────────────────────────────────────
//
// A positive statement of the declared boundary, not a defect (MEM:clean-code-is-a-valid-finding).
// Unbounded descent would make every JSON document a search space and put an unbounded walk on the
// hot record path, which is the cost ADR-29.D7 refuses elsewhere for the same reason.

TEST(CompoundKeyShape, DescentIsBoundedAtOneLevelSoDeepNestingIsNotClaimed)
{
    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto parsed{strategy.parse(record_with(R"("a":{"b":{"level":"error"}})"), arena)};
    ASSERT_TRUE(parsed.has_value());
    EXPECT_NE(parsed->level, LogLevel::Error)
        << "a role two levels deep was claimed. The bound is ONE level of descent: deeper is a "
           "search, and a search on the record path is an unbounded walk on every JSON line of "
           "every stream. This is a declared boundary, not a gap — if it must change, the cost is "
           "measured first";
}

TEST(CompoundKeyShape, ADeepDottedKeyIsNotClaimedEither)
{
    JsonStrategy strategy;
    ArenaAllocator arena{4096};

    const auto parsed{strategy.parse(record_with(R"("a.b.level":"error")"), arena)};
    ASSERT_TRUE(parsed.has_value());
    EXPECT_NE(parsed->level, LogLevel::Error)
        << "a multi-segment dotted key was claimed. The dotted and nested shapes must share ONE "
           "bound, or the same logical document is read differently depending on which wire form "
           "its producer chose";
}

// ── CARDINALITY SAFETY on the WHERE axis — an invariant ALREADY IN FORCE ──────────────────────
//
// This is not a bet on how compound-namespace resolution gets ruled. `component` is the WHERE tier:
// it becomes the per-template WHERE label and feeds the cube's WHERE axis. ADR-29.D3 already
// refuses putting a producer-controlled unbounded value there — that is the argument for not
// retaining a `trace_id`, and it does not depend on any rule chosen later. So this asserts a
// constraint that exists TODAY and that whatever lands must not break.
//
// THE CONCRETE HAZARD, and it is not hypothetical: `source` is ALREADY a component role word in
// canon's allowlist (component / source / logger / service / module / logGroup / logStream). So a
// resolution rule that lets a NAMESPACE carry the role, and then takes the namespace's first string
// child as the value, reads `"source":{"ip":"10.0.0.7"}` as component = "10.0.0.7". That is
// deterministic, plausible, and puts one WHERE label per client IP on the axis. The same shape
// waits under `host.ip`, `user.name`, `url.path`.
//
// STATED AS A CARDINALITY PROPERTY, deliberately — not as "the component must not equal an IP".
// A value-shaped assertion is a spelling test and dies to the first format that carries an
// unbounded value that does not look like an IP. What actually matters is that the number of
// distinct WHERE labels must NOT grow with the number of distinct producer-controlled values. That
// is measurable without naming any field, any format, or any rule.
TEST(CompoundKeyShape, AProducerControlledValueNeverBecomesAWhereLabel)
{
    constexpr int kDistinctProducerValues{256};

    JsonStrategy strategy;
    ArenaAllocator arena{1U << 20U};

    std::set<std::string> distinct_components;
    for (int index{0}; index < kDistinctProducerValues; ++index)
    {
        // Every record is identical except the value under a component-role NAMESPACE. Nothing
        // here is ECS-specific: it is the shape any compound-namespace rule must survive.
        const std::string line{
            std::string{R"({"@timestamp":"2026-03-30T10:00:00.000Z","level":"info",)"} +
            R"("source":{"ip":"10.0.)" + std::to_string(index / 256) + "." +
            std::to_string(index % 256) + R"("},"message":"request served"})"};

        const auto parsed{strategy.parse(line, arena)};
        ASSERT_TRUE(parsed.has_value()) << "record " << index << " failed to parse";
        distinct_components.insert(std::string{parsed->component});
    }

    EXPECT_LE(distinct_components.size(), 1U)
        << "the WHERE axis grew to " << distinct_components.size() << " distinct labels across "
        << kDistinctProducerValues
        << " records that differ ONLY in a producer-controlled value.\n"
           "    A compound-namespace rule has taken the namespace's child value as the component. "
           "`source` is a component role word, so `source.ip` resolves — and the WHERE axis now "
           "carries one label per client IP.\n"
           "    That is the unbounded-value class ADR-29.D3 refuses, and it is unbounded in the "
           "worst way: the values are chosen by whoever sends us traffic, not by the operator.\n"
           "    The constraint predates any compound-key rule and is not negotiable by one — "
           "resolution must not reach a value the producer controls without a bound.";
}

// ── ACCEPTANCE + THE DECLARED BOUNDARY: real ECS, and the half we deliberately do not read ────
//
// FIELD-position resolution SHIPS (`log.level` — the role word is the FIELD). NAMESPACE-position
// resolution is REFUSED (DN-30.D11), so `service.name` does NOT populate `component`. These arms
// state that as a BOUNDARY rather than leaving it as a caveat in prose — the same species as the
// descent bound above, which found a real defect precisely because it was written as a positive
// statement rather than assumed.
//
// WHY REFUSED, so nobody reads these as a gap waiting to be closed. On our vocabularies
// `service.name` and `source.ip` are STRUCTURALLY IDENTICAL — a role-word namespace plus a field
// in no vocabulary. The information that would separate them is not present in the instrument, so
// a rule admitting the first admits the second. Counting children does not rescue it: "exactly one
// string child" FAILS on LogCraft's OWN emitted ECS (`{"name":…,"type":…}` — two string children)
// and still ADMITS the trap (`{"ip":…,"port":54321}` — `port` is numeric, so `ip` is the only
// string). And the struct already drew this line: `component` declares itself low-cardinality and
// a cube dimension, while `host` exists beside it for node identity, outside the cube. So
// `source.ip → component` is not a cardinality RISK — it puts a host-class value into the one
// field that declares it is not one.
//
// DN-30.D5's zero-new-field-names clause stands: a red arm is not a licence to add a spelling.

TEST(CompoundKeyShape, EcsFlatLibraryShapeResolvesTheFieldPositionRoleAndDeclinesTheNamespaceOne)
{
    JsonStrategy strategy;
    ArenaAllocator arena{8192};

    // The form ecs-logging-{java,python,nodejs} emit.
    const auto parsed{
        strategy.parse(R"({"@timestamp":"2026-03-30T10:00:00.000Z","log.level":"ERROR",)"
                       R"("message":"connection pool exhausted","ecs.version":"8.11.0",)"
                       R"("service.name":"checkout-api","event.dataset":"checkout.log"})",
                       arena)};
    ASSERT_TRUE(parsed.has_value());

    EXPECT_EQ(parsed->level, LogLevel::Error)
        << "`log.level` carries the role word on the FIELD — the shipped case, it must resolve";
    EXPECT_EQ(parsed->content, "connection pool exhausted");

    EXPECT_TRUE(parsed->component.empty())
        << "`service.name` populated component with \"" << parsed->component
        << "\". Namespace-position resolution is REFUSED (DN-30.D11): it is structurally "
           "indistinguishable from `source.ip`, which would put a host-class value into the field "
           "that declares itself a low-cardinality cube dimension. A red here means either the "
           "refusal was reversed without this boundary being restated, or a field NAME was added — "
           "and DN-30.D5 forbids the second outright.";
}

TEST(CompoundKeyShape,
     EcsNestedCollectorShapeResolvesTheFieldPositionRoleAndDeclinesTheNamespaceOne)
{
    JsonStrategy strategy;
    ArenaAllocator arena{8192};

    // Byte-faithful to logcraft's own fmt_ecs() emission (log_formatter_cloud.cpp) — the exact
    // stream the seam arm drives, so this unit case and the seam case cannot drift apart. Note
    // `service` carries TWO string children (name, type): this record IS the counterexample that
    // kills the "exactly one string child" escape.
    const auto parsed{
        strategy.parse(R"({"@timestamp":"2026-03-30T10:00:00.000Z","ecs":{"version":"8.11.0"},)"
                       R"("log":{"level":"error"},"message":"connection pool exhausted",)"
                       R"("service":{"name":"checkout-api","type":"web_server"}})",
                       arena)};
    ASSERT_TRUE(parsed.has_value());

    EXPECT_EQ(parsed->level, LogLevel::Error)
        << "`log:{level}` carries the role word on the FIELD — the shipped case";
    EXPECT_EQ(parsed->content, "connection pool exhausted");

    EXPECT_TRUE(parsed->component.empty())
        << "`service:{name}` populated component with \"" << parsed->component
        << "\" — see the flat-shape arm above; the refusal is the ruling, not an omission.";
}

// The second acceptance criterion, at the unit grain: an ECS record is UNDERSTOOD, so the L2
// witness marker must be empty. Before the fix an ECS line yielded a message but no level and no
// component; the marker measures "no role at all", so this is the durable, vocabulary-free
// statement that the stream stopped being opaque — and it is the one that runs on a user's real
// stream rather than on our corpus.
TEST(CompoundKeyShape, AnEcsRecordLeavesNoRoleWitnessMarkerBehind)
{
    JsonStrategy strategy;
    ArenaAllocator arena{8192};

    const auto parsed{
        strategy.parse(R"({"@timestamp":"2026-03-30T10:00:00.000Z","ecs":{"version":"8.11.0"},)"
                       R"("log":{"level":"error"},"message":"connection pool exhausted",)"
                       R"("service":{"name":"checkout-api","type":"web_server"}})",
                       arena)};
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->no_role_witness_key.empty())
        << "an ECS record still reports that NO role was recognized — the marker names \""
        << parsed->no_role_witness_key
        << "\". The marker going quiet on ECS lines IS this fix's success criterion; while it "
           "fires, the record path is still telling the caller it understood nothing";
}

// NOLINTEND
