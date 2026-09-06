
// invariant: the declaration contract's SHAPE arm, homed as a canon-core unit suite — the
// degenerate declaration's shape, the catalogue's contract, and fail-closed resolution.
// invariant: every one of those is a property of ONE component over bytes this file authors, so it
// needs no seam and no corpus.
// invariant: homing it as a corpus gate would have been the mis-homing that discipline exists to
// stop — real bytes cannot prove a property that is about the EMPTY stack.
// invariant: FALSIFIABILITY is a REQUIREMENT here rather than an aside — the property must be
// OBSERVED red under a one-byte mutation, never merely asserted red-capable.
// invariant: the-empty-stack-changes-nothing is exactly the claim shape that passes for the wrong
// reason — it stays green if the peel is hard-coded to return its argument for EVERY stack.
// invariant: so the discharge is a PERMANENT ARM rather than a manual mutation: the same table runs
// through a DECLARED stack and must DIFFER wherever a line carries a stamp.
// invariant: the two arms fail in OPPOSITE directions, so no single implementation satisfies both
// by accident — that is what makes the empty-stack green mean something.
// invariant: what it does NOT discharge is the mutation observation on the corpus arm, and no
// sentence here may be read as having paid it.
// invariant: that debt is PAID, and not by this file — the peel-equivalence gate beside it ran
// three peel-path mutations and reverted each.
// invariant: byte-only determinism — a fixed authored table, no randomness, no clock, no float,
// and no allocation on the asserted path.
#include <gtest/gtest.h>

import insight.canon.test;

using insight::transport::find_transform;
using insight::transport::IngestDeclaration;
using insight::transport::kGhaApiPrefixWidth;
using insight::transport::kTransportCatalogRows;
using insight::transport::kTransportCatalogVersion;
using insight::transport::RawPeeledLine;
using insight::transport::resolve_transport_stack;
using insight::transport::TransportExtract;
using insight::transport::TransportStack;
using insight::transport::TransportTransformKind;

namespace
{

// invariant: the catalogue name is spelled as a LITERAL rather than read from the catalogue — a
// test that looks the name up from the thing under test cannot notice the name changing.
constexpr std::string_view kGhaTransform{"api-rfc3339-line-prefix"};

// invariant: the table is authored ADVERSARIAL to the identity claim, not merely representative,
// and each row names why it is here.
// invariant: the awkward rows are where a peel that TRUSTED its declared width rather than
// requiring the grammar to FILL it would corrupt bytes.
// invariant: they are also where a borrowed view mishandled as a null-terminated string would
// truncate.
struct LineCase
{
    std::string_view label;
    std::string_view bytes;
    bool carries_stamp;
};

constexpr std::string_view kStamp{"2026-04-15T22:20:38.2879579Z"};

// invariant: the embedded null is built with an EXPLICIT LENGTH so the view carries it rather than
// stopping at it — real logs carry these, which is why the corpus is read in binary.
// invariant: a peel that round-tripped through a null-terminated string would silently truncate
// here while every other row stayed green.
constexpr std::string_view kNulBearing{"2026-04-15T22:20:38.2879579Z ok\0after-nul", 41U};

const std::array<LineCase, 15> kLines{{
    {.label = "empty", .bytes = {}, .carries_stamp = false},
    {.label = "single space", .bytes = " ", .carries_stamp = false},
    {.label = "plain content", .bytes = "hello world", .carries_stamp = false},
    {.label = "stamped + content",
     .bytes = "2026-04-15T22:20:38.2879579Z ok",
     .carries_stamp = true},
    {.label = "stamped, no trailing space",
     .bytes = "2026-04-15T22:20:38.2879579Z",
     .carries_stamp = true},
    {.label = "stamped + tab separator",
     .bytes = "2026-04-15T22:20:38.2879579Z\tok",
     .carries_stamp = true},
    {.label = "stamped + indented content",
     .bytes = "2026-04-15T22:20:38.2879579Z    indented",
     .carries_stamp = true},
    // invariant: the leading byte-order-mark row is a live shipped defect present as a CASE, not a
    // fix — the empty stack leaves it alone and the declared stack must NOT claim it.
    // invariant: pinning the current behaviour is what lets the mark ruling land later without this
    // file quietly agreeing in advance.
    // refs: DN-25
    {.label = "BOM then stamp",
     .bytes = "\xEF\xBB\xBF"
              "2026-04-15T22:20:38.2879579Z ok",
     .carries_stamp = false},
    {.label = "NUL-bearing after stamp", .bytes = kNulBearing, .carries_stamp = true},
    {.label = "lone CR (no LF)", .bytes = "carriage\rreturn", .carries_stamp = false},
    // invariant: shorter than the declared width — a width-trusting peel would read past the end
    // or return garbage, and shape-checking returns the line untouched.
    {.label = "sub-width digits", .bytes = "2026-04-15T22", .carries_stamp = false},
    // invariant: correct in the invariant head and WRONG in the declared tail, so the complete
    // datetime is shorter than the declared width and the row's effect is nothing.
    // invariant: this is the ONE fixture the acceptor tightening was PRE-REGISTERED to move — it
    // asserted the opposite while the width was trusted rather than checked.
    // invariant: the flag stays a MEASURED property of the shipped algorithm and never
    // looks-like-a-stamp-to-a-human.
    {.label = "near-miss stamp (short fraction)",
     .bytes = "2026-04-15T22:20:38.287957 ok",
     .carries_stamp = false},
    // invariant: the third arm of the same root, and the oldest — a whole-second stamp whose head
    // is valid, on a line long enough that a width-trusting acceptor ate real content.
    // invariant: it was found on a real fixture, written down in a downstream comment and
    // compensated for THERE instead of being closed here.
    // invariant: nobody was hunting this case, which is exactly why it is pinned.
    {.label = "whole-second stamp, syslog payload",
     .bytes = "2024-01-15T10:30:00Z host1 myapp[123]: connection refused",
     .carries_stamp = false},
    {.label = "high-bit / non-UTF8 bytes", .bytes = "\xFF\xFE raw bytes", .carries_stamp = false},
    {.label = "whitespace only", .bytes = "   \t  ", .carries_stamp = false},
}};

// invariant: bytes are escaped for the failure message, because a null or a lone carriage return
// printed raw would corrupt the very diagnostic that has to explain the failure.
[[nodiscard]] std::string escape(std::string_view bytes)
{
    std::string out;
    out.reserve(bytes.size() + 8U);
    for (const char chr : bytes)
    {
        const auto raw{static_cast<unsigned char>(chr)};
        if (raw == '\\')
            out += "\\\\";
        else if (raw >= 0x20U && raw < 0x7FU)
            out += chr;
        else
        {
            constexpr std::string_view kHexDigits{"0123456789ABCDEF"};
            out += "\\x";
            out += kHexDigits[raw >> 4U];
            out += kHexDigits[raw & 0x0FU];
        }
    }
    return out;
}

// invariant: the backing array must OUTLIVE the declaration, because the stack field is a span and
// a temporary would dangle.
constexpr std::array<std::string_view, 1> kDeclaredGha{{kGhaTransform}};

TEST(TransportDeclaration, DefaultDeclarationResolvesToTheDegenerateStack)
{
    const IngestDeclaration declaration{};
    const TransportStack stack{resolve_transport_stack(declaration)};

    EXPECT_TRUE(stack.empty()) << "a default-constructed IngestDeclaration must resolve to the "
                                  "degenerate stack — declaring is SUBTRACTIVE: a caller who "
                                  "says nothing loses nothing. Got size "
                               << stack.size();
    EXPECT_EQ(stack.size(), 0U);

    // invariant: a default-constructed stack and a resolved-from-empty one must be the SAME thing,
    // or today's behaviour would depend on which door the caller came through.
    const TransportStack default_constructed{};
    EXPECT_EQ(default_constructed.empty(), stack.empty());
    EXPECT_EQ(default_constructed.size(), stack.size());
}

TEST(TransportDeclaration, DegenerateStackPeelIsByteIdentity)
{
    const TransportStack stack{resolve_transport_stack(IngestDeclaration{})};

    for (std::size_t idx{0}; idx < kLines.size(); ++idx)
    {
        const LineCase& line{kLines[idx]};
        const RawPeeledLine peeled{stack.peel_raw(line.bytes)};

        EXPECT_EQ(peeled.content, line.bytes)
            << "case [" << idx << "] '" << line.label << "': the degenerate peel changed bytes.\n"
            << "  in : \"" << escape(line.bytes) << "\" (" << line.bytes.size() << " B)\n"
            << "  out: \"" << escape(peeled.content) << "\" (" << peeled.content.size() << " B)";

        // invariant: byte-EQUAL is not enough — the peel must BORROW and never copy, and POINTER
        // IDENTITY is the only assertion that actually holds that.
        // invariant: a copy would compare equal and silently add an allocation per line.
        EXPECT_EQ(peeled.content.data(), line.bytes.data())
            << "case [" << idx << "] '" << line.label
            << "': the degenerate peel returned a COPY, not a borrowed view of the caller's buffer "
               "(canon.transport.cppm — `peel` only ever SHORTENS, so no allocation is involved).";

        EXPECT_FALSE(peeled.observation_time.has_value())
            << "case [" << idx << "] '" << line.label
            << "': the degenerate stack declares no extract, so it must set no observation time.";

        EXPECT_EQ(peeled.is_blank(), line.bytes.empty())
            << "case [" << idx << "] '" << line.label
            << "': is_blank() must mean 'peeled to empty' "
            << "and nothing else — content was \"" << escape(peeled.content) << "\"";
    }
}

// invariant: the arm that makes the two above NON-VACUOUS — the same table through a DECLARED
// stack, requiring the result to DIFFER on every stamped line.
// invariant: a peel hard-coded to return its argument, which is the mutation that leaves the
// degenerate arms green, fails HERE and loudly.
// invariant: the two arms are OPPOSED by construction: one demands byte-identity under the empty
// stack and this one a byte CHANGE under a declared one, over identical inputs.
TEST(TransportDeclaration, DeclaredStackActuallyPeels)
{
    const IngestDeclaration declaration{.stack = kDeclaredGha, .dialect = {}, .channel = {}};
    const TransportStack stack{resolve_transport_stack(declaration)};
    ASSERT_EQ(stack.size(), 1U) << "the declared stack must resolve exactly one row";
    ASSERT_FALSE(stack.empty());

    std::size_t shortened{0};
    for (std::size_t idx{0}; idx < kLines.size(); ++idx)
    {
        const LineCase& line{kLines[idx]};
        const RawPeeledLine peeled{stack.peel_raw(line.bytes)};

        if (line.carries_stamp)
        {
            ++shortened;
            EXPECT_LT(peeled.content.size(), line.bytes.size())
                << "case [" << idx << "] '" << line.label
                << "': a declared api-rfc3339-line-prefix stack must SHORTEN a stamped line. If "
                   "this "
                   "is green while DegenerateStackPeelIsByteIdentity is also green, `peel` is a "
                   "no-op for every stack and the transport vocabulary does nothing.\n"
                << "  in : \"" << escape(line.bytes) << "\"\n"
                << "  out: \"" << escape(peeled.content) << "\"";
            EXPECT_FALSE(peeled.content.starts_with(kStamp.substr(0U, 4U)))
                << "case [" << idx << "] '" << line.label << "': the stamp survived the peel: \""
                << escape(peeled.content) << "\"";
        }
        else
        {
            // invariant: TOTALITY IS ABOUT APPLICATION, NOT EFFECT — the row is applied to every
            // line unconditionally, and on these bytes its effect is nothing.
            // invariant: that is the declared rule's effect being nothing, and NOT the transform
            // asking whether the line is its own.
            EXPECT_EQ(peeled.content, line.bytes)
                << "case [" << idx << "] '" << line.label
                << "': the declared row must leave an unstamped line untouched — the row applies "
                   "to every line, and on these bytes its EFFECT is nothing; it does not get to "
                   "decline.\n"
                << "  in : \"" << escape(line.bytes) << "\"\n"
                << "  out: \"" << escape(peeled.content) << "\"";

            // invariant: the EXTRACT is the other half of the rule's effect being nothing, and it
            // is the half a downstream probe was built to lean on.
            // invariant: acquisition counts a row as evidence only when the peel SHORTENED the line
            // AND the extract parsed, precisely because the acceptor used to trust its width.
            // invariant: asserting it here makes that bar's discriminating power a property of THIS
            // component rather than a claim in a comment one repo away.
            EXPECT_FALSE(peeled.observation_time.has_value())
                << "case [" << idx << "] '" << line.label
                << "': the row declined these bytes, so it must publish no observation time; a "
                   "time extracted from a span the rule did not claim is a datum invented out of "
                   "content.\n"
                << "  in : \"" << escape(line.bytes) << "\"";
        }

        // invariant: the peel SHORTENS and never rewrites, so whatever it returns is a SUFFIX of
        // the input sharing its buffer.
        // invariant: that holds on both branches and is what keeps the content safe to hand to the
        // tokenizer without a copy.
        EXPECT_GE(peeled.content.data(), line.bytes.data())
            << "case [" << idx << "] '" << line.label << "': peel returned a view outside the "
            << "caller's buffer";
        EXPECT_LE(peeled.content.data() + peeled.content.size(),
                  line.bytes.data() + line.bytes.size())
            << "case [" << idx << "] '" << line.label << "': peel returned a view running past the "
            << "end of the caller's buffer";
    }

    // invariant: the shortened count is PINNED, because a table that stopped exercising the
    // shortening path would make this whole arm vacuous while every assertion stayed green.
    constexpr std::size_t kStampedCases{5};
    EXPECT_EQ(shortened, kStampedCases)
        << "the line table must keep exercising the shortening path; if a case was edited out, "
           "this arm stops falsifying anything and the degenerate green becomes meaningless";
}

TEST(TransportDeclaration, DeclaredStackExtractsObservationTimeOnlyWhenTheStampParses)
{
    const IngestDeclaration declaration{.stack = kDeclaredGha, .dialect = {}, .channel = {}};
    const TransportStack stack{resolve_transport_stack(declaration)};

    const RawPeeledLine stamped{stack.peel_raw("2026-04-15T22:20:38.2879579Z ok")};
    EXPECT_EQ(stamped.content, "ok") << "declared peel must strip the stamp AND the separator "
                                        "space (strip_leading_space is load-bearing)";
    EXPECT_TRUE(stamped.observation_time.has_value())
        << "the shipped row declares TransportExtract::EventObservationTime, so a parseable stamp "
           "must yield one";

    const RawPeeledLine unstamped{stack.peel_raw("no stamp here")};
    EXPECT_EQ(unstamped.content, "no stamp here");
    EXPECT_FALSE(unstamped.observation_time.has_value())
        << "a line the rule did not shorten must carry no observation time";

    // invariant: a line that is ENTIRELY transport peels to empty, and empty means DROP rather than
    // an empty template.
    // invariant: that is how the shipped strategy's timestamp-only-is-a-blank-line behaviour
    // survives the move to a declared peel.
    const RawPeeledLine bare{stack.peel_raw("2026-04-15T22:20:38.2879579Z ")};
    EXPECT_TRUE(bare.is_blank()) << "a stamp-only line must peel to blank, got \""
                                 << escape(bare.content) << "\"";
}

TEST(TransportCatalog, ShippedRowsAreExactlyWhatTheCatalogDeclares)
{
    // invariant: the catalogue's rows are pinned by COUNT, because this workspace does not ship
    // enum members with no algorithm, no row and no gate.
    // invariant: each row landed WITH its algorithm, its row and its gate, so adding a member
    // without them fails here — the anti-dormant rule with teeth.
    ASSERT_EQ(kTransportCatalogRows.size(), 3U)
        << "a new catalogue row is a catalogue-VERSION bump landing WITH its algorithm and its "
           "gate. If you are here because you added one, bump kTransportCatalogVersion and add "
           "its arm — the version is part of every composed semantic_identity.";

    const auto& row{kTransportCatalogRows[0]};
    EXPECT_EQ(row.name, kGhaTransform);
    EXPECT_EQ(row.kind, TransportTransformKind::LinePrefixTimestamp);
    EXPECT_EQ(row.extract, TransportExtract::EventObservationTime);
    EXPECT_EQ(row.prefix_width, kGhaApiPrefixWidth);
    EXPECT_EQ(row.prefix_width, 28U) << "\"YYYY-MM-DDTHH:MM:SS.fffffffZ\" is 28 bytes";
    EXPECT_TRUE(row.strip_leading_space);

    const auto& bracket_row{kTransportCatalogRows[1]};
    EXPECT_EQ(bracket_row.name, "bracket-rfc3339-line-prefix");
    EXPECT_EQ(bracket_row.kind, TransportTransformKind::LinePrefixBracketedTimestamp);
    EXPECT_EQ(bracket_row.extract, TransportExtract::EventObservationTime);
    EXPECT_EQ(bracket_row.prefix_width, 0U)
        << "the bracketed form is VARIABLE width — the field is unread (the LocationRow "
           "unread-parameter precedent) and 0 says so";
    EXPECT_TRUE(bracket_row.strip_leading_space)
        << "the bracketed row must strip the separator run too — bundled behavior #3 (the greedy "
           "[ \\t]+ strip) is reproduced by the declared row, byte-exactly";

    // invariant: the catalogue version is a component of every composed semantic identity, and it
    // is pinned as a LITERAL.
    // invariant: a silent bump would move every digest in the workspace, and a test that read the
    // constant back from the constant could never say so.
    EXPECT_EQ(kTransportCatalogVersion, "transport-catalog-3");
}

TEST(TransportCatalog, NamesAreUniqueAndLookupRoundTrips)
{
    for (std::size_t i{0}; i < kTransportCatalogRows.size(); ++i)
    {
        const auto& row{kTransportCatalogRows[i]};
        const auto* found{find_transform(row.name)};
        ASSERT_NE(found, nullptr) << "row [" << i << "] '" << row.name
                                  << "' is in the catalogue but find_transform cannot see it";
        EXPECT_EQ(found, &row) << "row [" << i << "] '" << row.name
                               << "' resolved to a DIFFERENT row — the names are not unique, so a "
                                  "declaration silently selects the first collision";
    }

    EXPECT_EQ(find_transform("no-such-transform"), nullptr);
    EXPECT_EQ(find_transform(""), nullptr)
        << "an empty name must not resolve — an ABSENT stack is expressed by an empty SPAN, never "
           "by an empty name";
}

TEST(TransportDeclarationDeathTest, UnknownTransformFailsClosedNamingTheCatalog)
{
    // invariant: canon VERIFIES a declared coordinate and never infers it — an UNKNOWN name is a
    // MISTAKE and fails closed, where an ABSENT name is a CHOICE and degrades.
    // invariant: the two must never share a code path, so the death message is part of the contract
    // rather than decoration.
    constexpr std::array<std::string_view, 1> kTypo{{"gha-api-line-prefx"}};
    const IngestDeclaration declaration{.stack = kTypo, .dialect = {}, .channel = {}};

    EXPECT_DEATH(
        { (void)resolve_transport_stack(declaration); }, "unknown transport transform")
        << "a typo'd transform name must fatal, not degrade to no peel";

    // invariant: the message must NAME the vocabulary — a fail-closed error the operator cannot
    // act on is only half the posture.
    EXPECT_DEATH({ (void)resolve_transport_stack(declaration); }, "api-rfc3339-line-prefix");
    EXPECT_DEATH({ (void)resolve_transport_stack(declaration); }, "bracket-rfc3339-line-prefix");
    EXPECT_DEATH({ (void)resolve_transport_stack(declaration); }, "transport-catalog-3");
}

// invariant: the WRITER dual's laws — one fixed lexical form, the corpus-attested spelling plus
// ONE separator.
TEST(TransportRenderer, BracketPrefixRendersOneFixedFormAndRoundTrips)
{
    const auto* bracket_row{find_transform("bracket-rfc3339-line-prefix")};
    ASSERT_NE(bracket_row, nullptr);

    const auto stamp{*insight::utils::parse_iso8601("2026-06-23T15:11:09Z")};
    std::string rendered;
    ASSERT_TRUE(insight::transport::render_transport_prefix(*bracket_row, stamp, rendered));
    EXPECT_EQ(rendered, "[2026-06-23T15:11:09.000Z] ");

    // invariant: determinism — two renders are byte-identical, and the semantics are APPEND, so
    // the caller's buffer grows.
    std::string second;
    ASSERT_TRUE(insight::transport::render_transport_prefix(*bracket_row, stamp, second));
    EXPECT_EQ(rendered, second);

    // invariant: the round-trip laws hold at the HONEST boundary — the peel of a rendered line
    // recovers the whitespace-stripped original, and the extract equals the parser's own reading.
    // invariant: the parser skips the fraction BY DESIGN, so the law holds at the parser's grain
    // and the sub-second digits are covered by the byte-exact form pin instead.
    const std::array<std::string_view, 1> declared_names{"bracket-rfc3339-line-prefix"};
    const TransportStack stack{resolve_transport_stack(IngestDeclaration{.stack = declared_names})};
    const std::string line{rendered + "hello world"};
    const auto peeled{stack.peel_raw(line)};
    EXPECT_EQ(peeled.content, "hello world");
    ASSERT_TRUE(peeled.observation_time.has_value());
    EXPECT_EQ(*peeled.observation_time, stamp);

    // invariant: the other row has NO writer dual, deliberately — that stamp is the platform's,
    // baked into its own writer — so the call returns false and leaves the buffer untouched.
    const auto* gha_row{find_transform(kGhaTransform)};
    ASSERT_NE(gha_row, nullptr);
    std::string untouched;
    EXPECT_FALSE(insight::transport::render_transport_prefix(*gha_row, stamp, untouched));
    EXPECT_TRUE(untouched.empty());
}

// invariant: the renderer's DOMAIN — which stamps the writer ACCEPTS, split from the form law
// because a failure must be able to name the window.
// invariant: the domain is the one property whose very EXPRESSIBILITY varies by build, because the
// timestamp type's tick period is standard-library-defined.
// invariant: on a nanosecond tick the clock spans a few centuries, so a stamp outside the
// four-digit-year window does not EXIST and an offset large enough to reach one is overflow.
// invariant: that is exactly how this arm was wrong before — a large offset WRAPPED on the ship
// toolchain, the renderer correctly rendered the wrapped stamp, and the ASSERTION was the defect.
// invariant: so the law is asserted over the domain that ACTUALLY EXISTS in this build, by probing
// every boundary that domain has.
// invariant: where a window edge lies outside the clock's reach the arm does not fall silent — it
// asserts the fact that makes the edge unreachable.
// invariant: that substitute is a claim about THIS build and it FAILS when the build's clock
// widens, which is what separates it from a skipped assertion.
constexpr int kFirstRenderableYear{0};
constexpr int kLastRenderableYear{9999};

// invariant: the rendered prefix width is a LITERAL, for the same reason the catalogue name is one.
constexpr std::size_t kRenderedPrefixBytes{27U};

// invariant: CARRY is the clock's reach and NAME is the ORACLE's, and they are separate bounds that
// must not be assumed away.
// invariant: flooring the limits DOWN to days divides and cannot overflow, where building a
// timestamp UP from an arbitrary day multiplies and can — the trap that produced the wrap.
// invariant: so everything compares DAYS and never ticks.
// invariant: the calendar year type is bounded, so on a wide tick the clock outruns the calendar
// and the year reads back WRAPPED — measured.
// invariant: an expectation computed from a wrapped year would be a coin flip dressed as an oracle,
// so the probe stops where the oracle stops.
constexpr std::chrono::sys_days kClockFloor{
    std::chrono::ceil<std::chrono::days>(insight::Timestamp::min())};
constexpr std::chrono::sys_days kClockCeiling{
    std::chrono::floor<std::chrono::days>(insight::Timestamp::max())};
constexpr std::chrono::sys_days kCalendarFloor{std::chrono::year::min() / std::chrono::January / 1};
constexpr std::chrono::sys_days kCalendarCeiling{std::chrono::year::max() / std::chrono::December /
                                                 31};
constexpr std::chrono::sys_days kFirstProbeDay{kClockFloor > kCalendarFloor ? kClockFloor
                                                                            : kCalendarFloor};
constexpr std::chrono::sys_days kLastProbeDay{kClockCeiling < kCalendarCeiling ? kClockCeiling
                                                                               : kCalendarCeiling};

// invariant: the oracle for which year a day falls in is the STANDARD calendar and deliberately not
// the renderer's own conversion — that would be the subject-equals-oracle tautology.
[[nodiscard]] int civil_year_of(std::chrono::sys_days day) noexcept
{
    return int{std::chrono::year_month_day{day}.year()};
}

[[nodiscard]] bool is_in_window(int year) noexcept
{
    return year >= kFirstRenderableYear && year <= kLastRenderableYear;
}

TEST(TransportRenderer, AcceptsExactlyTheStampsInsideTheFourDigitYearWindow)
{
    const auto* bracket_row{find_transform("bracket-rfc3339-line-prefix")};
    ASSERT_NE(bracket_row, nullptr);

    const auto probe{
        [bracket_row](std::chrono::sys_days day, bool expect_rendered, std::string_view why)
        {
            std::string out;
            const bool rendered{insight::transport::render_transport_prefix(
                *bracket_row, insight::Timestamp{day}, out)};
            EXPECT_EQ(rendered, expect_rendered)
                << why << " — day " << day.time_since_epoch().count() << " since epoch (civil year "
                << civil_year_of(day) << "): render_transport_prefix returned " << rendered
                << ", expected " << expect_rendered << "; buffer holds \"" << out << '"';
            if (expect_rendered)
                EXPECT_EQ(out.size(), kRenderedPrefixBytes)
                    << why << " — an accepted stamp must produce the ONE fixed form; got "
                    << out.size() << " bytes: \"" << out << '"';
            else
                EXPECT_TRUE(out.empty())
                    << why
                    << " — a refusal must leave the caller's buffer untouched, never a partial "
                       "or wrong prefix; got \""
                    << out << '"';
        }};

    // invariant: the extremes this build can carry and name are present WHATEVER the tick period,
    // so this leg runs on every standard library.
    // invariant: its expectation is DERIVED from the calendar, so on a wide clock it exercises the
    // refusal on both sides and on a narrow one it pins the containment the other leg needs.
    for (const auto day : {kFirstProbeDay, kLastProbeDay})
        probe(day, is_in_window(civil_year_of(day)),
              "the widest stamp this build can carry must obey the same law as any other");

    // invariant: the window's own edges are where an off-by-one in the guard hides.
    struct WindowEdge
    {
        std::chrono::year_month_day day;
        bool renderable;
        std::string_view why;
    };
    const std::array<WindowEdge, 4> edges{{
        {.day = std::chrono::year{kFirstRenderableYear} / std::chrono::January / 1,
         .renderable = true,
         .why = "year 0000 — the FIRST year the four-digit form can spell"},
        {.day = std::chrono::year{kLastRenderableYear} / std::chrono::December / 31,
         .renderable = true,
         .why = "year 9999 — the LAST"},
        {.day = std::chrono::year{kFirstRenderableYear - 1} / std::chrono::December / 31,
         .renderable = false,
         .why = "one day below the window — a negative year has no four-digit spelling"},
        {.day = std::chrono::year{kLastRenderableYear + 1} / std::chrono::January / 1,
         .renderable = false,
         .why = "one day above the window — a five-digit year cannot fit the fixed form"},
    }};

    for (const auto& edge : edges)
    {
        const std::chrono::sys_days day{edge.day};
        if (day > kLastProbeDay)
        {
            // invariant: NOT a skipped assertion — on a narrow tick this edge does not EXIST, so
            // what stands in its place is the fact that makes it unreachable.
            // invariant: the highest stamp this build carries is itself inside the window, so none
            // above it exists — a claim about THIS build that fails when it widens.
            EXPECT_LE(civil_year_of(kLastProbeDay), kLastRenderableYear)
                << edge.why
                << " — no such Timestamp exists in this build, which is only sound "
                   "while the highest carryable stamp (civil year "
                << civil_year_of(kLastProbeDay)
                << ") stays inside the window; it no longer does, so this edge is now reachable "
                   "and must be probed rather than explained away";
            continue;
        }
        if (day < kFirstProbeDay)
        {
            EXPECT_GE(civil_year_of(kFirstProbeDay), kFirstRenderableYear)
                << edge.why
                << " — no such Timestamp exists in this build, which is only sound "
                   "while the lowest carryable stamp (civil year "
                << civil_year_of(kFirstProbeDay)
                << ") stays inside the window; it no longer does, so this edge is now reachable "
                   "and must be probed rather than explained away";
            continue;
        }
        probe(day, edge.renderable, edge.why);
    }
}

} // namespace
