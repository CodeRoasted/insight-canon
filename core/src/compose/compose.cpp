module;
#include <picosha2.h> // header-only SHA-256 (impl-only, mirrors template_id.cpp) — the composed identity

module insight.canon.compose;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;
import insight.canon.transport;

// compose.cpp — the runtime composition (ADR-17). Sorts the manifest set by name (canonical
// order, argument-order-independent), concatenates rows in declared order into the
// ComposedSemantics tables, FATALS on an exact-duplicate key (the fail-closed startup invariant),
// records longest-match shadow notes, and computes semantic_identity as SHA-256[:16] over a
// canonical, fixed-endian serialization of the composed rule set (no addresses / paths / link order
// in the input — G-SP-4).

namespace insight::semantic
{

namespace
{
    constexpr std::size_t kSha256Bytes{32};
    constexpr std::size_t kIdentityBytes{16}; // 16-byte truncation, the TemplateId precedent (§4.1)
    constexpr unsigned kNibbleMask{0xFU};
    constexpr unsigned kNibbleShift{4U};
    constexpr std::array<char, 16> kHexDigits{'0', '1', '2', '3', '4', '5', '6', '7',
                                              '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    // ── Canonical serialization primitives (fixed little-endian; no size_t widths, no addresses)
    // ──
    void append_u8(std::string& out, std::uint8_t value)
    {
        out.push_back(static_cast<char>(value));
    }

    void append_u32_le(std::string& out, std::uint32_t value)
    {
        for (unsigned shift{0}; shift < 32U; shift += 8U)
            out.push_back(static_cast<char>((value >> shift) & 0xFFU));
    }

    void append_i64_le(std::string& out, std::int64_t value)
    {
        const auto bits{static_cast<std::uint64_t>(value)};
        for (unsigned shift{0}; shift < 64U; shift += 8U)
            out.push_back(static_cast<char>((bits >> shift) & 0xFFU));
    }

    // A string is length-prefixed (u32 LE) then raw bytes — unambiguous for arbitrary content.
    void append_str(std::string& out, std::string_view str)
    {
        append_u32_le(out, static_cast<std::uint32_t>(str.size()));
        out.append(str);
    }

    void append_str_span(std::string& out, std::span<const std::string_view> strings)
    {
        append_u32_le(out, static_cast<std::uint32_t>(strings.size()));
        for (const std::string_view str : strings)
            append_str(out, str);
    }

    // Sorted canonical package order: a stable index permutation by name (byte-wise string_view
    // compare — locale-independent, deterministic). Ties by name are impossible under SRC-SP-7 (a
    // package name is unique); a defensive tiebreak on version keeps the sort total regardless.
    [[nodiscard]] std::vector<std::size_t>
    canonical_order(std::span<const SemanticPackageManifest> packages)
    {
        std::vector<std::size_t> order(packages.size());
        for (std::size_t i{0}; i < packages.size(); ++i)
            order[i] = i;
        std::ranges::stable_sort(order,
                                 [packages](std::size_t lhs, std::size_t rhs) noexcept
                                 {
                                     if (packages[lhs].name != packages[rhs].name)
                                         return packages[lhs].name < packages[rhs].name;
                                     return packages[lhs].version < packages[rhs].version;
                                 });
        return order;
    }

    // Serialize one manifest into the canonical byte stream (§4.1). Field order is fixed; enums
    // enter as their single underlying byte; the code tier enters nominally as two presence bytes.
    void serialize_manifest(std::string& out, const SemanticPackageManifest& pkg)
    {
        append_str(out, pkg.name);
        append_str(out, pkg.version);
        append_u8(out, pkg.strategy != nullptr ? 1U : 0U); // code tier, nominal (§4.1)
        append_u8(out, pkg.echoed_source != nullptr ? 1U : 0U);

        append_u32_le(out, static_cast<std::uint32_t>(pkg.roles.size()));
        for (const StructuralRoleRow& row : pkg.roles)
        {
            append_str(out, row.prefix);
            append_u8(out, static_cast<std::uint8_t>(row.role));
            append_str(out, row.dialect_gate);
        }
        // ADR-22: the package's declared IntentChannel vocabulary is identity — it is the closed
        // set every channel_gate below is checked against, so the digest must move if the
        // vocabulary does. NB what enters the digest is the channel NAMES ("annotated"/"stripped"),
        // never the C++ spelling of the fields that carry them: identity is the RULESET's content,
        // so renaming a field is not a ruleset change and must not move the digest.
        append_str_span(out, pkg.channels);
        append_u32_le(out, static_cast<std::uint32_t>(pkg.markers.size()));
        for (const IntentMarkerRow& row : pkg.markers)
        {
            append_str(out, row.prefix);
            append_u8(out, static_cast<std::uint8_t>(row.kind));
            append_u8(out, static_cast<std::uint8_t>(row.child_order));
            append_str(out, row.dialect_gate);
            append_u8(out, static_cast<std::uint8_t>(row.extract));
            append_str_span(out, row.payload_excludes); // grammar-2: the exclusion set is identity
            append_str(
                out,
                row.channel_gate); // ADR-22: the channel gate is a recognition fact ⇒ identity
        }
        append_u32_le(out, static_cast<std::uint32_t>(pkg.level_lifts.size()));
        for (const LevelLiftRow& row : pkg.level_lifts)
        {
            append_str(out, row.prefix);
            append_u8(out, static_cast<std::uint8_t>(row.level));
            append_str(out, row.dialect_gate);
        }
        append_u32_le(out, static_cast<std::uint32_t>(pkg.locations.size()));
        for (const LocationRow& row : pkg.locations)
        {
            append_u8(out, static_cast<std::uint8_t>(row.kind));
            append_str_span(out, row.infixes);
            append_str_span(out, row.extensions);
            append_str_span(out, row.prefixes);
            append_str_span(out, row.suffixes);
        }
        append_u32_le(out, static_cast<std::uint32_t>(pkg.value_classes.size()));
        for (const ValueClassRow& row : pkg.value_classes)
        {
            append_str(out, row.key);
            append_u8(out, static_cast<std::uint8_t>(row.cls));
            append_str(out, row.schedule_id);
            append_i64_le(out, row.scale);
        }
        // grammar-2 (ADR-17): the run-outcome vocabulary enters the identity — a mapping change
        // honestly stales cross-artifact comparability (SP-4). Appended after the grammar-1
        // sections (fixed field order; the version string above already segregates the
        // generations).
        append_u32_le(out, static_cast<std::uint32_t>(pkg.outcome_tokens.size()));
        for (const OutcomeTokenRow& row : pkg.outcome_tokens)
        {
            append_str(out, row.token);
            append_u8(out, static_cast<std::uint8_t>(row.outcome));
            append_str(out, row.dialect_gate);
        }
        append_u32_le(out, static_cast<std::uint32_t>(pkg.outcome_markers.size()));
        for (const OutcomeMarkerRow& row : pkg.outcome_markers)
        {
            append_str(out, row.prefix);
            append_str(out, row.dialect_gate);
            // grammar-5 (ADR-17): the shape discriminator and the row's own verdict. Both are
            // serialized for EVERY row, including the RemainderToken rows for which `outcome` is
            // inert — the preimage is a fixed field layout per generation, not a per-row-shape
            // union, so a row changing shape moves the digest by the field that changed rather than
            // by the encoding shifting underneath it.
            append_u8(out, static_cast<std::uint8_t>(row.shape));
            append_u8(out, static_cast<std::uint8_t>(row.outcome));
        }
        // grammar-3 (ADR-23): the GENERATION projection enters the identity,
        // field-for-field as the recognition markers do. This closes the SRC-SID-2/G4 gap — before
        // it, changing how a
        // dialect MATERIALIZES an intent left the digest untouched, so two writers could claim one
        // RulesetIdentity. Appended after the grammar-2 sections, the same discipline grammar-2
        // used (fixed field order per generation; the version string above segregates the
        // generations). Content only, never the C++ spelling of the fields that carry it — a pure
        // rename is not a ruleset change and must not move the digest.
        append_u32_le(out, static_cast<std::uint32_t>(pkg.emits.size()));
        for (const IntentEmitRow& row : pkg.emits)
        {
            append_str(out, row.prefix);
            append_u8(out, static_cast<std::uint8_t>(row.kind));
            append_u8(out, static_cast<std::uint8_t>(row.child_order));
            append_str(out, row.dialect_gate);
            append_u8(out, static_cast<std::uint8_t>(row.emit));
            append_str(out, row.channel_gate);
        }
    }

    // Record longest-match shadow notes among a set of prefix rows: when one prefix is a PROPER
    // prefix of another and their gates intersect, a line matching the longer also matches the
    // shorter — longest wins (deterministic), surfaced here. Generic over any prefix+gate row via
    // projections.
    template <typename Row>
    void note_shadows(std::span<const Row> rows, std::string_view kind, CompositionReport& report)
    {
        for (const Row& lhs : rows)
            for (const Row& rhs : rows)
            {
                if (&lhs == &rhs || lhs.prefix.size() >= rhs.prefix.size())
                    continue;
                if (rhs.prefix.starts_with(lhs.prefix) &&
                    detail::gates_intersect(lhs.dialect_gate, rhs.dialect_gate))
                    report.shadows.push_back(
                        {.kind = kind, .shorter_prefix = lhs.prefix, .longer_prefix = rhs.prefix});
            }
    }

    // ADR-17 — an exact-duplicate match key has no deterministic resolution, so composition fails
    // closed rather than picking a winner by package order. The operator-facing text below states
    // that rule and its remedy without naming this record: canon ships public, and a reader of the
    // message cannot open the shelf it lives on.
    [[noreturn]] void fail_closed(const ConflictInfo& conflict)
    {
        std::cerr << "FATAL: insight::semantic::compose — exact-duplicate " << conflict.kind
                  << " match key \"" << conflict.key
                  << "\" across the composed packages. Composition fails closed: a duplicate rule "
                     "has no deterministic resolution. Fix the package rows or gate them.\n";
        std::terminate();
    }

    // ADR-22 — an UNKNOWN IntentChannel is a hard error, distinct from an absent one. Same
    // fail-closed posture as a duplicate row: a clear message naming the declared vocabulary, then
    // terminate. Degrading a typo to the fallback would hand back a silently structure-less
    // analysis for what is a misspelling.
    [[noreturn]] void fail_unknown_channel(std::string_view declared_channel,
                                           std::span<const std::string_view> declared)
    {
        std::cerr
            << "FATAL: insight::semantic::ComposedSemantics::for_channel — unknown IntentChannel \""
            << declared_channel << "\". The composed packages declare: ";
        if (declared.empty())
            std::cerr << "<none>";
        for (std::size_t i{0}; i < declared.size(); ++i)
            std::cerr << (i == 0 ? "" : ", ") << '"' << declared[i] << '"';
        std::cerr
            << ".\nAn IntentChannel is caller-declared provenance, never guessed. An unknown "
               "channel is a MISTAKE and fails closed here; an ABSENT channel is a CHOICE and "
               "degrades to the raw-text fallback. Declare one of the names above, or none.\n";
        std::terminate();
    }

    // ADR-23 — an UNKNOWN declared dialect is a hard error, symmetric with an unknown channel
    // (ADR-22) and an unknown transform. Same reasoning, three coordinates: an unknown name is
    // a MISTAKE and must not share a code path with an ABSENT one, which is a CHOICE.
    [[noreturn]] void fail_unknown_dialect(std::string_view declared_dialect,
                                           std::span<const ComposedPackage> packages)
    {
        std::cerr << "FATAL: insight::semantic::resolve_stream — unknown dialect \""
                  << declared_dialect << "\". The composed packages are: ";
        if (packages.empty())
            std::cerr << "<none>";
        for (std::size_t i{0}; i < packages.size(); ++i)
            std::cerr << (i == 0 ? "" : ", ") << '"' << packages[i].name << '"';
        std::cerr
            << ".\nA dialect is caller-declared provenance, never guessed: canon VERIFIES, it "
               "does not infer. An unknown dialect is a MISTAKE and fails closed here; an "
               "ABSENT dialect is a CHOICE and asserts nothing. Declare one of the names "
               "above, or none.\n";
        std::terminate();
    }

} // namespace

ResolvedStream resolve_stream(const ComposedSemantics& composed,
                              const insight::transport::IngestDeclaration& declaration)
{
    // Both semantic coordinates are verified and APPLIED in one construction (ADR-22):
    // `for_stream` fails closed on an unknown name in either, then filters the view. This is the
    // ONE evaluation point of the dialect gate in the whole engine — nothing below the view sees
    // the coordinate, which is what makes the per-line content dependence structurally impossible
    // rather than merely absent.
    return ResolvedStream{.semantics =
                              composed.for_stream(declaration.dialect, declaration.channel),
                          .transport = insight::transport::resolve_transport_stack(declaration)};
}

std::string ComposedSemantics::identity_hex() const
{
    std::string out;
    out.reserve(2U * kIdentityBytes);
    for (const std::uint8_t byte : identity_)
    {
        out.push_back(kHexDigits[(static_cast<unsigned>(byte) >> kNibbleShift) & kNibbleMask]);
        out.push_back(kHexDigits[static_cast<unsigned>(byte) & kNibbleMask]);
    }
    return out;
}

bool ComposedSemantics::withholds_markers_for(std::string_view declared_channel) const noexcept
{
    // The dialect is THIS VIEW'S (`declared_dialect_`), not a parameter: the question is whether
    // declaring a channel would unlock depth on the stream this view was resolved for. A row of
    // another dialect could never fire here whatever the caller declares, so counting it would
    // produce advice that cannot help — the fatigue the diagnostic exists to avoid.
    return std::ranges::any_of(all_markers_,
                               [this, declared_channel](const IntentMarkerRow& row) noexcept
                               {
                                   return dialect_admits(row.dialect_gate, declared_dialect_) &&
                                          !channel_admits(row.channel_gate, declared_channel);
                               });
}

ComposedSemantics ComposedSemantics::for_stream(std::string_view declared_dialect,
                                                std::string_view declared_channel) const
{
    // An unknown name in either coordinate fails closed BEFORE any table is built — the same shape
    // as compose's duplicate check, and in declaration order so the first mistake is the one
    // reported. Empty (Unspecified) is not unknown: it is the caller declining to declare.
    if (!declared_dialect.empty() &&
        std::ranges::none_of(packages_, [declared_dialect](const ComposedPackage& pkg) noexcept
                             { return pkg.name == declared_dialect; }))
        fail_unknown_dialect(declared_dialect, packages_);
    if (!declared_channel.empty() &&
        std::ranges::find(channels_, declared_channel) == channels_.end())
        fail_unknown_channel(declared_channel, channels_);

    ComposedSemantics out;
    // Dialect-independent tables (no gate on these row kinds — the "does not metastasize" rule),
    // carried over verbatim.
    out.locations_ = locations_;
    out.value_classes_ = value_classes_;
    out.channels_ = channels_;
    out.strategies_ = strategies_;
    out.provenance_hooks_ = provenance_hooks_;
    out.packages_ = packages_;
    out.report_ = report_;
    // The identity is carried VERBATIM: semantic_identity is the RULESET's identity (which rows
    // exist and how they gate), not a per-stream view of it. Two streams of one binary declaring
    // different dialects or IntentChannels are analyzed by the SAME ruleset, so they must report
    // the same identity — otherwise a cross-channel comparison (D5's legal case: BuildId N
    // annotated ↔ N+1 stripped) would look like a comparison across two different engines.
    out.identity_ = identity_;

    // Every filter is re-derived from the UNFILTERED tables, never from this view's own: a view is
    // already SOME resolution (the doubly-Unspecified one on a fresh composition), so filtering it
    // would be a monotonically shrinking chain and a second declaration could only remove rows the
    // first one kept. Re-deriving makes `for_stream` idempotent and order-free.
    out.all_roles_ = all_roles_;
    out.all_markers_ = all_markers_;
    out.all_level_lifts_ = all_level_lifts_;
    out.all_outcome_tokens_ = all_outcome_tokens_;
    out.all_outcome_markers_ = all_outcome_markers_;
    out.declared_dialect_ = declared_dialect;

    const auto admits{[declared_dialect](std::string_view dialect_gate) noexcept
                      { return dialect_admits(dialect_gate, declared_dialect); }};

    out.roles_.reserve(all_roles_.size());
    std::ranges::copy_if(all_roles_, std::back_inserter(out.roles_),
                         [&admits](const StructuralRoleRow& row)
                         { return admits(row.dialect_gate); });
    out.level_lifts_.reserve(all_level_lifts_.size());
    std::ranges::copy_if(all_level_lifts_, std::back_inserter(out.level_lifts_),
                         [&admits](const LevelLiftRow& row) { return admits(row.dialect_gate); });
    out.outcome_tokens_.reserve(all_outcome_tokens_.size());
    std::ranges::copy_if(all_outcome_tokens_, std::back_inserter(out.outcome_tokens_),
                         [&admits](const OutcomeTokenRow& row)
                         { return admits(row.dialect_gate); });
    out.outcome_markers_.reserve(all_outcome_markers_.size());
    std::ranges::copy_if(all_outcome_markers_, std::back_inserter(out.outcome_markers_),
                         [&admits](const OutcomeMarkerRow& row)
                         { return admits(row.dialect_gate); });
    // Markers carry BOTH gates — the Medium is `dialect × IntentChannel` — so both apply here.
    out.markers_.reserve(all_markers_.size());
    std::ranges::copy_if(
        all_markers_, std::back_inserter(out.markers_),
        [&admits, declared_channel](const IntentMarkerRow& row)
        { return admits(row.dialect_gate) && channel_admits(row.channel_gate, declared_channel); });
    return out;
}

ComposedSemantics compose(std::span<const SemanticPackageManifest> packages)
{
    // Fail-closed: an exact-duplicate key aborts before any table is built (the runtime half of
    // G-SP-5; the constexpr find_conflict is the build-time half a composition TU static_asserts).
    if (const ConflictInfo conflict{find_conflict(packages)}; conflict.has_conflict)
        fail_closed(conflict);

    const std::vector<std::size_t> order{canonical_order(packages)};

    ComposedSemantics composed;

    // ── Canonical serialization → semantic_identity (§4.1). Prefix the version components, then
    // each package in canonical order. No addresses / link order in the input ⇒ reproducible
    // (G-SP-4).
    std::string serialized;
    append_str(serialized, kSemanticGrammarVersion);
    append_str(serialized, insight::kCanonicalizationVersion);

    // ADR-23 — the TRANSPORT CATALOGUE is identity, and the per-run DECLARATION is not. This
    // is 0031's hash split and it is the whole quotient: the transform GRAMMAR (which transforms
    // exist and what bytes they own) is a property of the analyzing binary and belongs in the
    // comparability key; which transforms a given STREAM declared is provenance and rides to
    // MetaLog instead. Two runs ±a declared transform MUST therefore carry the same
    // `semantic_identity` — otherwise transport-invariance is not being built, it is only being
    // asserted. Canon-shipped and closed, so it enters here as a fixed component rather than as a
    // compose() parameter: it is core vocabulary like the ordinal/OTEL catalogs, not package data.
    append_str(serialized, insight::transport::kTransportCatalogVersion);
    append_u32_le(serialized,
                  static_cast<std::uint32_t>(insight::transport::kTransportCatalogRows.size()));
    for (const insight::transport::TransportTransformRow& row :
         insight::transport::kTransportCatalogRows)
    {
        append_str(serialized, row.name);
        append_u8(serialized, static_cast<std::uint8_t>(row.kind));
        append_u8(serialized, static_cast<std::uint8_t>(row.extract));
        append_u32_le(serialized, row.prefix_width);
        append_u8(serialized, row.strip_leading_space ? 1U : 0U);
    }

    for (const std::size_t idx : order)
    {
        const SemanticPackageManifest& pkg{packages[idx]};
        serialize_manifest(serialized, pkg);

        // Concatenate rows in canonical (package-sorted, declared-within) order, into the
        // UNFILTERED tables — the views are derived from them at the end, once.
        composed.all_roles_.insert(composed.all_roles_.end(), pkg.roles.begin(), pkg.roles.end());
        composed.all_markers_.insert(composed.all_markers_.end(), pkg.markers.begin(),
                                     pkg.markers.end());
        composed.all_level_lifts_.insert(composed.all_level_lifts_.end(), pkg.level_lifts.begin(),
                                         pkg.level_lifts.end());
        composed.locations_.insert(composed.locations_.end(), pkg.locations.begin(),
                                   pkg.locations.end());
        composed.value_classes_.insert(composed.value_classes_.end(), pkg.value_classes.begin(),
                                       pkg.value_classes.end());
        composed.all_outcome_tokens_.insert(composed.all_outcome_tokens_.end(),
                                            pkg.outcome_tokens.begin(), pkg.outcome_tokens.end());
        composed.all_outcome_markers_.insert(composed.all_outcome_markers_.end(),
                                             pkg.outcome_markers.begin(),
                                             pkg.outcome_markers.end());
        // ADR-22 — the composed IntentChannel vocabulary. De-duplicated: two dialects may
        // legitimately name the same materialization, and this set is a lookup key (validating a
        // caller's --channel), not a per-package tally. Package order is canonical, so the result
        // is deterministic.
        for (const std::string_view channel : pkg.channels)
            if (std::ranges::find(composed.channels_, channel) == composed.channels_.end())
                composed.channels_.push_back(channel);
        if (pkg.strategy != nullptr)
            composed.strategies_.push_back(pkg.strategy);
        if (pkg.echoed_source != nullptr)
            composed.provenance_hooks_.push_back(pkg.echoed_source);
        composed.packages_.push_back({.name = pkg.name,
                                      .version = pkg.version,
                                      .has_strategy = pkg.strategy != nullptr,
                                      .has_echoed_source = pkg.echoed_source != nullptr});
    }

    std::array<unsigned char, kSha256Bytes> digest{};
    picosha2::hash256(serialized.begin(), serialized.end(), digest.begin(), digest.end());
    for (std::size_t i{0}; i < kIdentityBytes; ++i)
        composed.identity_[i] = static_cast<std::uint8_t>(digest[i]);

    // Longest-match shadow notes over the composed prefix tables (the report; conflicts already
    // fatal). They use the FULL sets: a shadow is a property of the declared vocabulary, not of one
    // stream's view, and reporting it only when a particular dialect or channel is declared would
    // make the report declaration-dependent.
    note_shadows<StructuralRoleRow>(composed.all_roles_, "role", composed.report_);
    note_shadows<IntentMarkerRow>(composed.all_markers_, "marker", composed.report_);
    note_shadows<LevelLiftRow>(composed.all_level_lifts_, "level_lift", composed.report_);
    note_shadows<OutcomeMarkerRow>(composed.all_outcome_markers_, "outcome_marker",
                                   composed.report_);

    // ADR-22 + ADR-22 — a fresh composition IS the UNSPECIFIED view on BOTH axes:
    // nobody has declared a dialect or a channel, so every concretely-gated row stays out and only
    // kAnyDialect / kAnyChannel rows fire. A caller gets dialect depth by DECLARING (resolve_stream
    // / for_stream), which is the point: declaring is the path to depth, and the default can never
    // be a concrete dialect or channel — "both GHA Step rows live at once" is the phantom defect on
    // the channel axis, and "every dialect's rows live at once" is the same defect on the dialect
    // axis. Fail-closed is the DEFAULT, not an opt-in — a safety default that must be requested is
    // not a default.
    //
    // Derived through the SAME function a caller uses, so the default view and a declared one can
    // never be two different filters that drift.
    return composed.for_stream(kAnyDialect, kAnyChannel);
}

} // namespace insight::semantic
