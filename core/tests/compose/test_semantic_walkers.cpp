// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_semantic_walkers.cpp — the composed-recognition ALGORITHMS, canon's
// semantic-unaware core code, exercised over SYNTHETIC rows so the mechanism is proven
// VOCABULARY-FREE. This is the homing counterpart to the package suites: the github/test_frameworks
// suites prove the real VOCABULARY; here we prove canon's ALGORITHM (gate matching, longest-match,
// payload extraction, the three LocationMatchKind families + token-boundary mechanics) independent
// of any package — so a core-algorithm regression is caught in CORE's suite even with no package
// linked. Also carries the SRC-SP-5 dynamic guard: the recognizer probe path performs ZERO heap
// allocations (a global operator-new counter, legitimate in a test binary, never in the shipped
// library). Determinism: byte-only, no RNG/clock/float.
#include <cstdlib>
#include <new>

#include <gtest/gtest.h>

import insight.canon.test; // facade (compose / walkers / enums) + spi (row grammar) — white-box aggregate

using insight::LogLevel;
using insight::recognize_location;
using insight::StructuralRole;
using insight::semantic::compose;
using insight::semantic::ComposedSemantics;
using insight::semantic::IntentMarkerRow;
using insight::semantic::kAnyDialect;
using insight::semantic::LevelLiftRow;
using insight::semantic::LocationMatchKind;
using insight::semantic::LocationRow;
using insight::semantic::PayloadExtract;
using insight::semantic::SemanticPackageManifest;
using insight::semantic::StructuralRoleRow;
using insight::semantic::ValueClassRow;
using insight::tokenization::ChildOrder;
using insight::tokenization::classify;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::lift_level;
using insight::tokenization::recognize;

// The walkers take NormalizedContent (the stage-1 normalization precondition as a type). Every
// probe in this suite is an escape-free literal, so normalize() is the zero-copy FIXED POINT: the
// content views the literal itself and the shared scratch is never written — which is what keeps
// the SRC-SP-5 no-allocation guard meaningful over the full probe path below.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════
// SRC-SP-5 heap-allocation guard — a global operator-new replacement counting allocations while
// ARMED. The replacement is a plain passthrough (forwards to malloc) unless a RAII AllocGuard is
// live, so it never perturbs the rest of the test binary; armed only around the recognizer probe
// path. This lives in the TEST binary — a global new override must NEVER ship in the canon library
// (it would intercept every product allocation), which is exactly why the "no allocation in
// recognizers" leg is homed HERE, in core, not in the installed conformance module.
// ════════════════════════════════════════════════════════════════════════════════════════════════════
namespace
{
thread_local unsigned g_alloc_armed{0};
thread_local std::size_t g_alloc_count{0};

struct AllocGuard
{
    AllocGuard() noexcept
    {
        ++g_alloc_armed;
        g_alloc_count = 0;
    }
    AllocGuard(const AllocGuard&) = delete;
    AllocGuard& operator=(const AllocGuard&) = delete;
    ~AllocGuard()
    {
        --g_alloc_armed;
    }
    [[nodiscard]] std::size_t count() const noexcept
    {
        return g_alloc_count;
    }
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
void* operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete(void* ptr) noexcept
{
    std::free(ptr);
}
void operator delete[](void* ptr) noexcept
{
    std::free(ptr);
}
void operator delete(void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}
void operator delete[](void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}

// ════════════════════════════════════════════════════════════════════════════════════════════════════
// Synthetic vocabulary — deliberately NOT any real ecosystem's tokens, so a failure implicates the
// ALGORITHM, never a package's data. Static storage: the composed rows' string_views point here
// (SRC-SP-7).
// ════════════════════════════════════════════════════════════════════════════════════════════════════
namespace
{
// TWO synthetic packages, because after T4 the gate is a composed package NAME
// and the "does not leak across dialects" leg needs a real, different, composed name to declare.
// `synth` carries the rows under test; `synth_other` carries one level-lift row and exists so a
// FOREIGN declaration is expressible without fatalling the unknown-dialect path.
constexpr std::string_view kSynth{"synth"};
constexpr std::string_view kSynthOther{"synth_other"};
constexpr std::string_view kUndeclared{}; // the caller declined to declare

// Roles: three ungated (kAnyDialect) — one plain + a longest-match pair (one prefix a proper prefix
// of the other) — + one concretely gated to `synth` (to prove role gating, which real github rows
// never exercise: they are all kAnyDialect).
constexpr std::array<StructuralRoleRow, 4> kSynthRoles{{
    {.prefix = "<OPEN>", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect},
    {.prefix = "<G>", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect},
    {.prefix = "<G-LONGER>", .role = StructuralRole::GroupEnd, .dialect_gate = kAnyDialect},
    {.prefix = "GATED>", .role = StructuralRole::Terminator, .dialect_gate = kSynth},
}};

// One intent marker, concretely gated (SRC-II-6), RemainderAfterPrefix payload.
constexpr std::array<IntentMarkerRow, 1> kSynthMarkers{{
    {.prefix = "STEP ",
     .kind = IntentMarkerKind::Step,
     .child_order = ChildOrder::Ordered,
     .dialect_gate = kSynth,
     .extract = PayloadExtract::RemainderAfterPrefix},
}};

// One LocationRow per closed family, with synthetic vocabulary.
constexpr std::array<std::string_view, 1> kSpecInfix{".chk."};
constexpr std::array<std::string_view, 1> kSpecExt{"aa"};
constexpr std::array<std::string_view, 1> kPreExt{".zz"};
constexpr std::array<std::string_view, 1> kPrePrefix{"pre_"};
constexpr std::array<std::string_view, 1> kSufSet{"_end.qq"};
constexpr std::array<LocationRow, 3> kSynthLocations{{
    {.kind = LocationMatchKind::TestSpecExtension,
     .infixes = kSpecInfix,
     .extensions = kSpecExt,
     .prefixes = {},
     .suffixes = {}},
    {.kind = LocationMatchKind::PrefixAndExtension,
     .infixes = {},
     .extensions = kPreExt,
     .prefixes = kPrePrefix,
     .suffixes = {}},
    {.kind = LocationMatchKind::SuffixSet,
     .infixes = {},
     .extensions = {},
     .prefixes = {},
     .suffixes = kSufSet},
}};

// Level lifts: a FIRST-MATCH discriminator pair + one ungated row + one gated to a second format.
// The pair is the load-bearing part: "<LVL>" is a proper prefix of "<LVL>-LONG" and is declared
// FIRST, so a line matching both resolves to the FIRST row's level under the declared first-match
// rule, and to the SECOND row's level under a longest-match rule. The two rules are therefore
// distinguishable here — which is the whole reason the pair exists, since the eight real GHA rows
// have no nesting and cannot tell them apart.
constexpr std::array<LevelLiftRow, 3> kSynthLevelLifts{{
    {.prefix = "<LVL>", .level = LogLevel::Warn, .dialect_gate = kSynth},
    {.prefix = "<LVL>-LONG", .level = LogLevel::Error, .dialect_gate = kSynth},
    {.prefix = "<ANY-LVL>", .level = LogLevel::Debug, .dialect_gate = kAnyDialect},
}};

// The FOREIGN package's single row: same shape, another owner. A row may only ever gate to its own
// package or to kAnyDialect (all_dialect_gates_owned), so the cross-dialect leg has to be built out
// of two manifests rather than out of one manifest carrying a foreign gate.
constexpr std::array<LevelLiftRow, 1> kOtherLevelLifts{{
    {.prefix = "<OTHER-LVL>", .level = LogLevel::Fatal, .dialect_gate = kSynthOther},
}};

// Lines no level-lift row claims, each for a distinct reason: ordinary text; the empty content; a
// proper PREFIX of a row key (shorter than the key, so starts_with is false in the other
// direction); and a row key that occurs but does not START the content.
constexpr std::array<std::string_view, 4> kUnclaimedLines{
    {"plain body text", "", "<LVL", " <LVL> leading"}};

constexpr SemanticPackageManifest kSynthManifest{
    .name = "synth",
    .version = "1.0.0",
    .roles = kSynthRoles,
    .markers = kSynthMarkers,
    .level_lifts = kSynthLevelLifts,
    .locations = kSynthLocations,
    .value_classes = {},
    .strategy = nullptr,
    .echoed_source = nullptr,
};

constexpr SemanticPackageManifest kOtherManifest{
    .name = kSynthOther,
    .version = "1.0.0",
    .level_lifts = kOtherLevelLifts,
};

// The three views every leg below is scored under. All three are re-derived from ONE composition,
// so the legs exercise the FILTER rather than three separately-built tables.
[[nodiscard]] ComposedSemantics compose_both()
{
    const std::array manifests{kSynthManifest, kOtherManifest};
    return compose(manifests);
}
[[nodiscard]] ComposedSemantics synth()
{
    return compose_both().for_stream(kSynth, {});
}
[[nodiscard]] ComposedSemantics other()
{
    return compose_both().for_stream(kSynthOther, {});
}
[[nodiscard]] ComposedSemantics undeclared()
{
    return compose_both().for_stream(kUndeclared, {});
}
} // namespace

// ── The DIALECT gate: kAnyDialect fires under EVERY declaration; a concrete
// gate fires only on a stream declaring ITS package — and NOT on an undeclared stream, which is the
// fail-closed half.
//
// ⚠ WHAT THIS TEST IS ABOUT, so it is not "simplified" back later. Before T4 the equivalent test
// passed a `LogFormat` per call, sourced in production from `LogParser::routed_format()` — the
// per-line detector winner under a sticky-strategy fast path. Which DECLARED rows fired was
// therefore a function of the stream's CONTENT. There is no per-call coordinate any more: the views
// are built once, and the walkers below take a line and a table. A change that reintroduces a
// per-line gate argument has undone the fix even if every assertion here still passes.
TEST(SemanticWalkers, DialectGateSemantics)
{
    const ComposedSemantics own{synth()};
    const ComposedSemantics foreign{other()};
    const ComposedSemantics none{undeclared()};

    // Ungated role fires under EVERY declaration, the undeclared stream included.
    for (const auto& [view, label] : {std::pair{std::cref(own), "the OWN dialect"},
                                      std::pair{std::cref(foreign), "a FOREIGN dialect"},
                                      std::pair{std::cref(none), "an UNDECLARED stream"}})
        EXPECT_EQ(classify(norm_probe("<OPEN>x"), view.get()), StructuralRole::GroupBegin)
            << "kAnyDialect role must fire under " << label;

    // Concretely-gated role fires ONLY on a stream declaring its package.
    EXPECT_EQ(classify(norm_probe("GATED>x"), own), StructuralRole::Terminator);
    EXPECT_EQ(classify(norm_probe("GATED>x"), foreign), StructuralRole::None)
        << "a concrete gate must not leak into another dialect's view";
    EXPECT_EQ(classify(norm_probe("GATED>x"), none), StructuralRole::None)
        << "an UNDECLARED stream must fire no concretely-gated row (fail-closed on depth)";
}

// ── Longest-match: when two prefixes both match, the LONGER wins (declaration-order-free) ──
TEST(SemanticWalkers, LongestPrefixWins)
{
    const ComposedSemantics sc{synth()};
    // "<G>" (GroupBegin) is a proper prefix of "<G-LONGER>" (GroupEnd); a line matching both
    // resolves to the longer rule's role regardless of the rows' declared order.
    EXPECT_EQ(classify(norm_probe("<G-LONGER> details"), sc), StructuralRole::GroupEnd);
    EXPECT_EQ(classify(norm_probe("<G> details"), sc), StructuralRole::GroupBegin);
}

// ── recognize(): RemainderAfterPrefix payload = the content after the matched prefix, verbatim ──
TEST(SemanticWalkers, PayloadExtractionRemainderAfterPrefix)
{
    const ComposedSemantics sc{synth()};
    const auto mark{recognize(norm_probe("STEP build the widget"), sc)};
    EXPECT_EQ(mark.kind, IntentMarkerKind::Step);
    EXPECT_EQ(mark.name, "build the widget")
        << "payload must be the verbatim remainder after \"STEP \"";
    EXPECT_EQ(mark.child_order, ChildOrder::Ordered);
    // Gated: inert on a stream declaring another dialect, and on an undeclared one.
    EXPECT_EQ(recognize(norm_probe("STEP build the widget"), other()).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize(norm_probe("STEP build the widget"), undeclared()).kind,
              IntentMarkerKind::None);
}

// ── The three LocationMatchKind families + token-boundary mechanics (whitespace skip,
// trailing-coord exclusion, leading-glyph tolerance) — all VOCABULARY-FREE over the synthetic rows
// ──
TEST(SemanticWalkers, LocationFamiliesAndTokenBoundaries)
{
    const ComposedSemantics sc{synth()};
    // 1. TestSpecExtension: `<base>.chk.aa`
    EXPECT_EQ(recognize_location(norm_probe("PASS dir/thing.chk.aa"), sc), "dir/thing.chk.aa");
    // 2. PrefixAndExtension: basename `pre_*` + ext `.zz`
    EXPECT_EQ(recognize_location(norm_probe("ok src/pre_widget.zz done"), sc), "src/pre_widget.zz");
    // 3. SuffixSet: full-file suffix `_end.qq`
    EXPECT_EQ(recognize_location(norm_probe("a/b/module_end.qq"), sc), "a/b/module_end.qq");
    // Token boundaries: trailing :line:col excluded, leading glyph + tab skipped, no-match → empty.
    EXPECT_EQ(recognize_location(norm_probe("dir/thing.chk.aa:42:5"), sc), "dir/thing.chk.aa");
    EXPECT_EQ(recognize_location(norm_probe("\xE2\x9C\x93 \t dir/thing.chk.aa (7 ms)"), sc),
              "dir/thing.chk.aa");
    EXPECT_EQ(recognize_location(norm_probe("nothing here.txt"), sc), "");
}

// ── The location's START is established by the same byte class as its END ──
// A family fixes the END of a match; whitespace fixes only where the TOKEN began. When a producer
// glues its own annotation onto the path with no separator, those two are different positions, and
// slicing from token offset 0 published the annotation INSIDE the label
// (`##[error]fs/rc/rcserver/rcserver_test.go` — one measured GitHub-Actions alert). The property is
// asserted over the SYNTHETIC rows on purpose: the repair is a byte class, not a marker table, so
// it must hold with no dialect vocabulary linked at all.
TEST(SemanticWalkers, LocationStartExcludesGluedNonPathPrefix)
{
    const ComposedSemantics sc{synth()};
    // The glued-annotation family — one witness per real shape, all three LocationMatchKinds.
    EXPECT_EQ(recognize_location(norm_probe("##[error]a/b/module_end.qq"), sc),
              "a/b/module_end.qq");
    EXPECT_EQ(recognize_location(norm_probe("##[group]dir/thing.chk.aa"), sc), "dir/thing.chk.aa");
    // `##[debug]File:` is why the repair is a byte class rather than a marker strip: removing the
    // marker alone would leave `File:` welded to the path on every one of these lines.
    EXPECT_EQ(recognize_location(norm_probe("##[debug]File:/x/src/pre_widget.zz"), sc),
              "/x/src/pre_widget.zz");
    // Wrapping punctuation — the majority shape by count, and not a marker at all.
    EXPECT_EQ(recognize_location(norm_probe("(a/b/module_end.qq)"), sc), "a/b/module_end.qq");
    EXPECT_EQ(recognize_location(norm_probe("\"dir/thing.chk.aa\""), sc), "dir/thing.chk.aa");
    EXPECT_EQ(recognize_location(norm_probe("'dir/thing.chk.aa'"), sc), "dir/thing.chk.aa");
    // A JavaScript stack frame: `<fn>@<url>`. The `:` before the PORT is the cut, so the function
    // name, the scheme and the host go and the port digits ride along — the declared residue of
    // excluding ':', asserted rather than hidden. 17 such lines were measured.
    EXPECT_EQ(
        recognize_location(norm_probe("run@http://localhost:5173/a/b/module_end.qq:12:3"), sc),
        "5173/a/b/module_end.qq");

    // The other direction, and the one that costs more if it is wrong: a byte that IS part of a
    // real path must NOT cut the label short. Each of these was observed inside a genuine path.
    EXPECT_EQ(recognize_location(norm_probe("node_modules/@scope/pkg/dir/thing.chk.aa"), sc),
              "node_modules/@scope/pkg/dir/thing.chk.aa")
        << "an npm scoped-package '@' must stay inside the location — the largest population by "
           "far (33 445 lines measured), and truncating it would trade one defect for a worse one";
    EXPECT_EQ(recognize_location(norm_probe("external/devinfra+/pkg/module_end.qq"), sc),
              "external/devinfra+/pkg/module_end.qq")
        << "a Bazel external-repo '+' must stay inside the location";
    EXPECT_EQ(recognize_location(norm_probe("plugins/***Editor/***dir/thing.chk.aa"), sc),
              "plugins/***Editor/***dir/thing.chk.aa")
        << "GitHub's secret redaction rewrites a path SEGMENT in place; '***' is the most of that "
           "directory chain that is knowable, so it stays";
    EXPECT_EQ(recognize_location(norm_probe("a\\b\\RUNNER~1\\module_end.qq"), sc),
              "a\\b\\RUNNER~1\\module_end.qq")
        << "Windows separators and 8.3 short names must stay inside the location";
    // The declared price of excluding ':' — stated here rather than discovered later: a Windows
    // drive letter is cut off with it. Measured at 11 lines against the 24 `##[debug]File:` labels
    // that excluding ':' repairs.
    EXPECT_EQ(recognize_location(norm_probe("C:\\a\\module_end.qq"), sc), "\\a\\module_end.qq");

    // And a token that is ONLY the location is returned unchanged — the repair must be inert on
    // the overwhelming majority of lines.
    EXPECT_EQ(recognize_location(norm_probe("a/b/module_end.qq"), sc), "a/b/module_end.qq");
    EXPECT_EQ(recognize_location(norm_probe("PASS dir/thing.chk.aa"), sc), "dir/thing.chk.aa");
}

// ── lift_level(): FIRST match in declared order wins — NOT longest match ──
// The level-lift walk was relocated from the GHA package into core with the rows, their order and
// the first-match rule unchanged. This is the assertion that the first-match rule
// actually survived the move: a longest-match walker (the rule classify/recognize use, and the
// natural thing to "fix" this to) returns Error here and fails.
TEST(SemanticWalkers, LevelLiftFirstDeclaredMatchWins)
{
    const ComposedSemantics sc{synth()};
    const LogLevel both{lift_level("<LVL>-LONG boom", sc)};
    EXPECT_EQ(both, LogLevel::Warn)
        << "\"<LVL>-LONG boom\" matches BOTH \"<LVL>\" (declared 1st, Warn) and \"<LVL>-LONG\" "
           "(declared 2nd, Error). First-match-in-declared-order must win: expected Warn, got "
        << insight::to_string(both)
        << (both == LogLevel::Error
                ? " — that is the LONGEST-match answer, so the walk changed rule"
                : "");
    // The shorter row alone still resolves to its own level (the pair is not an artifact).
    EXPECT_EQ(lift_level("<LVL> plain", sc), LogLevel::Warn);
}

// ── lift_level(): the dialect gate is applied by the SAME filter classify/recognize see ──
TEST(SemanticWalkers, LevelLiftDialectGateSemantics)
{
    const ComposedSemantics own{synth()};
    const ComposedSemantics foreign{other()};
    const ComposedSemantics none{undeclared()};

    // kAnyDialect row fires under every declaration, the undeclared stream included.
    for (const auto& [view, label] : {std::pair{std::cref(own), "the OWN dialect"},
                                      std::pair{std::cref(foreign), "a FOREIGN dialect"},
                                      std::pair{std::cref(none), "an UNDECLARED stream"}})
    {
        const LogLevel got{lift_level("<ANY-LVL> body", view.get())};
        EXPECT_EQ(got, LogLevel::Debug) << "kAnyDialect level lift must fire under " << label
                                        << ", got " << insight::to_string(got);
    }
    // A concretely-gated row fires only on a stream declaring its own package.
    EXPECT_EQ(lift_level("<OTHER-LVL> body", foreign), LogLevel::Fatal);
    for (const auto& [view, label] : {std::pair{std::cref(own), "a SIBLING dialect"},
                                      std::pair{std::cref(none), "an UNDECLARED stream"}})
    {
        const LogLevel got{lift_level("<OTHER-LVL> body", view.get())};
        EXPECT_EQ(got, LogLevel::Unknown)
            << "a `synth_other`-gated level lift must stay inert under " << label << ", got "
            << insight::to_string(got);
    }
}

// ── lift_level(): no row claims the line ⇒ Unknown, which means ABSENCE, not a level ──
TEST(SemanticWalkers, LevelLiftUnclaimedLineIsUnknown)
{
    const ComposedSemantics sc{synth()};
    for (const std::string_view line : kUnclaimedLines)
    {
        const LogLevel got{lift_level(line, sc)};
        EXPECT_EQ(got, LogLevel::Unknown)
            << "no declared row claims \"" << line << "\" — expected Unknown, got "
            << insight::to_string(got);
    }
}

// ── SRC-SP-5: the recognizer probe path performs ZERO heap allocations (byte-scan pure) ──
TEST(SemanticWalkers, RecognizersDoNotHeapAllocate)
{
    const ComposedSemantics sc{synth()}; // compose() may allocate — done BEFORE arming.
    std::size_t observed{0};
    {
        const AllocGuard guard;
        // Drive every walker over a representative probe set.
        (void)classify(norm_probe("<OPEN>x"), sc);
        (void)classify(norm_probe("GATED>x"), sc);
        (void)recognize(norm_probe("STEP build the widget"), sc);
        (void)lift_level("<LVL>-LONG boom", sc);
        (void)lift_level("plain body text", sc);
        (void)recognize_location(norm_probe("PASS dir/thing.chk.aa:42"), sc);
        (void)recognize_location(norm_probe("ok src/pre_widget.zz"), sc);
        (void)recognize_location(norm_probe("a/b/module_end.qq"), sc);
        // The start-establishing walk is a backwards byte scan over the same view — it must stay
        // heap-free too, so the probe set carries a glued-prefix line.
        (void)recognize_location(norm_probe("##[error]a/b/module_end.qq"), sc);
        observed = guard.count();
    }
    EXPECT_EQ(observed, 0U)
        << "the composed recognition walkers must be heap-free (SRC-SP-5): observed " << observed
        << " allocation(s) over the probe path.";
}
// NOLINTEND
