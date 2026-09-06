// refs: ADR-17, ADR-17.D2, ADR-17.D3
// invariant: a consumer binary names its composition ONCE and threads the result into every
// Tokenizer; composition is STATIC — fixed by which manifests the call names.
// invariant: EXPLICIT — there is no static-init self-registration anywhere.
// invariant: CANONICALLY ORDERED — row order is a pure function of the package SET (core rows
// first, then packages sorted by name, declared order within), never link or registration order.
// note: table order is template identity, so a linker-dependent order would move the digest
// invariant: FAIL-CLOSED — an exact-duplicate match key across rows is a build error through the
// constexpr `find_conflict`, and a startup fatal in the runtime compose.
// invariant: IDENTITY-BEARING — the composed rule set carries a content hash over its canonical
// serialization, and the transport catalogue's version and rows enter it too.
// refs: SRC-II-7, SRC-SP-5
// invariant: HOT-PATH-INVISIBLE — composing MORE packages costs the tokenizer nothing on a line
// no package claims, and that is a MEASUREMENT rather than an assertion.
// invariant: three mechanism constraints carry it: no unconditional per-token indirection, no
// per-line allocation on the recognizer probe path, and rows partitioned by format.
// assert: the throughput benchmark over the composed set against its Degenerate control arm
// (`compose({})`), on a corpus carrying no dialect content, IS that claim.
// invariant: every composition-mechanism change re-runs it, and the gate lives in the bench leaf
// because that leaf may link the vocabulary packages core never may.
// refs: SRC-SP-1
// invariant: public and installed; the facade `export import`s this module, and it plain-imports
// the provider spi so the provider contract stays off a consumer's default surface.
module;

export module insight.canon.compose;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;
import insight.canon.transport;

export namespace insight::semantic
{

// refs: ADR-17.D3
// invariant: the truncated-SHA-256 width of `semantic_identity`, on the 16-byte TemplateId
// precedent.
inline constexpr std::size_t kSemanticIdentityBytes{16};

// invariant: a composed package's identity, carried for the wire block's legibility so an operator
// can read what vocabulary a report understood; the hash is the key, this list the label.
// invariant: `has_strategy` says the package shipped a dialect format strategy and
// `has_echoed_source` says it shipped a provenance hook — both are code-tier facts.
struct ComposedPackage
{
    std::string_view name;
    std::string_view version;
    bool has_strategy;
    bool has_echoed_source;
};

// refs: ADR-17.D2, ADR-17.D4
// invariant: a NON-FATAL shadowing record: prefix shadowing that is not an exact duplicate resolves
// longest-match-wins and is SURFACED here, never silent.
// invariant: `kind` is the rule class the shadow was found in, and the composition walks the role,
// marker, level-lift and outcome-marker tables.
// invariant: `shorter_prefix` is the shadowed prefix and `longer_prefix` the shadowing one, which
// wins on a line matching both.
// refs: F-SRC-insight-eidos:composition_test.cpp
// assert: the shipped four-package composition produces exactly TWO — a marker note for Jenkins and
// an outcome-marker note for GitLab — and an engine test pins both by kind and by prefix.
struct ShadowNote
{
    std::string_view kind;
    std::string_view shorter_prefix;
    std::string_view longer_prefix;
};

// invariant: the composition report is a pure record of the shadowing notes surfaced during
// composition.
// invariant: the fail-closed conflicts are NOT here — they abort composition before a report
// exists.
struct CompositionReport
{
    std::vector<ShadowNote> shadows;
};

// refs: ADR-17.D2
// invariant: an exact-duplicate conflict is same rule class, same key, and intersecting dialect
// gate across two rows; `has_conflict == false` is the empty sentinel.
// invariant: constexpr, so a composition TU can `static_assert` on it — the build-time half of
// the duplicate check, whose runtime half fatals on the same condition.
// invariant: `kind` is the rule class that collided and `find_conflict` owns that vocabulary; `key`
// is the duplicated prefix, value-class key or package name.
struct ConflictInfo
{
    bool has_conflict{false};
    std::string_view kind;
    std::string_view key;
};

// invariant: the one `kind` value that is COMPARED rather than only printed: the runtime
// fail-closed message selects its remedy on it.
// invariant: a name collision has no row to edit and no gate to narrow, so the row advice cannot be
// given there.
// note: producer and comparison sit in different TUs, so a repeated literal degrades silently
inline constexpr std::string_view kConflictKindPackageName{"package_name"};

// post: scans the manifest set for an exact-duplicate match key.
// invariant: pure over the manifest DATA — string_view compare plus span iteration — so it is
// usable in a `static_assert` AND by the runtime compose.
[[nodiscard]] constexpr ConflictInfo
find_conflict(std::span<const SemanticPackageManifest> packages) noexcept;

// refs: SRC-SP-7
// invariant: the composed rule set: the canonically-ordered, conflict-free tables the core
// mechanisms walk, the code-tier seams, the package list and the content hash.
// invariant: it OWNS its row storage — small PODs copied from the manifest spans in canonical
// order — while the pointed-at bytes stay alive in package static storage.
// invariant: built once per binary, passed by const reference to every Tokenizer, and move-only.
class ComposedSemantics
{
  public:
    ComposedSemantics(const ComposedSemantics&) = delete;
    ComposedSemantics& operator=(const ComposedSemantics&) = delete;
    ComposedSemantics(ComposedSemantics&&) noexcept = default;
    ComposedSemantics& operator=(ComposedSemantics&&) noexcept = default;
    ~ComposedSemantics() = default;

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
    // refs: ADR-17
    // invariant: the composed view over the package `ValueClassRow` seat. No package ships a value
    // class today, so this is empty — we do not build dormant vocabulary.
    // invariant: the UNIVERSAL value concepts stay core and are consumed directly; the registry is
    // the point where a package's client-ordinal or domain value classes would compose in.
    // note: the seat exists and is not unified with the core catalogs, which no package consumes
    [[nodiscard]] std::span<const ValueClassRow> value_classes() const noexcept
    {
        return value_classes_;
    }
    // refs: ADR-17
    // invariant: the run-outcome vocabulary — the composed dialect verdict maps plus the
    // console-tail markers — consumed by the three run-outcome walkers in the facade.
    [[nodiscard]] std::span<const OutcomeTokenRow> outcome_tokens() const noexcept
    {
        return outcome_tokens_;
    }
    [[nodiscard]] std::span<const OutcomeMarkerRow> outcome_markers() const noexcept
    {
        return outcome_markers_;
    }
    // refs: ADR-22
    // invariant: the composed INTENT CHANNEL vocabulary: every channel any composed package
    // declares. This is the closed set a caller's channel is validated against.
    // invariant: it is also the list an unknown-channel error names.
    [[nodiscard]] std::span<const std::string_view> channels() const noexcept
    {
        return channels_;
    }

    // refs: ADR-22, ADR-22.D4
    // post: builds the vocabulary ONE stream declares, at stream open: dialect and channel,
    // filtered ONCE.
    // pre: both coordinates are the caller's provenance facts, never auto-detected — canon
    // VERIFIES, it does not infer.
    // note: a content heuristic decides from a PREFIX, so a later line can contradict it
    // invariant: the returned composition drops every row gated to a different dialect and every
    // marker row gated to a different channel.
    // invariant: the HOT PATH therefore never sees either coordinate — the walkers take a plain
    // row span with no gate parameter at all, at zero per-line cost.
    // invariant: one dialect and one channel per tree are UNREPRESENTABLE otherwise: a sibling
    // dialect's or channel's rows are not in the table, so the bad state cannot be built.
    // invariant: `kAnyDialect` and `kAnyChannel` rows survive every filter, so a universal role row
    // and a single-materialization dialect are untouched.
    // invariant: the dialect filter is a DETERMINISM fix and not tidiness: the gate used to be the
    // per-line detector winner, so WHICH DECLARED ROWS FIRED WAS A FUNCTION OF CONTENT.
    // invariant: EMPTY as an argument means Unspecified: every concretely-gated row drops and the
    // raw-text fallback stands. Fail-closed on DEPTH, never on the run.
    // note: never default an undeclared stream to a concrete dialect or channel
    // invariant: it FATALS on an UNKNOWN name in either coordinate, listing the known vocabulary:
    // an unknown name is a MISTAKE, an absent one is a CHOICE, and they may not share a code path.
    // invariant: IDEMPOTENT and order-free by construction — every filter is re-derived from the
    // private UNFILTERED tables, so a view of a view equals the second view alone.
    // note: a monotonically shrinking chain is what makes a second call silently wrong
    // invariant: cold path by construction: called once per stream, copying POD rows only, so the
    // identity is preserved verbatim — it is the RULESET's identity, not a stream's view of it.
    [[nodiscard]] ComposedSemantics for_stream(std::string_view declared_dialect,
                                               std::string_view declared_channel) const;

    // refs: ADR-22
    // post: true iff some marker row of THIS VIEW'S DECLARED DIALECT is channel-gated to a channel
    // that `declared_channel` does not admit.
    // invariant: that is the question "would declaring a channel unlock recognition this view is
    // withholding?" — the dialect HAS materializations and the caller has not said which.
    // invariant: the dialect is no longer a parameter: it is the coordinate this view was resolved
    // for, and asking against a view resolved for a DIFFERENT dialect was never meaningful.
    // invariant: a narrow QUERY, deliberately not an `all_markers()` accessor: exposing the
    // unfiltered table would re-open the fail-open door this class exists to close.
    // invariant: it returns false for a single-materialization dialect, so such a user is never
    // told to declare a channel that does not apply to them.
    // note: a diagnostic that fires where it cannot help is the fatigue this product is against
    [[nodiscard]] bool withholds_markers_for(std::string_view declared_channel) const noexcept;

    [[nodiscard]] std::span<const StrategyFactory> strategy_factories() const noexcept
    {
        return strategies_;
    }
    [[nodiscard]] std::span<const ProvenanceHook> provenance_hooks() const noexcept
    {
        return provenance_hooks_;
    }

    [[nodiscard]] const std::array<std::uint8_t, kSemanticIdentityBytes>& identity() const noexcept
    {
        return identity_;
    }
    // invariant: the identity rendered as hex, produced only at seams.
    [[nodiscard]] std::string identity_hex() const;
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

    // refs: ADR-22
    // invariant: THE VIEW — what this stream's walkers see. Each of these five is already
    // filtered by the declared dialect and channel.
    // invariant: a freshly composed vocabulary is the UNSPECIFIED view on BOTH axes, so only
    // any-dialect, any-channel rows fire until a caller declares.
    // invariant: fail-closed has to be the DEFAULT and not an opt-in a caller can forget — a
    // safety default that must be requested is not a default.
    // note: that is why the accessors expose the VIEW and the unfiltered tables below are private
    std::vector<StructuralRoleRow> roles_;
    std::vector<IntentMarkerRow> markers_;
    std::vector<LevelLiftRow> level_lifts_;
    std::vector<OutcomeTokenRow> outcome_tokens_;
    std::vector<OutcomeMarkerRow> outcome_markers_;

    // invariant: THE UNFILTERED TABLES — every row the packages declared, gated or not. They are
    // the SOURCE `for_stream` re-derives each view from, and recognition never walks them.
    // invariant: keeping them is what makes `for_stream` idempotent and order-free: filtering a
    // view would be a shrinking chain, so a second declaration could only ever remove rows.
    std::vector<StructuralRoleRow> all_roles_;
    std::vector<IntentMarkerRow> all_markers_;
    std::vector<LevelLiftRow> all_level_lifts_;
    std::vector<OutcomeTokenRow> all_outcome_tokens_;
    std::vector<OutcomeMarkerRow> all_outcome_markers_;

    // refs: ADR-22
    // invariant: the dialect this view was resolved for; empty means Unspecified.
    // invariant: it is here because `withholds_markers_for` needs it to ask its question about the
    // RIGHT dialect, now that the dialect has stopped being a per-call parameter.
    std::string_view declared_dialect_;

    // refs: ADR-22
    // invariant: these row kinds carry no dialect gate, so they are carried verbatim through every
    // view; `channels_` is the composed declared channel vocabulary.
    std::vector<LocationRow> locations_;
    std::vector<ValueClassRow> value_classes_;
    std::vector<std::string_view> channels_;
    std::vector<StrategyFactory> strategies_;
    std::vector<ProvenanceHook> provenance_hooks_;
    std::vector<ComposedPackage> packages_;
    CompositionReport report_;
    std::array<std::uint8_t, kSemanticIdentityBytes> identity_{};
};

// refs: ADR-23
// invariant: what ONE stream analyzes with: the channel-filtered vocabulary and the resolved
// transport stack, resolved once before the first line.
// invariant: move-only, because `ComposedSemantics` is.
struct ResolvedStream
{
    ComposedSemantics semantics;
    insight::transport::TransportStack transport;
};

// refs: ADR-22, ADR-22.D5, ADR-23
// post: resolves a declaration against a composition — the ONE call a caller makes at stream
// open, and the only place the three declared coordinates are checked together.
// invariant: canon VERIFIES, never INFERS: each coordinate fails closed on an UNKNOWN value, naming
// the known vocabulary, and degrades on an ABSENT one.
// invariant: `dialect` must name a composed package; the row-level dialect gate is applied HERE,
// once, and filtered into the view, so no walker below ever sees a dialect coordinate.
// invariant: `channel` is the same construction in the same call; `stack` is delegated, and an
// unknown transform is a hard error listing the catalogue.
// invariant: a DEFAULT-CONSTRUCTED declaration IS the Unspecified stream — empty stack, no
// dialect, no channel — so only any-dialect, any-channel rows fire.
// invariant: declaring is purely ADDITIVE in depth: a caller who says nothing gets the raw-text
// reading, and a caller who says the wrong thing gets a named error, never a different answer.
// invariant: it returns NOTHING the tokenizer takes: the stack is handed back to the CALLER, who
// peels and passes the peeled content on, so there is no path from here into the identity path.
[[nodiscard]] ResolvedStream
resolve_stream(const ComposedSemantics& composed,
               const insight::transport::IngestDeclaration& declaration);

// refs: ADR-17.D2, ADR-17.D3
// post: sorts packages by name into canonical order independent of the caller's argument order,
// concatenates rows in declared order, and computes the identity over the serialization.
// invariant: it FATALS on an exact-duplicate key — the startup fail-closed invariant, a clear
// message then termination — and records longest-match shadow notes.
// invariant: the degenerate case, an empty span, is a defined and runnable state: no rows, no
// strategies, and the hash of the two version tokens alone.
[[nodiscard]] ComposedSemantics compose(std::span<const SemanticPackageManifest> packages);

namespace detail
{
    // post: two dialect gates INTERSECT when a single line could satisfy both — equal, or either
    // is the any-dialect sentinel.
    // invariant: this is the COMPOSITION-time predicate, SYMMETRIC, asking whether two ROWS could
    // ever both claim one line, and it drives the fail-closed duplicate check.
    // invariant: its RESOLUTION-time sibling is ASYMMETRIC and asks about one row against a
    // CALLER's declaration, where an ABSENT declaration must admit nothing concrete.
    // invariant: answering either with the other is PERMISSIVE both ways — a gated row would fire
    // on an undeclared stream, and an any-gate duplicate would go undetected by span order.
    // refs: ADR-22
    // invariant: there is no RECOGNITION-time gate predicate at all any more: the dialect is
    // evaluated once, at stream resolution, so the walkers walk a plain row span.
    [[nodiscard]] constexpr bool gates_intersect(std::string_view lhs,
                                                 std::string_view rhs) noexcept
    {
        return lhs == rhs || lhs == kAnyDialect || rhs == kAnyDialect;
    }
} // namespace detail

namespace detail
{
    // post: visits every unordered (package, row) pair exactly ONCE and returns the duplicated
    // prefix, or nullopt.
    // invariant: never a row against itself, and address-independent — position is the key, so it
    // detects an intra-package duplicate AND two packages sharing one static row array.
    // invariant: keyed on the shared `prefix` and `dialect_gate` members, which roles, markers and
    // level-lifts all carry.
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

// refs: ADR-17.D2, DN-17.D17
// post: returns the name two composed packages share, or nullopt.
// invariant: keyed on `name` ALONE and never on `(name, version)`: two packages at different
// versions under one name are still ambiguous, because the dialect gate carries the NAME.
// invariant: a name collision is a property of the manifest SET, so no package-local check can see
// it, and a fence living in the generator is absent from a hand-written composition.
// assert: derived from the shipped code: two packages both named `github` with DISJOINT row
// prefixes trip no other check, because disjoint prefixes never collide.
// invariant: the resolved view would then admit BOTH packages' gated rows flattened into one, and
// the package list would show the name twice.
namespace detail
{
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

// refs: ADR-17.D2
// post: the outcome-token variant of the prefix duplicate scan, keyed on the token plus an
// intersecting gate.
// invariant: exact-duplicate keys fail the build or startup UNCONDITIONALLY: two packages mapping
// the SAME token under intersecting gates is a conflict whatever the mapped outcome.
// note: a duplicate row has no deterministic resolution either way
namespace detail
{
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
    // invariant: the package NAME is checked first: it is the only key whose collision makes every
    // OTHER answer here ambiguous, since the reported duplicate could not name its package.
    if (const auto key{detail::first_package_name_dup(packages)})
        return {.has_conflict = true, .kind = kConflictKindPackageName, .key = *key};
    // invariant: an exact duplicate is same key plus, for prefix rows, an intersecting gate; each
    // unordered pair is checked once, quadratic at compile time.
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
    // invariant: value classes are keyed by their `key` alone — there is no gate on that row
    // kind.
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

// refs: ADR-22, SRC-D-PROV-1
// invariant: the level-lift walker is declared HERE rather than in the facade because its
// production consumer is the sealed parse shard, which sits BELOW the facade.
// invariant: the level is decided at the parse stage, where the echoed-source demotion overrides
// it, and the lift must be applied BEFORE that demotion.
// invariant: a walker declared in the facade is unreachable from there without inverting the
// facade-detail dependency arrow, so it is declared in the module that owns the composed tables.
// note: the facade `export import`s this module, so `import insight.canon;` still yields it
export namespace insight::tokenization
{

// refs: ADR-17, ADR-22
// post: lifts a line's `LogLevel` from the composed level-lift rows: the FIRST row whose prefix the
// content carries wins, and Unknown when none matches.
// invariant: Unknown is the caller's signal to keep whatever level the strategy inferred, never a
// level in its own right.
// invariant: no dialect parameter: `composed` is already the resolved stream's view, so a row that
// is present is a row that fires.
// invariant: FIRST-match and not longest-match, because first-match in declared order is what the
// pre-relocation package walk did and this relocation is output-neutral by construction.
// note: a prefix nesting among level-lift rows is surfaced as a shadow note at composition
[[nodiscard]] LogLevel lift_level(std::string_view content,
                                  const insight::semantic::ComposedSemantics& composed) noexcept;

} // namespace insight::tokenization
