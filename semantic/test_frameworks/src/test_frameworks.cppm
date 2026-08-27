// insight.semantic.test_frameworks — test-framework file-location semantic package.
// VOCABULARY as DATA: the jest/vitest/playwright/pytest/go/ruby test-file naming families, as
// LocationRows in the closed canon rule grammar. NO code tier — the matching ALGORITHM lives in
// canon core (recognize_location walks these composed rows via the closed LocationMatchKind
// families); framework file-naming is CI-dialect-independent, so this is pure data. Composed via
// insight::semantic::compose().
//
// `export import insight.canon.spi` so a consumer importing this module can name
// SemanticPackageManifest (kManifest's type).
module;

export module insight.semantic.test_frameworks;
import insight.canon.internal; // std
import insight.canon.api; // (grammar rows reference no api enum here, but api is the base import)
export import insight.canon.spi;

namespace insight::semantic::test_frameworks
{

// ── Family vocabulary (package-static; the LocationRow spans point here — SRC-SP-7 lifetime) ──
// jest/vitest/playwright/pytest `.test.<ext>` / `.spec.<ext>` with ext ∈ this set.
inline constexpr std::array<std::string_view, 2> kTestSpecInfixes{".test.", ".spec."};
inline constexpr std::array<std::string_view, 7> kTestSpecExtensions{"ts",  "tsx", "js", "jsx",
                                                                     "mjs", "cjs", "py"};
// pytest bare module: basename `test_*` / `*_test`, extension `.py`.
inline constexpr std::array<std::string_view, 1> kPytestBasenamePrefixes{"test_"};
inline constexpr std::array<std::string_view, 1> kPytestBasenameSuffixes{"_test"};
inline constexpr std::array<std::string_view, 1> kPytestExtensions{".py"};
// go / ruby: full file suffixes, word-boundary-terminated.
inline constexpr std::array<std::string_view, 3> kGoRubySuffixes{"_test.go", "_spec.rb",
                                                                 "_test.rb"};

// ── Location rows (§2.2) — one per family, mapping 1:1 to the three LocationMatchKinds ──
inline constexpr std::array<LocationRow, 3> kLocations{{
    {.kind = LocationMatchKind::TestSpecExtension,
     .infixes = kTestSpecInfixes,
     .extensions = kTestSpecExtensions,
     .prefixes = {},
     .suffixes = {}},
    {.kind = LocationMatchKind::PrefixAndExtension,
     .infixes = {},
     .extensions = kPytestExtensions,      // `.py` — the algorithm anchors on this extension
     .prefixes = kPytestBasenamePrefixes,  // basename starts_with `test_`
     .suffixes = kPytestBasenameSuffixes}, // …or basename ends_with `_test`
    {.kind = LocationMatchKind::SuffixSet,
     .infixes = {},
     .extensions = {},
     .prefixes = {},
     .suffixes = kGoRubySuffixes}, // full-file suffix set
}};

// ── The declared REVISION vocabulary (grammar-6, ADR-17.D9) ──
// This package has no VENDOR: the test-file naming families are a convention shared across jest /
// vitest / playwright / pytest / go / ruby, not one supplier's syntax. The coordinate is still
// declared and still non-empty, because the grammar admits no package that declares no generation
// at all, and because a reader comparing two digests must be able to see the same field in every
// manifest. `v1` names OUR first generation of these families; it moves when the FAMILY SET
// changes shape, which is the only referent this package has for the coordinate.
export inline constexpr std::array<std::string_view, 1> kDialectRevisions{{"v1"}};

// ── The manifest (§2.5) — the package's single composed contribution ──
// Ships only location rows (no roles/markers/level-lifts/value-classes, no code tier). version
// "1.0.0" (SRC-SP-7). name sorts after "github" → github rows precede test_frameworks rows in
// canonical order.
export inline constexpr SemanticPackageManifest kManifest{
    .name = "test_frameworks",
    .version = "1.0.0",
    .roles = {},
    .markers = {},
    .emits = {}, // no markers ⇒ no intents to generate
    .level_lifts = {},
    .locations = kLocations,
    .value_classes = {},
    .dialect_revisions = kDialectRevisions, // grammar-6 — the naming-family generation
    .strategy = nullptr,
    .echoed_source = nullptr,
};

// grammar-6 (ADR-17.D9) — the declared revision vocabulary, checked in the package that declares
// it. This package ships no dialect gates, so this is its ONLY compile-time declaration check.
static_assert(
    insight::semantic::all_revisions_named(kDialectRevisions),
    "test_frameworks: the declared dialect-revision vocabulary must be non-empty, with unique, "
    "non-empty names (grammar-6 — the coordinate is what a reader compares generations "
    "on, so an unnamed or repeated one is not a declaration)");

} // namespace insight::semantic::test_frameworks
