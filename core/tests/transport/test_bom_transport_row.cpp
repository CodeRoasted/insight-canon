// NOLINTBEGIN — unit test: short identifiers, byte literals and printed diagnostics are intended.
// test_bom_transport_row.cpp — G-BOM-1 (peel equivalence) and G-BOM-2 (stack order, with its RED
// arm), the two MECH arms DN-25.D5 owes for the `utf8-bom-line-prefix` catalogue row.
//
// ⚠ THIS FILE IS RED UNTIL THE ROW LANDS, AND THAT IS THE HANDOFF, NOT A REGRESSION.
// The arms are pre-registered ahead of the transform (DN-25.D5 / ADR-2.D7 — a member grows in with
// its algorithm, its row and its gate). Every test below opens with
// `ASSERT_NE(find_transform("utf8-bom-line-prefix"), nullptr)`, so the absence of the row is a
// FAILURE and never a skip: an absent subject that skips is how a gate goes green for the one
// reason that matters — it never looked. It is an ASSERT rather than a resolve, deliberately:
// `resolve_transport_stack` on an unknown name calls `std::terminate()`, which would take the whole
// `insight_canon_tests` binary down and destroy every sibling suite's verdict.
// Nothing here names `TransportTransformKind::LinePrefixByteOrderMark` — the enumerator does not
// exist yet, and a file that could not COMPILE before the row landed would block three other lanes
// sharing this tree. The row is reached by NAME (which is what a declaration references) and its
// kind is asserted by its serialized VALUE.
//
// ── HOMING (Kleio's call) ─────────────────────────────────────────────────────────────────────
// UNIT, in `core/tests/transport/`, 1:1 with `core/src/transport/` under the per-domain mirror,
// beside the sibling grains `test_transport_declaration.cpp` (G1 shape) and
// `test_transport_peel_equivalence_gate.cpp` (G1-PEEL + G-BOM-3, the corpus arm).
//
// WHY NOT INTEGRATION. The asserted property crosses no seam. `find_transform`,
// `resolve_transport_stack` and `TransportStack::peel_raw` are all `insight.canon.transport`; the
// inputs are bytes this file authors; the oracle is a byte string. An integration home would buy no
// extra proof, cost wall clock on every gate, and blur which package broke.
//
// WHY NOT THE CORPUS — AND THIS ONE IS MEASURED, NOT ASSERTED. The D11 slices CANNOT falsify the
// decisions this file exists to pin. Counted over both pinned slices (an independent
// second-language replica of the gate's population reader, oracle and peel; it reproduces every
// committed G1-PEEL pin exactly, which is what entitles these counts to be believed):
//
//                                    data/v1/sample     data/v1/full
//     lines starting with a BOM              511            17 487
//     ... immediately followed by a space      0                 0   <<< strip_leading_space
//     ... a SECOND BOM                         0                 0   <<< strip-once
//     a BOM anywhere but offset 0              0                 0   <<< prefix, not `find`
//
// So a corpus-homed G-BOM-1 would stay GREEN under `strip_leading_space = true` (DN-25.D3), GREEN
// under a greedy multi-BOM strip (DN-25.D7), and GREEN under a `find`-anywhere strip. Every real
// BOM line in both slices has the single production shape `<BOM><stamp><space><content>`, so the
// corpus scores one shape and would pronounce on four decisions. The authored table below is the
// CARRYING leg for those defect classes; the corpus arm is the carrying leg for the count. Neither
// substitutes for the other.
//
// ⚠ THIS TABLE IS A HOMING ARGUMENT, NEVER A REASON FOR ANY DECISION, and DN-25.D7 says so in as
// many words about the strip-once case. "Zero doubles in the corpus" is why the corpus cannot TEST
// the rule; it is emphatically not why the rule is what it is. Reading a measured absence as a
// justification is how a pin acquires a reason that a wider population would refute.
//
// ── FALSIFIABILITY — every arm names the implementation it kills ──────────────────────────────
// Red-capability is a requirement, not a note, so each case in `kCases` carries the defect it is
// the sole guard against, and the two stack-order arms pin EXACT BYTES ON BOTH SIDES of the
// ordering boundary rather than asserting a bare inequality: a relational assert pins a relation,
// never a magnitude, and "the reversed stack differs" is satisfied by a reversed stack that does
// something else wrong entirely. Here the reversed declaration's output is pinned to the exact
// bytes of the SHIPPED DEFECT — `<stamp><space><content>`, stamp intact — because that is what
// DN-25.D4 claims reversal reproduces, and a claim is worth its exact statement or nothing.
//
// Determinism: byte-only. A fixed authored table, no RNG, no clock, no float, no threads, no
// allocation on the asserted path (`peel_raw` borrows; pointer identity is itself asserted).

#include <gtest/gtest.h>

import insight.canon.test; // facade — insight.canon.transport arrives via the facade's export import

using insight::transport::find_transform;
using insight::transport::IngestDeclaration;
using insight::transport::kTransportCatalogRows;
using insight::transport::kTransportCatalogVersion;
using insight::transport::RawPeeledLine;
using insight::transport::resolve_transport_stack;
using insight::transport::TransportExtract;
using insight::transport::TransportStack;
using insight::transport::TransportTransformRow;

namespace
{

// The declaration keys, spelled as literals rather than read back from the catalogue: a test that
// looks its subject's name up from its subject cannot notice the name changing — SUT==ORACLE one
// layer down.
constexpr std::string_view kBomRow{"utf8-bom-line-prefix"};
constexpr std::string_view kGhaRow{"api-rfc3339-line-prefix"};

// The declared stacks. The backing arrays must outlive the declaration — `IngestDeclaration::stack`
// is a span, so a temporary would dangle.
constexpr std::array<std::string_view, 1> kBomOnly{{kBomRow}};
constexpr std::array<std::string_view, 1> kGhaOnly{{kGhaRow}};
// OUTSIDE-IN (ADR-23.D4): the bytes are `<BOM><stamp><content>`, so the BOM is the outer layer and
// comes off first. This is DN-25.D4's declaration, verbatim.
constexpr std::array<std::string_view, 2> kBomThenGha{{kBomRow, kGhaRow}};
// The WRONG order — kept as a first-class fixture, not as a comment. Reversing it makes the stamp
// acceptor meet `EF BB BF` at offset 0 and decline, which is the present shipped defect reproduced
// THROUGH the declaration.
constexpr std::array<std::string_view, 2> kGhaThenBom{{kGhaRow, kBomRow}};

constexpr std::string_view kBom{"\xEF\xBB\xBF"};
// A GHA stamp is exactly 28 bytes: "YYYY-MM-DDTHH:MM:SS.fffffffZ".
constexpr std::string_view kStamp{"2026-04-15T22:20:38.2879579Z"};

// The message every arm prints when the row is not in the catalogue yet. It states what is owed and
// who owns it, so a red read cold is actionable without opening the design note.
constexpr std::string_view kRowAbsent{
    "the catalogue does not declare \"utf8-bom-line-prefix\".\n"
    "These arms are PRE-REGISTERED (DN-25.D5) and are RED BY DESIGN until the row, its algorithm\n"
    "and its identity bump land in ONE commit (ADR-2.D7). A handoff, not a regression.\n"
    "It is an ASSERT rather than a skip because a gate that skips its absent subject is green for\n"
    "the one reason that matters: it never looked."};

// Render bytes legibly for a failure message — a BOM, a NUL or a lone CR printed raw would corrupt
// the very diagnostic that has to explain the failure.
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

// ══════════════════════════════════════════════════════════════════════════════════════════════
// The line table — authored ADVERSARIAL to `utf8-bom-line-prefix`, not representative of the corpus
// ══════════════════════════════════════════════════════════════════════════════════════════════
//
// `expected` is the byte-exact output of the BOM row applied ALONE. Every row states the
// implementation it is the sole guard against; a case with no such sentence does not belong here.
struct Case
{
    std::string_view label;
    std::string_view bytes;
    std::string_view expected; // what the BOM row alone must yield, byte-exact
    std::string_view kills;    // the implementation this case, and only this case, refutes
};

// NUL-bearing: built with an explicit length so the view CARRIES the NUL rather than stopping at
// it. Real CI logs carry these. 3 B BOM + "ok" + NUL + "after-nul" = 15 bytes.
constexpr std::string_view kNulAfterBom{"\xEF\xBB\xBF"
                                        "ok\0after-nul",
                                        15U};
constexpr std::string_view kNulAfterBomPeeled{"ok\0after-nul", 12U};

const std::array<Case, 16> kCases{{
    {.label = "production shape: BOM + stamp + content",
     .bytes = "\xEF\xBB\xBF"
              "2026-04-15T22:20:38.2879579Z ok",
     .expected = "2026-04-15T22:20:38.2879579Z ok",
     .kills = "any peel that does not remove the byte the whole defect is about"},
    {.label = "BOM + SPACE + content",
     .bytes = "\xEF\xBB\xBF"
              " indented",
     .expected = " indented",
     .kills = "strip_leading_space = true on this row — it would eat a real content byte "
              "(DN-25.D3). ZERO instances in either D11 slice: this case is the SOLE guard."},
    {.label = "BOM + TAB + content",
     .bytes = "\xEF\xBB\xBF"
              "\tindented",
     .expected = "\tindented",
     .kills = "strip_leading_space = true, tab flavour — same argument, second separator byte"},
    {.label = "BOM alone (the whole line is transport)",
     .bytes = "\xEF\xBB\xBF",
     .expected = "",
     .kills = "a peel that refuses to shorten a line to nothing. Empty means DROP, and the licence "
              "is ADR-23.D4 — 'declaring is purely SUBTRACTIVE' — never ADR-23.D2, which speaks "
              "about the transform and not about removing a LINE (DN-25.D8). The bound is "
              "STRUCTURAL, not a promise: is_blank() is bytes().empty() and is never "
              "whitespace-trimmed, so <BOM> followed by spaces peels to \"   \" and SURVIVES."},
    {.label = "double BOM",
     .bytes = "\xEF\xBB\xBF\xEF\xBB\xBF"
              "x",
     .expected = "\xEF\xBB\xBF"
                 "x",
     .kills = "a greedy `while (starts_with(BOM)) remove` loop. ONE removal is BY DEFINITION "
              "(DN-25.D7): U+FEFF is a byte-order mark only at STREAM HEAD — anywhere else it is "
              "ZWNBSP, a CONTENT character, so a loop would delete a codepoint and call it "
              "delivery. A loop is also ADR-23.D2's DETECTION shape N times over, which would "
              "retro-weaken the very argument that made this row admissible. The corpus is "
              "explicitly NOT the reason (zero doubles in either slice); it is only why nothing "
              "but this case can TEST it."},
    {.label = "truncated BOM (2 bytes, EF BB)",
     .bytes = "\xEF\xBB",
     .expected = "\xEF\xBB",
     .kills = "a two-byte prefix test, and a strip that trusts a declared width instead of "
              "shape-checking the full three bytes"},
    {.label = "near-miss BOM (EF BB BE)",
     .bytes = "\xEF\xBB\xBE"
              "x",
     .expected = "\xEF\xBB\xBE"
                 "x",
     .kills = "a prefix test that stops after two bytes — the third byte is load-bearing"},
    {.label = "UTF-16 LE BOM (FF FE)",
     .bytes = "\xFF\xFE"
              "utf16",
     .expected = "\xFF\xFE"
                 "utf16",
     .kills = "prefix_width carried as a PARAMETER a row could declare as 2 (DN-25.D3's exact "
              "argument): a width-2 acceptor strips two bytes off this line silently"},
    {.label = "UTF-16 BE BOM (FE FF)",
     .bytes = "\xFE\xFF"
              "utf16be",
     .expected = "\xFE\xFF"
                 "utf16be",
     .kills = "the same width-2 acceptor, opposite endianness"},
    {.label = "interior BOM (not at offset 0)",
     .bytes = "prefix\xEF\xBB\xBF"
              "suffix",
     .expected = "prefix\xEF\xBB\xBF"
                 "suffix",
     .kills = "a `find`-anywhere / erase-all-BOMs implementation. Zero instances in either slice — "
              "sole guard. DN-25.D2's declared limitation: a BOM the prefix test misses keeps "
              "TODAY's behavior, it never gains a new one."},
    {.label = "BOM at end of line only",
     .bytes = "trailing\xEF\xBB\xBF",
     .expected = "trailing\xEF\xBB\xBF",
     .kills = "a strip that trims BOMs from either end"},
    {.label = "NUL-bearing after the BOM",
     .bytes = kNulAfterBom,
     .expected = kNulAfterBomPeeled,
     .kills = "a peel that round-trips through a C string — it would truncate at the NUL while "
              "every other case stayed green"},
    {.label = "BOM + stamp with NO content",
     .bytes = "\xEF\xBB\xBF"
              "2026-04-15T22:20:38.2879579Z ",
     .expected = "2026-04-15T22:20:38.2879579Z ",
     .kills = "a BOM row that also knows about stamps — one row, one grammar, one owner"},
    {.label = "empty line",
     .bytes = "",
     .expected = "",
     .kills = "a peel that reads front() without checking size — an empty line is real corpus "
              "input (31 822 of them in data/v1/full)"},
    {.label = "plain content, no BOM",
     .bytes = "hello world",
     .expected = "hello world",
     .kills = "a row whose effect is not the identity where its prefix is absent — totality is "
              "about APPLICATION, never effect (ADR-23.D2)"},
    {.label = "high-bit bytes that are not any BOM",
     .bytes = "\xC3\xA9"
              "accented",
     .expected = "\xC3\xA9"
                 "accented",
     .kills = "a strip keyed on 'the first byte is >= 0x80'"},
}};

// ══════════════════════════════════════════════════════════════════════════════════════════════
// The row's SHAPE — DN-25.D3's four decisions, each pinned where it is decided
// ══════════════════════════════════════════════════════════════════════════════════════════════

TEST(BomTransportRow, RowShapeIsExactlyWhatTheDesignDecided)
{
    const TransportTransformRow* row{find_transform(kBomRow)};
    ASSERT_NE(row, nullptr) << kRowAbsent;

    EXPECT_EQ(row->name, kBomRow)
        << "the name is DELIVERY-shaped, never ecosystem-shaped: ADR-17 (SRC-SP-1) forbids an "
           "ecosystem literal in core, and the catalogue's own rule names a row for the byte "
           "grammar it peels. `gha-bom` is doubly excluded — any UTF-8 delivery declares this row.";

    EXPECT_EQ(row->extract, TransportExtract::None)
        << "the peel yields nothing but the shortened line — the ternary's DISCARD arm, and it is "
           "fail-safe-KEEP: an unrecognized residual falls to RawText rather than being dropped.";

    EXPECT_EQ(row->prefix_width, 0U)
        << "UNREAD, and 0 says so. The field's contract is 'a parameter a declaration may "
           "legitimately vary' (kGhaApiPrefixWidth is measured per delivery); a UTF-8 BOM is three "
           "bytes by RFC and nothing may declare otherwise. Carrying it as a parameter invites a "
           "row declaring width 2 — UTF-16's BOM, a different byte sequence — and the acceptor "
           "would strip two bytes off a UTF-8 stream silently. The width belongs to the grammar, "
           "in the acceptor (the LinePrefixBracketedTimestamp precedent).";

    EXPECT_FALSE(row->strip_leading_space)
        << "FALSE, and it is not cosmetic. The observed bytes are EF BB BF immediately followed by "
           "the stamp — NO separator. A true value would eat a real content byte from any line "
           "shaped <BOM><space>text, and it would DOUBLE-STRIP in the stack, where the stamp row "
           "already owns the separator after its own stamp. One separator, one owner.";
}

TEST(BomTransportRow, RowAppendsAtTheEndOfTheCatalogAndTheEnumIsNotRenumbered)
{
    const TransportTransformRow* row{find_transform(kBomRow)};
    ASSERT_NE(row, nullptr) << kRowAbsent;

    ASSERT_EQ(kTransportCatalogRows.size(), 3U)
        << "three rows: the two shipped ones plus this. A row that landed without its algorithm "
           "and its gate — or a gate written for a row that did not land — fails here.";

    // The row's POSITION, computed from the pointer rather than by indexing a constexpr array out
    // of range. Rows serialize in catalogue order (compose.cpp), so an insertion mid-array moves
    // the digest of every row after it.
    EXPECT_EQ(row - kTransportCatalogRows.data(), 2)
        << "the new row must APPEND. Rows serialize in catalogue order into every composed "
           "semantic_identity, so inserting one shifts the serialized bytes of rows nobody touched "
           "— and it does so SILENTLY: the diff is one line and the compiler says nothing.";

    // ⚠ THE ENUM'S NUMERIC VALUES ARE THE SERIALIZED BYTES (ADR-2 clause 2.2 —
    // identity-bearing, new members APPEND, never renumber, never insert mid-enum). The sibling
    // suite pins each row's kind SYMBOLICALLY, which cannot see a renumbering at all: renaming
    // nothing and renumbering everything leaves every symbolic assert green while every golden in
    // the workspace moves. This is the arm that sees it, and it is spelled numerically ON PURPOSE.
    // The BOM row's kind is asserted by VALUE and never by enumerator name, so this file compiles
    // before the enumerator exists.
    EXPECT_EQ(static_cast<std::uint8_t>(kTransportCatalogRows[0].kind), 0U)
        << "LinePrefixTimestamp is the anchor and is never renumbered";
    EXPECT_EQ(static_cast<std::uint8_t>(kTransportCatalogRows[1].kind), 1U)
        << "LinePrefixBracketedTimestamp appended at 1 (T5 5.2) and is never renumbered";
    EXPECT_EQ(static_cast<std::uint8_t>(row->kind), 2U)
        << "the BOM kind APPENDS at value 2. If this is 0, 1, or anything but 2, either the member "
           "was inserted mid-enum (every existing row's serialized byte just shifted) or the row "
           "reuses a shipped kind (its algorithm is then the wrong one).";
}

TEST(BomTransportRow, CatalogVersionCoFiredWithTheNewRow)
{
    ASSERT_NE(find_transform(kBomRow), nullptr) << kRowAbsent;

    // ⚠ THE VALUE IS NOT ASSERTED, AND THAT IS DELIBERATE. A monotonic token is ASSIGNED AT SHIP,
    // BY WHOEVER SHIPS, NEVER RESERVED (ADR-2, normative; DN-25.O1 names the bump as owed and
    // pointedly does not take the value). What this arm owns is the CO-FIRE: a row that serializes
    // bytes without moving the version silently re-uses an identity that no longer means what it
    // meant. So the assertion is that the token MOVED off the value the two-row catalogue shipped
    // under — satisfiable by exactly one act, and it fires precisely when the bump is forgotten.
    EXPECT_NE(kTransportCatalogVersion, "transport-catalog-2")
        << "the catalogue gained a third row and kept the version of the two-row shape. That "
           "version is a component of EVERY composed semantic_identity — including for streams "
           "that declare no transport at all — so two incomparable vocabularies are now digesting "
           "to the same identity. Bump it, and re-derive the goldens in the SAME pass (DN-25.O1 "
           "S2: "
           "no commit may exist in which the catalogue version and the goldens disagree).";
    EXPECT_FALSE(kTransportCatalogVersion.empty());
}

// ══════════════════════════════════════════════════════════════════════════════════════════════
// G-BOM-1 — peel equivalence: strip a leading BOM, and otherwise be the identity
// ══════════════════════════════════════════════════════════════════════════════════════════════

TEST(BomTransportRowGBom1, RowIsStripLeadingBomAndOtherwiseTheIdentity)
{
    ASSERT_NE(find_transform(kBomRow), nullptr) << kRowAbsent;
    const TransportStack stack{resolve_transport_stack(
        IngestDeclaration{.stack = kBomOnly, .dialect = {}, .channel = {}})};
    ASSERT_EQ(stack.size(), 1U) << "the declared stack must resolve exactly one row";

    for (std::size_t idx{0}; idx < kCases.size(); ++idx)
    {
        const Case& item{kCases[idx]};
        const RawPeeledLine peeled{stack.peel_raw(item.bytes)};

        EXPECT_EQ(peeled.content, item.expected)
            << "case [" << idx << "] '" << item.label << "'\n"
            << "  in       : \"" << escape(item.bytes) << "\" (" << item.bytes.size() << " B)\n"
            << "  expected : \"" << escape(item.expected) << "\" (" << item.expected.size()
            << " B)\n"
            << "  actual   : \"" << escape(peeled.content) << "\" (" << peeled.content.size()
            << " B)\n"
            << "  this case kills: " << item.kills;

        EXPECT_FALSE(peeled.observation_time.has_value())
            << "case [" << idx << "] '" << item.label
            << "': the row declares TransportExtract::None — it must never set an observation "
               "time. A BOM carries no clock.";

        // The peel SHORTENS from the head; it never rewrites and never copies. Pointer identity is
        // the only assertion that actually holds "allocation-free" — a copy would compare equal and
        // silently add an allocation per line on a hot path.
        EXPECT_GE(peeled.content.data(), item.bytes.data())
            << "case [" << idx << "] '" << item.label
            << "': peel returned a view OUTSIDE the caller's buffer — it copied.";
        EXPECT_EQ(peeled.content.data() + peeled.content.size(),
                  item.bytes.data() + item.bytes.size())
            << "case [" << idx << "] '" << item.label
            << "': peel must return a SUFFIX of the input — same end pointer, head advanced. A "
               "different end means bytes were removed from the tail, which no line-prefix rule "
               "may "
               "do.";

        EXPECT_EQ(peeled.is_blank(), item.expected.empty())
            << "case [" << idx << "] '" << item.label
            << "': is_blank() must mean 'peeled to empty' and nothing else — content was \""
            << escape(peeled.content) << "\"";
    }
}

// The equivalence law itself, stated as a law rather than as a table: prefixing ANY line with a BOM
// must be invisible to the row. This is the content-neutrality obligation ADR-23.D6 places on a
// transform, and it is the unit-grain statement of exactly what the corpus arm scores at scale.
TEST(BomTransportRowGBom1, PrefixingAnyLineWithABomIsInvisibleToTheRow)
{
    ASSERT_NE(find_transform(kBomRow), nullptr) << kRowAbsent;
    const TransportStack stack{resolve_transport_stack(
        IngestDeclaration{.stack = kBomOnly, .dialect = {}, .channel = {}})};

    std::size_t exercised{0};
    for (std::size_t idx{0}; idx < kCases.size(); ++idx)
    {
        const Case& item{kCases[idx]};
        // Skip the cases that already start with a BOM: prefixing THOSE tests the strip-once rule,
        // which the table above owns. Here the law is about lines the row must leave alone.
        if (item.bytes.starts_with(kBom))
            continue;
        ++exercised;

        const std::string prefixed{std::string{kBom} + std::string{item.bytes}};
        const RawPeeledLine with{stack.peel_raw(prefixed)};
        const RawPeeledLine without{stack.peel_raw(item.bytes)};

        EXPECT_EQ(with.content, without.content)
            << "case [" << idx << "] '" << item.label << "': peel(BOM + line) != peel(line).\n"
            << "  line          : \"" << escape(item.bytes) << "\"\n"
            << "  peel(BOM+line): \"" << escape(with.content) << "\"\n"
            << "  peel(line)    : \"" << escape(without.content) << "\"\n"
            << "  The BOM must be INVISIBLE to everything downstream. A difference here is the row "
               "doing something other than removing exactly the three prefix bytes.";
        EXPECT_EQ(with.content, item.bytes)
            << "case [" << idx << "] '" << item.label
            << "': and both must equal the ORIGINAL line — pinned on BOTH sides of the boundary, "
               "because `with == without` alone is satisfied by a row that mangles both "
               "identically.";
    }

    // A table that stopped exercising this law would leave the arm green while asserting nothing.
    constexpr std::size_t kBomFreeCases{9};
    EXPECT_EQ(exercised, kBomFreeCases)
        << "the table must keep enough BOM-free cases to make this law non-vacuous; if cases were "
           "edited out, this arm stopped falsifying anything.";
}

// ══════════════════════════════════════════════════════════════════════════════════════════════
// G-BOM-2 — stack ORDER, with its RED arm. The transport stack's first genuine composition.
// ══════════════════════════════════════════════════════════════════════════════════════════════
//
// ADR-23.D3 justified transport carrying its own catalogue on NESTING, whose named beneficiary
// (docker-inside-GHA) was never built — so the stack has shipped with NO consumer and every
// ordering property in it is untested BY CONSTRUCTION. These two arms are the first thing that
// makes stack order load-bearing.

TEST(BomTransportRowGBom2, DeclaredOrderPeelsTheProductionShapeToItsContent)
{
    ASSERT_NE(find_transform(kBomRow), nullptr) << kRowAbsent;
    const TransportStack stack{resolve_transport_stack(
        IngestDeclaration{.stack = kBomThenGha, .dialect = {}, .channel = {}})};
    ASSERT_EQ(stack.size(), 2U)
        << "the arm is meaningless unless BOTH rows resolved — a one-row stack would pass the "
           "content assertion below for the wrong reason";

    const std::string line{std::string{kBom} + std::string{kStamp} + " ok"};
    const RawPeeledLine peeled{stack.peel_raw(line)};

    EXPECT_EQ(peeled.content, "ok")
        << "outside-in (ADR-23.D4): the BOM is the outer delivery layer and comes off first, then "
           "the stamp row meets its grammar at offset 0.\n"
        << "  in     : \"" << escape(line) << "\"\n"
        << "  actual : \"" << escape(peeled.content) << "\"";

    // The SECOND coordinate, and it is the one that exists ONLY on the far side of the ordering
    // boundary: an observation time can be extracted only if the stamp acceptor actually ran on the
    // stamp. Its presence IS the proof that the composition happened, independently of the bytes.
    ASSERT_TRUE(peeled.observation_time.has_value())
        << "the stamp row declares TransportExtract::EventObservationTime, so the correctly "
           "ordered "
           "stack must extract one. Absent means the stamp acceptor never saw its grammar — i.e. "
           "the BOM was still in front of it.";
    const std::optional<insight::Timestamp> expected_time{insight::utils::parse_iso8601(kStamp)};
    ASSERT_TRUE(expected_time.has_value())
        << "the authored stamp must itself be parseable, or the value assertion below is comparing "
           "against nothing — the fixture is broken, not the SUT";
    EXPECT_EQ(*peeled.observation_time, *expected_time)
        << "the extracted observation time must be the stamp's own, parsed from the stamp's own "
           "28 bytes and not from an offset the BOM shifted (enrichment only — never an ordering "
           "key, never a replay input)";
}

// ── THE RED ARM ───────────────────────────────────────────────────────────────────────────────
// DN-25.D4's ordering claim is vacuous without this. It pins the reversed declaration's output to
// EXACT BYTES rather than to "differs": the claim is that reversal reproduces the SHIPPED DEFECT,
// and that claim is worth its exact statement or nothing.
TEST(BomTransportRowGBom2, ReversedOrderReproducesTheShippedDefectThroughTheDeclaration)
{
    ASSERT_NE(find_transform(kBomRow), nullptr) << kRowAbsent;
    const TransportStack reversed{resolve_transport_stack(
        IngestDeclaration{.stack = kGhaThenBom, .dialect = {}, .channel = {}})};
    ASSERT_EQ(reversed.size(), 2U) << "both rows must resolve — otherwise this arm is red for the "
                                      "wrong reason and proves nothing about ORDER";

    const std::string line{std::string{kBom} + std::string{kStamp} + " ok"};
    const RawPeeledLine peeled{reversed.peel_raw(line)};

    // The stamp row runs FIRST, meets EF BB BF at offset 0, and its effect is nothing. The BOM row
    // then removes the BOM — too late for anyone. What survives is the stamp, un-peeled: the exact
    // bytes that make GitHubActionsStrategy::parse decline and LogParser drop the line today.
    const std::string defect_shape{std::string{kStamp} + " ok"};
    EXPECT_EQ(peeled.content, defect_shape)
        << "the reversed declaration must leave the STAMP intact — that is the present shipped "
           "defect, reproduced through the declaration.\n"
        << "  in       : \"" << escape(line) << "\"\n"
        << "  expected : \"" << escape(defect_shape) << "\"  (stamp survives)\n"
        << "  actual   : \"" << escape(peeled.content) << "\"\n"
        << "  If this is \"ok\", the peel is ORDER-INSENSITIVE — it is sorting, retrying or "
           "looping the rows instead of applying them in declaration order, and ADR-23.D4's "
           "outside-in semantics are not implemented. Stack ORDER would then be untested by "
           "construction, exactly as it has been since the catalogue shipped with no consumer.";

    EXPECT_FALSE(peeled.observation_time.has_value())
        << "the reversed stack must extract NO observation time: the stamp acceptor met the BOM "
           "and "
           "declined, so nothing parsed. A value here means the stamp row somehow reached its "
           "grammar despite running first — the order is not being honoured.";

    // Both orders resolve, both are legal declarations, and they produce DIFFERENT bytes. That is
    // the whole content of "order is load-bearing", asserted after both sides were pinned exactly.
    const TransportStack correct{resolve_transport_stack(
        IngestDeclaration{.stack = kBomThenGha, .dialect = {}, .channel = {}})};
    EXPECT_NE(correct.peel_raw(line).content, reversed.peel_raw(line).content)
        << "the two orders produced identical content — the stack composes its rows in some "
           "order-free way, and no declaration can express nesting";
}

// The stack-grain statement of the equivalence law: the two-row stack on `BOM + X` yields exactly
// what the ONE-row stamp stack yields on the BOM-free twin `X`. This is DN-25.D5's G-BOM-2 as
// written, and it is the same oracle the corpus arm scores at 17 487 lines — the two grains JOINED
// on one object rather than each proven alone.
TEST(BomTransportRowGBom2, TwoRowStackOnBomLineEqualsStampRowOnTheBomFreeTwin)
{
    ASSERT_NE(find_transform(kBomRow), nullptr) << kRowAbsent;
    const TransportStack composed{resolve_transport_stack(
        IngestDeclaration{.stack = kBomThenGha, .dialect = {}, .channel = {}})};
    const TransportStack stamp_only{resolve_transport_stack(
        IngestDeclaration{.stack = kGhaOnly, .dialect = {}, .channel = {}})};
    ASSERT_EQ(composed.size(), 2U);
    ASSERT_EQ(stamp_only.size(), 1U);

    for (std::size_t idx{0}; idx < kCases.size(); ++idx)
    {
        const Case& item{kCases[idx]};
        if (item.bytes.starts_with(kBom))
            continue;

        const std::string twin{std::string{kBom} + std::string{item.bytes}};
        const RawPeeledLine on_bom{composed.peel_raw(twin)};
        const RawPeeledLine on_twin{stamp_only.peel_raw(item.bytes)};

        EXPECT_EQ(on_bom.content, on_twin.content)
            << "case [" << idx << "] '" << item.label
            << "': the two-row stack on the BOM'd line and the stamp row on its BOM-free twin must "
               "agree byte-for-byte.\n"
            << "  BOM'd    : \"" << escape(twin) << "\" -> \"" << escape(on_bom.content) << "\"\n"
            << "  twin     : \"" << escape(item.bytes) << "\" -> \"" << escape(on_twin.content)
            << "\"";
        EXPECT_EQ(on_bom.observation_time.has_value(), on_twin.observation_time.has_value())
            << "case [" << idx << "] '" << item.label
            << "': the EXTRACT must agree too — a BOM must not cost the line its observation time.";
    }

    // And the other direction of "otherwise the identity": on a line with NO BOM, adding the BOM
    // row to the stack must change nothing at all. This is the over-strip guard at unit grain — the
    // corpus arm counts the same property over 22 490 937 lines.
    for (std::size_t idx{0}; idx < kCases.size(); ++idx)
    {
        const Case& item{kCases[idx]};
        if (item.bytes.starts_with(kBom))
            continue;
        EXPECT_EQ(composed.peel_raw(item.bytes).content, stamp_only.peel_raw(item.bytes).content)
            << "case [" << idx << "] '" << item.label
            << "': declaring the BOM row moved a line that carries no BOM. Declaring is "
               "SUBTRACTIVE — a stream that gains this row must lose nothing it had.";
    }
}

// The RECOGNITION door must agree with the TOKENIZER-FEEDING door byte-for-byte. A divergence
// between `peel` and `peel_raw` is the two-implementations defect the whole transport contract is
// about, and stage 1 (ANSI strip) is a fixed point on a BOM — it has no ESC byte — so the two
// doors have no licence to differ here.
TEST(BomTransportRowGBom2, BothPeelDoorsAgreeOnBomBearingLines)
{
    ASSERT_NE(find_transform(kBomRow), nullptr) << kRowAbsent;
    const TransportStack stack{resolve_transport_stack(
        IngestDeclaration{.stack = kBomThenGha, .dialect = {}, .channel = {}})};

    for (std::size_t idx{0}; idx < kCases.size(); ++idx)
    {
        const Case& item{kCases[idx]};
        std::string scratch;
        const insight::tokenization::NormalizedLine normalized{
            insight::tokenization::normalize(item.bytes, scratch)};

        const RawPeeledLine raw{stack.peel_raw(item.bytes)};
        const insight::transport::PeeledLine declared{stack.peel(normalized)};

        EXPECT_EQ(declared.content.bytes(), raw.content)
            << "case [" << idx << "] '" << item.label
            << "': the recognition door and the tokenizer-feeding door disagree.\n"
            << "  peel     : \"" << escape(declared.content.bytes()) << "\"\n"
            << "  peel_raw : \"" << escape(raw.content) << "\"\n"
            << "  These are two doors onto ONE algorithm; a byte divergence is two "
               "implementations.";
        EXPECT_EQ(declared.is_blank(), raw.is_blank())
            << "case [" << idx << "] '" << item.label << "': the two doors disagree on DROP";
    }
}

} // namespace
// NOLINTEND
