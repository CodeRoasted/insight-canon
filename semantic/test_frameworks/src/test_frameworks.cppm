// invariant: this package ships DATA only: strategy and echoed_source are null, and the matching
// algorithm is canon's recognize_location over the composed rows.
// invariant: test-file naming is CI-dialect-independent, so one vocabulary covers jest, vitest,
// playwright, pytest, go and ruby rather than one set per dialect.
module;

export module insight.semantic.test_frameworks;
import insight.canon.internal;
// invariant: api is the base import of every semantic package; this one names no api entity, its
// rows being file-naming vocabulary rather than log grammar.
import insight.canon.api;
// invariant: spi is re-exported so a consumer importing this module can name
// SemanticPackageManifest, which is kManifest's type.
export import insight.canon.spi;

namespace insight::semantic::test_frameworks
{

// refs: SRC-SP-7
// invariant: LocationRow holds spans into these arrays, so each keeps package-static constexpr
// storage for as long as a composed manifest can be read.
inline constexpr std::array<std::string_view, 2> kTestSpecInfixes{".test.", ".spec."};
inline constexpr std::array<std::string_view, 7> kTestSpecExtensions{"ts",  "tsx", "js", "jsx",
                                                                     "mjs", "cjs", "py"};
inline constexpr std::array<std::string_view, 1> kPytestBasenamePrefixes{"test_"};
inline constexpr std::array<std::string_view, 1> kPytestBasenameSuffixes{"_test"};
inline constexpr std::array<std::string_view, 1> kPytestExtensions{".py"};
inline constexpr std::array<std::string_view, 3> kGoRubySuffixes{"_test.go", "_spec.rb",
                                                                 "_test.rb"};

inline constexpr std::array<LocationRow, 3> kLocations{{
    {.kind = LocationMatchKind::TestSpecExtension,
     .infixes = kTestSpecInfixes,
     .extensions = kTestSpecExtensions,
     .prefixes = {},
     .suffixes = {}},
    {.kind = LocationMatchKind::PrefixAndExtension,
     .infixes = {},
     .extensions = kPytestExtensions,
     .prefixes = kPytestBasenamePrefixes,
     .suffixes = kPytestBasenameSuffixes},
    {.kind = LocationMatchKind::SuffixSet,
     .infixes = {},
     .extensions = {},
     .prefixes = {},
     .suffixes = kGoRubySuffixes},
}};

// refs: ADR-17.D9
// invariant: this package has no vendor, so v1 names OUR first generation of these families and
// moves only when the family SET changes shape.
// invariant: the coordinate is declared and non-empty in every manifest, so a reader comparing two
// identity digests finds the same field in each of them.
export inline constexpr std::array<std::string_view, 1> kDialectRevisions{{"v1"}};

// refs: SRC-SP-7
// invariant: compose orders packages by name, so these rows follow github's in the canonical order
// that the identity digest is taken over.
export inline constexpr SemanticPackageManifest kManifest{
    .name = "test_frameworks",
    .version = "1.0.0",
    .roles = {},
    .markers = {},
    .emits = {},
    .level_lifts = {},
    .locations = kLocations,
    .value_classes = {},
    .dialect_revisions = kDialectRevisions,
    .strategy = nullptr,
    .echoed_source = nullptr,
};

// refs: ADR-17.D9
// invariant: this package ships no dialect gates, so this is its only compile-time declaration
// check.
static_assert(
    insight::semantic::all_revisions_named(kDialectRevisions),
    "test_frameworks: the declared dialect-revision vocabulary must be non-empty, with unique, "
    "non-empty names (grammar-6 — the coordinate is what a reader compares generations "
    "on, so an unnamed or repeated one is not a declaration)");

} // namespace insight::semantic::test_frameworks
