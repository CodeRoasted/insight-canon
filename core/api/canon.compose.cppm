// insight.canon.compose — static composition of semantic packages into a ComposedSemantics
// (ADR-17). The consumer binary names its composition ONCE —
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
//     canonical serialization — the SRC-II-7 comparability key.
//   - HOT-PATH-INVISIBLE (SRC-SP-5): composing MORE packages costs the tokenizer nothing on a line
//     no package claims. Three mechanism constraints carry it — no unconditional per-token
//     indirection, no per-line allocation on the recognizer probe path (byte-scan pure), and rows
//     partitioned by FORMAT so a non-matching format pays only its partition test. The claim is a
//     MEASUREMENT, never an assertion: `BM_TokenizationThroughput` over the composed set against
//     its Degenerate control arm (`compose({})`) on a corpus carrying no dialect content — that
//     delta IS the claim, expected noise, and every composition-mechanism change re-runs it. The
//     gate lives in the `insight_canon_bench` leaf, which must link the vocabulary packages that
//     core never may (SRC-SP-1 / R1); the contract is declared HERE, beside the mechanism, because
//     the harness has already moved package once and a rule declared in it would move with it.
//
// Public + installed (product binaries call compose). The facade `export import`s this so
// `import insight.canon;` yields Tokenizer + compose + ComposedSemantics. It plain-imports spi
// (does NOT re-export it) — a consumer naming SemanticPackageManifest imports the package module
// (which carries spi); the provider contract stays off the consumer's default surface.
module;

export module insight.canon.compose;
import insight.canon.internal; // std + global C fixed-width types
import insight.canon.api;      // StructuralRole/IntentMarkerKind/LogFormat/…
import insight.canon.spi; // the grammar rows + SemanticPackageManifest (plain import — not re-exported)
import insight.canon.transport; // the transform catalogue — its version + rows are identity (0044 §6)

export namespace insight::semantic
{

// The truncated-SHA-256 width of semantic_identity (§4.1 — the TemplateId 16-byte precedent).
inline constexpr std::size_t kSemanticIdentityBytes{16};

// A composed package's identity, carried for the wire block's legibility (§4.2 — an operator can
// read what vocabulary a report understood). The hash (§4.1) is the key; this list is the label.
struct ComposedPackage
{
    std::string_view name;
    std::string_view version;
    bool has_strategy;      // the package shipped a dialect format strategy (code tier)
    bool has_echoed_source; // the package shipped an echoed-source provenance hook (code tier)
};

// A non-fatal shadowing note (§3 — prefix shadowing that is NOT an exact duplicate resolves
// longest-match-wins and is SURFACED here, never silent). Empty for the current two packages (no
// nested prefixes), but the mechanism is real.
struct ShadowNote
{
    std::string_view kind;           // "role" | "marker" | "level_lift"
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
// dialect gate across two rows. `has_conflict == false` is the empty sentinel. constexpr so a
// composition TU can `static_assert(!find_conflict(pkgs).has_conflict, …)` — the build-time half of
// G-SP-5; the runtime compose fatals on the same condition (defense in depth).
struct ConflictInfo
{
    bool has_conflict{false};
    std::string_view kind; // the rule class that collided — `find_conflict` owns the vocabulary
    std::string_view key;  // the duplicated prefix / value-class key / package name
};

// The one `kind` value that is COMPARED rather than only printed: the runtime fail-closed message
// selects its remedy on it (a name collision has no row to edit and no gate to narrow, so the row
// advice cannot be given there). Named because the producer and the comparison site sit in
// different translation units — a literal repeated at the comparison would degrade silently to the
// wrong remedy if the spelling here ever moved.
inline constexpr std::string_view kConflictKindPackageName{"package_name"};

// Scan the manifest set for an exact-duplicate match key. Pure over the manifest DATA (constexpr:
// string_view compare + span iteration), so usable in a static_assert AND by the runtime compose.
[[nodiscard]] constexpr ConflictInfo
find_conflict(std::span<const SemanticPackageManifest> packages) noexcept;

// The composed rule set: the canonically-ordered, conflict-free tables the core mechanisms walk,
// the code-tier seams (strategy factories + provenance hooks), the package list, and the content
// hash. Owns its row storage (small POD copied from the manifest spans in canonical order; the
// pointed-at bytes stay alive in package static storage — SRC-SP-7). Built once per binary; passed
// by const-ref to every Tokenizer. Move-only.
class ComposedSemantics
{
  public:
    ComposedSemantics(const ComposedSemantics&) = delete;
    ComposedSemantics& operator=(const ComposedSemantics&) = delete;
    ComposedSemantics(ComposedSemantics&&) noexcept = default;
    ComposedSemantics& operator=(ComposedSemantics&&) noexcept = default;
    ~ComposedSemantics() = default;

    // ── The composed tables (canonical order) ──
    [[nodiscard]] std::span<const StructuralRoleRow> roles() const noexcept
    {
        return roles_;
    }
    [[nodiscard]] std::span<const IntentMarkerRow> markers() const noexcept
    {
        return markers_;
    }
    [[nodiscard]] std::span<const LevelLiftRow> level_lifts() const noexcept
    {
        return level_lifts_;
    }
    [[nodiscard]] std::span<const LocationRow> locations() const noexcept
    {
        return locations_;
    }
    // The christened ValueClassRegistry (ADR-17): the composed view over the package
    // ValueClassRow seat. No package ships a value class (we do not build dormant
    // vocabulary), so this is empty — the UNIVERSAL value concepts (kOrdinalFieldCatalog /
    // kOtelFieldCatalog / the KEEP lexicons) stay core (the ratified rule), consumed directly. The
    // registry is the point where a package's client-ordinal / domain value classes compose
    // in — the grammar seat exists; it is not unified with the core catalogs, which have no
    // package consumer.
    [[nodiscard]] std::span<const ValueClassRow> value_classes() const noexcept
    {
        return value_classes_;
    }
    // The run-outcome vocabulary (grammar-2, ADR-17): the composed dialect verdict maps + the
    // console-tail markers. Consumed by map_outcome_token / scan_run_outcome / resolve_run_outcome.
    [[nodiscard]] std::span<const OutcomeTokenRow> outcome_tokens() const noexcept
    {
        return outcome_tokens_;
    }
    [[nodiscard]] std::span<const OutcomeMarkerRow> outcome_markers() const noexcept
    {
        return outcome_markers_;
    }
    // ADR-22 — the composed INTENT CHANNEL vocabulary: every channel any package declares.
    // This is the closed set a caller's `--channel` is validated against, and the list an unknown
    // channel's error names.
    [[nodiscard]] std::span<const std::string_view> channels() const noexcept
    {
        return channels_;
    }

    // ── The STREAM view: dialect × channel, filtered ONCE (ADR-22, ADR-22) ──
    // Build the vocabulary ONE stream declares, at stream open. Both coordinates are the caller's
    // provenance facts, never auto-detected (canon VERIFIES, it does not infer — ADR-22): a
    // content heuristic decides from a PREFIX of the stream, so a later line can contradict an
    // earlier decision ⇒ content non-determinism the moment the stream is chunked.
    //
    // The returned composition drops every row gated to a different dialect, and every marker row
    // gated to a different channel. Consequences, all structural rather than asserted:
    //
    //   * the HOT PATH never sees either coordinate — classify / recognize / lift_level /
    //     map_outcome_token walk a plain row span with no gate parameter at all, zero per-line
    //     cost;
    //   * one dialect and one IntentChannel per TREE (D5) are UNREPRESENTABLE otherwise — a sibling
    //     dialect's or channel's rows are not in the table, so the bad state cannot be built;
    //   * `kAnyDialect` / `kAnyChannel` rows survive every filter, so a universal role row and a
    //     single-materialization dialect are untouched.
    //
    // ⚠ THE DIALECT FILTER IS A DETERMINISM FIX, not tidiness (ADR-22). Before T4 the
    // gate was `LogParser::routed_format()` — the per-line detector winner, served by a STICKY
    // strategy — so WHICH DECLARED ROWS FIRED WAS A FUNCTION OF CONTENT. Under a stream-scoped
    // declaration it is fixed before the first line. Anything that reintroduces a per-line format
    // input to a declared row's gate has undone this, however green the tests are.
    //
    // EMPTY as an ARGUMENT means Unspecified — the caller did not declare: every concretely-gated
    // row drops ⇒ no dialect structure ⇒ the raw-text fallback. Fail-closed on DEPTH, not on the
    // run. Never default an undeclared stream to a concrete dialect or channel: "both GHA Step rows
    // live at once" IS the phantom defect this exists to kill, and "both dialects' rows live at
    // once" is the same defect one axis over.
    //
    // FATALS on an UNKNOWN name in either coordinate — a non-empty dialect no composed package
    // carries (`--dialect=guthub`), or a non-empty channel no package declares
    // (`--channel=annotatd`) — listing the known vocabulary. An unknown name is a MISTAKE; an
    // absent one is a CHOICE; they must not share a code path, because silently degrading a typo to
    // the fallback is the exact silent-fallback bug class this workstream has already paid for
    // twice.
    //
    // IDEMPOTENT AND ORDER-FREE by construction: every filter is re-derived from the private
    // UNFILTERED tables, so `v.for_stream(a, b).for_stream(c, d) == v.for_stream(c, d)`. A
    // monotonically shrinking chain — the shape a separate `for_dialect` / `for_channel` pair would
    // have had — is what makes a second call silently wrong, so there is exactly one door.
    //
    // Cold path by construction: called once per stream, copies ~30 POD rows. The pointed-at bytes
    // stay in package-static storage (SRC-SP-7), so the copy is trivial and the identity is
    // preserved verbatim — semantic_identity is the RULESET's identity, not a stream's view of it.
    [[nodiscard]] ComposedSemantics for_stream(std::string_view declared_dialect,
                                               std::string_view declared_channel) const;

    // Would declaring an IntentChannel unlock recognition this view is withholding? (ADR-22's
    // diagnostic.) True iff some marker row of THIS VIEW'S DECLARED DIALECT is channel-gated to a
    // channel that `declared_channel` does not admit — i.e. this dialect HAS materializations and
    // the caller has not said which one it acquired, so depth is being withheld and saying so would
    // unlock it.
    //
    // The dialect is no longer a parameter: it is the coordinate this view was resolved for
    // (ADR-22). Asking the question against a view resolved for a DIFFERENT dialect was
    // never meaningful, and passing it separately made that mistake expressible.
    //
    // A narrow QUERY, deliberately not an `all_markers()` accessor: exposing the unfiltered table
    // would re-open the fail-open door this class exists to close (a caller could walk it and
    // recognize against every channel at once — the phantom defect). Returns false for a
    // single-materialization dialect (Jenkins), so a Jenkins user is never told to declare a
    // channel that does not apply to them — a diagnostic that fires where it cannot help is exactly
    // the fatigue the product is against.
    [[nodiscard]] bool withholds_markers_for(std::string_view declared_channel) const noexcept;

    // ── The code-tier seams ──
    [[nodiscard]] std::span<const StrategyFactory> strategy_factories() const noexcept
    {
        return strategies_;
    }
    [[nodiscard]] std::span<const ProvenanceHook> provenance_hooks() const noexcept
    {
        return provenance_hooks_;
    }

    // ── Identity + legibility (§4) ──
    [[nodiscard]] const std::array<std::uint8_t, kSemanticIdentityBytes>& identity() const noexcept
    {
        return identity_;
    }
    [[nodiscard]] std::string identity_hex() const; // rendered hex, only at seams
    [[nodiscard]] std::span<const ComposedPackage> packages() const noexcept
    {
        return packages_;
    }
    [[nodiscard]] const CompositionReport& report() const noexcept
    {
        return report_;
    }

  private:
    ComposedSemantics() = default;
    friend ComposedSemantics compose(std::span<const SemanticPackageManifest>);

    // ── The VIEW: what this stream's walkers see. Every one of these five is already filtered by
    // the declared (dialect, channel); a freshly composed vocabulary is the UNSPECIFIED view on
    // BOTH axes, so only kAnyDialect / kAnyChannel rows fire until a caller declares.
    //
    // Fail-closed has to be the DEFAULT, not an opt-in a caller can forget (ADR-22's promoted
    // MUST — a safety default that must be requested is not a default). That is why the accessors
    // expose the VIEW and the unfiltered tables below are private: if the full set were the public
    // `markers()`, the default composition would fire BOTH GHA Step rows at once (the phantom
    // defect) and, one axis over, every dialect's rows on every stream.
    std::vector<StructuralRoleRow> roles_;
    std::vector<IntentMarkerRow> markers_;
    std::vector<LevelLiftRow> level_lifts_;
    std::vector<OutcomeTokenRow> outcome_tokens_;
    std::vector<OutcomeMarkerRow> outcome_markers_;

    // ── The UNFILTERED tables: every row the packages declared, gated or not. The SOURCE
    // `for_stream()` re-derives each view from, never walked by recognition. Keeping them makes
    // `for_stream` IDEMPOTENT and order-free: filtering a view would be a monotonically shrinking
    // chain, so a second declaration could only ever remove rows the first one kept.
    std::vector<StructuralRoleRow> all_roles_;
    std::vector<IntentMarkerRow> all_markers_;
    std::vector<LevelLiftRow> all_level_lifts_;
    std::vector<OutcomeTokenRow> all_outcome_tokens_;
    std::vector<OutcomeMarkerRow> all_outcome_markers_;

    // The dialect this view was resolved for; empty = Unspecified. Not a copy of caller state for
    // its own sake — `withholds_markers_for` needs it to ask its question about the RIGHT dialect
    // now that the dialect has stopped being a per-call parameter (ADR-22).
    std::string_view declared_dialect_;

    // Dialect-independent (no gate on these row kinds), so they are carried verbatim through every
    // view.
    std::vector<LocationRow> locations_;
    std::vector<ValueClassRow> value_classes_;
    std::vector<std::string_view> channels_; // ADR-22 — the composed declared channel vocabulary
    std::vector<StrategyFactory> strategies_;
    std::vector<ProvenanceHook> provenance_hooks_;
    std::vector<ComposedPackage> packages_;
    CompositionReport report_;
    std::array<std::uint8_t, kSemanticIdentityBytes> identity_{};
};

// ── The per-stream resolution of an IngestDeclaration (ADR-23) ───────────────────────────────────
// What ONE stream analyzes with: the channel-filtered vocabulary and the resolved transport stack.
// Move-only, because ComposedSemantics is.
struct ResolvedStream
{
    ComposedSemantics semantics;                  // the declared channel's view of the ruleset
    insight::transport::TransportStack transport; // resolved once, before the first line (§4)
};

// Resolve a declaration against a composition — the ONE call a caller makes at stream open, and the
// only place the three declared coordinates are checked together.
//
// CANON VERIFIES, NEVER INFERS (ADR-22's split, not reopened). Each coordinate fails closed on an
// UNKNOWN value, naming the known vocabulary, and degrades on an ABSENT one:
//   * `dialect`   — must name a composed package; unknown ⇒ hard error listing the composed names.
//                   VERIFIED and GATING since T4 (ADR-22): the row-level dialect gate is applied
//                   HERE, once, and filtered into the view, so no walker below ever sees a dialect
//                   coordinate. Absent ⇒ every concretely-gated row drops — fail-closed on DEPTH.
//   * `channel`   — same construction, same call; the fail-closed posture is ADR-22's and is
//                   unchanged here.
//   * `stack`     — delegated to resolve_transport_stack(); unknown transform ⇒ hard error listing
//                   the catalogue.
//
// A DEFAULT-CONSTRUCTED DECLARATION IS THE UNSPECIFIED STREAM — empty stack (the peel is the
// identity function), no dialect and no channel, so only kAnyDialect / kAnyChannel rows fire. That
// is the G1 case. Declaring is purely ADDITIVE in depth: a caller who says nothing gets the
// raw-text reading, and a caller who says the wrong thing gets a named error rather than a quietly
// different answer.
//
// Note what this function does NOT return: anything the tokenizer takes. The stack is handed back
// to the CALLER, who peels and passes `PeeledLine::content` on. There is deliberately no path from
// here into the identity path (§4).
[[nodiscard]] ResolvedStream
resolve_stream(const ComposedSemantics& composed,
               const insight::transport::IngestDeclaration& declaration);

// Compose the manifest set. Sorts packages by name (canonical order — independent of the caller's
// argument order), concatenates rows in declared order, FATALS on an exact-duplicate key (the
// startup fail-closed invariant — a clear message then std::terminate), records longest-match
// shadow notes, and computes semantic_identity over the canonical serialization (§4). The
// degenerate case (empty span) is a defined, runnable state: no rows, no strategies, the hash of
// just (grammar-version + kCanonicalizationVersion).
[[nodiscard]] ComposedSemantics compose(std::span<const SemanticPackageManifest> packages);

// ── find_conflict — constexpr definition (inline so it is usable in static_assert at any TU) ──
namespace detail
{
    // Two dialect gates INTERSECT when a single line could satisfy both: equal, or either is
    // kAnyDialect. The COMPOSITION-time predicate: it answers "could these two rows ever both
    // claim one line?" and drives the fail-closed duplicate check.
    //
    // ⚠ NOT `insight::semantic::dialect_admits`, its RESOLUTION-time sibling, and the two must not
    // be swapped. This one is SYMMETRIC and asks about two ROWS; that one is ASYMMETRIC and asks
    // about one row against a CALLER's declaration, where an ABSENT declaration must admit nothing
    // concrete. Answering either question with the other's predicate fails closed in one direction
    // and fails OPEN in the other.
    //
    // There is no longer a RECOGNITION-time gate predicate at all (ADR-22): the dialect
    // is evaluated ONCE, at stream resolution, and filtered into the view, so the walkers walk a
    // plain row span with no gate left to test. The `gate_matches` that used to live here — the
    // per-line `row_gate == kAnyFormat || row_gate == line_format` every walker shared — is gone
    // with the coordinate it tested.
    [[nodiscard]] constexpr bool gates_intersect(std::string_view lhs,
                                                 std::string_view rhs) noexcept
    {
        return lhs == rhs || lhs == kAnyDialect || rhs == kAnyDialect;
    }
} // namespace detail

namespace detail
{
    // Every unordered (package,row) pair exactly ONCE: pkg_b>pkg_a, or pkg_b==pkg_a with
    // idx_j>idx_i. Never a row against itself, and address-independent — detects an intra-package
    // duplicate AND two packages that happen to share the same static row array (position, not
    // address, is the key). Keyed on the shared .prefix + .dialect_gate members (roles / markers /
    // level-lifts). Returns the duplicated prefix, or nullopt.
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
                            gates_intersect(rows_a[idx_i].dialect_gate, rows_b[idx_j].dialect_gate))
                            return rows_a[idx_i].prefix;
                }
        }
        return std::nullopt;
    }
} // namespace detail

namespace detail
{
    // Two composed packages sharing a manifest `name` (DN-17.D17). Keyed on `name` ALONE, never on
    // `(name, version)`: two packages at different versions under one name are still ambiguous,
    // because `dialect_gate` carries the NAME, so `--dialect github` admits both and no rule would
    // pick one.
    //
    // WHY THIS BELONGS HERE AND NOWHERE ELSE. A name collision is a property of the manifest SET,
    // so no package-local check can see it — `all_dialect_gates_owned` takes ONE manifest and is
    // blind by construction. And the generator cannot be the fence either: a customer may
    // hand-write a package, or compose one generated package with three hand-written ones, so a
    // fence living in the tool is absent from exactly the composition that needs it.
    //
    // What it prevents, derived from the shipped code rather than imagined: two packages both named
    // "github" with DISJOINT row prefixes trip no other check — `gates_intersect("github",
    // "github")` is true, but disjoint prefixes never collide. `for_stream("github")` then admits
    // BOTH packages' gated rows flattened into one view, `packages()` lists "github" twice, and the
    // unknown-dialect message prints the name twice. A silent shadow, reachable today.
    [[nodiscard]] constexpr std::optional<std::string_view>
    first_package_name_dup(std::span<const SemanticPackageManifest> packages) noexcept
    {
        for (std::size_t pkg_a{0}; pkg_a < packages.size(); ++pkg_a)
            for (std::size_t pkg_b{pkg_a + 1}; pkg_b < packages.size(); ++pkg_b)
                if (packages[pkg_a].name == packages[pkg_b].name)
                    return packages[pkg_a].name;
        return std::nullopt;
    }
} // namespace detail

namespace detail
{
    // The outcome-token variant of first_prefix_dup: keyed on .token + intersecting gate.
    // `ADR-17.D2` — exact-duplicate keys fail the build or startup, UNCONDITIONALLY: two packages
    // mapping the SAME token under intersecting gates is a conflict whatever the mapped outcome,
    // because a duplicate row has no deterministic resolution either way.
    [[nodiscard]] constexpr std::optional<std::string_view>
    first_outcome_token_dup(std::span<const SemanticPackageManifest> packages) noexcept
    {
        for (std::size_t pkg_a{0}; pkg_a < packages.size(); ++pkg_a)
        {
            const std::span<const OutcomeTokenRow> rows_a{packages[pkg_a].outcome_tokens};
            for (std::size_t idx_i{0}; idx_i < rows_a.size(); ++idx_i)
                for (std::size_t pkg_b{pkg_a}; pkg_b < packages.size(); ++pkg_b)
                {
                    const std::span<const OutcomeTokenRow> rows_b{packages[pkg_b].outcome_tokens};
                    for (std::size_t idx_j{(pkg_b == pkg_a) ? idx_i + 1 : 0}; idx_j < rows_b.size();
                         ++idx_j)
                        if (rows_a[idx_i].token == rows_b[idx_j].token &&
                            gates_intersect(rows_a[idx_i].dialect_gate, rows_b[idx_j].dialect_gate))
                            return rows_a[idx_i].token;
                }
        }
        return std::nullopt;
    }
} // namespace detail

constexpr ConflictInfo find_conflict(std::span<const SemanticPackageManifest> packages) noexcept
{
    // The package NAME first: it is the only key whose collision makes every OTHER answer here
    // ambiguous — two packages under one name mean the reported duplicate could not name which
    // package it came from.
    if (const auto key{detail::first_package_name_dup(packages)})
        return {.has_conflict = true, .kind = kConflictKindPackageName, .key = *key};
    // An exact duplicate is same key + (for prefix rows) intersecting dialect gate. Each unordered
    // (package,row) pair is checked once; O(rows²) over a handful of rows at compile time.
    if (const auto key{
            detail::first_prefix_dup<StructuralRoleRow>(packages, &SemanticPackageManifest::roles)})
        return {.has_conflict = true, .kind = "role", .key = *key};
    if (const auto key{
            detail::first_prefix_dup<IntentMarkerRow>(packages, &SemanticPackageManifest::markers)})
        return {.has_conflict = true, .kind = "marker", .key = *key};
    if (const auto key{detail::first_prefix_dup<LevelLiftRow>(
            packages, &SemanticPackageManifest::level_lifts)})
        return {.has_conflict = true, .kind = "level_lift", .key = *key};
    if (const auto key{detail::first_outcome_token_dup(packages)})
        return {.has_conflict = true, .kind = "outcome_token", .key = *key};
    if (const auto key{detail::first_prefix_dup<OutcomeMarkerRow>(
            packages, &SemanticPackageManifest::outcome_markers)})
        return {.has_conflict = true, .kind = "outcome_marker", .key = *key};
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

// ── The level-lift walker (ADR-22) ───────────────────────────────────────────────────────────────
// The last row kind whose matching algorithm lived in a semantic PACKAGE: `LevelLiftRow` was walked
// by the GitHub-Actions strategy over its own `kLevelLifts` array, inside `parse()`. Canon owns the
// ALGORITHM for every other row kind (classify / recognize / recognize_location / map_outcome_token
// over the composed tables), and this one now joins them — so `ComposedSemantics::level_lifts()`,
// which was serialized into `semantic_identity` while no production code read it, has a reader.
//
// WHY IT IS DECLARED HERE and not in the facade beside `classify`/`recognize`: its production
// consumer is `LogParser` (`insight.canon.detail.parse`), a SEALED shard that sits BELOW the
// facade. The level is decided at the parse stage — that is where the pre-existing echoed-source
// demotion (SRC-D-PROV-1) overrides it, and the lift must be applied BEFORE that demotion to
// reproduce the pre-relocation order exactly. A walker declared in `insight.canon` is unreachable
// from there without inverting the facade↔detail dependency arrow, so it is declared in the module
// that owns the composed tables. The facade `export import`s this module, so `import
// insight.canon;` still yields it alongside the other walkers.
export namespace insight::tokenization
{

// Lift a line's LogLevel from the composed level-lift rows: the FIRST row whose prefix the content
// carries wins. Unknown when no row matches — which is the caller's signal to keep whatever level
// the strategy inferred, never a level in its own right.
//
// No dialect parameter (ADR-22): `composed` is already the resolved stream's view, so a
// row that is present is a row that fires. This is what removed the live determinism hazard — the
// gate used to be `LogParser::routed_format()`, a per-line detector winner under a sticky strategy,
// so which DECLARED rows fired was a function of content.
//
// FIRST-match, not longest-match (the rule `classify`/`recognize` use), because first-match in
// declared order is what the pre-relocation package walk did and this relocation is
// output-neutral by construction. Composition already surfaces the only case where the two rules
// could differ: a prefix nesting among level-lift rows is reported as a `ShadowNote` (kind
// "level_lift") and an exact duplicate fails composition closed.
[[nodiscard]] LogLevel lift_level(std::string_view content,
                                  const insight::semantic::ComposedSemantics& composed) noexcept;

} // namespace insight::tokenization
