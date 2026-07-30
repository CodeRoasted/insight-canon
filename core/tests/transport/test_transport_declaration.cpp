// NOLINTBEGIN — unit test: short identifiers, string literals and printed diagnostics are intended.
// test_transport_declaration.cpp — G1's SHAPE arm (ADR 0044 §9), homed as a canon-core unit suite.
//
// HOMING (Kleio). ADR 0044 §9 states G1's property and leaves the test design to me; the property
// decomposes into three grains with three different homes, and this file is the first:
//
//   • THIS FILE — the degenerate declaration's shape: empty stack, `peel` is byte-identity, the
//     catalogue's contract, and fail-closed resolution. Every one of these is a property of ONE
//     component (`insight.canon.transport`) over bytes this file authors. It needs no seam and no
//     corpus, so it is a UNIT test and belongs in `core/tests/transport/`, 1:1 with `src/transport/`
//     (§11.9.11's per-domain mirror). Homing it as a corpus gate would have been the mis-homing this
//     role exists to stop: real bytes cannot prove a property that is about the empty stack.
//   • `tests/compose/test_transport_identity.cpp` — that declaring transport does not move
//     `semantic_identity`. Also unit, but its seam is compose's, not transport's.
//   • The CORPUS arm (byte-identical behavior over the D11 slice) + G1-PEEL — the sibling
//     `test_transport_peel_equivalence_gate.cpp` in THIS directory. It once homed in
//     `semantic/github/tests/` because it needed the shipped `GitHubActionsStrategy::parse` in
//     scope; post-T4 the oracle is frozen INLINE in the gate and the SUT is core's
//     `TransportStack::peel`, so it is core's (corpus_backed_gates.md § 5).
//
// FALSIFIABILITY — what this file DOES discharge, and what it explicitly does NOT.
// §9 makes falsifiability a requirement, not a note: G1 must be OBSERVED red under a one-byte
// mutation of the peel path, never asserted red-capable ([[synthetic-gate-vacuity-vs-judgment]]).
// A gate that cannot fail is vacuous, and "the empty stack changes nothing" is exactly the shape of
// claim that passes for the wrong reason — it would stay green if `peel` were hard-coded to return
// its argument for EVERY stack, which would make the whole transform vocabulary a no-op.
//
// So the discharge here is a PERMANENT ARM, not a manual mutation: `DeclaredStackActuallyPeels`
// runs the SAME line table through a DECLARED stack and requires the result to DIFFER wherever a
// line carries a stamp. The identity arm and the peel arm therefore fail in opposite directions —
// no single implementation satisfies both by accident. That is what makes the empty-stack green
// mean something.
//
// What it does NOT discharge: §9's mutation observation on the CORPUS arm. That one is owed and
// remains owed; it cannot be faked at unit grain, and no sentence here may be read as having paid
// it.
//
// Determinism: byte-only. A fixed authored line table, no RNG, no clock, no float, no allocation on
// the asserted path (`peel` returns a borrowed view — the pointer identity is itself asserted).

#include <gtest/gtest.h>

import insight.canon.test; // facade — insight.canon.transport arrives via the facade's export import

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

// The shipped catalogue name. Spelled as a literal rather than read from the catalogue: a test that
// looks the name up from the thing under test cannot notice the name changing, which is the
// SUT==ORACLE tautology one layer down ([[synthetic-gate-vacuity-vs-judgment]]).
constexpr std::string_view kGhaTransform{"api-rfc3339-line-prefix"};

// ── The line table ────────────────────────────────────────────────────────────────────────────
// Authored to be ADVERSARIAL to "the empty stack is the identity", not merely representative. Each
// row names why it is here; the awkward ones (NUL-bearing, lone-CR, sub-width, near-miss stamp) are
// the cases where a peel that trusted its declared width rather than shape-checking would corrupt
// bytes, and where a `std::string_view` mishandled as a C string would truncate.
struct LineCase
{
    std::string_view label;
    std::string_view bytes;
    bool carries_stamp; // does a DECLARED api-rfc3339-line-prefix stack actually shorten this line?
};

// A GHA stamp is exactly 28 bytes: "YYYY-MM-DDTHH:MM:SS.fffffffZ".
constexpr std::string_view kStamp{"2026-04-15T22:20:38.2879579Z"};

// Embedded NUL: built with an explicit length so the view carries the NUL rather than stopping at
// it. Real CI logs carry these (the corpus is read with `rg -a` for exactly this reason,
// [[workspace-search-rg-not-raw-grep]]), and a peel that round-tripped through a C string would
// silently truncate here while every other row stayed green.
// 28 B stamp + " ok" + NUL + "after-nul" = 41 bytes. The length is spelled out because the whole
// point of this row is that the view must NOT stop at the NUL.
constexpr std::string_view kNulBearing{"2026-04-15T22:20:38.2879579Z ok\0after-nul", 41U};

const std::array<LineCase, 14> kLines{{
    {.label = "empty", .bytes = {}, .carries_stamp = false},
    {.label = "single space", .bytes = " ", .carries_stamp = false},
    {.label = "plain content", .bytes = "hello world", .carries_stamp = false},
    {.label = "stamped + content", .bytes = "2026-04-15T22:20:38.2879579Z ok", .carries_stamp = true},
    {.label = "stamped, no trailing space",
     .bytes = "2026-04-15T22:20:38.2879579Z",
     .carries_stamp = true},
    {.label = "stamped + tab separator",
     .bytes = "2026-04-15T22:20:38.2879579Z\tok",
     .carries_stamp = true},
    {.label = "stamped + indented content",
     .bytes = "2026-04-15T22:20:38.2879579Z    indented",
     .carries_stamp = true},
    // Offset-0 BOM — the live shipped defect (bugs.md 2026-07-27). Present here as a CASE, not as a
    // fix: the empty stack must leave it alone exactly like everything else, and the declared stack
    // must NOT claim it (the stamp is not at offset 0). Pinning the current behavior is what lets
    // the BOM ruling land later without this file quietly agreeing in advance.
    {.label = "BOM then stamp",
     .bytes = "\xEF\xBB\xBF"
              "2026-04-15T22:20:38.2879579Z ok",
     .carries_stamp = false},
    {.label = "NUL-bearing after stamp", .bytes = kNulBearing, .carries_stamp = true},
    {.label = "lone CR (no LF)", .bytes = "carriage\rreturn", .carries_stamp = false},
    // Shorter than the declared 28-byte width: a width-trusting peel would read past the end or
    // return garbage. Shape-checking returns the line untouched (transport.cpp has_stamp_at_head).
    {.label = "sub-width digits", .bytes = "2026-04-15T22", .carries_stamp = false},
    // Correct shape in the invariant head, WRONG in the declared tail (6 fractional digits, no Z).
    // The core-side `has_stamp_at_head` validates only the 19-byte invariant head and trusts the
    // declared width for the rest, so it DOES claim this line. Asserted as it behaves, not as it
    // might be wished to behave — this row is why the flag below is `carries_stamp`, a measured
    // property of the shipped algorithm, and not "looks like a stamp to a human".
    {.label = "near-miss stamp (short fraction)",
     .bytes = "2026-04-15T22:20:38.287957 ok",
     .carries_stamp = true},
    {.label = "high-bit / non-UTF8 bytes", .bytes = "\xFF\xFE raw bytes", .carries_stamp = false},
    {.label = "whitespace only", .bytes = "   \t  ", .carries_stamp = false},
}};

// Render bytes legibly for a failure message — a NUL or a lone CR printed raw would corrupt the
// very diagnostic that has to explain the failure.
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

// A stack declaring the one shipped transform. The backing array must outlive the declaration —
// `IngestDeclaration::stack` is a span, so a temporary here would dangle.
constexpr std::array<std::string_view, 1> kDeclaredGha{{kGhaTransform}};

// ══════════════════════════════════════════════════════════════════════════════════════════════
// G1 — the degenerate declaration
// ══════════════════════════════════════════════════════════════════════════════════════════════

TEST(TransportDeclaration, DefaultDeclarationResolvesToTheDegenerateStack)
{
    const IngestDeclaration declaration{};
    const TransportStack stack{resolve_transport_stack(declaration)};

    EXPECT_TRUE(stack.empty()) << "a default-constructed IngestDeclaration must resolve to the "
                                  "degenerate stack (ADR 0044 §6 — declaring is SUBTRACTIVE: a "
                                  "caller who says nothing loses nothing). Got size "
                               << stack.size();
    EXPECT_EQ(stack.size(), 0U);

    // A default-constructed TransportStack and a resolved-from-empty one must be the same thing.
    // If they ever diverge, "today's behavior" would depend on which door the caller came through.
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

        // Byte-EQUAL is not enough: the peel must BORROW, never copy. `peel` is documented
        // allocation-free and hot-path-safe, and pointer identity is the only assertion that
        // actually holds that. A copy would compare equal and silently add an allocation per line.
        EXPECT_EQ(peeled.content.data(), line.bytes.data())
            << "case [" << idx << "] '" << line.label
            << "': the degenerate peel returned a COPY, not a borrowed view of the caller's buffer "
               "(canon.transport.cppm — `peel` only ever SHORTENS, so no allocation is involved).";

        EXPECT_FALSE(peeled.observation_time.has_value())
            << "case [" << idx << "] '" << line.label
            << "': the degenerate stack declares no extract, so it must set no observation time.";

        EXPECT_EQ(peeled.is_blank(), line.bytes.empty())
            << "case [" << idx << "] '" << line.label << "': is_blank() must mean 'peeled to empty' "
            << "and nothing else — content was \"" << escape(peeled.content) << "\"";
    }
}

// ══════════════════════════════════════════════════════════════════════════════════════════════
// The FALSIFICATION arm — permanent, not a manual mutation
// ══════════════════════════════════════════════════════════════════════════════════════════════

// The arm that makes the two tests above non-vacuous. It runs the SAME table through a DECLARED
// stack and requires the result to DIFFER on every stamped line. A `peel` hard-coded to return its
// argument — the mutation that leaves the degenerate arms green — fails HERE, loudly.
//
// The two arms are opposed by construction: no implementation satisfies both by accident, because
// arm 1 demands byte-identity under the empty stack and this one demands a byte CHANGE under a
// declared one, over identical inputs.
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
                << "': a declared api-rfc3339-line-prefix stack must SHORTEN a stamped line. If this "
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
            // TOTALITY IS ABOUT APPLICATION, NOT EFFECT (ADR 0044 §2). The row is applied to every
            // line unconditionally; on these bytes its effect is nothing. That is the declared
            // rule's effect being nothing — NOT the transform asking "is this line mine?".
            EXPECT_EQ(peeled.content, line.bytes)
                << "case [" << idx << "] '" << line.label
                << "': the declared row must leave an unstamped line untouched (§2 — its EFFECT is "
                   "nothing; it does not get to decline).\n"
                << "  in : \"" << escape(line.bytes) << "\"\n"
                << "  out: \"" << escape(peeled.content) << "\"";
        }

        // The peel SHORTENS — it never rewrites. Whatever it returns must be a suffix of the input,
        // sharing its buffer. This holds on both branches and is what keeps `content` safe to hand
        // to the tokenizer without a copy.
        EXPECT_GE(peeled.content.data(), line.bytes.data())
            << "case [" << idx << "] '" << line.label << "': peel returned a view outside the "
            << "caller's buffer";
        EXPECT_LE(peeled.content.data() + peeled.content.size(),
                  line.bytes.data() + line.bytes.size())
            << "case [" << idx << "] '" << line.label << "': peel returned a view running past the "
            << "end of the caller's buffer";
    }

    // A table that stopped exercising the shortening path would make this whole arm vacuous while
    // every EXPECT above stayed green — the can't-FAIL mode. Pin the count.
    constexpr std::size_t kStampedCases{6};
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
                                        "space (strip_leading_space is load-bearing, ADR 0044 §8)";
    EXPECT_TRUE(stamped.observation_time.has_value())
        << "the shipped row declares TransportExtract::EventObservationTime, so a parseable stamp "
           "must yield one";

    const RawPeeledLine unstamped{stack.peel_raw("no stamp here")};
    EXPECT_EQ(unstamped.content, "no stamp here");
    EXPECT_FALSE(unstamped.observation_time.has_value())
        << "a line the rule did not shorten must carry no observation time";

    // A line that is ENTIRELY transport peels to empty, and empty means DROP — not an empty
    // template. This is how the shipped GHA strategy's "a timestamp-only line is a blank line"
    // behavior survives the move to a declared peel (ADR 0044 §8's bundled behavior 3).
    const RawPeeledLine bare{stack.peel_raw("2026-04-15T22:20:38.2879579Z ")};
    EXPECT_TRUE(bare.is_blank()) << "a stamp-only line must peel to blank, got \""
                                 << escape(bare.content) << "\"";
}

// ══════════════════════════════════════════════════════════════════════════════════════════════
// The catalogue's contract, and fail-closed resolution
// ══════════════════════════════════════════════════════════════════════════════════════════════

TEST(TransportCatalog, ShippedRowsAreExactlyWhatTheCatalogDeclares)
{
    // ONE row today, deliberately (ADR 0044 §3 sketches seven; this workspace does not ship enum
    // members with no algorithm, no row and no gate). Pinned so that adding a member without its
    // algorithm and its gate fails here — the anti-dormant rule with teeth
    // ([[rip-dormant-no-premature-specialization]]).
    ASSERT_EQ(kTransportCatalogRows.size(), 1U)
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

    // The catalogue version is a component of every composed semantic_identity. Pinned as a
    // LITERAL: a silent bump would move every digest in the workspace, and a test that read the
    // constant back from the constant could never say so.
    EXPECT_EQ(kTransportCatalogVersion, "transport-catalog-1");
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
    // canon VERIFIES, never infers (ADR 0044 §6 / ADR 0030's split). An UNKNOWN name is a MISTAKE
    // and fails closed; an ABSENT name is a CHOICE and degrades. They must never share a code path,
    // so the death message is part of the contract, not decoration.
    constexpr std::array<std::string_view, 1> kTypo{{"gha-api-line-prefx"}}; // one byte dropped
    const IngestDeclaration declaration{.stack = kTypo, .dialect = {}, .channel = {}};

    EXPECT_DEATH(
        { (void)resolve_transport_stack(declaration); },
        "unknown transport transform")
        << "a typo'd transform name must fatal, not degrade to no peel";

    // The message must NAME the vocabulary — a fail-closed error the operator cannot act on is
    // only half the posture.
    EXPECT_DEATH({ (void)resolve_transport_stack(declaration); }, "api-rfc3339-line-prefix");
    EXPECT_DEATH({ (void)resolve_transport_stack(declaration); }, "transport-catalog-1");
}

} // namespace
// NOLINTEND
