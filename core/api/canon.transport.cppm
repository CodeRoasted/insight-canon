// insight.canon.transport — the TRANSPORT VOCABULARY (ADR-23). The declared, stream-scoped peel
// that runs OUTSIDE-IN of everything else: transport → logformat → intent (0031's frozen parse
// order). Public + installed; the facade `export import`s it, so `import insight.canon;` yields the
// declaration types alongside Tokenizer.
//
// WHY THIS IS NOT A SEMANTIC PACKAGE CONCERN, and why the catalogue does not live in a manifest
// (ADR-23). 0031's ratified model is a PRODUCT — `TransportStack × LogFormat × dialect intent
// markers`. The GHA per-line stamp is a property of GitHub's *delivery*, not of the GHA *dialect*;
// Jenkins Timestamper is a *plugin*, not the Jenkins dialect. Filing transform rows in
// `SemanticPackageManifest` would re-couple exactly the two factors the model separates, and would
// make NESTING (docker-inside-GHA — 0031's named beneficiary) inexpressible, since a stack must be
// able to compose transforms of different origins over any dialect. So the catalogue is canon's
// own, orthogonal to the packages, and canon owns every transform ALGORITHM exactly as it owns
// every matcher algorithm.
//
// The catalogue is CANON-SHIPPED and closed, not package-extensible: transport transforms are a
// core vocabulary like the ordinal/OTEL field catalogs, not dialect data. Its version + rows enter
// EVERY composed `semantic_identity` (§6 — the transform GRAMMAR is identity; the per-run
// declaration is provenance and goes to MetaLog instead).
module;

export module insight.canon.transport;
import insight.canon.internal; // std + global C fixed-width types
import insight.canon.api;      // Timestamp, parse_iso8601

export namespace insight::transport
{

// The catalogue's version — a component of the composed identity, exactly as the semantic grammar
// version is. A stream analyzed under two different transport vocabularies is not comparable, and
// the digest must say so.
//
// THE BUMP RULE (ADR-2.3, which SHARPENED the over-broad rule this comment first
// carried):
//
//     Bump on a change to what the catalogue SERIALIZES — plus any change to its serialization
//     SHAPE, even when the bytes happen to coincide.
//
// The second half is the token's whole reason to exist (ADR-2): a shape change whose
// bytes happen not to move is exactly the collision a content hash cannot catch. The first half is
// why "a new kind" is NOT on the list: `kind` is serialized only as a value ON a row, so an enum
// member with no row serializes ZERO bytes and changes no behavior — bumping on it would move every
// golden for nothing. That cannot happen anyway, since a kind never lands without its row (below):
// "a new kind" and "a new row" always co-fire, and two criteria that always agree are one
// criterion.
//
// LIKE EVERY MONOTONIC TOKEN HERE, THIS IS ASSIGNED AT SHIP AND NEVER RESERVED (ADR-2,
// NORMATIVE): the value means "the Nth shape", and which change causes the Nth shape is not
// knowable in advance. `-2` was taken when the SECOND transform landed with its row (T5 5.2:
// `bracket-rfc3339-line-prefix`, DN-15 — the co-fire the comment above
// predicted); `-3` was taken when the THIRD transform landed with its row
// (`utf8-bom-line-prefix`, DN-25 — the same co-fire again); `-4` moves next, at whatever
// ship makes the fourth shape.
inline constexpr std::string_view kTransportCatalogVersion{"transport-catalog-3"};

// ⚠ NORMATIVE — CATALOGUE ENUM VALUES ARE IDENTITY-BEARING: NEW MEMBERS APPEND (ADR-2
// clause 2.2). A value is never renumbered and never inserted mid-enum. Both enums below serialize
// as their `uint8_t` VALUE on every row (`compose.cpp`), so inserting a member in the middle shifts
// the serialized byte of EVERY EXISTING ROW — the digest moves for rows nobody touched, and it
// moves SILENTLY: the diff is one line, the compiler says nothing, and no reviewer would attribute
// the golden churn to it. The explicit `= 0` on each first member is the anchor; append after the
// last.

// ════════════════════════════════════════════════════════════════════════════════════════════════
// §3 — the closed transform vocabulary. Rows-as-data, NEVER a callable (SRC-SID-2 applied to
// transport, for SRC-SID-2's reason: a callable is un-hashable and lets the declaration and the
// behavior diverge silently). Enum-not-tag for the closed sets (ADR-20).
// ════════════════════════════════════════════════════════════════════════════════════════════════

// Which transform ALGORITHM a row selects and parameterizes.
//
// TWO members today, each grown in WITH ITS ALGORITHM, ITS ROW AND ITS GATE (ADR-2.1),
// as `PayloadExtract` and `LocationMatchKind` grew. ADR-23 listed a wider anticipated
// vocabulary — FramingLine, AnsiEchoWrap, StreamTag, Truncation, Chunking, Encoding — and **§3's
// enum bodies are to be read as a SKETCH, never as a normative closed set**; §3's normative
// content is the row shape, the rows-as-data rule and the ternary extract routing, all of which
// shipped intact.
//
// The decisive argument is not anti-dormant, it is arithmetic: an enum member with no row
// serializes zero bytes, so declaring the full set up front either moves no digest (and buys
// nothing — the members are unreachable) or costs one gratuitous identity bump and golden re-derive
// for zero capability. Growing in makes the bump CO-FIRE with the row that makes the kind real,
// which is the irreducible cost either way. It changes no error message either: `find_transform`
// resolves a declaration by NAME against the rows, so an unused sibling kind would not improve the
// hard error.
//
// Each further member APPENDS (see the identity-bearing note above).
enum class TransportTransformKind : std::uint8_t
{
    // A fixed-width timestamp stamped at the head of EVERY line by the delivery layer. The GHA API
    // per-line RFC 3339 prefix is the shipped member.
    LinePrefixTimestamp = 0,
    // A BRACKETED strict-RFC3339 stamp at the head of every line of a declared stream, VARIABLE
    // width: `[` + the shared full-datetime grammar (`insight::utils::rfc3339_datetime_length` —
    // the one owner, bibles/jenkins_dialect.md §4 item 3) + `]`, then the declared separator/
    // indentation strip. The Jenkins Timestamper plugin's whole-stream scoping is the attested
    // population (12/113 in jenkins-markers/v2, ADR-23 Part 2), and the row landed exactly where
    // the earlier refusal said it could not YET land: WITH its algorithm, its row and its gate
    // (G-T5-PEEL — a real population and a real frozen oracle, the shipped strategy strip frozen
    // per ADR-8 since this cut deletes it). Admissibility argument:
    // DN-15. Peel-equivalence is the ONLY obligation this row
    // carries — the invariance cell stays empty (ADR-23: no world vehicle exists), so declaring
    // it certifies OUR refactor and nothing about the world.
    LinePrefixBracketedTimestamp,
    // A UTF-8 byte-order mark at the head of a line, `EF BB BF`. A FIXED three-byte prefix
    // removed ONCE — not a greedy loop and not a `find` anywhere in the line: the BOM is a
    // delivery artifact of the stream's first bytes, so a second one is content and is left
    // alone. `prefix_width` is unread (the width is the grammar, not a parameter) and
    // `strip_leading_space` is false: a space after a BOM is a real content byte.
    LinePrefixByteOrderMark,
};

// What the peeled bytes YIELD, if anything. 0031's ternary extract routing, now typed: identity
// NEVER; enrichment MOSTLY (typed fields, dimensions, provenance); discard ONLY by this closed
// declared catalogue, FAIL-SAFE-KEEP — an unrecognized residual falls to RawText rather than being
// dropped.
//
// `StreamLabel` from ADR-23's sketch is not shipped: no transform produces one today (same
// rule as the kinds above, ratified by ADR-2.1). It APPENDS if it ever lands.
enum class TransportExtract : std::uint8_t
{
    None = 0, ///< the peel yields nothing but the shortened line
    // ⚠ An OBSERVATION time, NEVER an ordering key (ADR-23, and this is NORMATIVE). Because
    // only TOTAL-scope transforms are transport, a whole-stream stamp necessarily covers lines
    // written by DIFFERENT clocks — measured on Jenkins: the controller-stamped annotations are
    // strictly monotone (0 inversions on every one of 12 logs) while the agent-stamped payload
    // carries 7–701 inversions per log. One declared transform, one prefix form, TWO timelines.
    // So this value MAY enrich (typed field, dimension, provenance); it may NEVER re-order
    // anything, may NEVER be asserted monotone, and may NEVER be a replay input (root CLAUDE.md
    // § Determinism & Replay — no wall-clock dependence in replay logic).
    EventObservationTime,
};

// One declared transform: a NAME (what a declaration references), the algorithm it selects, the
// algorithm's parameters, and what it extracts. POD, constexpr-constructible, canonically
// serializable — the same discipline as the semantic grammar rows.
//
// Not every parameter is read by every kind (the `LocationRow` precedent): `prefix_width` is
// `LinePrefixTimestamp`'s alone — `LinePrefixBracketedTimestamp` is VARIABLE width and its
// acceptor computes the width from the shared grammar, so its row leaves the field 0 and unread.
struct TransportTransformRow
{
    std::string_view name; ///< the declaration's reference key; unique within the catalogue
    TransportTransformKind kind;
    TransportExtract extract;
    // LinePrefixTimestamp: the fixed byte width of the stamp at line head.
    std::uint32_t prefix_width;
    // LinePrefixTimestamp: after removing the stamp, also drop the separator space and any
    // delivery-layer indentation. Load-bearing, not cosmetic — ADR-23 names this as one of the
    // bundled behaviors a "conceptual" peel silently drops, which is why G1-PEEL must MEASURE
    // content-neutrality rather than assume it.
    bool strip_leading_space;
};

// A per-line RFC 3339 prefix — `YYYY-MM-DDTHH:MM:SS.fffffffZ` + a separator space.
// TOTAL scope (every line the serving API stamps carries it), so it is admissible transport under
// ADR-23, and it is the one transform that has BOTH a corpus (22 030 real logs) and an
// INDEPENDENT oracle to score against — which is what makes it shippable where Timestamper is not.
// That oracle was `GitHubActionsStrategy`'s peel; T4 deleted the detection, and the oracle is now
// FROZEN inside G1-PEEL itself (ADR-8), where it still scores this row over 22 490 937 lines.
//
// THE NAME IS DELIVERY-SHAPED, NOT ECOSYSTEM-SHAPED, and that is load-bearing twice over. ADR-23
// §3 is the reason the row lives here at all: the catalogue is orthogonal to the dialect packages
// because this prefix is a property of the *delivery*, not of the GHA *dialect* — so naming the row
// after an ecosystem would contradict the argument that placed it in core. ADR-17 (SRC-SP-1)
// independently forbids an ecosystem literal in the core mechanism, and the two together leave
// exactly one coherent name: the byte grammar it peels. Any serving API that stamps this shape
// declares this same row. Provenance stays in prose, where a reference belongs; the identifier
// below still records where the 28-byte width was measured.
inline constexpr std::uint32_t kGhaApiPrefixWidth{28U}; // "YYYY-MM-DDTHH:MM:SS.fffffffZ"

inline constexpr std::array<TransportTransformRow, 3> kTransportCatalogRows{{
    {.name = "api-rfc3339-line-prefix",
     .kind = TransportTransformKind::LinePrefixTimestamp,
     .extract = TransportExtract::EventObservationTime,
     .prefix_width = kGhaApiPrefixWidth,
     .strip_leading_space = true},
    // The bracketed variable-width form (T5 §2.1). Delivery-shaped name, same argument as the row
    // above; Jenkins-Timestamper provenance lives HERE in prose, never in the identifier. The
    // greedy `[ \t]+` strip reproduces the shipped strategy's bundled behavior #3 byte-exactly
    // because ADR-23 verdict (a) requires items 2–4 reproduced — NOT because the strip's merit
    // is settled: that question stays PARKED, measurement-gated (flaws.md Eqya·9), and this row
    // does not move shipped canonicalization as a second change in its pass. `prefix_width` unread
    // (variable width — the acceptor computes it from the shared grammar). The shipped strictness
    // carve-outs — Proxifier `[10.20.30.40]`, ApacheError `[Mon Oct …]`, bare `[12:34:56]`,
    // `[Pipeline]`, `[v1.2.3]` — fail the shared grammar by construction (one owner, both
    // consumers). The blank decline (bundled #4) is already expressed catalogue-side as
    // `PeeledLine::is_blank` ⇒ DROP.
    {.name = "bracket-rfc3339-line-prefix",
     .kind = TransportTransformKind::LinePrefixBracketedTimestamp,
     .extract = TransportExtract::EventObservationTime,
     .prefix_width = 0U,
     .strip_leading_space = true},
    // The UTF-8 BOM at line head (DN-25). Delivery-shaped name, same argument as the two rows
    // above: a BOM is a property of how the bytes were DELIVERED, never of an ecosystem, so the
    // row is named for the byte grammar it peels. It is the first row in this catalogue that
    // extracts NOTHING — a BOM carries no datum, only noise, so `extract = None` and the peel is
    // pure removal. `prefix_width` unread (the three bytes ARE the grammar, and carrying the
    // width as a parameter is what would let a row declare 2 and silently eat a UTF-16 BOM's
    // first two bytes); `strip_leading_space` FALSE, because `<BOM><space>` has a real content
    // space and the GHA row's true is what makes that distinction load-bearing rather than
    // cosmetic.
    {.name = "utf8-bom-line-prefix",
     .kind = TransportTransformKind::LinePrefixByteOrderMark,
     .extract = TransportExtract::None,
     .prefix_width = 0U,
     .strip_leading_space = false},
}};

// Look a declared name up in the catalogue. Returns nullptr when unknown — the caller decides
// whether that is a hard error (canon, at declaration resolution) or a query.
[[nodiscard]] constexpr const TransportTransformRow* find_transform(std::string_view name) noexcept
{
    for (const TransportTransformRow& row : kTransportCatalogRows)
        if (row.name == name)
            return &row;
    return nullptr;
}

// ── The WRITER dual of the catalogue (T5 §2.3) — one owner, no third spelling ──────────────────
//
// render_transport_prefix appends the row's line prefix (stamp + the single separator space) to
// `out` and returns true, or returns false for a row whose kind has NO writer dual. The killed
// third spelling is the point (bibles/jenkins_dialect.md §4 item 3: spike / strategy / masker →
// one grammar, one owner): a LogCraft-side bracket renderer would be that spelling returning
// through the writer door. Canon owns every transform ALGORITHM; LogCraft supplies the stamp
// value and the plumbing, exactly as it supplies payloads to render_row.
//
// * `LinePrefixBracketedTimestamp` renders ONE fixed lexical form — the corpus-attested
//   millisecond-`Z` spelling `[YYYY-MM-DDTHH:MM:SS.mmmZ]` + one space. Integer/manual formatting
//   only (no iostream, no locale, no strftime — determinism MUST M8), and ALLOCATION-FREE beyond
//   the caller's buffer: the trailer of a declared wrap is stamped inside the writer's `noexcept`
//   end-of-stream path (§3.1's high-water-mark rule), so this function may not allocate on its
//   own account — a stack scratch is filled and appended in one call. NORMATIVE, not an accident.
// * `LinePrefixTimestamp` has NO writer dual, deliberately (returns false): the GHA API stamp is
//   the PLATFORM's, baked into the GHA IntentFormat's own writer (T5 §3.3 — a writer-side ± knob
//   there would generate only our own ablation and move shipped SRC-SID-3 bytes for nothing). A
//   declared output wrap naming that row is rejected at declaration time, loudly, via this exact
//   predicate.
//
// Conformance laws (asserted in tests, stated here as the contract):
//   * `parse_iso8601(<the rendered interior>) == floor<seconds>(stamp)` — the shipped parser
//     reads whole seconds (fraction skipped by design), so the round-trip law holds at the
//     parser's grain; the rendered millisecond digits are covered by the byte-exact peel law:
//   * `peel_raw(render ∥ ℓ).content == strip_ws(ℓ)` and the extracted observation time equals
//     `floor<seconds>(stamp)` — the peel is the renderer's inverse at the declared strip's
//     boundary (T5 §2.3's round-trip law, stated at the honest `strip_ws` boundary).
//
// Returns false also for a stamp outside the four-digit-year window the fixed form can spell
// (0000-01-01 … 9999-12-31) — a caller-contract violation surfaced as "not renderable", never a
// silently wrong prefix. Deliberately NOT noexcept: `out.append` may grow the caller's buffer,
// and an allocating function must not wear the keyword (the escape-NOLINT that would preserve it
// defeats the very tripwire it decorates) — "allocation-free" is the function's own account: a
// caller that reserves once appends forever without a further allocation. Defined in
// transport.cpp.
[[nodiscard]] bool render_transport_prefix(const TransportTransformRow& row,
                                           insight::Timestamp stamp, std::string& out);

// ════════════════════════════════════════════════════════════════════════════════════════════════
// §6 — the per-run, per-stream DECLARATION. Generalizes `IntentChannel` rather than sitting beside
// it: the channel is the degenerate one-field case of this.
// ════════════════════════════════════════════════════════════════════════════════════════════════

// FAIL-CLOSED BY DEFAULT is a MUST (ADR-22, unchanged). A default-constructed declaration —
// empty stack, empty dialect, empty channel — is exactly today's behavior: no peel, no dialect
// verification, the Unspecified channel view. That is the G1 case, and it is why declaring is
// purely SUBTRACTIVE: a caller who says nothing loses nothing they had.
//
// canon VERIFIES, never infers (ADR-22's split, not reopened): every named transform must be in
// the catalogue and every named dialect must be composed, or resolution is a HARD ERROR listing the
// known names. ACQUISITION may infer (0030's CLI peek is deterministic given the bytes); canon may
// not. An unknown name is a MISTAKE and fails closed; an ABSENT name is a CHOICE and degrades —
// they must never share a code path.
//
// It is PROVENANCE, NOT IDENTITY (0031's hash split, and the whole quotient): this per-run
// declaration goes to MetaLog; the transform GRAMMAR (the catalogue above) goes to
// `semantic_identity`. Two runs ±a transform MUST carry the same `semantic_identity`, or
// transport-invariance is not being built.
struct IngestDeclaration
{
    // ORDERED, outside-in — the order the delivery layers were applied, so the peel unwinds them
    // in declaration order. Empty = the degenerate case.
    std::span<const std::string_view> stack;
    // The declared dialect (a composed package name). VERIFIED and GATING since T4 (ADR-22):
    // `resolve_stream` checks it against the composed packages, then filters every dialect-gated
    // row into the stream's view — so no walker below ever sees a dialect coordinate, and which
    // declared rows fire stopped being a function of the stream's content. An unknown name is a
    // named error rather than a silently structure-less analysis, the same posture ADR-22
    // gives an unknown channel; an ABSENT one withholds every concretely-gated row.
    std::string_view dialect;
    // Today's IntentChannel (ADR-22), unchanged in meaning. Verified and applied by
    // `ComposedSemantics::for_channel`.
    std::string_view channel;
};

// ════════════════════════════════════════════════════════════════════════════════════════════════
// §4 — the `transport_context` boundary. The tokenizer NEVER learns the stack existed.
// ════════════════════════════════════════════════════════════════════════════════════════════════

// What one line's DECLARED peel yielded on the RECOGNITION path. `content` carries the ingest
// precondition as a TYPE (ADR-21.D3): this peel takes a
// `NormalizedLine`, so holding a PeeledLine is proof that stage 1 ran and the declared stage 2
// followed — the currency the content walkers accept.
struct PeeledLine
{
    // The line with every declared transform unwound. Borrows from the NormalizedLine's storage —
    // the peel only ever SHORTENS, never rewrites, so no allocation and no arena are involved.
    insight::tokenization::NormalizedContent content;
    // The observation time a `LinePrefixTimestamp` extracted, if the stack declared one and the
    // line actually carried a parseable stamp. ⚠ Read `TransportExtract::EventObservationTime`
    // before using it: enrichment only, never an ordering key, never a replay input.
    std::optional<insight::Timestamp> observation_time;

    // A line whose entire content was transport (a bare stamp with nothing after it) peels to
    // EMPTY, and empty means DROP — not an empty template. This is how the shipped GHA strategy's
    // "timestamp-only line is a blank line: decline it" behavior survives the move to a declared
    // peel; ADR-23 lists that decline as one of the bundled behaviors content-neutrality
    // depends on, so it is expressed here rather than lost.
    [[nodiscard]] constexpr bool is_blank() const noexcept
    {
        return content.bytes().empty();
    }
};

// What one line's peel yielded on the TOKENIZER-FEEDING path (`peel_raw`). `content` is a plain
// view of the caller's RAW bytes with the declared transforms unwound — deliberately NOT a
// `NormalizedContent`, because no stage 1 has run and this struct must not pretend one has. It
// cannot reach a content walker (the type forbids it); what it CAN do is feed
// `Tokenizer::process_line`, which performs stage 1 itself and reads the raw bytes beside it
// (SRC-D-PROV-1: the GHA command-echo SGR wrapper survives ONLY there — pre-normalizing this path
// is the §5.4 trap and would silently kill the echoed-source demotion).
struct RawPeeledLine
{
    std::string_view content;
    std::optional<insight::Timestamp> observation_time;

    [[nodiscard]] constexpr bool is_blank() const noexcept
    {
        return content.empty();
    }
};

// The resolved stack — built ONCE per stream, from the declaration, BEFORE the first line
// (ADR-23: peeling is stream-scoped). Cheap to hold: a handful of pointers to
// catalogue-static rows.
//
// NORMATIVE, and the reason this type exists at all: LINE IDENTITY IS A PURE FUNCTION OF PEELED
// CONTENT. SRC-SID-1/SRC-II-1 is preserved BY CONSTRUCTION, not by review — `peel()` hands back a
// `string_view` and the tokenizer takes a `string_view`, so there is NO parameter anywhere on the
// identity path through which a declaration could reach an identity. No future edit can make an
// identity depend on a declaration without first adding such a parameter, which is a visible,
// reviewable change rather than a silent one. That is strictly stronger than asserting the property
// in a test, and it is why the stack is never handed to the Tokenizer.
class TransportStack
{
  public:
    TransportStack() = default; ///< the degenerate stack: `peel` is the identity function (G1)

    // Build from resolved catalogue rows, ordered outside-in. Prefer `resolve()` below, which
    // verifies names; this exists so a caller holding rows can construct directly.
    explicit TransportStack(std::vector<const TransportTransformRow*> ordered_rows) noexcept
        : rows_{std::move(ordered_rows)}
    {
    }

    // Unwind every declared transform, outside-in. Pure, allocation-free, deterministic, and
    // `noexcept`: a byte function over a borrowed view.
    //
    // TOTALITY IS ABOUT APPLICATION, NOT EFFECT (ADR-23 — the bright line against re-admitting
    // detection). Every declared transform is applied to every line, unconditionally. Its EFFECT on
    // a given line may be the identity: a `LinePrefixTimestamp` whose declared stamp shape is not
    // present in these bytes removes nothing. That is the rule's effect being nothing — it is NOT
    // the transform asking "is this line mine?", which is the per-line inference the declaration
    // model exists to forbid.
    //
    // A WRONG DECLARATION STAYS WRONG, LOUDLY, AND THE DECLARER OWNS IT: declaring this transform
    // on a payload-stamped stream would also strip applicative log4j prefixes (0031's argument,
    // attested at 16 250 measured lines). Declaration moves RESPONSIBILITY to the party that owns
    // the knowledge; it does not make a wrong declaration harmless.
    //
    // TWO DOORS, TWO PATHS, TWO RETURN TYPES — the never-in-place refusal made structural
    // (ADR-21.D2, scoped by ADR-21.D4):
    //   * `peel(const NormalizedLine&)` — the RECOGNITION path's DECLARED stage 2. Stage 1 first
    //     (the type carries the proof), then the catalogue rows; the result is the walkers'
    //     currency. The order is load-bearing: an escape sitting BEFORE the transport prefix is
    //     invisible to this peel unless the strip ran first.
    //   * `peel_raw(std::string_view)` — the TOKENIZER-FEEDING path. `process_line` performs
    //     stage 1 itself and MUST see the raw (ANSI-bearing) bytes beside it (SRC-D-PROV-1's
    //     echoed-source register survives nowhere else), so this path must NOT pre-normalize.
    //     Its result carries no stage-1 claim and cannot reach a walker — the types close what
    //     the old single string_view door left to convention.
    [[nodiscard]] PeeledLine peel(const insight::tokenization::NormalizedLine& line) const noexcept;
    [[nodiscard]] RawPeeledLine peel_raw(std::string_view line) const noexcept;

    [[nodiscard]] bool empty() const noexcept
    {
        return rows_.empty();
    }
    [[nodiscard]] std::size_t size() const noexcept
    {
        return rows_.size();
    }

  private:
    // Points at `kTransportCatalogRows` static storage — the rows outlive every stack.
    std::vector<const TransportTransformRow*> rows_;
};

// Resolve a declaration's `stack` against the catalogue. Every name must be known; an unknown name
// is a HARD ERROR naming the catalogue's vocabulary, symmetric with ADR-22's treatment of an
// unknown `--channel` (canon fatals with a legible message rather than degrading a typo). An EMPTY
// stack resolves to the degenerate `TransportStack`, which is not an error — it is the G1 case.
[[nodiscard]] TransportStack resolve_transport_stack(const IngestDeclaration& declaration);

} // namespace insight::transport
