// invariant: the composed-recognition ALGORITHMS — canon's semantic-unaware core, exercised over
// SYNTHETIC rows so the mechanism is proven VOCABULARY-FREE.
// invariant: this is the homing counterpart to the package suites — those prove the real
// VOCABULARY, this proves canon's ALGORITHM independent of any package.
// invariant: so a core-algorithm regression is caught in CORE's suite even with no package linked.
// invariant: it also carries the dynamic no-allocation guard — the recognizer probe path performs
// ZERO heap allocations, measured by a global operator-new counter.
// invariant: that counter is legitimate in a test binary and NEVER in the shipped library.
// invariant: determinism — byte-only, with no RNG, clock or float.
// refs: SRC-SP-5
#include <cstdlib>
#include <new>

#include <gtest/gtest.h>

// invariant: ONE import gives the facade and the row grammar together, because the test module is a
// deliberate WHITE-BOX aggregate.
import insight.canon.test;

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

// invariant: every probe here is an escape-free literal, so normalization is the zero-copy FIXED
// POINT — the content views the literal itself and the shared scratch is never written.
// invariant: that is what keeps the no-allocation guard meaningful over the full probe path.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

// invariant: a global operator-new replacement counting allocations while ARMED.
// invariant: the replacement is a plain passthrough unless a scoped guard is live, so it never
// perturbs the rest of the test binary, and it is armed only around the recognizer probe path.
// invariant: this lives in the TEST binary, because a global new override must NEVER ship in the
// canon library, where it would intercept every product allocation.
// invariant: that is exactly why the no-allocation leg is homed HERE, in core, rather than in the
// installed conformance module.
// refs: SRC-SP-5
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

// invariant: the synthetic vocabulary is deliberately NOT any real ecosystem's tokens, so a failure
// implicates the ALGORITHM and never a package's data.
// invariant: static storage, because the composed rows' views point here.
// refs: SRC-SP-7
namespace
{
// invariant: TWO synthetic packages, because the gate is a composed package NAME and the
// does-not-leak-across-dialects leg needs a real, different, composed name to declare.
// invariant: the second package carries one level-lift row and exists so a FOREIGN declaration is
// expressible without fatalling the unknown-dialect path.
constexpr std::string_view kSynth{"synth"};
constexpr std::string_view kSynthOther{"synth_other"};
constexpr std::string_view kUndeclared{};

// invariant: three ungated roles — one plain plus a longest-match pair where one prefix is a
// proper prefix of the other — and one concretely gated row to prove role gating.
// invariant: the real rows never exercise role gating, because they are all ungated.
constexpr std::array<StructuralRoleRow, 4> kSynthRoles{{
    {.prefix = "<OPEN>", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect},
    {.prefix = "<G>", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect},
    {.prefix = "<G-LONGER>", .role = StructuralRole::GroupEnd, .dialect_gate = kAnyDialect},
    {.prefix = "GATED>", .role = StructuralRole::Terminator, .dialect_gate = kSynth},
}};

// invariant: one intent marker, concretely gated, with a remainder-after-prefix payload.
// refs: SRC-II-6
constexpr std::array<IntentMarkerRow, 1> kSynthMarkers{{
    {.prefix = "STEP ",
     .kind = IntentMarkerKind::Step,
     .child_order = ChildOrder::Ordered,
     .dialect_gate = kSynth,
     .extract = PayloadExtract::RemainderAfterPrefix},
}};

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

// invariant: a FIRST-MATCH discriminator pair plus one ungated row and one gated to a second
// format.
// invariant: the PAIR is the load-bearing part — the shorter prefix is a proper prefix of the
// longer and is declared FIRST.
// invariant: so a line matching both resolves to the FIRST row's level under first-match and to the
// SECOND row's under longest-match, which makes the two rules DISTINGUISHABLE here.
// invariant: that is the whole reason the pair exists, since the real rows have no nesting and
// cannot tell the rules apart.
constexpr std::array<LevelLiftRow, 3> kSynthLevelLifts{{
    {.prefix = "<LVL>", .level = LogLevel::Warn, .dialect_gate = kSynth},
    {.prefix = "<LVL>-LONG", .level = LogLevel::Error, .dialect_gate = kSynth},
    {.prefix = "<ANY-LVL>", .level = LogLevel::Debug, .dialect_gate = kAnyDialect},
}};

// invariant: a row may only ever gate to its OWN package or to the any-dialect gate, so the
// cross-dialect leg has to be built out of TWO manifests rather than one carrying a foreign gate.
constexpr std::array<LevelLiftRow, 1> kOtherLevelLifts{{
    {.prefix = "<OTHER-LVL>", .level = LogLevel::Fatal, .dialect_gate = kSynthOther},
}};

// invariant: lines no level-lift row claims, each for a DISTINCT reason.
// invariant: ordinary text, the empty content, a proper PREFIX of a row key, and a row key that
// occurs but does not START the content.
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

// invariant: all three views are re-derived from ONE composition, so the legs exercise the FILTER
// rather than three separately-built tables.
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

// invariant: the any-dialect gate fires under EVERY declaration; a concrete gate fires only on a
// stream declaring ITS package, and not on an undeclared stream, which is the fail-closed half.
// invariant: WHAT THIS TEST IS ABOUT, so it is not simplified back later.
// invariant: the equivalent test once passed a per-call format sourced from the per-line detector
// winner under a sticky-strategy fast path.
// invariant: which DECLARED rows fired was therefore a function of the stream's CONTENT.
// invariant: there is no per-call coordinate any more — the views are built once and the walkers
// take a line and a table.
// invariant: a change that reintroduces a per-line gate argument has UNDONE the fix even if every
// assertion here still passes.
TEST(SemanticWalkers, DialectGateSemantics)
{
    const ComposedSemantics own{synth()};
    const ComposedSemantics foreign{other()};
    const ComposedSemantics none{undeclared()};

    // invariant: an ungated role fires under EVERY declaration, the undeclared stream included.
    for (const auto& [view, label] : {std::pair{std::cref(own), "the OWN dialect"},
                                      std::pair{std::cref(foreign), "a FOREIGN dialect"},
                                      std::pair{std::cref(none), "an UNDECLARED stream"}})
        EXPECT_EQ(classify(norm_probe("<OPEN>x"), view.get()), StructuralRole::GroupBegin)
            << "kAnyDialect role must fire under " << label;

    // invariant: a concretely-gated role fires ONLY on a stream declaring its package.
    EXPECT_EQ(classify(norm_probe("GATED>x"), own), StructuralRole::Terminator);
    EXPECT_EQ(classify(norm_probe("GATED>x"), foreign), StructuralRole::None)
        << "a concrete gate must not leak into another dialect's view";
    EXPECT_EQ(classify(norm_probe("GATED>x"), none), StructuralRole::None)
        << "an UNDECLARED stream must fire no concretely-gated row (fail-closed on depth)";
}

TEST(SemanticWalkers, LongestPrefixWins)
{
    const ComposedSemantics sc{synth()};
    // invariant: the shorter prefix is a proper prefix of the longer, and a line matching both
    // resolves to the LONGER rule's role regardless of the rows' declared order.
    EXPECT_EQ(classify(norm_probe("<G-LONGER> details"), sc), StructuralRole::GroupEnd);
    EXPECT_EQ(classify(norm_probe("<G> details"), sc), StructuralRole::GroupBegin);
}

TEST(SemanticWalkers, PayloadExtractionRemainderAfterPrefix)
{
    const ComposedSemantics sc{synth()};
    const auto mark{recognize(norm_probe("STEP build the widget"), sc)};
    EXPECT_EQ(mark.kind, IntentMarkerKind::Step);
    EXPECT_EQ(mark.name, "build the widget")
        << "payload must be the verbatim remainder after \"STEP \"";
    EXPECT_EQ(mark.child_order, ChildOrder::Ordered);
    // invariant: a gated marker is INERT on a stream declaring another dialect, and on an
    // undeclared one.
    EXPECT_EQ(recognize(norm_probe("STEP build the widget"), other()).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize(norm_probe("STEP build the widget"), undeclared()).kind,
              IntentMarkerKind::None);
}

// invariant: the three location families plus the token-boundary mechanics — whitespace skip,
// trailing-coordinate exclusion and leading-glyph tolerance — all VOCABULARY-FREE.
TEST(SemanticWalkers, LocationFamiliesAndTokenBoundaries)
{
    const ComposedSemantics sc{synth()};
    EXPECT_EQ(recognize_location(norm_probe("PASS dir/thing.chk.aa"), sc), "dir/thing.chk.aa");
    EXPECT_EQ(recognize_location(norm_probe("ok src/pre_widget.zz done"), sc), "src/pre_widget.zz");
    EXPECT_EQ(recognize_location(norm_probe("a/b/module_end.qq"), sc), "a/b/module_end.qq");
    // invariant: token boundaries — a trailing line and column are excluded, a leading glyph and
    // tab are skipped, and no match yields empty.
    EXPECT_EQ(recognize_location(norm_probe("dir/thing.chk.aa:42:5"), sc), "dir/thing.chk.aa");
    EXPECT_EQ(recognize_location(norm_probe("\xE2\x9C\x93 \t dir/thing.chk.aa (7 ms)"), sc),
              "dir/thing.chk.aa");
    EXPECT_EQ(recognize_location(norm_probe("nothing here.txt"), sc), "");
}

// invariant: THE LOCATION'S START IS ESTABLISHED BY THE SAME BYTE CLASS AS ITS END.
// invariant: a family fixes the END of a match while whitespace fixes only where the TOKEN began,
// and when a producer glues its own annotation onto the path those are different positions.
// invariant: slicing from token offset 0 published the annotation INSIDE the label, which was one
// measured alert.
// invariant: the property is asserted over the SYNTHETIC rows on purpose — the repair is a BYTE
// CLASS, not a marker table, so it must hold with no dialect vocabulary linked at all.
TEST(SemanticWalkers, LocationStartExcludesGluedNonPathPrefix)
{
    const ComposedSemantics sc{synth()};
    // invariant: the glued-annotation family, one witness per real shape, across all three location
    // families.
    EXPECT_EQ(recognize_location(norm_probe("##[error]a/b/module_end.qq"), sc),
              "a/b/module_end.qq");
    EXPECT_EQ(recognize_location(norm_probe("##[group]dir/thing.chk.aa"), sc), "dir/thing.chk.aa");
    // invariant: an annotation ending in a colon-terminated word is why the repair is a BYTE CLASS
    // rather than a marker strip.
    // invariant: removing the marker alone would leave that word welded to the path on every one of
    // these lines.
    EXPECT_EQ(recognize_location(norm_probe("##[debug]File:/x/src/pre_widget.zz"), sc),
              "/x/src/pre_widget.zz");
    // invariant: wrapping punctuation is the MAJORITY shape by count, and it is not a marker at
    // all.
    EXPECT_EQ(recognize_location(norm_probe("(a/b/module_end.qq)"), sc), "a/b/module_end.qq");
    EXPECT_EQ(recognize_location(norm_probe("\"dir/thing.chk.aa\""), sc), "dir/thing.chk.aa");
    EXPECT_EQ(recognize_location(norm_probe("'dir/thing.chk.aa'"), sc), "dir/thing.chk.aa");
    // invariant: in a script stack frame the colon before the PORT is the cut, so the function
    // name, the scheme and the host go while the port digits ride along.
    // invariant: that is the declared RESIDUE of excluding the colon, asserted rather than hidden,
    // and 17 such lines were measured.
    EXPECT_EQ(
        recognize_location(norm_probe("run@http://localhost:5173/a/b/module_end.qq:12:3"), sc),
        "5173/a/b/module_end.qq");

    // invariant: the other direction, and the one that costs more if it is wrong — a byte that IS
    // part of a real path must NOT cut the label short.
    // invariant: each of these was OBSERVED inside a genuine path.
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
    // invariant: the declared PRICE of excluding the colon, stated here rather than discovered
    // later — a drive letter is cut off with it.
    // invariant: measured at 11 lines against the 24 annotated labels that excluding the colon
    // repairs.
    EXPECT_EQ(recognize_location(norm_probe("C:\\a\\module_end.qq"), sc), "\\a\\module_end.qq");

    // invariant: a token that is ONLY the location is returned unchanged, because the repair must
    // be INERT on the overwhelming majority of lines.
    EXPECT_EQ(recognize_location(norm_probe("a/b/module_end.qq"), sc), "a/b/module_end.qq");
    EXPECT_EQ(recognize_location(norm_probe("PASS dir/thing.chk.aa"), sc), "dir/thing.chk.aa");
}

// invariant: FIRST match in declared order wins, and NOT longest match.
// invariant: the level-lift walk was relocated into core with the rows, their order and the
// first-match rule unchanged, and this is the assertion that the rule survived the move.
// invariant: a longest-match walker — the rule the other walkers use, and the natural thing to
// `fix` this to — returns the wrong level here and FAILS.
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
    // invariant: the shorter row alone still resolves to its own level, so the pair is not an
    // artifact.
    EXPECT_EQ(lift_level("<LVL> plain", sc), LogLevel::Warn);
}

// invariant: the dialect gate is applied by the SAME filter the other walkers see.
TEST(SemanticWalkers, LevelLiftDialectGateSemantics)
{
    const ComposedSemantics own{synth()};
    const ComposedSemantics foreign{other()};
    const ComposedSemantics none{undeclared()};

    for (const auto& [view, label] : {std::pair{std::cref(own), "the OWN dialect"},
                                      std::pair{std::cref(foreign), "a FOREIGN dialect"},
                                      std::pair{std::cref(none), "an UNDECLARED stream"}})
    {
        const LogLevel got{lift_level("<ANY-LVL> body", view.get())};
        EXPECT_EQ(got, LogLevel::Debug) << "kAnyDialect level lift must fire under " << label
                                        << ", got " << insight::to_string(got);
    }
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

// invariant: when no row claims the line the result is Unknown, which means ABSENCE and not a
// level.
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

// invariant: the recognizer probe path performs ZERO heap allocations — it is a pure byte scan.
// refs: SRC-SP-5
TEST(SemanticWalkers, RecognizersDoNotHeapAllocate)
{
    // invariant: composition MAY allocate, so it is done BEFORE the counter is armed.
    const ComposedSemantics sc{synth()};
    std::size_t observed{0};
    {
        const AllocGuard guard;
        (void)classify(norm_probe("<OPEN>x"), sc);
        (void)classify(norm_probe("GATED>x"), sc);
        (void)recognize(norm_probe("STEP build the widget"), sc);
        (void)lift_level("<LVL>-LONG boom", sc);
        (void)lift_level("plain body text", sc);
        (void)recognize_location(norm_probe("PASS dir/thing.chk.aa:42"), sc);
        (void)recognize_location(norm_probe("ok src/pre_widget.zz"), sc);
        (void)recognize_location(norm_probe("a/b/module_end.qq"), sc);
        // invariant: the start-establishing walk is a BACKWARDS byte scan over the same view, so it
        // must stay heap-free too — which is why the probe set carries a glued-prefix line.
        (void)recognize_location(norm_probe("##[error]a/b/module_end.qq"), sc);
        observed = guard.count();
    }
    EXPECT_EQ(observed, 0U)
        << "the composed recognition walkers must be heap-free (SRC-SP-5): observed " << observed
        << " allocation(s) over the probe path.";
}
