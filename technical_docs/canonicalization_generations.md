# The canonicalization generation ledger — every value `kCanonicalizationVersion` has held

`kCanonicalizationVersion` (declared in `core/api/canon.api.cppm`, the code `SRC-D-TID-16` names)
is the single canon-owned identifier of the canonicalization **contract**: the masking rules that turn a raw line into its
`template_str`, plus every classification rule whose output is serialized. Every MetaLog producer
defaults to it, so a rules change is one edit at that declaration and impossible to skip — bump it
and old/new metalogs become incomparable at the specification's §2.4 gate — re-derive, never
migrate (`SRC-D-TID-9`).

It names the **rules generation, not the package version**. The two are decoupled: a patch release
that does not touch the rules must not change it, and a rules change inside an unreleased window
does not wait for a release to take it.

*Decisions: [ADR-2.D5 / ADR-2.D9](../../technical_docs/adr/002-release-model-and-version-tokens.md)
— what a generation token names, how a comparability event is priced, and the tombstone
discipline. This file is the ledger those slots refer to; it carries the record, never the rule.*

---

## How to read this file

One section per generation, in mint order, append-only. A section states **what changed**, **which
serialized fields move**, and **why the bump was owed** — because those are the three things a
consumer crossing the generation needs and the code cannot say.

Two recurring classes are named rather than re-argued at each entry:

* **The identity class** (`-4`, `-8`, `-11`, `-12`, `-13`, and rider 2 of `-15`) — the masker
  itself moved, so `template_str` and `template_id` move.
* **The classification class** (`-7`, `-9`, `-14`'s level half, `-15`'s rider 1) — the masker is
  untouched and identity does not move, but a **serialized** field does (`dominant_level` gates
  NewErrorPattern and diff polarity; `component` is the cube's WHERE axis). That is still
  output-affecting, and it is the state a reader most easily mistakes for *nothing moved*.

Every entry from `-11` onward closes with the same standing statement, so it is made once here: a
generation bump is a **content re-base** under ADR-31, never a determinism regression. Two runs of
one generation over the same bytes stay bit-identical; old and new documents are incomparable at
the §2.4 gate by construction.

---

## `-1` — the stateless masker

The stateless per-line masker plus the F13 class set. The first generation.

## `-2` — OTEL awareness

Severity-from-`severity_number`, trace-context routing, and the trace-scoped graph
(ADR-29 `D-OTEL-2`, unconditional).

## `-3` — currency-marker numerics

`SRC-D-TID-22` — `$463` / `total=$463` mask to `$<*>` / `total=$<*>`.

## `-4` — the 1.6.4 masking batch

Three masking rules in one generation:

* `SRC-D-MSK-1` — generalized **diagnostic-composite** masking — a per-`:` / per-`/`-segment digit-leading rule
  that collapses the Chromium/glog prefix `[PID:DATE/TIME:LEVEL:file.cc:line]` and subsumes the
  source-location rule;
* `SRC-D-MSK-2` — the **ephemeral-root path catalog** (`/tmp/…` → `/tmp/<*>`);
* `SRC-D-MSK-3` — JSON **nested-`fields` component/level descent**, a cube-axis change folded
  into the same bump.

Content changes ONLY for inputs carrying a diagnostic-composite or ephemeral-root token, or a
nested-`fields` JSON record; every other document is byte-identical except this version string.

## `-5` — `D-OTEL-15`

Landed at `4e46af0`.

## `-6` — the ephemeral-root batch

`SRC-D-MSK-4` — canon ephemeral-root masking plus the lexicon-context precision fix (`9c5db20`).

## `-7` — the NOTE register

`SRC-D-NOTE-1`. A failure word inside a compiler note's message (`<path>:<line>:<col>: note: … failed:`) no longer
confers a failure verdict, so the serialized `dominant_level` of a gcc/clang cascade's note lines
moves Error → Unknown.

`template_str` and `template_id` do NOT move — the masker is untouched — but `dominant_level` is
serialized and gates NewErrorPattern and diff polarity, so it is an output-affecting
canonicalization change and takes the bump. **This is the entry that established the
classification class.**

## `-8` — the bracket timestamp

*(`bibles/jenkins_dialect.md` §4; ADR-23 erratum 2 — "the bracket is the entire difference".)*

`SRC-D-MSK-5` — a WHOLE-token bracketed RFC3339 full datetime (`[2026-06-23T15:11:09.020Z]`) masks to `[<*>]`
instead of falling through to literal KEEP. `template_str` / `template_id` move ONLY for lines
carrying that token class; every other document is byte-identical except this version string.

## `-9` — the compound-key shapes

`SRC-D-ECS-1`. A top-level key is resolved to its LAST SEGMENT (`log.level` → `level`) and an object value is
descended EXACTLY ONE level (`"log":{"level":…}`), each resolved name matched against canon's
existing four role vocabularies. **ZERO field names are added** — the grammar learns two shapes,
never a vendor's spelling.

Same class as `-7`, and it takes the bump for the same stated reason: the masker is untouched, so
`template_str` / `template_id` do not move, but a producer that namespaces its fields (ECS, pino,
Serilog, Bunyan, GELF) now yields `level` and `component` where it previously yielded none — and
both are serialized. A stream whose fields were already canon-named is byte-identical except this
string: the compound pass runs only when a role is still MISSING, and it can add a role, never
move one.

## `-10` — RETIRED, never reused

Minted for the opaque-identity mask (`c70ee8d`; the shape axis is closed for that class,
ADR-16.D6) and reverted before any tag. **No document was ever produced under it**, so the revert
restores `-9` rather than minting a false incomparability.

The number is **BURNT**. Append-only means a generation is never re-bound to different rules, so
the next bump was `-11`. This is the standing instance ADR-2.D9's tombstone clause names.

## `-11` — the wrapper-shell repair

`SRC-D-MSK-6`. `kWrapperPairs` (`canon.detail.scan`) declares the six byte pairs a producer wraps a whole token
in — `[]` `()` `{}` `<>` `""` `''` — and closes a **grammar defect, not a missing rule**: rule 4
already tolerated a shell (`\[?…\]?`) but only for the ONE pair the first corpus showed, so
`(163.27.187.39)` failed at byte 0, was not digit-leading, carried no byte in the composite
pre-gate's separator set, and fell to literal KEEP. Six template rows of the published render
`coderoast-hub/showcase/canon/loghub.canon.txt` carry a real third-party address for exactly that
reason.

Two touch points, both reading the one table:

1. `is_ipv4_token` accepts any declared opener and up to two trailing shell/sentence bytes — a
   **strict superset** of the retired grammar, so nothing that masked before stops masking;
2. `TokenShape::has_separator` gains the wrapper bytes, so a hex run ≥ 16 wrapped in `(` `{` `<`
   `"` `'` reaches `embedded_identity` and yields the `(<*>)` normal form the bracketed and UUID
   forms ALREADY produced — this restores one normal form per class rather than minting a second.

`template_str` / `template_id` move ONLY for lines carrying a shell-wrapped IPv4 or a shell-wrapped
long hash.

**Measured** on 32 000 lines of real third-party logs across 16 producers
(`coderoast-hub/samples/loghub/samples`, the `f13_cardinality_measure` instrument): distinct
templates 6 712 → 6 709, singletons 5 238 → 5 233. Three templates move out of 6 712 — the leaked
instances collapsing into their class, which is the shape a leak repair is supposed to have.

The bump is taken because the masker itself moved: the identity class, not the classification one.

## `-12` — the claim-and-projection batch

*(DN-43.)* Four changes, one generation, because the token is spent ONCE at a cut head (ADR-16.D2)
and every one of them is output-affecting:

1. **`SyslogStrategy` claims a line only on the syslog HEADER** (`TIMESTAMP HOST TAG:`), never on a
   timestamp prefix, and its tag search is bounded to ONE token — so a line whose remainder carried
   no `[` or `:` no longer moves its whole message body into `component` and leaves `content`
   empty. That empty content is what produced a `template_id` equal to the SHA-256 prefix of the
   EMPTY STRING, published as an ordinary identity.
2. **The leading-RFC-3339 LAYOUT gets its own core strategy** (`LogFormat::Rfc3339Text`): the stamp
   is the timestamp, the whole remainder is content, and `component` is empty.
3. **Both syslog branches now INFER the level from the message body**
   (`infer_leading_log_level`, `EventLevel::inferred`) instead of assigning `EventLevel{}`
   unconditionally — the level was never read at all on either arm. A strict refinement:
   `EventLevel{}` and `inferred(Unknown)` compare equal, so a line can only move from NO level to
   SOME level, and `apply_level_lift` still outranks the inference, so a declared marker keeps
   precedence.
4. **CLF's client IP moves from `component` to `host`**, and `component` becomes EMPTY — the field
   contract already ruled it (`component` = the low-card FUNCTIONAL SOURCE, never the node
   identity), and the same octets were being masked in `content` and left unmasked on the cube's
   WHERE axis.

`template_str` / `template_id`, `dominant_level` and `component` all move, so this is
output-affecting three times over.

## `-13` — the compact UTC instant masking arm

`normalize_embedded_identity` gains a third alphabet arm recognising an ISO-8601 **extended** date
plus **basic** (colon-free) time plus a mandatory `Z` — exactly 18 bytes, delimiter-bounded on both
sides — so an embedded instant masks to `<*>` instead of entering template identity verbatim.

The bump is owed because the **rule's own acceptance set widens**; it is not a `kCompositeRules`
add / reorder / remove, which is the same clause reached by a different limb.

**Measured** at the v1.10.2 cut over a 1 000-pair GitHub Actions bench: 17 hand-read false alerts →
1, 6/6 true incidents kept, 0 rows added anywhere, HIGH+CRITICAL 104 → 88.

The arm is admissible where the reverted `is_opaque_identity` (`-10`, burnt) was not, and the
difference is **structural**: a CLOSED grammar pinned by literal bytes at fixed offsets, every
member of whose acceptance set is an INSTANCE value by ISO-8601's own semantics — no stable name
can inhabit it.

`template_str` / `template_id` move for lines carrying an embedded compact instant, and `params`
moves for a line whose WHOLE token is one: the token becomes a normalized literal rather than a
masked parameter, so it stops feeding metalog's param histograms while `template_str` stays `<*>`.

**The residue is declared, not pending**: a GitHub run id tailing an artifact name
(`playwright-frontend-coverage-<run id>`) is byte-isomorphic to stable names and gets NO masking
fix.

## `-14` — BGL's alert-label column, claimed instead of rejected

`BGLStrategy`'s grammar gains LogHub's leading alert-class column — `-` on a normal record,
`[A-Z][A-Z0-9_]{0,15}` on an anomalous one — which the predicate VALIDATES and every projection
field DROPS, because the column is the corpus curators' answer key rather than producer content
(DN-43.D14).

The bump is owed because the **rule's own acceptance set widens**: 348 460 of the pinned `BGL.log`'s
4 747 963 lines and 226 095 of `Thunderbird_5M.log`'s 5 000 000 move from a whole-line raw-text
template to a parsed BGL record, and on BGL they carry a DECLARED fatal-class level (`FATAL`
348 398, `FAILURE` 62) that nothing was reading. A labelled line now projects IDENTICALLY to its
`-` twin, so `template_str` / `template_id` move for those lines and one event class stops being
splittable 42 ways by curation label.

Three narrower moves ride the same generation, all in the same strategy:

* a second BGL header shape (306 lines with no repeated node) parses instead of yielding `level`
  Unknown and `component` = `FATAL`;
* the Thunderbird branch takes DN-43.D3's one-token tag bound, so 405 401 lines stop having a
  message fragment cut onto `component` and 1 309 of those stop projecting to empty content
  entirely;
* 10 BGL lines whose `<node2>` field holds a spliced message fragment now DECLINE to raw text
  rather than publishing a mis-aligned parse.

`template_str` / `template_id`, `dominant_level` and `component` all move, so this is
output-affecting three times over.

## `-15` — the recognized location establishes its START

This generation is spent on a **classification** change with the masker untouched — the `-7`/`-9`
class, not the `-4`/`-8` one.

`recognize_location` (`core/src/compose/semantic_walkers.cpp`) fixed a match's END, where every
`LocationMatchKind` family already applies a word-boundary test, and then sliced the token from
offset 0 — so the location's START was never established, and a producer annotation glued to a path
with no separator was published INSIDE the resolved WHERE
(`##[error]fs/rc/rcserver/rcserver_test.go`). A `loc_is_path` byte class walked backwards from the
match now establishes the start: the exact mirror of the boundary test at the other end, and
semantic-unaware (`SRC-SP-1`) — no dialect literal, no marker table.

`template_str` / `template_id` do NOT move for this rider (the masker is untouched), and neither
does any `dedup_id_of`, which hashes the class tag and the template ids only. The serialized
`component` DOES move, on the `MaskConfig::recognize_test_where` path.

**Measured** over the 4 082-file GitHub revert corpus (2.34 GB, 694 484 lines resolving a
location): 738 resolved lines change label, distinct labels 12 299 → 12 265, and resolution
coverage is EXACTLY invariant — the walk moves a slice's start, never whether a token matches.

### The flag's default does not exempt the generation

`recognize_test_where` is default OFF, so every default-path document is byte-identical across
`-14` and `-15` except this string — but the **rules function** differs for a flag-ON producer, and
a generation naming two different serialized `component` vocabularies is the defect `-9` was minted
for: masker untouched, identity unmoved, a serialized field moved, therefore output-affecting.

### The second spend inside one open cut

Ruled by the Founder on 2026-09-03. `-14` landed after v1.10.3 and is in no tag, so ADR-16.D2's
land-once-at-a-cut-head and ADR-2.D9's batch-or-do-not-spend would otherwise have had this change
RIDE `-14`. What the second spend buys is **honesty inside the unreleased window**, not
compatibility: v1.10.3 ships `-13`, so a consumer crossing the next cut pays exactly ONE
comparability event whether that cut carries `-14` or `-15`. What it costs is re-blessing
`insight-metalog`'s three committed vector files, whose only moved bytes are this string.

### Rider 2 — HealthApp recall, and it DOES move `template_id`

`is_health_app_prefix` demanded a 2-digit minute and 3-digit milliseconds; LogHub's own
`HealthApp_2k.log` is not zero-padded anywhere, so 247 of its 2 000 lines (12.35 %) were declined
to RawText, where the whole line — timestamp and process id included — became the template. The
predicate now accepts 1–2 digits per clock field and 1–3 millisecond digits, and
`parse_health_app_ts` widens its minute to match (DN-43.O5; DN-43.D16 for the arity half).

**Measured** over that corpus through the public `Tokenizer::process_line` at a zero-package
composition: routing 1 753 HealthApp / 247 RawText → 2 000 HealthApp / 0 RawText; sum of
`template_str` bytes 68 699 → 70 555; sum of `component` bytes 20 811 → 23 623; lines with a typed
event time 1 813 → 2 000; empty projections 0 → 0; declines 0 → 0. So 247 lines change
`template_str`, `template_id`, `component` and `format`, and every one of them was being published
as an unmasked whole-line literal and now is not.

**It rides `-15` rather than spending `-16`**, which is ADR-2.D9's batch-or-do-not-spend and
ADR-16.D2's land-once-at-a-cut-head applied unchanged: `-15` landed after v1.10.3 and
`git tag --contains a1eee2e` is empty, so this repair lands inside the SAME unreleased window the
entry above occupies. A consumer crossing the next cut pays exactly ONE comparability event
whichever token that cut carries — v1.10.3 ships `-13` — so a third token buys nothing and costs a
second re-base of every committed vector. The Founder's 2026-09-03 ruling authorised a second spend
for the location-recognition change on an honesty argument that **does not reach here**: that
change moved a serialized field while leaving identity untouched, which is exactly the state a
reader can mistake for *nothing moved*. This one moves `template_id` itself, so it is already
legible in the identity a consumer compares.

**Why no vector re-base is owed.** Swept 2026-09-03 across every repo: no committed golden, fixture
or vector outside `insight-canon`'s own tests carries a HealthApp-format line, `insight-metalog`'s
three vector files included. The one PUBLISHED artifact that does is
`coderoast-hub/showcase/canon/loghub.canon.txt`, which renders this corpus in four transport
declarations; it is a release-cut surface and is deliberately not re-rendered there.

---

*See also: [masking.md](masking.md) (what the current generation's rules actually are) ·
[determinism.md](determinism.md) (what the `canonicalization_version` gate protects).*
