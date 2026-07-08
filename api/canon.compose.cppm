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
    [[nodiscard]] const std::array<std::uint8_t, 16>& identity() const noexcept { return identity_; }
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
    std::array<std::uint8_t, 16> identity_{};
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

constexpr ConflictInfo find_conflict(std::span<const SemanticPackageManifest> packages) noexcept
{
    // Prefix-keyed rows: an exact duplicate is same prefix + intersecting gate. O(rows²) over a
    // handful of rows, at compile time — cost is irrelevant, clarity is not.
    // Roles.
    for (std::size_t a{0}; a < packages.size(); ++a)
        for (const StructuralRoleRow& lhs : packages[a].roles)
            for (std::size_t b{a}; b < packages.size(); ++b)
                for (const StructuralRoleRow& rhs : packages[b].roles)
                {
                    if (&lhs == &rhs)
                        continue;
                    if (lhs.prefix == rhs.prefix && detail::gates_intersect(lhs.format_gate, rhs.format_gate))
                        return {.has_conflict = true, .kind = "role", .key = lhs.prefix};
                }
    // Markers.
    for (std::size_t a{0}; a < packages.size(); ++a)
        for (const IntentMarkerRow& lhs : packages[a].markers)
            for (std::size_t b{a}; b < packages.size(); ++b)
                for (const IntentMarkerRow& rhs : packages[b].markers)
                {
                    if (&lhs == &rhs)
                        continue;
                    if (lhs.prefix == rhs.prefix && detail::gates_intersect(lhs.format_gate, rhs.format_gate))
                        return {.has_conflict = true, .kind = "marker", .key = lhs.prefix};
                }
    // Level lifts.
    for (std::size_t a{0}; a < packages.size(); ++a)
        for (const LevelLiftRow& lhs : packages[a].level_lifts)
            for (std::size_t b{a}; b < packages.size(); ++b)
                for (const LevelLiftRow& rhs : packages[b].level_lifts)
                {
                    if (&lhs == &rhs)
                        continue;
                    if (lhs.prefix == rhs.prefix && detail::gates_intersect(lhs.format_gate, rhs.format_gate))
                        return {.has_conflict = true, .kind = "level_lift", .key = lhs.prefix};
                }
    // Value classes (keyed by `key`).
    for (std::size_t a{0}; a < packages.size(); ++a)
        for (const ValueClassRow& lhs : packages[a].value_classes)
            for (std::size_t b{a}; b < packages.size(); ++b)
                for (const ValueClassRow& rhs : packages[b].value_classes)
                {
                    if (&lhs == &rhs)
                        continue;
                    if (lhs.key == rhs.key)
                        return {.has_conflict = true, .kind = "value_class", .key = lhs.key};
                }
    return {};
}

} // namespace insight::semantic
