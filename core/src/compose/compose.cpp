module;
#include <picosha2.h>

module insight.canon.compose;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;
import insight.canon.transport;

namespace insight::semantic
{

namespace
{
    constexpr std::size_t kSha256Bytes{32};
    constexpr std::size_t kIdentityBytes{16};
    constexpr unsigned kNibbleMask{0xFU};
    constexpr unsigned kNibbleShift{4U};
    constexpr std::array<char, 16> kHexDigits{'0', '1', '2', '3', '4', '5', '6', '7',
                                              '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    // invariant: every integer enters fixed-width little-endian; no size_t width and no address
    // enters the preimage.
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

    // post: a stable index permutation by package name, compared byte-wise and locale-independent.
    // invariant: a name tie is impossible; the version tiebreak keeps the sort total regardless.
    // refs: SRC-SP-7
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

    // invariant: the field order is fixed per grammar generation; an enum enters as one byte and
    // the code tier nominally, as presence bytes.
    // refs: ADR-17.D3, ADR-17.D9
    void serialize_manifest(std::string& out, const SemanticPackageManifest& pkg)
    {
        append_str(out, pkg.name);
        append_str(out, pkg.version);
        append_u8(out, pkg.strategy != nullptr ? 1U : 0U);
        append_u8(out, pkg.echoed_source != nullptr ? 1U : 0U);

        append_u32_le(out, static_cast<std::uint32_t>(pkg.roles.size()));
        for (const StructuralRoleRow& row : pkg.roles)
        {
            append_str(out, row.prefix);
            append_u8(out, static_cast<std::uint8_t>(row.role));
            append_str(out, row.dialect_gate);
        }
        // note: the channel NAMES enter the digest, never the C++ spelling of their fields.
        // refs: ADR-22.D4
        append_str_span(out, pkg.channels);
        append_u32_le(out, static_cast<std::uint32_t>(pkg.markers.size()));
        for (const IntentMarkerRow& row : pkg.markers)
        {
            append_str(out, row.prefix);
            append_u8(out, static_cast<std::uint8_t>(row.kind));
            append_u8(out, static_cast<std::uint8_t>(row.child_order));
            append_str(out, row.dialect_gate);
            append_u8(out, static_cast<std::uint8_t>(row.extract));
            append_str_span(out, row.payload_excludes);
            append_str(out, row.channel_gate);
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
            // note: shape and outcome are serialized for EVERY row, an inert field included.
            append_u8(out, static_cast<std::uint8_t>(row.shape));
            append_u8(out, static_cast<std::uint8_t>(row.outcome));
        }
        // note: two writers materializing one intent differently are two rulesets.
        // refs: SRC-SID-2, ADR-18.D4
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
        // note: byte-identical rows under different declared generations are different claims.
        // refs: ADR-17.D9
        append_str_span(out, pkg.dialect_revisions);
    }

    // post: one shadow note per pair whose shorter prefix properly prefixes the longer and whose
    // gates intersect.
    // refs: ADR-17.D4
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

    // note: the message states the rule and the remedy and names no record: canon ships public.
    // refs: ADR-6.D10, ADR-17.D2
    [[noreturn]] void fail_closed(const ConflictInfo& conflict)
    {
        std::cerr << "FATAL: insight::semantic::compose — exact-duplicate " << conflict.kind
                  << " match key \"" << conflict.key
                  << "\" across the composed packages. Composition fails closed: ";
        if (conflict.kind == kConflictKindPackageName)
            std::cerr << "two packages in this set declare that manifest name. No row edit or "
                         "dialect gate resolves it — the name is what selects a package "
                         "downstream, so a single view would serve both packages' rows. Rename "
                         "one of the two packages, or compose only one of them.\n";
        else
            std::cerr << "a duplicate rule has no deterministic resolution. Fix the package rows "
                         "or gate them.\n";
        std::terminate();
    }

    // refs: DN-17.D17
    [[noreturn]] void fail_unnamed_package(std::size_t index, std::size_t total)
    {
        std::cerr << "FATAL: insight::semantic::compose — the package at position " << index
                  << " of " << total
                  << " declares an EMPTY manifest name. Composition fails closed: kAnyDialect IS "
                     "the empty string, so an unnamed package satisfies the dialect-gate ownership "
                     "check VACUOUSLY and its every ungated row then reads as universally gated to "
                     "downstream readers — the package would claim a gate it never asked for. Name "
                     "the package. A set declared in a translation unit should static_assert "
                     "all_packages_named() instead: the same fence, one build earlier.\n";
        std::terminate();
    }

    // refs: ADR-22.D5
    [[noreturn]] void fail_unknown_channel(std::string_view declared_channel,
                                           std::span<const std::string_view> declared)
    {
        std::cerr
            << "FATAL: insight::semantic::ComposedSemantics::for_stream — unknown IntentChannel \""
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

    // refs: ADR-22.D5, ADR-23.D4
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

// post: both declared coordinates are verified and applied in ONE construction; nothing below the
// view carries either coordinate.
// refs: ADR-22.D6, ADR-23.D4
ResolvedStream resolve_stream(const ComposedSemantics& composed,
                              const insight::transport::IngestDeclaration& declaration)
{
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

// post: true when a marker of THIS view's dialect is withheld by the channel gate alone.
bool ComposedSemantics::withholds_markers_for(std::string_view declared_channel) const noexcept
{
    return std::ranges::any_of(all_markers_,
                               [this, declared_channel](const IntentMarkerRow& row) noexcept
                               {
                                   return dialect_admits(row.dialect_gate, declared_dialect_) &&
                                          !channel_admits(row.channel_gate, declared_channel);
                               });
}

// post: an unknown dialect or channel name fails closed before any table is built; an empty name is
// not unknown.
// post: every table is re-derived from the UNFILTERED sets, so this is idempotent and order-free.
// refs: ADR-22.D5, ADR-22.D6
ComposedSemantics ComposedSemantics::for_stream(std::string_view declared_dialect,
                                                std::string_view declared_channel) const
{
    if (!declared_dialect.empty() &&
        std::ranges::none_of(packages_, [declared_dialect](const ComposedPackage& pkg) noexcept
                             { return pkg.name == declared_dialect; }))
        fail_unknown_dialect(declared_dialect, packages_);
    if (!declared_channel.empty() &&
        std::ranges::find(channels_, declared_channel) == channels_.end())
        fail_unknown_channel(declared_channel, channels_);

    ComposedSemantics out;
    out.locations_ = locations_;
    out.value_classes_ = value_classes_;
    out.channels_ = channels_;
    out.strategies_ = strategies_;
    out.provenance_hooks_ = provenance_hooks_;
    out.packages_ = packages_;
    out.report_ = report_;
    // note: the identity is the RULESET's: every view of one composition reports the same one.
    // refs: ADR-17.D3, ADR-22.D4
    out.identity_ = identity_;

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
    // refs: ADR-22.D4
    out.markers_.reserve(all_markers_.size());
    std::ranges::copy_if(
        all_markers_, std::back_inserter(out.markers_),
        [&admits, declared_channel](const IntentMarkerRow& row)
        { return admits(row.dialect_gate) && channel_admits(row.channel_gate, declared_channel); });
    return out;
}

// post: composition is canonically ordered and fails closed; an unnamed package is refused.
// post: a fresh composition IS the doubly-unspecified view: only any-dialect, any-channel rows
// fire.
// refs: ADR-17.D2, DN-17.D17
ComposedSemantics compose(std::span<const SemanticPackageManifest> packages)
{
    // note: the unnamed-package fence answers first: every other diagnostic names the package.
    for (std::size_t index{0}; index < packages.size(); ++index)
        if (packages[index].name.empty())
            fail_unnamed_package(index, packages.size());

    if (const ConflictInfo conflict{find_conflict(packages)}; conflict.has_conflict)
        fail_closed(conflict);

    const std::vector<std::size_t> order{canonical_order(packages)};

    ComposedSemantics composed;

    // note: the preimage is version then packages in canonical order; no address, no link order.
    // refs: ADR-17.D3
    std::string serialized;
    append_str(serialized, kSemanticGrammarVersion);
    append_str(serialized, insight::kCanonicalizationVersion);

    // note: the transform GRAMMAR is identity; a run's declared stack rides the document unhashed.
    // refs: ADR-23.D4
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

    // note: shadows use the FULL sets: a shadow is the vocabulary's property, not a view's.
    note_shadows<StructuralRoleRow>(composed.all_roles_, "role", composed.report_);
    note_shadows<IntentMarkerRow>(composed.all_markers_, "marker", composed.report_);
    note_shadows<LevelLiftRow>(composed.all_level_lifts_, "level_lift", composed.report_);
    note_shadows<OutcomeMarkerRow>(composed.all_outcome_markers_, "outcome_marker",
                                   composed.report_);

    // refs: ADR-22.D5
    return composed.for_stream(kAnyDialect, kAnyChannel);
}

} // namespace insight::semantic
