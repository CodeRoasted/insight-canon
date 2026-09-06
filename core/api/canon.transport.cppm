// refs: ADR-23, ADR-23.D3
// invariant: the TRANSPORT VOCABULARY: the declared, stream-scoped peel that runs OUTSIDE-IN of
// everything else — transport, then log format, then intent.
// invariant: public and installed; the facade `export import`s it, so `import insight.canon;`
// yields the declaration types alongside the Tokenizer.
// invariant: the catalogue does NOT live in a manifest: the ratified model is a product of
// transport, format and dialect markers, and a delivery stamp is not a dialect's property.
// note: filing transform rows in a manifest would re-couple the two factors the model separates
// invariant: so the catalogue is canon's own and orthogonal to the packages, and canon owns every
// transform ALGORITHM exactly as it owns every matcher algorithm.
// invariant: it is CANON-SHIPPED and CLOSED, not package-extensible — a core vocabulary like the
// ordinal and OTEL field catalogs, never dialect data.
// invariant: its version and rows enter EVERY composed `semantic_identity`: the transform GRAMMAR
// is identity, while the per-run declaration is provenance and goes to MetaLog instead.
module;

export module insight.canon.transport;
import insight.canon.internal;
import insight.canon.api;

export namespace insight::transport
{

// refs: ADR-2.D5
// invariant: the catalogue's version is a component of the composed identity, exactly as the
// semantic grammar version is: two transport vocabularies are not comparable.
// invariant: THE BUMP RULE: bump on a change to what the catalogue SERIALIZES, plus any change to
// its serialization SHAPE even when the bytes happen to coincide.
// note: a shape change whose bytes do not move is the collision a content hash cannot catch
// invariant: a new KIND is NOT on the list: a kind is serialized only as a value ON a row, so an
// enum member with no row serializes zero bytes and changes no behaviour.
// invariant: a kind never lands without its row anyway, so "a new kind" and "a new row" always
// co-fire, and two criteria that always agree are one criterion.
// invariant: like every monotonic token here it is assigned AT SHIP and never reserved: the value
// means "the Nth shape", and which change causes the Nth shape is not knowable in advance.
inline constexpr std::string_view kTransportCatalogVersion{"transport-catalog-3"};

// refs: ADR-2.D7, ADR-23.D3, SRC-SID-2
// invariant: NORMATIVE — catalogue enum values are IDENTITY-BEARING: new members APPEND, a value
// is never renumbered and never inserted mid-enum.
// invariant: both enums below serialize as their `uint8_t` VALUE on every row, so inserting a
// member in the middle shifts the serialized byte of EVERY EXISTING ROW.
// note: the digest then moves for rows nobody touched, and it moves silently
// invariant: the explicit zero on each first member is the anchor; append after the last.
// invariant: the transform vocabulary is CLOSED and is rows-as-data, NEVER a callable: a callable
// is un-hashable and lets the declaration and the behaviour diverge silently.
// invariant: members grow in WITH their algorithm, their row and their gate, and an anticipated
// vocabulary is prose rather than an enum body.
// note: the argument is arithmetic: a member with no row serializes zero bytes
// invariant: it changes no error message either: a declaration is resolved by NAME against the
// rows, so an unused sibling kind would not improve the hard error.
enum class TransportTransformKind : std::uint8_t
{
    // invariant: a fixed-width timestamp stamped at the head of EVERY line by the delivery layer.
    LinePrefixTimestamp = 0,
    // refs: ADR-8, ADR-23.D6, ADR-23.O2, BIB:jenkins_dialect, DN-15
    // invariant: a BRACKETED strict-RFC3339 stamp at the head of every line of a declared stream,
    // VARIABLE width: bracket, the shared datetime grammar, bracket, then the strip.
    // invariant: that grammar has ONE owner — the shared datetime-length function — and both
    // the masking rule and this peel delegate their byte grammar to it.
    // invariant: the whole-stream scoping of the Jenkins Timestamper plugin is the attested
    // population, 12 of the 113 traces, and the scoping split is provenance.
    // invariant: peel-equivalence is the ONLY obligation this row carries: the invariance cell
    // stays empty because no world vehicle exists.
    LinePrefixBracketedTimestamp,
    // refs: DN-25
    // invariant: a UTF-8 byte-order mark at the head of a line: a FIXED three-byte prefix removed
    // ONCE — not a greedy loop and not a search anywhere in the line.
    // invariant: the mark is a delivery artifact of the stream's first bytes, so a second one is
    // content and is left alone.
    // invariant: `prefix_width` is unread here (the width IS the grammar) and `strip_leading_space`
    // is false, because a space after a mark is a real content byte.
    LinePrefixByteOrderMark,
};

// refs: ADR-2.D7, ADR-23.D3
// invariant: what the peeled bytes YIELD, if anything — the ternary extract routing: identity
// NEVER, enrichment MOSTLY, discard ONLY by this closed declared catalogue.
// invariant: it is FAIL-SAFE-KEEP: an unrecognized residual falls to raw text rather than being
// dropped.
// invariant: a stream-label extract is not shipped, since no transform produces one today; it
// APPENDS if one ever lands.
enum class TransportExtract : std::uint8_t
{
    // invariant: the peel yields nothing but the shortened line.
    None = 0,
    // refs: ADR-23.D5, STU-10
    // invariant: NORMATIVE: an OBSERVATION time, NEVER an ordering key. Only TOTAL-scope transforms
    // are transport, so a whole-stream stamp covers DIFFERENT clocks.
    // assert: measured on Jenkins — the controller-stamped annotations are strictly monotone, 0
    // inversions on every one of 12 logs, while the agent-stamped payload carries 7 to 701 per log.
    // invariant: one declared transform, one prefix form, TWO timelines: this value MAY enrich, and
    // may NEVER re-order anything, be asserted monotone, or be a replay input.
    EventObservationTime,
};

// invariant: ONE declared transform: a NAME a declaration references, the algorithm it selects,
// that algorithm's parameters, and what it extracts.
// invariant: POD, constexpr-constructible and canonically serializable — the same discipline as
// the semantic grammar rows.
// invariant: not every parameter is read by every kind: `prefix_width` is the fixed-width row's
// alone, and the bracketed row leaves it zero and unread.
struct TransportTransformRow
{
    // invariant: the declaration's reference key, unique within the catalogue.
    std::string_view name;
    TransportTransformKind kind;
    TransportExtract extract;
    // invariant: for the fixed-width kind, the byte width of the stamp at line head.
    std::uint32_t prefix_width;
    // refs: ADR-23
    // invariant: for the fixed-width kind, whether to drop the separator space and any
    // delivery-layer indentation after removing the stamp.
    // invariant: load-bearing and not cosmetic: it is one of the bundled behaviours a conceptual
    // peel silently drops, which is why the gate MEASURES neutrality.
    bool strip_leading_space;
};

// refs: ADR-8, ADR-17, ADR-23.D3, SRC-SP-1
// invariant: a per-line RFC 3339 prefix plus a separator space, 28 bytes wide.
// invariant: TOTAL scope — every line the serving API stamps carries it — so it is admissible
// transport, and it is the one transform with BOTH a corpus and an INDEPENDENT oracle.
// invariant: that oracle was a dialect strategy's peel; the detection was deleted and the oracle is
// now FROZEN inside the peel-equivalence gate, which still scores this row.
// invariant: THE NAME IS DELIVERY-SHAPED, NOT ECOSYSTEM-SHAPED: the catalogue is orthogonal to the
// packages precisely because this prefix is a property of the DELIVERY.
// invariant: naming the row after an ecosystem would contradict the argument that placed it in
// core, and the core mechanism independently forbids an ecosystem literal.
// note: the two together leave exactly one coherent name: the byte grammar it peels
inline constexpr std::uint32_t kGhaApiPrefixWidth{28U};

inline constexpr std::array<TransportTransformRow, 3> kTransportCatalogRows{{
    {.name = "api-rfc3339-line-prefix",
     .kind = TransportTransformKind::LinePrefixTimestamp,
     .extract = TransportExtract::EventObservationTime,
     .prefix_width = kGhaApiPrefixWidth,
     .strip_leading_space = true},
    // refs: ADR-23.D6, BIB:jenkins_dialect
    // invariant: the bracketed variable-width form, delivery-shaped name on the same argument as
    // the row above; the Timestamper provenance lives in prose, never in the identifier.
    // invariant: the greedy whitespace strip reproduces the deleted strategy's bundled behaviour
    // byte-exactly because peel-equivalence is the obligation this row owes.
    // note: the strip's merit stays parked and measurement-gated, and is not ruled here
    // invariant: `prefix_width` is unread — the width is variable and the acceptor computes it
    // from the shared grammar.
    // invariant: the shipped strictness carve-outs fail the shared grammar by construction; the
    // blank decline is expressed catalogue-side as a blank peel meaning DROP.
    {.name = "bracket-rfc3339-line-prefix",
     .kind = TransportTransformKind::LinePrefixBracketedTimestamp,
     .extract = TransportExtract::EventObservationTime,
     .prefix_width = 0U,
     .strip_leading_space = true},
    // refs: DN-25
    // invariant: the UTF-8 mark at line head, delivery-shaped name on the same argument as the two
    // rows above: a mark is a property of how the bytes were DELIVERED, never of an ecosystem.
    // invariant: it is the first row in this catalogue that extracts NOTHING — a mark carries no
    // datum, only noise — so the peel is pure removal.
    // invariant: `prefix_width` is unread because the three bytes ARE the grammar: carrying a width
    // is what would let a row declare two and eat half a UTF-16 mark.
    // invariant: `strip_leading_space` is FALSE, because a mark followed by a space has a real
    // content space, and the fixed-width row's true is what makes that distinction load-bearing.
    {.name = "utf8-bom-line-prefix",
     .kind = TransportTransformKind::LinePrefixByteOrderMark,
     .extract = TransportExtract::None,
     .prefix_width = 0U,
     .strip_leading_space = false},
}};

// refs: DN-69.D2
// invariant: the bytes ONE bracketed-timestamp row renders: the fixed lexical form plus its single
// separator space.
// invariant: published beside the catalogue because a WRITER cannot promise not to allocate without
// first SIZING its buffer, and the size is rows times this.
// note: one declared row fits in 32 bytes and two do not, so a round number is accidental
// invariant: a value one surface owns is never re-spelled in another.
inline constexpr std::size_t kBracketedTimestampPrefixBytes{27U};

// post: looks a declared name up in the catalogue and returns nullptr when it is unknown.
// invariant: the CALLER decides whether an unknown name is a hard error, as canon does at
// declaration resolution, or merely a query.
[[nodiscard]] constexpr const TransportTransformRow* find_transform(std::string_view name) noexcept
{
    for (const TransportTransformRow& row : kTransportCatalogRows)
        if (row.name == name)
            return &row;
    return nullptr;
}

// refs: BIB:determinism_model, BIB:jenkins_dialect, DN-69.D3, SRC-SID-3
// invariant: the WRITER dual of the catalogue, and there is no third spelling: canon owns every
// transform ALGORITHM while the caller supplies the stamp value and the plumbing.
// post: it appends the row's line prefix — stamp plus the single separator space — or answers
// with the no-render result for a row whose kind has NO writer dual.
// invariant: the bracketed row renders ONE fixed lexical form, the corpus-attested
// millisecond-and-Z spelling plus one space.
// invariant: integer and manual formatting only — no iostream, no locale, no strftime — because
// a locale-dependent rendering makes the same input produce different bytes.
// invariant: NORMATIVE and ALLOCATION-FREE: the trailer of a declared wrap is stamped inside a
// writer's noexcept end-of-stream path, and the caller-buffer signature DISCHARGES that.
// invariant: the fixed-width row has NO writer dual, deliberately: that stamp is the PLATFORM's,
// baked into its own writer, and a declared output wrap naming it is rejected at declaration.
// assert: the conformance laws are that parsing the rendered interior equals the stamp floored to
// seconds, the parser reading whole seconds by design.
// assert: and that peeling the rendered prefix off a line returns that line whitespace-stripped,
// with the extracted observation time equal to the same floored stamp.
// invariant: both forms answer no-render for a stamp outside the four-digit-year window the fixed
// form can spell — a caller-contract violation surfaced, never a silently wrong prefix.
// invariant: TWO SIGNATURES, ONE ALGORITHM: the span form IS the algorithm and the appending form
// wraps it, because a string signature makes the no-allocation obligation UNDISCHARGEABLE.
// note: the capacity is invisible to the callee and to a static analyzer, which charges the caller
// invariant: pre-reserving would make such a caller honest and leave the check red, which is the
// branch where a suppression becomes tempting; the second signature removes the temptation.
// post: the span form writes into memory the caller already owns, touches no string, and returns
// the bytes written or 0 for each of the three refusals.
// invariant: on 0 the buffer is UNTOUCHED — the bytes are composed in a stack scratch and copied
// once — so a partial prefix can never reach a document.
[[nodiscard]] std::size_t render_transport_prefix(const TransportTransformRow& row,
                                                  insight::Timestamp stamp,
                                                  std::span<char> out) noexcept;

// invariant: the appending form is convenience over the span form, for callers with no noexcept
// obligation.
// invariant: NOT noexcept, and the reason is local to this wrapper: appending may grow the caller's
// buffer, and an allocating function must not wear the keyword.
// note: the escape suppression that would preserve it defeats the very tripwire it decorates
[[nodiscard]] bool render_transport_prefix(const TransportTransformRow& row,
                                           insight::Timestamp stamp, std::string& out);

// refs: ADR-22, ADR-22.D4, ADR-22.D5, ADR-23
// invariant: the per-run, per-stream DECLARATION. It GENERALIZES the intent channel rather than
// sitting beside it: the channel is the degenerate one-field case of this.
// invariant: FAIL-CLOSED BY DEFAULT is a MUST. A default-constructed declaration — empty stack,
// empty dialect, empty channel — is exactly today's behaviour and is the degenerate case.
// invariant: declaring is purely SUBTRACTIVE: a caller who says nothing loses nothing they had,
// which is what makes a declaration safe to add to a working pipeline.
// invariant: canon VERIFIES, never infers: every named transform must be in the catalogue and every
// named dialect must be composed, or resolution is a HARD ERROR listing the known names.
// invariant: acquisition MAY infer, canon may not. An unknown name is a MISTAKE and fails closed;
// an ABSENT name is a CHOICE and degrades; they must never share a code path.
// invariant: it is PROVENANCE, NOT IDENTITY: this per-run declaration goes to MetaLog while the
// transform GRAMMAR goes to the composed identity.
// invariant: two runs differing by one declared transform MUST carry the same `semantic_identity`,
// or transport-invariance is not being built.
struct IngestDeclaration
{
    // invariant: ORDERED, outside-in — the order the delivery layers were applied, so the peel
    // unwinds them in declaration order. Empty is the degenerate case.
    std::span<const std::string_view> stack;
    // refs: ADR-22
    // invariant: the declared dialect is a composed package name, VERIFIED and GATING: stream
    // resolution checks it, then filters every dialect-gated row into the stream's view.
    // invariant: so no walker below ever sees a dialect coordinate, and which declared rows fire
    // stopped being a function of the stream's content.
    // invariant: an unknown name is a named error rather than a silently structure-less analysis;
    // an ABSENT one withholds every concretely-gated row.
    std::string_view dialect;
    // refs: ADR-22, ADR-22.D4
    // invariant: the declared intent channel, unchanged in meaning, verified and applied by the
    // same stream-resolution call as the dialect — there is no separate per-axis door.
    std::string_view channel;
};

// refs: ADR-21.D3, ADR-23.D4
// invariant: the `transport_context` boundary: the tokenizer NEVER learns the stack existed.
// invariant: what one line's DECLARED peel yielded on the RECOGNITION path; `content` carries the
// ingest precondition as a TYPE.
// invariant: this peel takes a normalized line, so holding one of these is proof that stage 1 ran
// and the declared stage 2 followed — the currency the content walkers accept.
struct PeeledLine
{
    // invariant: the line with every declared transform unwound. It BORROWS from the normalized
    // line's storage: the peel only ever SHORTENS, never rewrites, so no allocation and no arena.
    insight::tokenization::NormalizedContent content;
    // refs: ADR-23.D5
    // invariant: the observation time a fixed-width stamp extracted, present only when the stack
    // declared one and the line actually carried a parseable stamp.
    // invariant: enrichment only — never an ordering key and never a replay input.
    std::optional<insight::Timestamp> observation_time;

    // refs: ADR-23
    // post: a line whose entire content was transport — a bare stamp with nothing after it —
    // peels to EMPTY, and empty means DROP, not an empty template.
    // invariant: that is how the shipped strategy's timestamp-only-line decline survives the move
    // to a declared peel, and it is one of the bundled behaviours content-neutrality depends on.
    [[nodiscard]] constexpr bool is_blank() const noexcept
    {
        return content.bytes().empty();
    }
};

// refs: SRC-D-PROV-1
// invariant: what one line's peel yielded on the TOKENIZER-FEEDING path: a plain view of the
// caller's RAW bytes with the declared transforms unwound.
// invariant: deliberately NOT the normalized type, because no stage 1 has run and this struct must
// not pretend one has — so it cannot reach a content walker.
// invariant: what it CAN do is feed the raw tokenizer door, which performs stage 1 itself and reads
// the raw bytes beside it: the command-echo wrapper survives ONLY there.
// note: pre-normalizing this path would silently kill the echoed-source demotion
struct RawPeeledLine
{
    std::string_view content;
    std::optional<insight::Timestamp> observation_time;

    [[nodiscard]] constexpr bool is_blank() const noexcept
    {
        return content.empty();
    }
};

// refs: ADR-23, SRC-II-1, SRC-SID-1
// invariant: the resolved stack is built ONCE per stream, from the declaration, BEFORE the first
// line, and is cheap to hold: a handful of pointers to catalogue-static rows.
// invariant: NORMATIVE, and the reason this type exists: LINE IDENTITY IS A PURE FUNCTION OF PEELED
// CONTENT, preserved BY CONSTRUCTION rather than by review.
// invariant: the peel hands back a view and the tokenizer takes a view, so there is NO parameter
// anywhere on the identity path through which a declaration could reach an identity.
// invariant: no future edit can make an identity depend on a declaration without first adding such
// a parameter, which is a visible reviewable change rather than a silent one.
// note: that is stronger than asserting the property in a test
class TransportStack
{
  public:
    // invariant: the degenerate stack: the peel is the identity function.
    TransportStack() = default;

    // pre: the rows are resolved catalogue rows, ordered outside-in.
    // note: prefer the resolving free function below, which verifies names
    explicit TransportStack(std::vector<const TransportTransformRow*> ordered_rows) noexcept
        : rows_{std::move(ordered_rows)}
    {
    }

    // refs: ADR-21.D2, ADR-21.D4, ADR-23.D2, SRC-D-PROV-1
    // post: unwinds every declared transform, outside-in. Pure, allocation-free, deterministic and
    // noexcept: a byte function over a borrowed view.
    // invariant: TOTALITY IS ABOUT APPLICATION, NOT EFFECT. Every declared transform is applied to
    // every line, unconditionally, and its effect on a given line may be the identity.
    // invariant: that is the rule's effect being nothing; it is NOT the transform asking whether
    // the line is its own, which is the per-line inference the declaration model forbids.
    // invariant: A WRONG DECLARATION STAYS WRONG AND THE DECLARER OWNS IT — declaring a prefix
    // strip over applicative prefixes destroys real content, attested at 16 250 measured lines.
    // invariant: neither failure is ANNOUNCED, and that is not an oversight: no coordinate is
    // diagnosed at peel time, and the one loud path fires on an unknown transform NAME alone.
    // note: a clause promising that wrongness is loud is what licenses not looking
    // invariant: TWO DOORS, TWO PATHS, TWO RETURN TYPES — the never-in-place refusal made
    // structural.
    // invariant: the normalized door is the RECOGNITION path's DECLARED stage 2: stage 1 first, the
    // type carrying the proof, then the catalogue rows, and the result is the walkers' currency.
    // invariant: that order is load-bearing — an escape sitting BEFORE the transport prefix is
    // invisible to this peel unless the strip ran first.
    // invariant: the raw door is the TOKENIZER-FEEDING path and must NOT pre-normalize, because the
    // echoed-source register survives nowhere else; its result carries no stage-1 claim.
    // note: the types close what the old single-view door left to convention
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
    // invariant: the rows point at catalogue static storage, which outlives every stack.
    std::vector<const TransportTransformRow*> rows_;
};

// refs: ADR-22.D5, ADR-23
// post: resolves a declaration's stack against the catalogue; every name must be known, and an
// unknown one is a HARD ERROR naming the catalogue's vocabulary.
// invariant: that is symmetric with the treatment of an unknown channel — canon fatals with a
// legible message rather than degrading a typo.
// invariant: an EMPTY stack resolves to the degenerate stack, which is not an error: it is the
// degenerate case.
[[nodiscard]] TransportStack resolve_transport_stack(const IngestDeclaration& declaration);

} // namespace insight::transport
