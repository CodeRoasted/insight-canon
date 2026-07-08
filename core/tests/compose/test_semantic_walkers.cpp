// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_semantic_walkers.cpp — the composed-recognition ALGORITHMS (ADR 0024 §3), canon's semantic-unaware
// core code, exercised over SYNTHETIC rows so the mechanism is proven VOCABULARY-FREE. This is the homing
// counterpart to the package suites: the github/test_frameworks suites prove the real VOCABULARY; here we
// prove canon's ALGORITHM (gate matching, longest-match, payload extraction, the three LocationMatchKind
// families + token-boundary mechanics) independent of any package — so a core-algorithm regression is
// caught in CORE's suite even with no package linked. Also carries the SP-5 dynamic guard: the recognizer
// probe path performs ZERO heap allocations (a global operator-new counter, legitimate in a test binary,
// never in the shipped library). Determinism: byte-only, no RNG/clock/float.
#include <cstdlib>
#include <new>

#include <gtest/gtest.h>

import insight.canon.test; // facade (compose / walkers / enums) + spi (row grammar) — white-box aggregate

using insight::LogFormat;
using insight::StructuralRole;
using insight::recognize_location;
using insight::semantic::ComposedSemantics;
using insight::semantic::compose;
using insight::semantic::IntentMarkerRow;
using insight::semantic::kAnyFormat;
using insight::semantic::LocationMatchKind;
using insight::semantic::LocationRow;
using insight::semantic::LevelLiftRow;
using insight::semantic::PayloadExtract;
using insight::semantic::SemanticPackageManifest;
using insight::semantic::StructuralRoleRow;
using insight::semantic::ValueClassRow;
using insight::tokenization::ChildOrder;
using insight::tokenization::classify;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

// ════════════════════════════════════════════════════════════════════════════════════════════════════
// SP-5 heap-allocation guard — a global operator-new replacement counting allocations while ARMED. The
// replacement is a plain passthrough (forwards to malloc) unless a RAII AllocGuard is live, so it never
// perturbs the rest of the test binary; armed only around the recognizer probe path. This lives in the
// TEST binary — a global new override must NEVER ship in the canon library (it would intercept every
// product allocation), which is exactly why the "no allocation in recognizers" leg is homed HERE, in
// core, not in the installed conformance module.
// ════════════════════════════════════════════════════════════════════════════════════════════════════
namespace
{
thread_local unsigned g_alloc_armed{0};
thread_local std::size_t g_alloc_count{0};

struct AllocGuard
{
    AllocGuard() noexcept { ++g_alloc_armed; g_alloc_count = 0; }
    AllocGuard(const AllocGuard&) = delete;
    AllocGuard& operator=(const AllocGuard&) = delete;
    ~AllocGuard() { --g_alloc_armed; }
    [[nodiscard]] std::size_t count() const noexcept { return g_alloc_count; }
};
} // namespace

void* operator new(std::size_t size)
{
    if (g_alloc_armed != 0)
        ++g_alloc_count;
    void* ptr{std::malloc(size != 0 ? size : 1)};
    if (ptr == nullptr)
        throw std::bad_alloc{};
    return ptr;
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

// ════════════════════════════════════════════════════════════════════════════════════════════════════
// Synthetic vocabulary — deliberately NOT any real ecosystem's tokens, so a failure implicates the
// ALGORITHM, never a package's data. Static storage: the composed rows' string_views point here (SP-7).
// ════════════════════════════════════════════════════════════════════════════════════════════════════
namespace
{
// Roles: one ungated (kAnyFormat) + a longest-match pair (one prefix a proper prefix of the other) + one
// concretely gated to Syslog (to prove role gating, which real github rows never exercise — they are all
// kAnyFormat).
constexpr std::array<StructuralRoleRow, 4> kSynthRoles{{
    {.prefix = "<OPEN>", .role = StructuralRole::GroupBegin, .format_gate = kAnyFormat},
    {.prefix = "<G>", .role = StructuralRole::GroupBegin, .format_gate = kAnyFormat},
    {.prefix = "<G-LONGER>", .role = StructuralRole::GroupEnd, .format_gate = kAnyFormat},
    {.prefix = "GATED>", .role = StructuralRole::Terminator, .format_gate = LogFormat::Syslog},
}};

// One intent marker, concretely gated (II-6), RemainderAfterPrefix payload.
constexpr std::array<IntentMarkerRow, 1> kSynthMarkers{{
    {.prefix = "STEP ",
     .kind = IntentMarkerKind::Step,
     .child_order = ChildOrder::Ordered,
     .format_gate = LogFormat::Syslog,
     .extract = PayloadExtract::RemainderAfterPrefix},
}};

// One LocationRow per closed family, with synthetic vocabulary.
constexpr std::array<std::string_view, 1> kSpecInfix{".chk."};
constexpr std::array<std::string_view, 1> kSpecExt{"aa"};
constexpr std::array<std::string_view, 1> kPreExt{".zz"};
constexpr std::array<std::string_view, 1> kPrePrefix{"pre_"};
constexpr std::array<std::string_view, 1> kSufSet{"_end.qq"};
constexpr std::array<LocationRow, 3> kSynthLocations{{
    {.kind = LocationMatchKind::TestSpecExtension, .infixes = kSpecInfix, .extensions = kSpecExt,
     .prefixes = {}, .suffixes = {}},
    {.kind = LocationMatchKind::PrefixAndExtension, .infixes = {}, .extensions = kPreExt,
     .prefixes = kPrePrefix, .suffixes = {}},
    {.kind = LocationMatchKind::SuffixSet, .infixes = {}, .extensions = {}, .prefixes = {},
     .suffixes = kSufSet},
}};

constexpr SemanticPackageManifest kSynthManifest{
    .name = "synth",
    .version = "1.0.0",
    .roles = kSynthRoles,
    .markers = kSynthMarkers,
    .level_lifts = {},
    .locations = kSynthLocations,
    .value_classes = {},
    .strategy = nullptr,
    .echoed_source = nullptr,
};

[[nodiscard]] ComposedSemantics synth()
{
    const std::array manifests{kSynthManifest};
    return compose(manifests);
}
} // namespace

// ── gate_matches: kAnyFormat fires on ANY format; a concrete gate fires ONLY on its exact format (and
// NOT on an Unknown-routed line — the pre-split `format != X → {}` guard) ──
TEST(SemanticWalkers, FormatGateSemantics)
{
    const ComposedSemantics sc{synth()};
    // Ungated role fires regardless of routed format.
    for (const LogFormat fmt : {LogFormat::Unknown, LogFormat::JSON, LogFormat::Syslog, LogFormat::RawText})
        EXPECT_EQ(classify("<OPEN>x", fmt, sc), StructuralRole::GroupBegin)
            << "kAnyFormat role must fire under " << insight::to_string(fmt);
    // Concretely-gated role fires ONLY on its format, never on a mismatch or Unknown.
    EXPECT_EQ(classify("GATED>x", LogFormat::Syslog, sc), StructuralRole::Terminator);
    EXPECT_EQ(classify("GATED>x", LogFormat::JSON, sc), StructuralRole::None) << "concrete gate must not leak";
    EXPECT_EQ(classify("GATED>x", LogFormat::Unknown, sc), StructuralRole::None)
        << "an Unknown-routed line must not trigger a concretely-gated row";
}

// ── Longest-match: when two prefixes both match, the LONGER wins (declaration-order-free) ──
TEST(SemanticWalkers, LongestPrefixWins)
{
    const ComposedSemantics sc{synth()};
    // "<G>" (GroupBegin) is a proper prefix of "<G-LONGER>" (GroupEnd); a line matching both resolves to
    // the longer rule's role regardless of the rows' declared order.
    EXPECT_EQ(classify("<G-LONGER> details", LogFormat::JSON, sc), StructuralRole::GroupEnd);
    EXPECT_EQ(classify("<G> details", LogFormat::JSON, sc), StructuralRole::GroupBegin);
}

// ── recognize(): RemainderAfterPrefix payload = the content after the matched prefix, verbatim ──
TEST(SemanticWalkers, PayloadExtractionRemainderAfterPrefix)
{
    const ComposedSemantics sc{synth()};
    const auto mark{recognize("STEP build the widget", LogFormat::Syslog, sc)};
    EXPECT_EQ(mark.kind, IntentMarkerKind::Step);
    EXPECT_EQ(mark.name, "build the widget") << "payload must be the verbatim remainder after \"STEP \"";
    EXPECT_EQ(mark.child_order, ChildOrder::Ordered);
    // Gated: inert under a different format.
    EXPECT_EQ(recognize("STEP build the widget", LogFormat::JSON, sc).kind, IntentMarkerKind::None);
}

// ── The three LocationMatchKind families + token-boundary mechanics (whitespace skip, trailing-coord
// exclusion, leading-glyph tolerance) — all VOCABULARY-FREE over the synthetic rows ──
TEST(SemanticWalkers, LocationFamiliesAndTokenBoundaries)
{
    const ComposedSemantics sc{synth()};
    // 1. TestSpecExtension: `<base>.chk.aa`
    EXPECT_EQ(recognize_location("PASS dir/thing.chk.aa", sc), "dir/thing.chk.aa");
    // 2. PrefixAndExtension: basename `pre_*` + ext `.zz`
    EXPECT_EQ(recognize_location("ok src/pre_widget.zz done", sc), "src/pre_widget.zz");
    // 3. SuffixSet: full-file suffix `_end.qq`
    EXPECT_EQ(recognize_location("a/b/module_end.qq", sc), "a/b/module_end.qq");
    // Token boundaries: trailing :line:col excluded, leading glyph + tab skipped, no-match → empty.
    EXPECT_EQ(recognize_location("dir/thing.chk.aa:42:5", sc), "dir/thing.chk.aa");
    EXPECT_EQ(recognize_location("\xE2\x9C\x93 \t dir/thing.chk.aa (7 ms)", sc), "dir/thing.chk.aa");
    EXPECT_EQ(recognize_location("nothing here.txt", sc), "");
}

// ── SP-5: the recognizer probe path performs ZERO heap allocations (byte-scan pure) ──
TEST(SemanticWalkers, RecognizersDoNotHeapAllocate)
{
    const ComposedSemantics sc{synth()}; // compose() may allocate — done BEFORE arming.
    std::size_t observed{0};
    {
        const AllocGuard guard;
        // Drive every walker over a representative probe set.
        (void)classify("<OPEN>x", LogFormat::JSON, sc);
        (void)classify("GATED>x", LogFormat::Syslog, sc);
        (void)recognize("STEP build the widget", LogFormat::Syslog, sc);
        (void)recognize_location("PASS dir/thing.chk.aa:42", sc);
        (void)recognize_location("ok src/pre_widget.zz", sc);
        (void)recognize_location("a/b/module_end.qq", sc);
        observed = guard.count();
    }
    EXPECT_EQ(observed, 0U)
        << "the composed recognition walkers must be heap-free (SP-5): observed " << observed
        << " allocation(s) over the probe path.";
}
// NOLINTEND
