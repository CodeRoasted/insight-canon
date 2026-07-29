// insight.semantic.github — the GitHub Actions / Azure Pipelines dialect semantic package
// (ADR 0024). VOCABULARY as DATA in the closed canon rule grammar (structural roles, intent
// markers, level lifts) + the CODE tier (the dialect format strategy + the echoed-source provenance
// hook). Fully self-contained: imports only insight.canon.api (types + utils) + insight.canon.spi
// (the provider contract) — never a sealed detail shard. The composition
// (insight::semantic::compose) statically links this package's kManifest into a binary.
//
// `export import insight.canon.spi` so a consumer that imports this module can name
// SemanticPackageManifest (the type of kManifest) without a separate spi import.
module;

export module insight.semantic.github;
import insight.canon.internal; // std + global C fixed-width types
import insight.canon.api;      // StructuralRole, LogLevel, LogFormat, IntentMarkerKind, ChildOrder
export import insight.canon.spi;

namespace insight::semantic::github
{

// ── The code-tier seam (defined in github_provenance.cpp, this module's impl unit) ──
// The echoed-source raw-line provenance hook (matches spi::ProvenanceHook — pure
// bool(string_view) noexcept). Declared here so kManifest can take its address.
//
// There is NO strategy factory any more (T4): `GitHubActionsStrategy` detected and peeled GitHub's
// per-line delivery stamp, and that peel is now DECLARED transport (ADR 0044 §3/§8 —
// `api-rfc3339-line-prefix`). This package's code tier is one byte predicate, so the dialect is
// DATA: rows plus canon's walkers ARE the GHA parser (ADR 0065 clause 5).
export bool is_echoed_source(std::string_view raw_line) noexcept;

// The dialect NAME every gated row below carries, and the name a caller declares
// (`IngestDeclaration::dialect` / `--dialect`). Exported because a caller has to be able to name it
// without spelling a literal, exactly as the channel names are exported: the value is this
// package's manifest `.name`, and `all_dialect_gates_owned` static_asserts the two agree.
export inline constexpr std::string_view kDialect{"github"};

// ── Structural-role rows (§1.2) ──
// The announced GitHub-Actions/Azure markers. dialect_gate = kAnyDialect: these fire whatever the
// caller declared, reproducing the pre-split UNGATED StructuralRoleRegistry::classify EXACTLY (a
// `##[group]` on an undeclared CI line still classifies — byte-identity, G-SP-1).
//
// ⚠ THE UNGATED READING IS DELIBERATE AND IS NOT AN OVERSIGHT. T4 changed the gate's TYPE, never a
// row's VALUE: these six were `kAnyFormat` and are now `kAnyDialect`, which is the same claim in
// the new vocabulary. Narrowing them to \"github\" would be a recognition change riding a
// type change — a different decision, with its own gate, and not this cut's.
inline constexpr std::array<StructuralRoleRow, 6> kRoles{{
    {.prefix = "##[group]", .role = insight::StructuralRole::GroupBegin, .dialect_gate = kAnyDialect},
    {.prefix = "::group::", .role = insight::StructuralRole::GroupBegin, .dialect_gate = kAnyDialect},
    {.prefix = "##[endgroup]",
     .role = insight::StructuralRole::GroupEnd,
     .dialect_gate = kAnyDialect},
    {.prefix = "::endgroup::",
     .role = insight::StructuralRole::GroupEnd,
     .dialect_gate = kAnyDialect},
    {.prefix = "##[error]", .role = insight::StructuralRole::Terminator, .dialect_gate = kAnyDialect},
    {.prefix = "::error::", .role = insight::StructuralRole::Terminator, .dialect_gate = kAnyDialect},
}};

// ── The declared INTENT CHANNEL vocabulary (ADR 0030 D1/D3) ──
// `Medium = IntentFormat × IntentChannel`. GHA is ONE IntentFormat that canon receives in TWO
// materializations — but they are NOT two channels GitHub serves, and saying so was the error ADR
// 0030 corrects:
//
//   annotated — GHA's ONE REAL CHANNEL. The runner's raw job log, which is what the API serves,
//   what
//               downloadJobLogsForWorkflowRun fetches, and what the shipped Action feeds. Step
//               banners are `##[group]Run <cmd>`; the workflow-command markers (`##[error]`,
//               `##[group]`) are present. A user's GHA log is annotated in every path we ship.
//   stripped  — OUR OWN LAB ABLATION, not a GitHub product. Produced by
//               ci_revert_corpus.transform.degrade(), which strips the `##[…]` workflow commands to
//               build the structure-poor arm of the template-lattice lift experiment (it measures
//               how much the structure buys us). Step banners are the bare `Run <cmd>`. Canon must
//               READ it — the lattice experiment is a real consumer — so it stays a declared
//               channel, and the G-SINK-2 control arm proves the bare `Run ` row is load-bearing
//               for it.
//
// ⚠ DO NOT re-describe this pair as "two GHA materializations" or claim MATERIALIZATION INVARIANCE
// across it (ADR 0030 D1). The pair tests canon against OUR OWN degrade(), which is a real and
// useful property — but calling it invariance across two real materializations of a dialect is
// exactly the endogamy trap the corpus discipline exists to prevent. The prior comment here
// asserted a "clean partition MEASURED on 22 030 real annotated/stripped pairs"; only the ANNOTATED
// arm is real bytes.
//
// What that correction does NOT touch: the annotated arm IS real GHA, so the phantom-Step defect
// (9.05 % of 22 030 real logs) is real, and the fix below is correct and necessary.
//
// The NAMES are `annotated` / `stripped` and stay so (D3): they name what a user can verify by
// looking at their own file, which `api` / `download` would not.
//
// EXPORTED: a package's channel vocabulary is part of its public declaration. Every caller that
// acquires a GHA stream must name the channel it acquired — or derive it at Acquisition (D2: canon
// never infers; the caller may) — so the names have to be nameable: the CLI validates `--channel`
// against them and peeks to deduce one, the Action declares `annotated` because it chose
// downloadJobLogsForWorkflowRun, and a test says which form it fixtures. It is also the MEDIUM
// SELECTOR's input on the writer side, whose default is `annotated` — the real channel (D1).
export inline constexpr std::string_view kChannelAnnotated{"annotated"};

// Two things wear this one name, and conflating them is the trip sessions keep making — which is
// why it is stated HERE, at the definition, and not only in the design doc that already declares
// it. A limitation that lives one repo away does not travel to the point of use.
//
//   The BYTES are ours. Anything measured on a stripped corpus was measured against
//   ci_revert_corpus.transform.degrade(), so it evidences how canon reads OUR ablation — never how
//   a dialect behaves in the field. It may not stand as corpus evidence for a product claim, and
//   an A/B across the pair is not materialization invariance (adr/0030 D1 rules on this).
//
//   The NAME is real, and it is a recognition state that ABSENCE CANNOT DECIDE. Markers present
//   positively proves the annotated channel — only that channel carries them — but no observation
//   proves this one: a log with no markers is equally consistent with "stripped" and "annotated
//   that happened to emit none". So this name is never inferred from absence; a caller with no
//   evidence claims no channel at all. Guessing it is unsound in the DANGEROUS direction — it
//   re-arms the bare `Run ` row against annotated prose and reinstates the phantom Step the fix
//   below exists to kill. The deduction that gets this right, and the fail-closed branch, belong
//   to sift's channel probe; canon never infers (adr/0030 D2).
export inline constexpr std::string_view kChannelStripped{"stripped"};
export inline constexpr std::array<std::string_view, 2> kChannels{
    {kChannelAnnotated, kChannelStripped}};

// ── Intent-marker rows (§1.2/§2.2) ──
// DIALECT-GATED to this package (II-6 — `Run ` is GHA-runner-specific and would misfire elsewhere).
// The hierarchy rides the rows: Job = Unordered (jobs parallel-by-construction), Step = Ordered
// (steps sequential-by-YAML) — the ADR 0023 level-typed alignment declaration. The payload is the
// content after the prefix, verbatim (core's canonicalize_intent/discriminant_of derive the class +
// instance).
//
// CHANNEL-GATED per Step (ADR 0029 D5 — this is the phantom fix, and it REPLACES the reasoning that
// used to sit here). The two Step prefixes are the same intent in two channels, so each gates to
// ITS channel:
//
//   `Run `         → stripped   — REQUIRED, and the row cannot simply be deleted: our ablation arm
//                                 genuinely uses the bare `Run ` as its banner (G-SINK-2's control
//                                 arm is the fence). In the ANNOTATED channel a line starting with
//                                 `Run ` is ordinary PROSE, so an ungated row mints a phantom Step
//                                 quantum out of it. Measured on REAL annotated bytes: 9.05 % of 22
//                                 030 logs, 7 752 lines, 62 distinct payloads, EVERY ONE prose (``
//                                 `npm audit` for details. `` dominates). Confirmed end-to-end to
//                                 fabricate a VanishedPhase.
//   `##[group]Run ` → annotated — self-gating in principle (`##[` never occurs in a stripped log),
//   but
//                                 gated anyway so the declared channel selects EXACTLY ONE Step
//                                 row: a multi-channel tree becomes unrepresentable rather than
//                                 merely unattested (D5), fail-closed is symmetric across both
//                                 forms, and the writer's medium selector (D4) has a gate to select
//                                 ON.
//
// The superseded claim, recorded so it is not re-derived: shipping BOTH prefixes ungated was
// believed to make the step name "materialization-INVARIANT" and to dissolve the
// which-form-was-Sift-fed hazard. It does the opposite — the PAYLOAD does read back the same from
// both (both yield `yarn lint`), but ungated recognition of the *bare* prefix in the annotated
// channel is exactly the defect. The two gated rows preserve that same-payload property under their
// own channels — but note what it is and is not: it is canon reading our ablation back to the same
// intent, NOT invariance across two real GHA materializations (ADR 0030 D1).
//
// `Complete job name: ` stays kAnyChannel: it is the banner in BOTH channels (the strip does not
// touch it), so it has no channel-dependent reading and needs no gate — a gate is required exactly
// when one channel's marker occurs as ordinary content in a sibling channel, which is why `Run `
// needs one and this does not (the "does not metastasize" rule). The two Step prefixes never
// shadow: neither is a proper prefix of the other, so compose's longest-match note stays empty.
inline constexpr std::array<IntentMarkerRow, 3> kMarkers{{
    {.prefix = "Complete job name: ",
     .kind = insight::tokenization::IntentMarkerKind::Job,
     .child_order = insight::tokenization::ChildOrder::Unordered,
     .dialect_gate = kDialect,
     .extract = PayloadExtract::RemainderAfterPrefix,
     .channel_gate = kAnyChannel}, // the job banner is identical in both channels
    {.prefix = "Run ",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .dialect_gate = kDialect,
     .extract = PayloadExtract::RemainderAfterPrefix,
     .channel_gate = kChannelStripped}, // in the annotated channel this prefix is PROSE
    {.prefix = "##[group]Run ", // the runner-wrapped materialization of the same step banner
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .dialect_gate = kDialect,
     .extract = PayloadExtract::RemainderAfterPrefix,
     .channel_gate = kChannelAnnotated},
}};

// ── Generation-template rows (studies/008, shared_intent_declaration §3.2) — the WRITER dual ──
// One emit row per recognition row, paired by (prefix, kind, dialect_gate, channel_gate) — the
// MEDIUM is `dialect × IntentChannel` (ADR 0030 D3 / ADR 0065 clause 1), so each projection names
// the same dialect and channel as its dual. Both Step media are present: `##[group]Run <cmd>` materializes into the ANNOTATED
// channel (the real one, and the writer's default), `Run <cmd>` into our STRIPPED ablation — each
// read back to the same identity under its own channel, so the round-trip closes per channel (G2).
// The writer picks WHICH by the declared channel (the medium selector), never by array order. Each
// emit is PayloadAfterPrefix, the exact inverse of the reader's RemainderAfterPrefix:
// render_row(row, "yarn lint") reproduces the banner canon segments back to Step "yarn lint".
inline constexpr std::array<IntentEmitRow, 3> kEmitMarkers{{
    {.prefix = "Complete job name: ",
     .kind = insight::tokenization::IntentMarkerKind::Job,
     .child_order = insight::tokenization::ChildOrder::Unordered,
     .dialect_gate = kDialect,
     .emit = PayloadEmit::PayloadAfterPrefix,
     .channel_gate = kAnyChannel},
    {.prefix = "Run ",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .dialect_gate = kDialect,
     .emit = PayloadEmit::PayloadAfterPrefix,
     .channel_gate = kChannelStripped},
    {.prefix = "##[group]Run ", // the runner-wrapped medium of the same step banner
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .dialect_gate = kDialect,
     .emit = PayloadEmit::PayloadAfterPrefix,
     .channel_gate = kChannelAnnotated},
}};

// The C2 bidirectionality obligation: this dialect exposes BOTH projections, and every recognition
// row is paired with a generation row. DialectIntent fails to compile if a reader ships without a
// writer.
export struct Dialect
{
    static constexpr std::span<const IntentMarkerRow> markers{kMarkers};
    static constexpr std::span<const IntentEmitRow> emit_markers{kEmitMarkers};
};
static_assert(
    insight::semantic::DialectIntent<Dialect>,
    "github: a recognition marker has no paired generation row (reader without a writer), or "
    "a reader/writer pair straddles two IntentChannels (ADR 0029 D1 — the projections must "
    "name the same Medium)");

// ADR 0029 D5 — the channel vocabulary and the rows that gate to it must agree, at COMPILE time,
// here.
static_assert(
    insight::semantic::all_channels_named(kChannels),
    "github: a declared IntentChannel name is empty — the empty name IS kAnyChannel, so it may "
    "not also name a concrete channel");
static_assert(insight::semantic::all_channel_gates_declared(kMarkers, kEmitMarkers, kChannels),
              "github: a row gates to an IntentChannel this package never declared (a typo in "
              ".channel_gate?) — the declared vocabulary is kChannels");

// ── Level-lift rows (§1.2) ──
// The GHA workflow-command level lift. DIALECT-GATED to this package. Pure DATA: the package
// declares the rows, canon walks them — `insight::tokenization::lift_level` over the composed
// `level_lifts()` table, applied by LogParser to every parsed line (ADR 0063 clause 2). Until then
// this package's own strategy walked the array itself, which made LevelLiftRow the last row kind
// whose algorithm lived in a package and left these rows feeding `semantic_identity` from a
// single package-local reader. First match in DECLARED order wins, so order is content here.
// Also serialized into semantic_identity.
inline constexpr std::array<LevelLiftRow, 8> kLevelLifts{{
    {.prefix = "##[error]",
     .level = insight::LogLevel::Error,
     .dialect_gate = kDialect},
    {.prefix = "::error::",
     .level = insight::LogLevel::Error,
     .dialect_gate = kDialect},
    {.prefix = "##[warning]",
     .level = insight::LogLevel::Warn,
     .dialect_gate = kDialect},
    {.prefix = "::warning::",
     .level = insight::LogLevel::Warn,
     .dialect_gate = kDialect},
    {.prefix = "##[debug]",
     .level = insight::LogLevel::Debug,
     .dialect_gate = kDialect},
    {.prefix = "::debug::",
     .level = insight::LogLevel::Debug,
     .dialect_gate = kDialect},
    {.prefix = "##[notice]",
     .level = insight::LogLevel::Info,
     .dialect_gate = kDialect},
    {.prefix = "::notice::",
     .level = insight::LogLevel::Info,
     .dialect_gate = kDialect},
}};

// ── Run-outcome token rows (grammar-2, ADR 0025 §4 — the GitHub reshape) ──
// The GHA native job/run conclusion strings (`${{ needs.<job>.result }}` / the API `conclusion`),
// mapped into the core four-class vocabulary. This RETIRES the render-side `deriveBuildStatus`
// binary at its root: the verdict now enters the ENGINE four-class-aware through the same channel
// as Jenkins (the Action forwards the native token to `sift --*-outcome`; Argos rewires the JS).
// GHA has no native UNSTABLE string today — the category stays core, this dialect simply ships no
// row for it. skipped/neutral/action_required carry no pass↔fail verdict → Unknown (honest, never
// a guess). NO OutcomeMarkerRow: GHA emits no single run-verdict console line (`Process completed
// with exit code N` is per-step) — the degenerate console path is correctly Unknown (§3.2).
inline constexpr std::array<OutcomeTokenRow, 7> kOutcomeTokens{{
    {.token = "success",
     .outcome = insight::RunOutcome::Success,
     .dialect_gate = kDialect},
    {.token = "failure",
     .outcome = insight::RunOutcome::Failure,
     .dialect_gate = kDialect},
    {.token = "cancelled",
     .outcome = insight::RunOutcome::Aborted,
     .dialect_gate = kDialect},
    {.token = "timed_out",
     .outcome = insight::RunOutcome::Aborted,
     .dialect_gate = kDialect},
    {.token = "skipped",
     .outcome = insight::RunOutcome::Unknown,
     .dialect_gate = kDialect},
    {.token = "neutral",
     .outcome = insight::RunOutcome::Unknown,
     .dialect_gate = kDialect},
    {.token = "action_required",
     .outcome = insight::RunOutcome::Unknown,
     .dialect_gate = kDialect},
}};

// ── The manifest (§2.5) — the package's single composed contribution ──
// name "github", version "1.4.0" — bumped from 1.3.0 for T4: the package's CODE TIER lost its
// format strategy (`.strategy` is serialized as a presence byte, so the manifest's content genuinely
// moved) and every gated row's coordinate became the package NAME. 1.3.0 was the IntentChannel
// coordinate (the declared
// channel vocabulary + the channel-gated Step rows). SP-7 immutable-release discipline: a released
// version's rows are frozen, a content change is a new version; the bump also rides the II-7
// semantic_identity hash, an honest comparability boundary — a diff across this boundary is
// comparing two different recognition rulesets, and the digest says so.
//
// ADR 0029's rename (Sink → IntentChannel) did NOT bump this, deliberately: it renamed C++
// identifiers, and what enters the digest is the channel NAMES ("annotated"/"stripped" — ruled
// unchanged) and the row content, neither of which moved. SP-7 keys on CONTENT, not on spelling;
// bumping for a rename would declare a new ruleset that recognizes exactly what the old one did,
// and make two identical rulesets look incomparable. Ships no locations (that is the
// test_frameworks package) and no value classes (none has a consumer yet). Code tier: the
// echoed-source hook, and nothing else.
export inline constexpr SemanticPackageManifest kManifest{
    .name = "github",
    .version = "1.4.0",
    .roles = kRoles,
    .markers = kMarkers,
    .emits = kEmitMarkers, // ADR 0044 §7 — the generation projection is identity-bearing
    .level_lifts = kLevelLifts,
    .locations = {},
    .value_classes = {},
    .outcome_tokens = kOutcomeTokens,
    .outcome_markers = {},
    .channels = kChannels, // ADR 0029 D1 — the two GHA materializations
    .echoed_source = &is_echoed_source, // the only code tier left: a byte predicate, not a grammar
};

// ADR 0065 clause 1 — every gated row names THIS package or kAnyDialect, checked at COMPILE time,
// here. A gate naming another package would reach across a boundary this package does not own, and
// a typo would produce a row that silently never fires under any declaration; composition FLATTENS,
// so nothing downstream could tell either apart from a legitimate row.
static_assert(insight::semantic::all_dialect_gates_owned(kManifest),
              "github: a row's dialect_gate is neither kAnyDialect nor this package's own name (a "
              "typo in .dialect_gate, or a row reaching for another package's vocabulary?)");
static_assert(kManifest.name == kDialect,
              "github: kDialect and the manifest name must be the same string — kDialect is what a "
              "caller declares and what every gated row carries");

} // namespace insight::semantic::github
