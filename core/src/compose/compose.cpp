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

    // invariant: the KeyedFields roles enter in member order, every one, an empty one included.
    // refs: DN-162.D4
    void append_field_roles(std::string& out, const KeyedFieldRoles& fields)
    {
        append_str(out, fields.name);
        append_str(out, fields.display);
        append_str(out, fields.segment);
        append_str(out, fields.ordinal);
        append_str(out, fields.anonymous);
        append_str(out, fields.outcome);
        append_str(out, fields.propagated_outcome);
    }

    // invariant: the row sections whose rows carry a revision_since, in the order the trailing
    // ownership section writes them — the serialization order of their own sections.
    // refs: DN-162.D1
    constexpr std::uint32_t kOwnerBearingSections{8U};

    template <typename Row> void append_row_owners(std::string& out, std::span<const Row> rows)
    {
        append_u32_le(out, static_cast<std::uint32_t>(rows.size()));
        for (const Row& row : rows)
            append_str(out, row.revision_since);
    }

    // post: the index of `since` in the package's ordered revision span; 0 for an empty one, the
    // single-revision package's spelling of its one revision.
    // pre: a non-empty `since` names a revision of the span, the fence every package asserts in
    // its own translation unit (`all_row_revisions_declared`).
    // refs: DN-162.D1
    [[nodiscard]] std::uint32_t revision_rank_of(const SemanticPackageManifest& pkg,
                                                 std::string_view since) noexcept
    {
        const auto found{std::ranges::find(pkg.dialect_revisions, since)};
        if (found == pkg.dialect_revisions.end())
            return 0U;
        return static_cast<std::uint32_t>(found - pkg.dialect_revisions.begin());
    }

    // post: a stable index permutation by package name, compared byte-wise and locale-independent.
    // invariant: a name tie is impossible; the version tiebreak keeps the sort total regardless.
    // refs: ADR-17.D3
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

    // note: an instance value decides what canon masks, so it is identity.
    // invariant: written only by a package that declares one, after a zero-length opener, so a
    // package declaring none keeps the preimage it had before the section existed.
    // invariant: a package declaring instance values and no stop announcement writes the stop
    // section's opener and a zero count first, so the two openers never read as one another.
    // refs: DN-163.D1, DN-163.D7, ADR-2.D5
    void append_instance_values(std::string& out, const SemanticPackageManifest& pkg)
    {
        if (pkg.instance_values.empty())
            return;
        if (pkg.stop_announcements.empty())
        {
            append_str(out, {});
            append_u32_le(out, 0U);
        }
        append_str(out, {});
        append_u32_le(out, static_cast<std::uint32_t>(pkg.instance_values.size()));
        for (const InstanceValueRow& row : pkg.instance_values)
        {
            append_u8(out, static_cast<std::uint8_t>(row.form));
            append_str(out, row.bytes);
            append_str_span(out, row.flags);
            append_str_span(out, row.keys);
            append_str(out, row.dialect_gate);
        }
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
            append_u8(out, static_cast<std::uint8_t>(row.match));
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
            append_str(out, row.version.introducer);
            append_u8(out, static_cast<std::uint8_t>(row.version.shape));
            append_u8(out, static_cast<std::uint8_t>(row.role));
            if (row.extract == PayloadExtract::KeyedFields)
                append_field_roles(out, row.fields);
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
        // refs: ADR-18.D4
        append_u32_le(out, static_cast<std::uint32_t>(pkg.emits.size()));
        for (const IntentEmitRow& row : pkg.emits)
        {
            append_str(out, row.prefix);
            append_u8(out, static_cast<std::uint8_t>(row.kind));
            append_u8(out, static_cast<std::uint8_t>(row.child_order));
            append_str(out, row.dialect_gate);
            append_u8(out, static_cast<std::uint8_t>(row.emit));
            append_str(out, row.channel_gate);
            append_u8(out, static_cast<std::uint8_t>(row.role));
            if (row.emit == PayloadEmit::KeyedFieldsThenClosingBracket)
                append_field_roles(out, row.fields);
        }
        // note: byte-identical rows under different declared generations are different claims.
        // refs: ADR-17.D9
        append_str_span(out, pkg.dialect_revisions);
        // invariant: a declared key, its markers, its floor, its kind, its prefixes and whether a
        // row extends its base decide what canon masks, so they are identity.
        // refs: DN-133.D2, DN-162.D13, DN-163.D4, DN-163.D6
        append_u32_le(out, static_cast<std::uint32_t>(pkg.declared_values.size()));
        for (const DeclaredValueRow& row : pkg.declared_values)
        {
            append_str(out, row.key);
            append_str_span(out, row.markers);
            append_u32_le(out, row.min_digits);
            append_u8(out, static_cast<std::uint8_t>(row.value));
            append_str_span(out, row.prefixes);
            append_str(out, row.dialect_gate);
            append_u8(out, row.extends ? 1U : 0U);
        }
        // note: a stop announcement orders a unit's errors against the stop, so it is identity.
        // invariant: written only by a package that declares one, so a package declaring none
        // keeps the preimage it had before the section existed.
        // invariant: a zero-length string opens the section, and no package name is empty
        // (`all_packages_named`), so the next package's name can never read as it.
        // invariant: the preimage therefore stays a prefix-free encoding of the manifest list.
        // refs: ADR-17.D15, ADR-2.D5
        if (!pkg.stop_announcements.empty())
        {
            append_str(out, {});
            append_u32_le(out, static_cast<std::uint32_t>(pkg.stop_announcements.size()));
            for (const StopAnnouncementRow& row : pkg.stop_announcements)
            {
                append_str(out, row.shape);
                append_str(out, row.dialect_gate);
            }
        }
        append_instance_values(out, pkg);
        // note: which revision introduced each row is identity: it is what a generation owns.
        // invariant: written only by a package declaring several revisions, so every
        // single-revision package keeps the preimage it had before the section existed.
        // invariant: a decoder knows it is present from the revisions already serialized, and it
        // opens with a non-zero count, so it never reads as the stop section's empty opener.
        // refs: DN-162.D1, ADR-17.D9
        if (pkg.dialect_revisions.size() > 1U)
        {
            append_u32_le(out, kOwnerBearingSections);
            append_row_owners(out, pkg.roles);
            append_row_owners(out, pkg.markers);
            append_row_owners(out, pkg.level_lifts);
            append_row_owners(out, pkg.outcome_tokens);
            append_row_owners(out, pkg.emits);
            append_row_owners(out, pkg.declared_values);
            append_row_owners(out, pkg.stop_announcements);
            append_row_owners(out, pkg.instance_values);
        }
    }

    // post: one shadow note per pair whose shorter prefix properly prefixes the longer and whose
    // gates intersect.
    // refs: ADR-17.D4
    // invariant: a shape role row shadows nothing and is shadowed by nothing: it claims the whole
    // content and wins over every prefix row, so a prefix relation between its bytes means nothing.
    // refs: ADR-17.D4, ADR-17.D14
    template <typename Row>
    void note_shadows(std::span<const Row> rows, std::string_view kind, CompositionReport& report)
    {
        for (const Row& lhs : rows)
            for (const Row& rhs : rows)
            {
                if constexpr (std::same_as<Row, StructuralRoleRow>)
                    if (lhs.match == RoleMatchKind::Shape || rhs.match == RoleMatchKind::Shape)
                        continue;
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

    // note: the message states the rule and the remedy and names no record: canon ships public.
    [[noreturn]] void fail_version_coordinate(const SemanticPackageManifest& pkg,
                                              const IntentMarkerRow& row)
    {
        std::cerr << "FATAL: insight::semantic::compose — package \"" << pkg.name
                  << "\", marker row \"" << row.prefix
                  << "\": a version coordinate is declared whole or not at all. An introducer "
                     "needs a payload shape and a shape needs an introducer; declare both on the "
                     "row, or neither.\n";
        std::terminate();
    }

    // note: the message states the rule and the remedy and names no record: canon ships public.
    [[noreturn]] void fail_unit_role_row(const SemanticPackageManifest& pkg,
                                         const IntentMarkerRow& row)
    {
        std::cerr << "FATAL: insight::semantic::compose — package \"" << pkg.name
                  << "\", marker row \"" << row.prefix << "\": ";
        if (row.kind == insight::tokenization::IntentMarkerKind::InnerStep ||
            row.extract == PayloadExtract::KeyedFields || row.role == MarkerRole::Ends)
            std::cerr << "an inner step is read by the KeyedFields extractor alone, and a "
                         "KeyedFields row is an inner step's. Its naming row declares exactly the "
                         "field roles name, display, segment, ordinal and anonymous; its ending "
                         "row (role Ends, legal on kind InnerStep only) declares exactly outcome "
                         "and propagated_outcome, two distinct keys; neither declares a version "
                         "coordinate or a payload exclusion, and no other row declares a field "
                         "role. Fix the row's kind, extractor or field roles.\n";
        else if (row.role == MarkerRole::Closes)
            std::cerr << "a row that CLOSES its job's steps carries no identity and is of the "
                         "step kind. It needs kind Step, and no payload extractor, version "
                         "coordinate or payload exclusion: the job's epilogue it enters is the "
                         "segmenter's own unit. Make the row's kind Step and remove the rest, or "
                         "make it a naming row.\n";
        else
            std::cerr << "a row that only OPENS a unit opens a job or a step and carries no "
                         "identity. It needs kind Job or Step, and no payload extractor, version "
                         "coordinate or payload exclusion: the naming row of its kind that follows "
                         "names the unit. Fix the row, or make it a naming row.\n";
        std::terminate();
    }

    // note: the message states the rule and the remedy and names no record: canon ships public.
    // refs: DN-133.D1
    [[noreturn]] void fail_declared_value(const SemanticPackageManifest& pkg,
                                          const DeclaredValueRow& row)
    {
        std::cerr << "FATAL: insight::semantic::compose — package \"" << pkg.name
                  << "\", declared value \"" << row.key
                  << "\": a digit value is found behind markers, every one non-empty and ending "
                     "in a byte that is not a digit (a marker ending in a digit would make the "
                     "digit run after it ambiguous), or from a digit floor of at least "
                  << kMinDeclaredValueFloor
                  << " (a shorter value would meet ordinary numbers in a word), or both; a text "
                     "value declares non-empty prefixes and no marker, floor or extension; an "
                     "extension adds to a digit base row of its key declared before it in the "
                     "same package, under the same gate. Fix the row.\n";
        std::terminate();
    }

    // note: the message states the rule and the remedy and names no record: canon ships public.
    [[noreturn]] void fail_stop_announcement(const SemanticPackageManifest& pkg,
                                             const StopAnnouncementRow& row)
    {
        std::cerr << "FATAL: insight::semantic::compose — package \"" << pkg.name
                  << "\", stop announcement \"" << row.shape
                  << "\": a stop announcement is a whole-line shape of literal bytes, with {n} "
                     "holes admitted and none required, no other brace, no trailing whitespace, "
                     "and no hole against a digit, another hole or a decimal point leading into "
                     "either. Fix the row.\n";
        std::terminate();
    }

    // note: the message states the rule and the remedy and names no record: canon ships public.
    [[noreturn]] void fail_role_row(const SemanticPackageManifest& pkg,
                                    const StructuralRoleRow& row)
    {
        std::cerr << "FATAL: insight::semantic::compose — package \"" << pkg.name
                  << "\", role row \"" << row.prefix
                  << "\": a role row is a non-empty prefix, or a whole-line shape of literal bytes "
                     "with at least one {n} hole, no other brace, no trailing whitespace, and no "
                     "hole against a digit, another hole or a decimal point leading into either, "
                     "so that a hole is always exactly the number the line prints. The Progress "
                     "role is declared by a shape only, because its samples share their prefix "
                     "with lines that are content. Fix the row.\n";
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

    // post: returns only when every intent marker row carries a whole version coordinate and an
    // admitted unit role; otherwise fails closed naming the row.
    void fence_marker_rows(std::span<const SemanticPackageManifest> packages)
    {
        for (const SemanticPackageManifest& pkg : packages)
            for (const IntentMarkerRow& row : pkg.markers)
            {
                if (!version_coordinate_whole(row.version))
                    fail_version_coordinate(pkg, row);
                if (!unit_role_row_admitted(row))
                    fail_unit_role_row(pkg, row);
            }
    }

    // post: true when an extension row follows a digit base row of its key in its own package,
    // under its own gate; true for any base row.
    // refs: DN-163.D4
    [[nodiscard]] bool extension_has_base(std::span<const DeclaredValueRow> rows,
                                          std::size_t index) noexcept
    {
        const DeclaredValueRow& row{rows[index]};
        return !row.extends ||
               std::ranges::any_of(rows.first(index),
                                   [&row](const DeclaredValueRow& base) noexcept
                                   {
                                       return !base.extends && base.key == row.key &&
                                              base.value == DeclaredValueKind::Digits &&
                                              base.dialect_gate == row.dialect_gate;
                                   });
    }

    // post: returns only when every declared-value row is well formed, and every extension
    // extends a base row; otherwise fails closed naming the row.
    // post: also fails closed when two rows under one key, in any package, declare different
    // kinds, since one value cannot be a digit run and text at once.
    // refs: DN-163.D4, DN-163.D6
    void fence_declared_value_rows(std::span<const SemanticPackageManifest> packages)
    {
        for (const SemanticPackageManifest& pkg : packages)
            for (std::size_t index{0}; index < pkg.declared_values.size(); ++index)
            {
                const DeclaredValueRow& row{pkg.declared_values[index]};
                if (!declared_value_row_well_formed(row) ||
                    !extension_has_base(pkg.declared_values, index))
                    fail_declared_value(pkg, row);
                for (const SemanticPackageManifest& other : packages)
                    for (const DeclaredValueRow& peer : other.declared_values)
                        if (peer.key == row.key && peer.value != row.value)
                            fail_declared_value(pkg, row);
            }
    }

    // post: returns only when every instance-value row is well formed; otherwise fails closed
    // naming the row.
    // refs: DN-163.D1, DN-163.D7
    void fence_instance_value_rows(std::span<const SemanticPackageManifest> packages)
    {
        for (const SemanticPackageManifest& pkg : packages)
            for (const InstanceValueRow& row : pkg.instance_values)
                if (!instance_value_row_well_formed(row))
                {
                    std::cerr << "FATAL: insight::semantic::compose — package \"" << pkg.name
                              << "\", instance value \"" << row.bytes
                              << "\": a line row is a whole-line prefix ending in a space; a "
                                 "shape row is literal bytes with exactly one {n} hole, no space "
                                 "and no digit against the hole; an argument row is a line prefix "
                                 "ending in a space with at least one flag, and every flag and "
                                 "key non-empty, every key ending in `=`. Fix the row.\n";
                    std::terminate();
                }
    }

    // post: returns only when every structural role row is well formed; otherwise fails closed
    // naming the row.
    void fence_role_rows(std::span<const SemanticPackageManifest> packages)
    {
        for (const SemanticPackageManifest& pkg : packages)
            for (const StructuralRoleRow& row : pkg.roles)
                if (!role_row_well_formed(row))
                    fail_role_row(pkg, row);
    }

    // post: returns only when every stop-announcement row is well formed; otherwise fails closed
    // naming the row.
    void fence_stop_announcement_rows(std::span<const SemanticPackageManifest> packages)
    {
        for (const SemanticPackageManifest& pkg : packages)
            for (const StopAnnouncementRow& row : pkg.stop_announcements)
                if (!stop_announcement_row_well_formed(row))
                    fail_stop_announcement(pkg, row);
    }

    // post: returns only when every package is named, the set holds no conflict, and every
    // marker, declared-value, role and stop-announcement row is well formed.
    // post: otherwise it fails closed, naming the package and the row.
    // note: the unnamed-package fence answers first: every other diagnostic names the package.
    void fence_manifests(std::span<const SemanticPackageManifest> packages)
    {
        for (std::size_t index{0}; index < packages.size(); ++index)
            if (packages[index].name.empty())
                fail_unnamed_package(index, packages.size());

        if (const ConflictInfo conflict{find_conflict(packages)}; conflict.has_conflict)
            fail_closed(conflict);
        fence_marker_rows(packages);
        fence_declared_value_rows(packages);
        fence_role_rows(packages);
        fence_stop_announcement_rows(packages);
        fence_instance_value_rows(packages);
    }

    // post: appends the transport catalog's row count, then each row's name, kind, extract,
    // prefix width and leading-space flag, in catalog order.
    void append_transport_catalog(std::string& serialized)
    {
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

// post: the first refusal in context order; a valid context returns nothing.
// refs: DN-133.D1, DN-133.D2
std::expected<void, std::string>
check_stream_context(const insight::tokenization::StreamContext& context,
                     const ComposedSemantics& composed)
{
    const std::span<const std::string_view> keys{composed.declared_value_keys()};
    std::vector<std::string_view> seen;
    for (const insight::tokenization::ContextValue& entry : context.values)
    {
        if (!std::ranges::contains(keys, entry.key))
        {
            std::string declared;
            for (const std::string_view key : keys)
                declared += (declared.empty() ? "\"" : ", \"") + std::string{key} + '"';
            return std::unexpected("unknown context key \"" + entry.key +
                                   "\"; the composed packages declare: " +
                                   (declared.empty() ? std::string{"<none>"} : declared) +
                                   ". A context value is declared under a key a dialect "
                                   "declares, never guessed.");
        }
        if (std::ranges::contains(seen, entry.key))
            return std::unexpected("context key \"" + entry.key +
                                   "\" is declared twice; a stream declares one value per key.");
        seen.push_back(entry.key);
        // refs: DN-163.D6
        if (composed.declared_value_kind(entry.key) == DeclaredValueKind::Text)
        {
            if (entry.value.empty())
                return std::unexpected("context value under key \"" + entry.key +
                                       "\" is empty; a text key declares a non-empty value.");
            continue;
        }
        if (!insight::tokenization::is_declarable_value(entry.value))
            return std::unexpected("context value \"" + entry.value + "\" under key \"" +
                                   entry.key +
                                   "\" is not a run of decimal digits with no leading zero.");
        // refs: DN-162.D13
        if (const std::uint32_t floor{composed.declared_value_floor(entry.key)};
            entry.value.size() < floor)
            return std::unexpected("context value \"" + entry.value + "\" under key \"" +
                                   entry.key + "\" has " + std::to_string(entry.value.size()) +
                                   " digits; the key declares no marker, so its value must hold "
                                   "at least " +
                                   std::to_string(floor) + " digits.");
    }
    return {};
}

std::vector<insight::tokenization::DeclaredRun>
declared_runs_of(const insight::tokenization::StreamContext& context,
                 const ComposedSemantics& composed)
{
    if (const auto checked{check_stream_context(context, composed)}; !checked)
    {
        std::cerr << "FATAL: insight::semantic::declared_runs_of — " << checked.error()
                  << " Validate a caller-supplied context with check_stream_context before it "
                     "reaches a tokenizer or a segmentation.\n";
        std::terminate();
    }
    std::vector<insight::tokenization::DeclaredRun> runs;
    for (const DeclaredValueRow& row : composed.declared_values())
    {
        const auto entry{
            std::ranges::find(context.values, row.key, &insight::tokenization::ContextValue::key)};
        if (entry != context.values.end())
            runs.push_back({.markers = row.markers,
                            .value = entry->value,
                            .min_digits = row.min_digits,
                            .kind = row.value,
                            .prefixes = row.prefixes});
    }
    return runs;
}

// refs: DN-163.D1, DN-163.D7
std::vector<insight::tokenization::InstanceValue>
instance_values_of(const ComposedSemantics& composed)
{
    std::vector<insight::tokenization::InstanceValue> values;
    values.reserve(composed.instance_values().size());
    for (const InstanceValueRow& row : composed.instance_values())
        values.push_back(
            {.form = row.form, .bytes = row.bytes, .flags = row.flags, .keys = row.keys});
    return values;
}

std::string ComposedSemantics::identity_hex() const
{
    std::string out;
    out.reserve(2U * identity_.size());
    for (const std::uint8_t byte : identity_)
    {
        out.push_back(kHexDigits[(static_cast<unsigned>(byte) >> kNibbleShift) & kNibbleMask]);
        out.push_back(kHexDigits[static_cast<unsigned>(byte) & kNibbleMask]);
    }
    return out;
}

std::string ComposedSemantics::canonicalization_version() const
{
    return std::string{insight::kCanonicalizationVersion} + ":" + identity_hex();
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
    // invariant: the view keeps the matched package's own name, never the caller's bytes, which
    // may be a temporary; an empty declaration keeps the empty view, which is Unspecified.
    std::string_view resolved_dialect;
    if (!declared_dialect.empty())
    {
        const auto matched{std::ranges::find(packages_, declared_dialect, &ComposedPackage::name)};
        if (matched == packages_.end())
            fail_unknown_dialect(declared_dialect, packages_);
        resolved_dialect = matched->name;
    }
    if (!declared_channel.empty() &&
        std::ranges::find(channels_, declared_channel) == channels_.end())
        fail_unknown_channel(declared_channel, channels_);

    ComposedSemantics out;
    out.locations_ = locations_;
    out.value_classes_ = value_classes_;
    out.channels_ = channels_;
    out.declared_value_keys_ = declared_value_keys_;
    out.strategies_ = strategies_;
    out.provenance_hooks_ = provenance_hooks_;
    out.packages_ = packages_;
    out.report_ = report_;
    // note: the identity is the RULESET's: every view of one composition reports the same one.
    // refs: ADR-17.D3, ADR-22.D4
    out.identity_ = identity_;

    out.all_roles_ = all_roles_;
    out.all_markers_ = all_markers_;
    out.all_marker_ranks_ = all_marker_ranks_;
    out.all_level_lifts_ = all_level_lifts_;
    out.all_outcome_tokens_ = all_outcome_tokens_;
    out.all_outcome_markers_ = all_outcome_markers_;
    out.all_declared_values_ = all_declared_values_;
    out.all_stop_announcements_ = all_stop_announcements_;
    out.all_instance_values_ = all_instance_values_;
    out.declared_dialect_ = resolved_dialect;

    const auto admits{[resolved_dialect](std::string_view dialect_gate) noexcept
                      { return dialect_admits(dialect_gate, resolved_dialect); }};

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
    // refs: DN-133.D2
    out.declared_values_.reserve(all_declared_values_.size());
    std::ranges::copy_if(all_declared_values_, std::back_inserter(out.declared_values_),
                         [&admits](const DeclaredValueRow& row)
                         { return admits(row.dialect_gate); });
    // refs: ADR-17.D15
    out.stop_announcements_.reserve(all_stop_announcements_.size());
    std::ranges::copy_if(all_stop_announcements_, std::back_inserter(out.stop_announcements_),
                         [&admits](const StopAnnouncementRow& row)
                         { return admits(row.dialect_gate); });
    // refs: DN-163.D1, DN-163.D7
    out.instance_values_.reserve(all_instance_values_.size());
    std::ranges::copy_if(all_instance_values_, std::back_inserter(out.instance_values_),
                         [&admits](const InstanceValueRow& row)
                         { return admits(row.dialect_gate); });
    // note: the ranks are filtered in lockstep with their rows, so the two stay parallel.
    // refs: ADR-22.D4, DN-162.D1
    out.markers_.reserve(all_markers_.size());
    out.marker_ranks_.reserve(all_markers_.size());
    for (std::size_t index{0}; index < all_markers_.size(); ++index)
    {
        const IntentMarkerRow& row{all_markers_[index]};
        if (!admits(row.dialect_gate) || !channel_admits(row.channel_gate, declared_channel))
            continue;
        out.markers_.push_back(row);
        out.marker_ranks_.push_back(all_marker_ranks_[index]);
    }
    return out;
}

// post: composition is canonically ordered and fails closed; an unnamed package is refused.
// post: a fresh composition IS the doubly-unspecified view: only any-dialect, any-channel rows
// fire.
// refs: ADR-17.D2, DN-17.D17
ComposedSemantics compose(std::span<const SemanticPackageManifest> packages)
{
    fence_manifests(packages);

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
    append_transport_catalog(serialized);

    for (const std::size_t idx : order)
    {
        const SemanticPackageManifest& pkg{packages[idx]};
        serialize_manifest(serialized, pkg);

        composed.all_roles_.insert(composed.all_roles_.end(), pkg.roles.begin(), pkg.roles.end());
        composed.all_markers_.insert(composed.all_markers_.end(), pkg.markers.begin(),
                                     pkg.markers.end());
        for (const IntentMarkerRow& row : pkg.markers)
            composed.all_marker_ranks_.push_back(revision_rank_of(pkg, row.revision_since));
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
        composed.all_declared_values_.insert(composed.all_declared_values_.end(),
                                             pkg.declared_values.begin(),
                                             pkg.declared_values.end());
        composed.all_stop_announcements_.insert(composed.all_stop_announcements_.end(),
                                                pkg.stop_announcements.begin(),
                                                pkg.stop_announcements.end());
        composed.all_instance_values_.insert(composed.all_instance_values_.end(),
                                             pkg.instance_values.begin(),
                                             pkg.instance_values.end());
        for (const DeclaredValueRow& row : pkg.declared_values)
            if (std::ranges::find(composed.declared_value_keys_, row.key) ==
                composed.declared_value_keys_.end())
                composed.declared_value_keys_.push_back(row.key);
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
    // invariant: the truncation width is the identity member's OWN extent, never a second literal
    // beside it — two spellings of one width drift silently and the wider one overruns the array.
    static_assert(kSemanticIdentityBytes <= kSha256Bytes,
                  "the composed identity truncates SHA-256; it cannot be wider than the digest");
    for (std::size_t i{0}; i < composed.identity_.size(); ++i)
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
