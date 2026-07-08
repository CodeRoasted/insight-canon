// insight.canon.compose — static composition of semantic packages into a ComposedSemantics
// (ADR 0024 §3/§4). The consumer binary names its composition ONCE —
// `compose(std::array{github::kManifest, test_frameworks::kManifest})` — and threads the result
// into every Tokenizer. Composition is:
//   - STATIC: fixed by which manifests the call names (no dynamic loading).
//   - EXPLICIT: no static-init self-registration.
//   - CANONICALLY ORDERED: row order is a pure function of the package SET — core rows first (none
//     today), then packages sorted by name, declared order within — never link/registration order,
//     because table order is template identity.
//   - FAIL-CLOSED: an exact-duplicate match key across rows is a build error (constexpr
//     `find_conflict`, usable in static_assert) / a startup fatal invariant (the runtime compose).
//   - IDENTITY-BEARING: the composed rule set gets a content hash (semantic_identity, §4) over its
//     canonical serialization — the II-7 comparability key.
//
// Public + installed (product binaries call compose). The facade `export import`s this so
// `import insight.canon;` yields Tokenizer + compose + ComposedSemantics. It plain-imports spi (does
// NOT re-export it) — a consumer naming SemanticPackageManifest imports the package module (which
// carries spi); the provider contract stays off the consumer's default surface.
module;

export module insight.canon.compose;
import insight.canon.internal; // std + global C fixed-width types
import insight.canon.api;      // StructuralRole/IntentMarkerKind/LogFormat/…
import insight.canon.spi;      // the grammar rows + SemanticPackageManifest (plain import — not re-exported)

export namespace insight::semantic
{

// The truncated-SHA-256 width of semantic_identity (§4.1 — the TemplateId 16-byte precedent).
inline constexpr std::size_t kSemanticIdentityBytes{16};

// A composed package's identity, carried for the wire block's legibility (§4.2 — an operator can read
// what vocabulary a report understood). The hash (§4.1) is the key; this list is the label.
struct ComposedPackage
{
    std::string_view name;
    std::string_view version;
    bool has_strategy;       // the package shipped a dialect format strategy (code tier)
    bool has_echoed_source;  // the package shipped an echoed-source provenance hook (code tier)
};

// A non-fatal shadowing note (§3 — prefix shadowing that is NOT an exact duplicate resolves
// longest-match-wins and is SURFACED here, never silent). Empty for the current two packages (no
// nested prefixes), but the mechanism is real.
struct ShadowNote
{
    std::string_view kind;          // "role" | "marker" | "level_lift"
    std::string_view shorter_prefix; // the shadowed (shorter) prefix
    std::string_view longer_prefix;  // the shadowing (longer) prefix — wins on a line matching both
};

// The composition report (§3): the shadowing notes surfaced during composition. A pure record; the
// fail-closed conflicts are NOT here (they abort composition before a report exists).
struct CompositionReport
{
    std::vector<ShadowNote> shadows;
};

// An exact-duplicate conflict (§3, fail-closed): same rule class + same key (prefix) + intersecting
// format gate across two rows. `has_conflict == false` is the empty sentinel. constexpr so a
// composition TU can `static_assert(!find_conflict(pkgs).has_conflict, …)` — the build-time half of
// G-SP-5; the runtime compose fatals on the same condition (defense in depth).
struct ConflictInfo
{
    bool has_conflict{false};
    std::string_view kind; // "role" | "marker" | "level_lift" | "value_class"
    std::string_view key;  // the duplicated prefix / value-class key
};

// Scan the manifest set for an exact-duplicate match key. Pure over the manifest DATA (constexpr:
// string_view compare + span iteration), so usable in a static_assert AND by the runtime compose.
[[nodiscard]] constexpr ConflictInfo find_conflict(std::span<const SemanticPackageManifest> packages) noexcept;

// The composed rule set: the canonically-ordered, conflict-free tables the core mechanisms walk, the
// code-tier seams (strategy factories + provenance hooks), the package list, and the content hash.
// Owns its row storage (small POD copied from the manifest spans in canonical order; the pointed-at
// bytes stay alive in package static storage — SP-7). Built once per binary; passed by const-ref to
// every Tokenizer. Move-only.
class ComposedSemantics
{
  public:
    ComposedSemantics(const ComposedSemantics&) = delete;
    ComposedSemantics& operator=(const ComposedSemantics&) = delete;
    ComposedSemantics(ComposedSemantics&&) noexcept = default;
    ComposedSemantics& operator=(ComposedSemantics&&) noexcept = default;
    ~ComposedSemantics() = default;

    // ── The composed tables (canonical order) ──
    [[nodiscard]] std::span<const StructuralRoleRow> roles() const noexcept { return roles_; }
    [[nodiscard]] std::span<const IntentMarkerRow> markers() const noexcept { return markers_; }
    [[nodiscard]] std::span<const LevelLiftRow> level_lifts() const noexcept { return level_lifts_; }
    [[nodiscard]] std::span<const LocationRow> locations() const noexcept { return locations_; }
    [[nodiscard]] std::span<const ValueClassRow> value_classes() const noexcept { return value_classes_; }

    // ── The code-tier seams ──
    [[nodiscard]] std::span<const StrategyFactory> strategy_factories() const noexcept { return strategies_; }
    [[nodiscard]] std::span<const ProvenanceHook> provenance_hooks() const noexcept { return provenance_hooks_; }

    // ── Identity + legibility (§4) ──
    [[nodiscard]] const std::array<std::uint8_t, kSemanticIdentityBytes>& identity() const noexcept { return identity_; }
    [[nodiscard]] std::string identity_hex() const;                        // rendered hex, only at seams
    [[nodiscard]] std::span<const ComposedPackage> packages() const noexcept { return packages_; }
    [[nodiscard]] const CompositionReport& report() const noexcept { return report_; }

  private:
    ComposedSemantics() = default;
    friend ComposedSemantics compose(std::span<const SemanticPackageManifest>);

    std::vector<StructuralRoleRow> roles_;
    std::vector<IntentMarkerRow> markers_;
    std::vector<LevelLiftRow> level_lifts_;
    std::vector<LocationRow> locations_;
    std::vector<ValueClassRow> value_classes_;
    std::vector<StrategyFactory> strategies_;
    std::vector<ProvenanceHook> provenance_hooks_;
    std::vector<ComposedPackage> packages_;
    CompositionReport report_;
    std::array<std::uint8_t, kSemanticIdentityBytes> identity_{};
};

// Compose the manifest set. Sorts packages by name (canonical order — independent of the caller's
// argument order), concatenates rows in declared order, FATALS on an exact-duplicate key (the startup
// fail-closed invariant — a clear message then std::terminate), records longest-match shadow notes,
// and computes semantic_identity over the canonical serialization (§4). The degenerate case (empty
// span) is a defined, runnable state: no rows, no strategies, the hash of just
// (grammar-version + kCanonicalizationVersion).
[[nodiscard]] ComposedSemantics compose(std::span<const SemanticPackageManifest> packages);

// ── find_conflict — constexpr definition (inline so it is usable in static_assert at any TU) ──
namespace detail
{
    // Two format gates INTERSECT when a single line could satisfy both: equal, or either is kAnyFormat.
    [[nodiscard]] constexpr bool gates_intersect(insight::LogFormat lhs, insight::LogFormat rhs) noexcept
    {
        return lhs == rhs || lhs == kAnyFormat || rhs == kAnyFormat;
    }
} // namespace detail

namespace detail
{
    // Every unordered (package,row) pair exactly ONCE: pkg_b>pkg_a, or pkg_b==pkg_a with idx_j>idx_i.
    // Never a row against itself, and address-independent — detects an intra-package duplicate AND two
    // packages that happen to share the same static row array (position, not address, is the key).
    // Keyed on the shared .prefix + .format_gate members (roles / markers / level-lifts). Returns the
    // duplicated prefix, or nullopt.
    template <typename Row>
    [[nodiscard]] constexpr std::optional<std::string_view>
    first_prefix_dup(std::span<const SemanticPackageManifest> packages,
                     std::span<const Row> SemanticPackageManifest::* member) noexcept
    {
        for (std::size_t pkg_a{0}; pkg_a < packages.size(); ++pkg_a)
        {
            const std::span<const Row> rows_a{packages[pkg_a].*member};
            for (std::size_t idx_i{0}; idx_i < rows_a.size(); ++idx_i)
                for (std::size_t pkg_b{pkg_a}; pkg_b < packages.size(); ++pkg_b)
                {
                    const std::span<const Row> rows_b{packages[pkg_b].*member};
                    for (std::size_t idx_j{(pkg_b == pkg_a) ? idx_i + 1 : 0}; idx_j < rows_b.size();
                         ++idx_j)
                        if (rows_a[idx_i].prefix == rows_b[idx_j].prefix &&
                            gates_intersect(rows_a[idx_i].format_gate, rows_b[idx_j].format_gate))
                            return rows_a[idx_i].prefix;
                }
        }
        return std::nullopt;
    }
} // namespace detail

constexpr ConflictInfo find_conflict(std::span<const SemanticPackageManifest> packages) noexcept
{
    // An exact duplicate is same key + (for prefix rows) intersecting format gate. Each unordered
    // (package,row) pair is checked once; O(rows²) over a handful of rows at compile time.
    if (const auto key{detail::first_prefix_dup<StructuralRoleRow>(packages,
                                                                   &SemanticPackageManifest::roles)})
        return {.has_conflict = true, .kind = "role", .key = *key};
    if (const auto key{detail::first_prefix_dup<IntentMarkerRow>(packages,
                                                                 &SemanticPackageManifest::markers)})
        return {.has_conflict = true, .kind = "marker", .key = *key};
    if (const auto key{
            detail::first_prefix_dup<LevelLiftRow>(packages, &SemanticPackageManifest::level_lifts)})
        return {.has_conflict = true, .kind = "level_lift", .key = *key};
    // Value classes are keyed by `.key` (no format gate).
    for (std::size_t pkg_a{0}; pkg_a < packages.size(); ++pkg_a)
        for (std::size_t idx_i{0}; idx_i < packages[pkg_a].value_classes.size(); ++idx_i)
            for (std::size_t pkg_b{pkg_a}; pkg_b < packages.size(); ++pkg_b)
                for (std::size_t idx_j{(pkg_b == pkg_a) ? idx_i + 1 : 0};
                     idx_j < packages[pkg_b].value_classes.size(); ++idx_j)
                    if (packages[pkg_a].value_classes[idx_i].key ==
                        packages[pkg_b].value_classes[idx_j].key)
                        return {.has_conflict = true,
                                .kind = "value_class",
                                .key = packages[pkg_a].value_classes[idx_i].key};
    return {};
}

} // namespace insight::semantic
