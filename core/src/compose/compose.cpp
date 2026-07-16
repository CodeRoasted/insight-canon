module;
#include <picosha2.h> // header-only SHA-256 (impl-only, mirrors template_id.cpp) — the composed identity

module insight.canon.compose;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;

// compose.cpp — the runtime composition (ADR 0024 §3/§4). Sorts the manifest set by name (canonical
// order, argument-order-independent), concatenates rows in declared order into the ComposedSemantics
// tables, FATALS on an exact-duplicate key (the fail-closed startup invariant), records longest-match
// shadow notes, and computes semantic_identity as SHA-256[:16] over a canonical, fixed-endian
// serialization of the composed rule set (no addresses / paths / link order in the input — G-SP-4).

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

// ── Canonical serialization primitives (fixed little-endian; no size_t widths, no addresses) ──
void append_u8(std::string& out, std::uint8_t value) { out.push_back(static_cast<char>(value)); }

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

// Sorted canonical package order: a stable index permutation by name (byte-wise string_view compare —
// locale-independent, deterministic). Ties by name are impossible under SP-7 (a package name is
// unique); a defensive tiebreak on version keeps the sort total regardless.
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

// Serialize one manifest into the canonical byte stream (§4.1). Field order is fixed; enums enter as
// their single underlying byte; the code tier enters nominally as two presence bytes.
void serialize_manifest(std::string& out, const SemanticPackageManifest& pkg)
{
    append_str(out, pkg.name);
    append_str(out, pkg.version);
    append_u8(out, pkg.strategy != nullptr ? 1U : 0U);       // code tier, nominal (§4.1)
    append_u8(out, pkg.echoed_source != nullptr ? 1U : 0U);

    append_u32_le(out, static_cast<std::uint32_t>(pkg.roles.size()));
    for (const StructuralRoleRow& row : pkg.roles)
    {
        append_str(out, row.prefix);
        append_u8(out, static_cast<std::uint8_t>(row.role));
        append_u8(out, static_cast<std::uint8_t>(row.format_gate));
    }
    // ADR 0028: the package's declared Sink vocabulary is identity — it is the closed set every
    // sink_gate below is checked against, so the digest must move if the vocabulary does.
    append_str_span(out, pkg.sinks);
    append_u32_le(out, static_cast<std::uint32_t>(pkg.markers.size()));
    for (const IntentMarkerRow& row : pkg.markers)
    {
        append_str(out, row.prefix);
        append_u8(out, static_cast<std::uint8_t>(row.kind));
        append_u8(out, static_cast<std::uint8_t>(row.child_order));
        append_u8(out, static_cast<std::uint8_t>(row.format_gate));
        append_u8(out, static_cast<std::uint8_t>(row.extract));
        append_str_span(out, row.payload_excludes); // grammar-2: the exclusion set is identity
        append_str(out, row.sink_gate); // ADR 0028: the Sink gate is a recognition fact ⇒ identity
    }
    append_u32_le(out, static_cast<std::uint32_t>(pkg.level_lifts.size()));
    for (const LevelLiftRow& row : pkg.level_lifts)
    {
        append_str(out, row.prefix);
        append_u8(out, static_cast<std::uint8_t>(row.level));
        append_u8(out, static_cast<std::uint8_t>(row.format_gate));
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
    // grammar-2 (ADR 0025): the run-outcome vocabulary enters the identity — a mapping change
    // honestly stales cross-artifact comparability (SP-4). Appended after the grammar-1 sections
    // (fixed field order; the version string above already segregates the generations).
    append_u32_le(out, static_cast<std::uint32_t>(pkg.outcome_tokens.size()));
    for (const OutcomeTokenRow& row : pkg.outcome_tokens)
    {
        append_str(out, row.token);
        append_u8(out, static_cast<std::uint8_t>(row.outcome));
        append_u8(out, static_cast<std::uint8_t>(row.format_gate));
    }
    append_u32_le(out, static_cast<std::uint32_t>(pkg.outcome_markers.size()));
    for (const OutcomeMarkerRow& row : pkg.outcome_markers)
    {
        append_str(out, row.prefix);
        append_u8(out, static_cast<std::uint8_t>(row.format_gate));
    }
}

// Record longest-match shadow notes among a set of prefix rows: when one prefix is a PROPER prefix of
// another and their gates intersect, a line matching the longer also matches the shorter — longest
// wins (deterministic), surfaced here. Generic over any prefix+gate row via projections.
template <typename Row>
void note_shadows(std::span<const Row> rows, std::string_view kind, CompositionReport& report)
{
    for (const Row& lhs : rows)
        for (const Row& rhs : rows)
        {
            if (&lhs == &rhs || lhs.prefix.size() >= rhs.prefix.size())
                continue;
            if (rhs.prefix.starts_with(lhs.prefix) &&
                detail::gates_intersect(lhs.format_gate, rhs.format_gate))
                report.shadows.push_back(
                    {.kind = kind, .shorter_prefix = lhs.prefix, .longer_prefix = rhs.prefix});
        }
}

[[noreturn]] void fail_closed(const ConflictInfo& conflict)
{
    std::cerr << "FATAL: insight::semantic::compose — exact-duplicate " << conflict.kind
              << " match key \"" << conflict.key
              << "\" across the composed packages. Composition fails closed (ADR 0024 §3): a "
                 "duplicate rule has no deterministic resolution. Fix the package rows or gate them.\n";
    std::terminate();
}

// ADR 0028 D1 — an UNKNOWN Sink is a hard error, distinct from an absent one. Same fail-closed posture
// as a duplicate row: a clear message naming the declared vocabulary, then terminate. Degrading a typo
// to D5's fallback would hand back a silently structure-less analysis for what is simply a misspelling.
[[noreturn]] void fail_unknown_sink(std::string_view declared_sink,
                                    std::span<const std::string_view> declared)
{
    std::cerr << "FATAL: insight::semantic::ComposedSemantics::for_sink — unknown Sink \""
              << declared_sink << "\". The composed packages declare: ";
    if (declared.empty())
        std::cerr << "<none>";
    for (std::size_t i{0}; i < declared.size(); ++i)
        std::cerr << (i == 0 ? "" : ", ") << '"' << declared[i] << '"';
    std::cerr << ".\nA Sink is caller-declared provenance (ADR 0028 D2), never guessed. An unknown "
                 "Sink is a MISTAKE and fails closed here; an ABSENT Sink is a CHOICE and degrades to "
                 "the raw-text fallback (D5). Pass one of the declared names, or none.\n";
    std::terminate();
}

} // namespace

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

bool ComposedSemantics::withholds_markers_for(insight::LogFormat format,
                                              std::string_view declared_sink) const noexcept
{
    return std::ranges::any_of(all_markers_,
                               [format, declared_sink](const IntentMarkerRow& row) noexcept
                               {
                                   return (row.format_gate == format || row.format_gate == kAnyFormat) &&
                                          !sink_admits(row.sink_gate, declared_sink);
                               });
}

ComposedSemantics ComposedSemantics::for_sink(std::string_view declared_sink) const
{
    // An unknown Sink fails closed BEFORE any table is built — the same shape as compose's duplicate
    // check. Empty (Unspecified) is not unknown: it is the caller declining to declare (D5).
    if (!declared_sink.empty() && std::ranges::find(sinks_, declared_sink) == sinks_.end())
        fail_unknown_sink(declared_sink, sinks_);

    ComposedSemantics out;
    // Everything except the marker rows is Sink-independent (D1's "does not metastasize" rule: only
    // IntentMarkerRow/IntentEmitRow carry a sink_gate today), so it is carried over verbatim.
    out.roles_ = roles_;
    out.level_lifts_ = level_lifts_;
    out.locations_ = locations_;
    out.value_classes_ = value_classes_;
    out.outcome_tokens_ = outcome_tokens_;
    out.outcome_markers_ = outcome_markers_;
    out.sinks_ = sinks_;
    out.strategies_ = strategies_;
    out.provenance_hooks_ = provenance_hooks_;
    out.packages_ = packages_;
    out.report_ = report_;
    // The identity is carried VERBATIM: semantic_identity is the RULESET's identity (which rows exist
    // and how they gate), not a per-stream view of it. Two streams of one binary declaring different
    // Sinks are analyzed by the SAME ruleset, so they must report the same identity — otherwise a
    // cross-Sink comparison (D4's legal case: BuildId N annotated ↔ N+1 stripped) would look like a
    // comparison across two different engines.
    out.identity_ = identity_;

    // Filter from all_markers_, never from markers_: markers_ is already SOME view (the Unspecified one
    // on a fresh composition), so filtering it would be a monotonically shrinking chain — for_sink()
    // must be idempotent in the Sink, not cumulative.
    out.all_markers_ = all_markers_;
    out.markers_.reserve(all_markers_.size());
    std::ranges::copy_if(all_markers_, std::back_inserter(out.markers_),
                         [declared_sink](const IntentMarkerRow& row)
                         { return sink_admits(row.sink_gate, declared_sink); });
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

    // ── Canonical serialization → semantic_identity (§4.1). Prefix the two version components, then
    // each package in canonical order. No addresses / link order in the input ⇒ reproducible (G-SP-4).
    std::string serialized;
    append_str(serialized, kSemanticGrammarVersion);
    append_str(serialized, insight::kCanonicalizationVersion);

    for (const std::size_t idx : order)
    {
        const SemanticPackageManifest& pkg{packages[idx]};
        serialize_manifest(serialized, pkg);

        // Concatenate rows in canonical (package-sorted, declared-within) order.
        composed.roles_.insert(composed.roles_.end(), pkg.roles.begin(), pkg.roles.end());
        composed.all_markers_.insert(composed.all_markers_.end(), pkg.markers.begin(),
                                     pkg.markers.end());
        composed.level_lifts_.insert(composed.level_lifts_.end(), pkg.level_lifts.begin(),
                                     pkg.level_lifts.end());
        composed.locations_.insert(composed.locations_.end(), pkg.locations.begin(),
                                   pkg.locations.end());
        composed.value_classes_.insert(composed.value_classes_.end(), pkg.value_classes.begin(),
                                       pkg.value_classes.end());
        composed.outcome_tokens_.insert(composed.outcome_tokens_.end(), pkg.outcome_tokens.begin(),
                                        pkg.outcome_tokens.end());
        composed.outcome_markers_.insert(composed.outcome_markers_.end(),
                                         pkg.outcome_markers.begin(), pkg.outcome_markers.end());
        // ADR 0028 — the composed Sink vocabulary. De-duplicated: two dialects may legitimately name
        // the same materialization, and this set is a lookup key (validating a caller's --sink), not a
        // per-package tally. Package order is canonical, so the result is deterministic.
        for (const std::string_view sink : pkg.sinks)
            if (std::ranges::find(composed.sinks_, sink) == composed.sinks_.end())
                composed.sinks_.push_back(sink);
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

    // ADR 0028 D5 — a fresh composition is the UNSPECIFIED view: nobody has declared a Sink, so every
    // concretely-gated row stays out and only kAnySink rows fire. A caller gets dialect depth by
    // declaring its Sink (for_sink), which is the point: declaring is the path to depth, and the
    // default can never be a concrete Sink (both-rows-live is the defect).
    std::ranges::copy_if(composed.all_markers_, std::back_inserter(composed.markers_),
                         [](const IntentMarkerRow& row)
                         { return sink_admits(row.sink_gate, kAnySink); });

    // Longest-match shadow notes over the composed prefix tables (the report; conflicts already fatal).
    // Markers use the FULL set: a shadow is a property of the declared vocabulary, not of one stream's
    // view, and reporting it only when a particular Sink is declared would make the report Sink-dependent.
    note_shadows<StructuralRoleRow>(composed.roles_, "role", composed.report_);
    note_shadows<IntentMarkerRow>(composed.all_markers_, "marker", composed.report_);
    note_shadows<LevelLiftRow>(composed.level_lifts_, "level_lift", composed.report_);
    note_shadows<OutcomeMarkerRow>(composed.outcome_markers_, "outcome_marker", composed.report_);

    return composed;
}

} // namespace insight::semantic
