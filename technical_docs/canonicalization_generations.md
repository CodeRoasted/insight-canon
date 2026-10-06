# The canonicalization generation ledger — every value `kCanonicalizationVersion` has held

`kCanonicalizationVersion` (declared in `core/api/canon.api.cppm`, `F-SRC-insight-canon:canon.api.cppm:kCanonicalizationVersion`)
is the single canon-owned identifier of the canonicalization **contract**: the masking rules that turn a raw line into its
`template_str`, plus every classification rule whose output is serialized. Every MetaLog producer
defaults to it, so a rules change is one edit at that declaration and impossible to skip — bump it
and old/new metalogs become incomparable at the specification's §2.4 gate — re-derive, never
migrate (`F-SRC-insight-canon:canon.api.cppm:TemplateId`).

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

* **The identity class** (`-4`, `-8`, `-11`, `-12`, `-13`, rider 2 of `-15`, and `-16`) — the masker
  itself moved, so `template_str` and `template_id` move.
* **The classification class** (`-7`, `-9`, `-14`'s level half, `-15`'s rider 1, `-16`'s level
  half) — the masker is
  untouched and identity does not move, but a **serialized** field does (`dominant_level` gates
  NewErrorPattern and diff polarity; `component` is the cube's WHERE axis). That is still
  output-affecting, and it is the state a reader most easily mistakes for *nothing moved*.

Every entry from `-11` onward closes with the same standing statement, so it is made once here: a
generation bump is a **content re-base** under ADR-31, never a determinism regression. Two runs of
one generation over the same bytes stay bit-identical; old and new documents are incomparable at
the §2.4 gate by construction.

---

## `-1` — the stateless masker

The stateless per-line masker plus the FLAW-13 class set. The first generation.

## `-2` — OTEL awareness

Severity-from-`severity_number`, trace-context routing, and the trace-scoped graph
(ADR-29 `D-OTEL-2`, unconditional).

## `-3` — currency-marker numerics

`F-SRC-insight-canon:mask.cpp:normalize_marker_number` — `$463` / `total=$463` mask to `$<*>` / `total=$<*>`.

## `-4` — the 1.6.4 masking batch

Three masking rules in one generation:

* `LSRC-11` — generalized **diagnostic-composite** masking — a per-`:` / per-`/`-segment digit-leading rule
  that collapses the Chromium/glog prefix `[PID:DATE/TIME:LEVEL:file.cc:line]` and subsumes the
  source-location rule;
* `F-SRC-insight-canon:mask.cpp:normalize_ephemeral_root` — the **ephemeral-root path catalog** (`/tmp/…` → `/tmp/<*>`);
* `F-SRC-insight-canon:simdjson_scratch.hpp:get_nested_object` — JSON **nested-`fields` component/level descent**, a cube-axis change folded
  into the same bump.

Content changes ONLY for inputs carrying a diagnostic-composite or ephemeral-root token, or a
nested-`fields` JSON record; every other document is byte-identical except this version string.

## `-5` — `D-OTEL-15`

Landed at `4e46af0`.

## `-6` — the ephemeral-root batch

The `-6` batch (canon `9c5db20`) — canon ephemeral-root masking plus the lexicon-context precision fix (`9c5db20`).

## `-7` — the NOTE register

The NOTE register (`F-SRC-insight-canon:failure_lexicon.cpp:note_register_begin`). A failure word inside a compiler note's message (`<path>:<line>:<col>: note: … failed:`) no longer
confers a failure verdict, so the serialized `dominant_level` of a gcc/clang cascade's note lines
moves Error → Unknown.

`template_str` and `template_id` do NOT move — the masker is untouched — but `dominant_level` is
serialized and gates NewErrorPattern and diff polarity, so it is an output-affecting
canonicalization change and takes the bump. **This is the entry that established the
classification class.**

## `-8` — the bracket timestamp

*(`bibles/jenkins_dialect.md` §4; ADR-23 erratum 2 — "the bracket is the entire difference".)*

`LSRC-12` — a WHOLE-token bracketed RFC3339 full datetime (`[2026-06-23T15:11:09.020Z]`) masks to `[<*>]`
instead of falling through to literal KEEP. `template_str` / `template_id` move ONLY for lines
carrying that token class; every other document is byte-identical except this version string.

## `-9` — the compound-key shapes

The compound-key shapes (`F-SRC-insight-canon:simdjson_scratch.hpp:compound_key_name`). A top-level key is resolved to its LAST SEGMENT (`log.level` → `level`) and an object value is
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

The wrapper-shell repair (`F-SRC-insight-canon:canon.detail.scan.cppm:kWrapperPairs`). `kWrapperPairs` (`canon.detail.scan`) declares the six byte pairs a producer wraps a whole token
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
(ADR-16.D11).

The bump is owed because the **rule's own acceptance set widens**: 348 460 of the pinned `BGL.log`'s
4 747 963 lines and 226 095 of `Thunderbird_5M.log`'s 5 000 000 move from a whole-line raw-text
template to a parsed BGL record, and on BGL they carry a DECLARED fatal-class level (`FATAL`
348 398, `FAILURE` 62) that nothing was reading. A labelled line now projects IDENTICALLY to its
`-` twin, so `template_str` / `template_id` move for those lines and one event class stops being
splittable 42 ways by curation label.

Three narrower moves ride the same generation, all in the same strategy:

* a second BGL header shape (306 lines with no repeated node) parses instead of yielding `level`
  Unknown and `component` = `FATAL`;
* the Thunderbird branch takes ADR-16.D10's one-token tag bound, so 405 401 lines stop having a
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
semantic-unaware (`ADR-17.D1`) — no dialect literal, no marker table.

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
`parse_health_app_ts` widens its minute to match (DN-43.O5; ADR-16.D11 for the arity half).

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

## `-16` — the Log4j locator: a pid-less prefix is not OpenStack, and a stamp's fraction is read whole

This generation is owed by the Founder's ruling of 2026-09-26 (`LEXICON.md` § *Rulings closed*):
the token moves at most once per cut, and only if canon's serialized output changed for some input
since the previous release tag — decided by bytes, never by intent. `-15` SHIPPED in `v1.10.4`
(canon `dfbeddd`), and a generation is open only while no release tag carries it, so the two
output-moving commits below could not ride it, although both commit messages say they do.
`DN-108.D24` carries the analysis; this section is the record.

**What changed.** One locator, `find_log4j_stamp`, is read by the detector's candidate gate,
`Log4jStrategy::confidence()` and `Log4jStrategy::parse()` alike, so the three cannot disagree.

* **`cc8bfbf` — the prefixed Log4j (OpenStack) layout requires its process id.** Since `bb3fd3a`,
  which `v1.10.4` carries, the locator accepted any whitespace-preceded `YYYY-MM-DD HH:MM:SS.fff`
  in the first 96 bytes, and the OpenStack branch then read a pid-less line's first token after
  the stamp as the level and the next as the component — stamp, clock, level word and first
  message word dropped from the template.
* **`81c69bf` — the stamp's fraction is read to the end of its digit run.** The locator took a
  fixed 23-byte stamp, so a longer fraction was cut at its third digit, and the leftover digits
  posed as the process id `cc8bfbf` made mandatory. `parse_log4j_timestamp`'s floor drops from
  23 bytes to 20, so a stamp with a fraction of any length parses, and its level and component
  are read where they stand.

**Which serialized fields move.** `template_str` / `template_id` (the identity class),
`dominant_level` through the event `level` (the classification class), and the event time, for
the Log4j-shaped lines those locators reach. Recorded at the commits against their parents on the
gcc-16.2 leg, over private corpora: `cc8bfbf` moves 40 event rows of the Jenkins payload-stamped
slice and 3 574 rows per view of the GitHub revert corpus's GitHub arm; `81c69bf` moves 1 281 of
2 393 candidate files, 217 732 rows keep more bytes, 2 456 empty templates gain content, and
207 548 levels move Unknown → Debug. The public hub samples and the determinism-golden corpus do
not move under either commit.

**Witness inputs.** Each line below was rendered by `det_proof` (`proof/det_proof.cpp`, the
`linux-gcc16-release` profile) built from canon at `v1.10.4`, at `cc8bfbf` and at `e00f1cb`, on
2026-09-26. Every one of its four arms (no dialect; GitHub with the RFC 3339 line prefix; GitLab;
Jenkins) gave the same row, shown as level and template:

* **For `cc8bfbf`:** `[2026-07-09T07:49:08.059Z] 2026-07-09 07:49:07.847 INFO NEM logging has
  been bootstrapped! (took 12 ms)` — at `v1.10.4`: `Info`, `logging has been bootstrapped! (took
  <*> ms)`; at `cc8bfbf` and after: `Info`, `[<*>] <*> <*> INFO NEM logging has been bootstrapped!
  (took <*> ms)`.
* **For `81c69bf`, identity:** `--- /dev/null<TAB>2025-03-12 23:17:31.994125421 +0000` — at
  `v1.10.4` and at `cc8bfbf`: `Unknown`, the EMPTY template; after `81c69bf`: `Unknown`,
  `--- /dev/null<TAB>2025-03-12 <*> <*>`.
* **For `81c69bf`, classification:** `2025-03-12 23:17:31.994125 DEBUG [main] com.acme.pool.Pool -
  pool resized to 8` — at `v1.10.4` and at `cc8bfbf`: `Unknown`, `DEBUG [main] com.acme.pool.Pool
  - pool resized to <*>`; after `81c69bf`: `Debug`, `com.acme.pool.Pool - pool resized to <*>`.

The `det_proof` of that date rendered no event time, so that half of `81c69bf`'s move rests on its
commit record, not on these witnesses; it renders every `CanonicalEvent` member since `DN-108.D24`'s
generation gate. `fb23b09` (a lifetime repair: a stream view keeps the matched package's own
dialect name) and `7aba631` (a deleted overload, compile time only) move no output and owe nothing.

**Why one step.** A consumer crossing the next cut pays exactly one comparability event: `v1.10.4`
ships `-15`, and every further output change before the next tag rides `-16`. The re-base cost is
`-15`'s: `core/tests/mask/mask_rules.golden` and `insight-metalog`'s three committed vector files,
whose only moved bytes are this string; the hub's published determinism golden is re-rendered at
the cut.

## `-17` — a unit's version is a coordinate its dialect declares

`-16` SHIPPED in `v1.10.5`, so an output change after that tag rides a new value, and this is
the one step this cut takes.

**What changed.** The intent class and the instance discriminant of a unit are derived by the
core from the marker's payload. Until now three rules that know no dialect did it all: a
`v`-number or dotted number becomes `vX`, a run of two or more digits `N`, a parenthesis `(M)`.
A reference that none of them claims, a commit hash or a branch name, stayed in the class, so one
step pinned at two commits was two units.

* **A marker row may declare its payload's version coordinate**: the introducer byte sequence
  and the payload shape it applies to (`VersionCoordinate` on `IntentMarkerRow`, a closed shape
  enum). The recognizer returns the declared version with the marker (`IntentMarker::version`).
* **The core applies it as a mechanism.** `canonicalize_intent(marker)` masks the declared
  version whole with the one version mask, and the marker's discriminant carries its bytes
  verbatim. The core holds no introducer, no hash length and no reference grammar.
* **The GitHub dialect declares it** on both Step rows: introducer `@`, on a payload that is one
  token. Its ruleset version moves `1.4.0` to `1.5.0`, and the rule grammar, which gained a row
  member and a closed enum, moves `semantic-grammar-6` to `semantic-grammar-7`.
* **A dialect that declares none is unchanged**, byte for byte: the one-argument
  `canonicalize_intent(name)` and `discriminant_of(name)` are the undeclared path and did not
  move.

**Which serialized fields move.** The intent class and the instance discriminant of a GitHub
step whose banner is one token holding `@`, and every identity a consumer derives from them. No
`CanonicalEvent` member moves: `template_str`, `template_id`, the level and the event time are
what they were. A step at a major tag (`actions/checkout@v4`) keeps its class and its instance;
what moves is a reference the three rules did not claim, or claimed in part.

**Witness inputs.** The recognized marker of each line below, under the GitHub dialect and the
annotated channel, shown as class and instance. `det_proof` renders `CanonicalEvent` members
only and none of them moves, so these are witnessed by the recognizer, as
`semantic/github/tests/test_github_version_coordinate.cpp` and
`core/tests/identity/test_declared_version_coordinate.cpp` pin them.

* `##[group]Run actions/checkout@c0f6160ff80057923ff50e5e5676a2dbcf6d9c3a` — at `v1.10.5`:
  class `actions/checkout@c0f6160ff80057923ff50e5e5676a2dbcf6d9c3a`, no instance; now: class
  `actions/checkout@vX`, instance `c0f6160ff80057923ff50e5e5676a2dbcf6d9c3a`.
* `##[group]Run dtolnay/rust-toolchain@stable` — at `v1.10.5`: class
  `dtolnay/rust-toolchain@stable`, no instance; now: class `dtolnay/rust-toolchain@vX`,
  instance `stable`.
* `##[group]Run pytorch/test-infra/.github/actions/setup-uv@release/2.13` — at `v1.10.5`: class
  `pytorch/test-infra/.github/actions/setup-uv@release/vX`, instance `2.13`; now: class
  `pytorch/test-infra/.github/actions/setup-uv@vX`, instance `release/2.13`.
* Unmoved, the guard: `##[group]Run docker pull ghcr.io/acme/tool@c0f6160ff80057923ff50e5e5676a2dbcf6d9c3a`
  holds `@` among several tokens, so it is outside the declared shape and keeps its class.

**Why one step.** A consumer crossing the next cut pays exactly one comparability event:
`v1.10.5` ships `-16`, and every further output change before the next tag rides `-17`. The
re-base cost is this string in `core/tests/mask/mask_rules.golden` and in `insight-metalog`'s
three committed vector files; the hub's published determinism golden is re-rendered at the cut.

### Rider — Apache 2.4's error-log line (`DN-43.D21`)

**What changed.** `ApacheErrorLogStrategy` read a 2.4 line's level seat by its word prefix, so
`[core:notice]` read `core`, an Unknown level, and `parse_apache_error_ts` required a space after
the seconds, so 2.4's `HH:MM:SS.ffffff` clock (and LogCraft's `HH:MM:SS.mmm`) gave no event time.
Now the seat (only the bracket right after the clock) splits at its LAST colon: the level is the
word after it, the module before it becomes `component`. Apache's `trace1`–`trace8` read Trace in
that seat only, never in the shared lexicon. The clock accepts a `.` and 1–9 digits before the
year, checked and never read, so the event time keeps one-second grain.

**Which serialized fields move.** `level`, `component` and the event time of an `ApacheError` line
whose seat holds a colon or a trace word, or whose clock carries a fraction. `content` does not
move: the seat and the clock were already outside it, so `template_str` and `template_id`, which
are derived from it, do not move either. A 2.2 line (colon-free seat, fraction-free clock) moves
nothing.

**Witness inputs.** Measured through the public `Tokenizer::process_line` at a zero-package
composition. On the first-party httpd 2.4.68 capture (coderoast-corpora `5515deb`, 21 lines):
19 bracketed lines move from Unknown, `httpd` and no event time to the level, module and instant
their bytes declare, for example `[mpm_event:notice]` gives Info and `mpm_event`; the 2 headerless
`AH00558` lines stay RawText. On LogHub's 2.2 `Apache_2k.log` (the Zenodo 8196385 re-cut, 2 000
lines, sha256 `22c51ca1…`) the projection digest is `9dad52ad…` before and after the change.

**It rides `-17`** under the Founder's ruling of 2026-09-26 (`LEXICON.md` § *Rulings closed*):
one move per cut, and `-17` is this cut's.

### Rider — the sanitizer process tag

**What changed.** AddressSanitizer, libFuzzer and valgrind open a line with `==<pid>==`. The token
is not digit-leading and no composite rule claimed it, so it stayed literal and the same line made
a new template in every process. A tenth composite rule, `sanitizer_pid`, placed after
`embedded_identity` and before `kv_value`, keeps both fences and masks the pid: the tag must open
the token, and what follows the closing fence is empty or letter-leading and kept verbatim. A digit
after the closing fence, or any byte before the opening one, declines (a version pin such as
`pin==26==3` stays literal).

**Which serialized fields move.** `template_str` and `template_id` of a line holding such a token,
and `params`, which gains the pid. Nothing else: a token claimed by an earlier rule today is still
claimed by it.

**Witness inputs.** `core/tests/mask/mask_rules.golden` pins the rule's rows
(`==4242== ERROR: harness timed out` → `==<*>== ERROR: harness timed out`, and the glued tail
`==77==ABORTING now` → `==<*>==ABORTING now`) and the two literal controls. Measured through the
public `Tokenizer::process_line` at a zero-package composition over three private CI-log views
(coderoast-corpora `8d97e14`, registered before the build): distinct templates 296 022 → 296 022,
260 933 → 260 933 and 3 311 342 → 3 311 283; the 59 removed are 4 templates each split across
15–16 pids, and in no log do two pids map to one normal form.

**It rides `-17`**, as the rider above does.

### Rider — a run's own pull-request number, declared by the acquirer

**What changed.** A run's own pull-request number glued into a word (`/stirling/V2-PR-6656/docker-compose.yml`)
stayed literal, so every pull request minted a new template and a diff of two runs that changed nothing read a
line appeared and a line vanished. No per-line rule can mask it: the line alone cannot say whether `PR-6656` is
this run's request or another one a ref listing names. Now a stream may carry a **declared context**
(`StreamContext`, a required `Tokenizer` argument; `Tokenizer::declare_context` replaces it between windows),
and a dialect may declare a key with the markers behind which its value is masked (`DeclaredValueRow` on the
manifest, a new member, serialized into the composed identity). The masker's declared-run step reads each
token's normal form after every claiming rule and replaces the maximal digit run equal to the declared value,
directly behind a declared marker at a non-alphanumeric boundary, by `<*>`. The GitHub dialect declares
`pull_request` behind `PR-`, `pr-`, `Pr-`, `pull-`, `PULL-`, `Pull-`; its ruleset version moves `1.6.0` to
`1.7.0`, and the rule grammar, which gained a manifest member, rides `semantic-grammar-7`, the value of the
release window still open.

**Which serialized fields move.** `template_str` and `template_id` of a line holding such a token, on a stream
that declares a value under a key its dialect declares, and nothing else: the run is a normalization inside a
token, so `params` does not gain it, as a composite rule's normalization does not. A stream that declares no
value — every MetaLog producer, the server, the hosted door, and Sift without its context flags — is
byte-identical to the generation before this rider. The GitHub package's composed identity moves, because its
manifest carries the new row.

**Witness inputs.** `core/tests/mask/mask_rules.golden` pins the rule's rows under a declared value
(`/stirling/V2-PR-6656/docker-compose.yml` with `PR-=6656` → `/stirling/V2-PR-<*>/docker-compose.yml`; the
shadow case `deploy/pr-6656/main.go:42` with `pr-=6656` → `deploy/pr-<*>/main.go:<*>`) and its literal controls
(another value, no value, `python3` with value 3, `XPR-6656`, `PR-66560`, and a digit-leading token masked whole).
Measured through the public `Tokenizer::process_line` over three private CI-log views, each log under its own
pair record's pull-request number (coderoast-corpora `9830858` registered before the build): distinct templates
296 022 → 295 978, 260 933 → 260 889 and 3 311 283 → 3 301 177, with 0 within-log false merges.

**It rides `-17`**, as the riders above do.

### Rider — `PR#` and `pulls/` join the GitHub declared-value markers (DN-133.D7)

**What moves.** The GitHub dialect's `pull_request` row declares eight markers instead of six: `PR#` (a
changed-files action names the run's own request `PR#<n>`) and `pulls/` (GitHub's REST path for a pull request).
The mechanism is unchanged; the predicate that claims a run is now one exported function,
`claim_declared_runs`, which the masker's normal-form pass calls and which also keys a unit's instance
(`declared_discriminant_of`, read by Sift's segmentation), so the two never disagree about a marker. The ruleset
version stays `1.7.0` and the grammar stays `semantic-grammar-7`: both are the values of the release window
still open, which already carries the rider above.

**Which serialized fields move.** `template_str` and `template_id` of a line holding the run's own declared
number behind `PR#` or `pulls/`, on a stream declaring it, and nothing else; `params` does not move. A stream
that declares no value is byte-identical. The GitHub package's composed identity moves, because its row's marker
list moved.

**Witness inputs.** `core/tests/mask/mask_rules.golden` pins `PR#6656` and `/repos/o/r/pulls/6656/files` under
6656 and the controls `PR#6657`, `XPR#6656`, `pulls/66560`, `pull/6656` and `issues/6656`. Measured through the
public `Tokenizer::process_line` over the same three private CI-log views (coderoast-corpora `a69ffd3`, registered
before the build): distinct templates 295 978 → 295 920, 260 889 → 260 831 and 3 301 177 → 3 295 875, with 0
within-log false merges.

**It rides `-17`.**

### Rider — a number behind `;` masks per segment, and a whole-line JSON value's member order is presentation (DN-134.D2, DN-134.D3)

**What changed.** Two steps, neither a claiming rule. **K:** after every claiming rule and before the declared-run
step, a token's normal form holding `;` and `=` is cut at `;`, and each `<key>=<digit-leading value>` segment has its
value masked to the segment's end, the kv-value rule's disposition (status carve-out included) applied per segment.
The kv-value rule reads one `key=value` per token and declines when the first value is a word, so the runner's
`##[end-action id=build;outcome=success;duration_ms=12]` kept its duration and every run minted a new template.
**J:** a `content` that is, whole, one strict RFC 8259 object or array has the members of every object permuted into
the order of their unescaped names before tokenization; every other byte stays where it was, arrays never move, and a
repeated name, a value nested past 128 levels or any non-strict text leaves the line as it is.

**Which serialized fields move.** K: `template_str` and `template_id` of a line holding such a token, nothing else
(`params` does not gain the segment value). J: `template_str` and `template_id` of a whole-line JSON value whose
members were out of name order, and the ORDER of its `params`, which are then views into the event's arena rather
than into the line. No extracted field moves.

**Witness inputs.** `core/tests/mask/mask_rules.golden` pins K's rows (`##[end-action
id=build;outcome=success;duration_ms=12]` → `##[end-action id=build;outcome=success;duration_ms=<*>`, `order
item=book;total=$18 placed` → `order item=book;total=$<*> placed`) and literal controls (`id=build;status=200`, the
`,`-delimited `id=build,duration_ms=12`), and J's two permutation rows (`{"result":"ok","event":"done"}` →
`{"event":"done","result":"ok"}`, a nested object inside an array-carrying member). Measured through the public
`Tokenizer::process_line` over the same three private CI-log views (coderoast-corpora `65b2b93` registered before the
build, `c210757` measured): distinct templates over every non-empty line 295 919 → 294 801, 260 830 → 259 850 and
3 295 874 → 3 269 858; J rewrites 386, 386 and 6 639 lines.

**K's extent (DN-134.D11), a further change in the same window.** K no longer masks a value to its segment's end: it
masks the value's EXTENT, from its first digit over `[A-Za-z0-9._+%-]` and any `<*>` an earlier composite wrote,
across one `,` `:` or `/` directly followed by a digit or `<*>`. The rest of the segment is swallowed when it is only
wrapper closers and `,;:.`, and is otherwise kept byte for byte behind `<*>`; the status carve-out reads the extent.
As built it also swallowed one carriage return as the token's last byte; the line-ending rider below made that clause
unreachable and deleted it. Fields moved: `template_str` and `template_id` of a line holding
such a token, nothing else. Witness inputs: the golden's `segment_kv` row `bundle
Import-Package=okio;version=1.15,javax.annotation;version=1.3,* resolved` → `bundle
Import-Package=okio;version=<*>,javax.annotation;version=<*>,* resolved`, and the segment-step unit rows (`id=a;t=12:30:01` and `id=a;r=7/8` through their composites' normal forms,
`id=a;status=200]` literal). Measured through the public `Tokenizer::process_line` over the same three private CI-log
views and the Jenkins marker corpus v2 (coderoast-corpora `72398a7` registered before the build): 0, 0 and 127 lines
move (10 templates renamed, 0 split, 0 merged; distinct templates 254 756, 221 075 and 2 797 262 unchanged), and
1 Jenkins line; `JenkinsBareNullGate` moves 1 of 82 traces.

**It rides `-17`.**

### Rider — rule 5 reads a number through a complete wrapper shell, and rule 4 decides every address it accepts (DN-134.D1, DN-134.D8)

**What changed.** **S:** a token rule 6 would keep, made of a wrapper-catalog opener at byte 0, a digit-leading core
holding neither byte of that pair, the opener's own closer and at most two trailing bytes from `,;:.`, now takes
rule 5's disposition: `(1.7s)`, `[02:16:00]`, `(96.4%),` and `"2220"` mask whole, as their bare forms always did. A
core of at most 3 digits behind a status keyword stays literal through the shell, as rule 1 keeps the bare form, and
an incomplete shell (`(25 warnings)` splits into `(25` and `warnings)`) is no shell. **D8:** rule 4 decides its
whole acceptance set before rule 5 and S read the token: with `mask_ip_addresses` on an IPv4 address masks, bare or
shelled, as before; with it off it stays literal, bare and complete-shelled forms included, where rule 5 used to
mask them anyway.

**Which serialized fields move.** S: `template_str` and `template_id` of a line holding such a token, and `params`,
which gains the raw shelled token. D8: nothing at the default configuration (the switch is on); with the switch off,
`template_str`, `template_id` and `params` of a line holding a bare or complete-shelled IPv4 token, which is now kept.

**Witness inputs.** `core/tests/mask/mask_rules.golden` pins S's rows (`step finished in (1.7s)` → `step finished in
<*>`, `[02:16:00] Starting deploy` → `<*> Starting deploy`), its status controls (`exit code (1)`, `status (200),`)
and its literal boundary (`(25 warnings)`, `(v1.2.3)`), and rule 4's witnesses now include a bare and a closer-only
address, each literal with the switch off. Measured through the public `Tokenizer::process_line` over the same three
private CI-log views (coderoast-corpora `4e09766` registered before the build): distinct templates over every
non-empty line 294 801 → 260 336, 259 850 → 226 500 and 3 269 858 → 2 845 664.

**It rides `-17`.**

### Rider — a whole-token wildcard is a param, and every param is one (DN-128.D6)

**What changed.** A token whose final normal form — after the composite step, the `;`-segment step and the
declared runs — is exactly `<*>` takes the mask path: it contributes `<*>` and pushes its SOURCE token as a param.
A source token that is literally `<*>` takes the same path. Until now the embedded-identity composite claimed a
whole dashed UUID, wrote a bare `<*>` and pushed nothing, because a dash admits the token to the composite pre-gate
before the whole-token UUID mask can reach it. So the template now states its own binding: a template token that is
exactly `<*>` is a param, every param is one, and param *i* is the (*i* + 1)-th such token.

**Which serialized fields move.** `template_str` and `template_id` move on no line. `params` move on a line holding a
whole dashed UUID (or a literal `<*>` token): the UUID is pushed in its token order, so a param after it shifts one
index. Through MetaLog, `param_histograms` of those templates gain an entry and re-index; a param pushed past the
tracked width loses its histogram.

**Witness inputs.** `core/tests/mask/mask_rules.golden`'s embedded-identity row `session <uuid> opened` now carries
its UUID as a param, and `MaskRuleGolden.EveryWholeTokenWildcardIsBoundToItsParamOnEveryWitness` reads every
witness's params against its whole-token wildcards; `StatelessTemplate.AWholeTokenWildcardIsAParamAndEveryParamIsOne`
pins the whole UUID, an embedded one (`run-<uuid>.log`, no param), a literal `<*>` and a UUID before a latency.
Measured through the public determinism proof over three private CI-log views (coderoast-corpora `4997b9a`, registered
before the build, and re-measured on the base after `DN-134.D9` and `DN-134.D1` landed, `6f54f30`): every event's template byte-identical; 0 events breaking the binding, against 9 237, 9 234 and
20 036 before; params moved on exactly the 63, 62 and 132 templates that carried a param-less whole-token `<*>`,
each to the predicted list.

**It rides `-17`.**

### Rider — the hash counter reads a complete wrapper shell (DN-136.D1)

**What changed.** A hash counter inside a complete wrapper shell — a catalog opener at byte 0, a core the bare counter
claims (`#`, a digit run, then no letter or digit) holding neither byte of that pair, the opener's own closer and at
most two trailing bytes from `,;:.` — now takes the counter's normal form inside the kept shell: `(#9767):` →
`(#<*>):`, `[#42]` → `[#<*>]`, `"#7",` → `"#<*>",`. It used to stay literal, so a logger's per-process tag made one
template per process. The shell is read by `complete_shell_core`, the one exported reader. `(#42a)`, `(#42`,
`fix(#123):` and `(#)` stay literal, and `[42]` stays the bracket index's `[<*>]`.

**Which serialized fields move.** `template_str` and `template_id` of a line holding such a token. `params` move on
none: the form embeds `<*>` inside the token, which is a normalization.

**Witness inputs.** `core/tests/mask/mask_rules.golden` pins the four shelled rows, the bare `#9767):` and `[42]`
controls and the four literal boundaries. Measured through the public determinism proof over every log of three
private CI-log views (26 234 logs, 287 296 164 events; coderoast-corpora `d8903c9`, registered before the build): 309 910
events move, every moved token is the rule's normal form of its base token and every token the rule reaches moves;
params byte-identical on every event. Distinct templates holding such a token on the GitHub arm: 2 347 → 98,
2 347 → 98 and 18 084 → 1 291.

**It rides `-17`.**

### Rider — a path starts after a declared lead, `\` separates path components, and three roots join the catalog (DN-136.D4)

**What changed.** A `TokenStart` root now sits at the PATH's first component, and the path starts at byte 0 or right
after a declared lead (a run of wrapper openers, an optional `<key>=`, an optional `file://`), so `file:///tmp/<run>`,
`(/tmp/<run>/x.ts:12:5)`, `"/tmp/<run>/s.json",` and `TMPDIR=/tmp/<run>` mask their instance. For the root, a run of
`\` separates components as `/` does, at both call sites; the source-location walk still segments at `:` and `/`
only, and inside a segment only the component directly under a matched root masks. The catalog gains
`AppData/Local/Temp` (`Floating`, `Subtree`), `/private/var/folders` and `/private/tmp` (`TokenStart`, `Subtree`).

**Which serialized fields move.** `template_str` and `template_id` of a line holding such a path. `params` move on
none.

**Witness inputs.** `core/tests/mask/mask_rules.golden` pins the eight forms of `DN-136.D4`, a `/private/tmp` row and
the eight literal boundaries (`/home/u/proj/tmp/x`, `build/tmp/x`, `user@host:/tmp/x`, `\"/tmp/x`, the runner's
`_temp`, `(/tmp)`, `file://host/tmp/x`, `AppData\Local\Templates`). Measured as above, against the build with the
rider before it: 360 747 events move, every moved token is the rule's normal form of its base token and every token
the rule reaches moves; params byte-identical on every event. Distinct templates holding such a path on the GitHub
arm: 3 617 → 263, 3 433 → 234 and 32 179 → 547.

**It rides `-17`.**

### Rider — a line's ending is removed at canon's doors, and is never content (DN-134.D13)

**What changed.** A line is the bytes a consumer hands canon in one call; its ending is the maximal run of carriage
returns closing them. Canon removes it, through the one exported `without_line_ending`, at its three doors: inside
`normalize` on the raw bytes before the escape scan, at the entry of `LogParser::parse_line`, and at the entry of
`LogParser::parse_stable`. A carriage return followed by any byte, an escape byte included, is content and stays. K's
token-final carriage-return clause (`DN-134.D11`) became unreachable and is deleted, and the carriage return leaves the
intent trim set, since no name ends in a line ending any more.

**Which serialized fields move.** None for a consumer that already framed a line without one final carriage return,
as Sift's splitter did. For a consumer that frames on LF alone, a CRLF line now reads as its LF twin, so every
projected member can move: `template_str` and `template_id` (a literal last token loses its carriage return; a mask
the carriage return blocked now fires — a complete wrapper shell, the long hash, the address and the hash counter;
the status KEEP holds again for `exit 1` + CR, which rule 5 masked, so `exit 0` and `exit 1` no longer share a
template), `params` (a masked value loses the carriage return it carried), `format` (a `KEY=` + CR line), and a line
that is only its ending stops being an event.

**Witness inputs.** `core/tests/tokenizer/test_line_ending.cpp` pins the definition, the three doors, a CR before an
escape and an interior CR kept, the end-action line equal to its LF twin through `process_line`, the exit-code status,
a shelled duration masking, a lone carriage return on the skip counter and a `KEY=` + CR line routing as `KEY=`; the
segment-step rows keep `id=a;n=5]` + CR literal behind `<*>`. Measured through the public `Tokenizer::process_line`
over every LF-framed line of the three private CI-log views and the Jenkins marker corpus v2 (coderoast-corpora
`b3d6343` registered before the build): 249 290, 242 616, 3 483 005 and 0 lines move, every one ending in a carriage
return; distinct templates 254 756 → 201 302, 221 075 → 169 662, 2 797 262 → 2 620 167 and 116 235 unchanged;
9 373, 9 287, 191 382 and 0 lines stop being events; 3, 3, 1 and 0 base templates split, each an exit code; every
merge is a carriage-return line into its LF twin. Under Sift's framing 0 lines move, and the 21 596-pair Sift replay
is byte-identical.

**It rides `-17`.**

---

*See also: [masking.md](masking.md) (what the current generation's rules actually are) ·
[determinism.md](determinism.md) (what the `canonicalization_version` gate protects).*
