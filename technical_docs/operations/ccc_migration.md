# The Code & Comment as Contract migration — `insight-canon`, the ledger

This file is the **evidence** of the migration of `insight-canon` to the closed comment grammar:
for every unit converted, the claims its deleted comments carried, the cold-reader interrogation
that tested whether the code alone still carries them, and where every claim the code did not
carry was re-homed. It is a record, written as each unit lands; it decides nothing. The protocol
it follows is `ADR-26.D8`, its operator's order of steps is `OPS-8`, and the grammar is
`ADR-26.D5`; the gate that judges a converted unit is the second phase of `malf format --check`,
reading post-format text.

**Claim classes.** Every comment block of a unit was read before deletion and each claim it
carried was classed: **M** a mirror of the code beside it · **H** history or intention (*"this
used to"*, *"will"*) · **C** a contract (`pre` / `post` / `invariant` / `assert`) · **X** a
citation · **R** rationale — a why, a measurement, a rejected alternative, an ordering that is
content. M and H are deleted; C becomes tagged lines; X becomes `refs:`; **R is held** until a
fresh agent, reading the converted working tree only and never git, answers one neutral question
per R claim. *Recovered* means the prose was redundant and stays deleted. *Not recovered* or
*wrong* means the claim needs a home above the comment rung — a law block, a paragraph in an
owning doc, or a single `note:` — and never comes back as prose. **A claim that cannot be
re-derived today is deleted and becomes a finding**, never re-homed (`OPS-8.S9`): re-asserting an
unsourced measurement is the conversion inventing a fact and signing it.

**Witnesses every unit carries.** Comment-only: the code token stream of each file, comments
removed and whitespace dropped, is byte-identical to `HEAD`'s. Grammar: `malf format --check
<unit>` reports zero would-be violations for the unit. Behaviour: `malf test insight-canon` green
on clang-21 and on gcc-16 after the commit, against a green baseline taken before the first
conversion. A binary diff is not a witness — `__LINE__` legitimately changes when a comment is
deleted.

**Baseline, 2026-09-05, before any conversion (the gate's own count):** 126 files, 14 489 comment
lines, 14 242 would-be violations — bare 12 279, `///` 92, spacer 738, ruler 217, trailing 831,
trailing `NOLINT` 30, suppression without a why 43, tag mid-line 11, block prose 1; tool forms
already present 247. Behaviour baseline the same day, both toolchains **equal**: `insight_canon`
734 tests, `insight_semantic_github` 32, `insight_semantic_gitlab` 25, `insight_semantic_jenkins`
13, `insight_semantic_test_frameworks` 5 — **809 of 809 passing** on clang-21 and on gcc-16.2.
`benchmarks/` declares no ctest target and is built, never run, by `malf test`.

**This repo's shape differs from every repo migrated before it, and it set the plan.**
`coderoast-ipc` was 46 % `///` and `insight-twin` 47 % — a retired form that deletes mechanically.
`insight-canon` is **86 % bare prose** (12 279 of 14 242) and only 0.6 % `///`. Bare prose is
narrative that must be read and classed claim by claim, so the per-unit cost here is the reading,
not the stripping, and the unit count is set by reader load rather than by line count.

**Law numbering.** This repo declares **zero** law blocks. The workspace's next free number was
ruled to be LogCraft's start, so a mint here would be a collision only the Founder can resolve;
every disposition that needed a home above the comment rung found one in an existing tagged form
or an owning document. The token is not spelled in this file: the registry lint reads a spelled
`D-LSRC-<digits>` anywhere as a declaration.

---

## The unit plan

126 files and 14 242 would-be violations, ordered **source before tests** (`OPS-8.S2`) so a test's
`refs:` can cite the slot the source unit names. The first unit is deliberately the smallest in the
repo: none of the migration scripts had ever run against `insight-canon`, and calibrating them on a
37-violation file costs minutes where calibrating them on the 1 193-violation `canon.api.cppm`
would have cost a day. Source tier, by violation count: `core/src/arena` 37 · `core/src` 8 ·
`core/src/transport` 99 · `core/src/tokenizer` 112 · `core/src/identity` 132 · `core/src/parse` 265 ·
`core/src/scan` 284 · `core/src/conformance` 313 · `core/src/compose` 395 · `core/src/mask` 530 ·
`core/src/utils` 630 · `core/src/strategy` 1 275 · `core/api/utils` 23 · `core/api/det` 72 ·
`core/api/canon.cppm` 242 · `canon.compose.cppm` 268 · `canon.transport.cppm` 322 ·
`canon.spi.cppm` 672 · `canon.api.cppm` 1 193. Then the harness tier (`core/tools` 420, `proof` 142,
`benchmarks/src` 53, `core/test_package` 15), the test tier (4 682) and the three dialect packages
(2 058).

## Unit 1 — `core/src/arena/` (1 file, 379 lines, 37 would-be violations)

The arena allocator's implementation unit. Chosen first as the calibration unit; its prose is
dominated by one argument — why `reset()` overwrites the bytes it releases — which is a
falsifiability claim rather than a description, so it exercised the held-claim path properly
despite the unit's size.

| files | comment lines HEAD → gate | forms written |
|---|---|---|
| `arena_allocator.cpp` | 39 → 17 | invariant 1 · assert 1 · note 6 · refs 1 · 1 continuation · 7 tool |

The single `refs:` addresses `ADR-3.D4` (the module/api layering that makes `utils/log_macros.hpp`
a textual GMF include rather than an import).

### Census (`OPS-8.S4`), and the leg this repo needed that the runbook has no step for

`NOLINT` 5 → 5, namespace closers 2 → 2, `/*name=*/` 0, `clang-format off` 0, `wall-clock:` 0,
`SPDX` 0, `SRC-` registry codes 0 → 0. **Zero differences, so zero census decisions** — but the
five suppressions all changed SHAPE, and each change was measured rather than reasoned.

### The five suppressions, every one measured

`clang-tidy-21 -p core/build-clang21-libcxx-release` over a suppression-stripped copy of the file,
against the same run with them in place. **In place: 0 findings. Stripped: 6 findings.** So every
one of the five silences a real diagnostic and all five were kept; the repair was to their FORM,
which the grammar constrains, never to their existence.

| site | was | measured without it | became |
|---|---|---|---|
| `make_block` | `NOLINTNEXTLINE(readability-convert-member-functions-to-static)` with the why on the DIRECTIVE's own line | `readability-convert-member-functions-to-static` | the why moved above as a `note:`, directive alone on its line |
| `allocate` base cast | trailing `// NOLINT` | `cppcoreguidelines-pro-type-reinterpret-cast` | `note:` + `NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)` |
| `allocate` return cast | trailing `// NOLINT` | `cppcoreguidelines-pro-type-reinterpret-cast` **and** `performance-no-int-to-ptr` | `note:` + a NEXTLINE naming **both** checks |
| `owns` pointer cast | trailing `// NOLINT` | `cppcoreguidelines-pro-type-reinterpret-cast` | `note:` + `NOLINTBEGIN(…)` |
| `owns` block-base cast | trailing `// NOLINT` | `cppcoreguidelines-pro-type-reinterpret-cast` | covered by the same `NOLINTEND(…)` region |

**The bare `// NOLINT` that silenced TWO checks is the finding, not the four that silenced one.** A
bare `NOLINT` suppresses everything on its line; the grammar's named form does not. Converting that
site to `NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)` alone would have re-armed
`performance-no-int-to-ptr` and moved the repo's `malf lint` baseline from 21 findings to 22 — a
behaviour change inside a commit whose whole claim is that it changes no behaviour. Measured before
and after: **21 findings at HEAD, 21 after the unit**, unchanged.

**`owns`'s second cast could not take a `NOLINTNEXTLINE` at all.** It sits on a CONTINUATION line of
a braced initializer, so the directive would have had to go INSIDE the expression, which is
`OPS-8.S7`'s shape ② — clang-format hoists it onto the statement line and it becomes a trailing
comment, detached from its `note:`. A `NOLINTBEGIN`/`NOLINTEND` region spanning both of `owns`'s
casts is what the shape admits. `NOLINTEND` needs no `note:` above it: the gate classes it as a
tool form unconditionally, while `NEXTLINE` and `BEGIN` require one.

### Interrogation

One fresh agent, 10 questions built from the held R claims, 20 tool uses, 92 k tokens, 3.5 minutes.
No git command was run; the transcript was checked. The reader was additionally forbidden this
repo's own ledger — see the `OPS-8` verdict below, the fixed prompt names only LogCraft's.

**10 of 10 recovered, 0 not recovered, 0 wrong. No line the conversion wrote was found false.**

Recovery repeatedly landed **above** the deleted prose, which is the outcome the protocol wants:

* Q2 (why poison at all) was recovered with the prose's own green-blind argument **plus** the
  verified instance it never named — `DN-29.D13`, a window close resetting the line arena under a
  live `CanonicalEvent` — and the eidos-side arm, `insight-eidos/engine/tests/pipeline/arena_lifetime_test.cpp`.
* Q4 (why the instrument adds no field) was recovered from the written `invariant:` **and
  independently** from `core/CMakeLists.txt`, where the `PRIVATE` define sits beside the installed
  module file set. The reader named the mechanism the old prose only gestured at — a
  macro-conditional member gives the consumer a class layout the linked canon does not share, an
  ODR violation with no diagnostic.
* Q10 (why address arithmetic runs on integers) was recovered with an argument the prose never
  carried: the aligned address is computed BEFORE the capacity check, so it can land past the end
  of the block, and forming that as a pointer would be UB while the integer form is well defined.

### A stale claim found in a file this unit did not touch — a finding for the `canon.api.cppm` unit

The reader's Q8 answer surfaced it and it was then re-derived at the artifact. `canon.api.cppm`,
above `arena_poisons_on_reset()`, states: *"A lifetime gate downstream must SKIP on false, never
pass."* **That is no longer true.** `core/tests/arena/test_arena_allocator.cpp` carries a block
headed *"THE TWO CASES BELOW ARE COMPILE-TIME GATED, NOT RUNTIME-SKIPPED (Founder, 2026-09-04)"*,
recording that the cases *"used to open with `if (!arena_poisons_on_reset()) GTEST_SKIP()`"* and
that the skip was the wrong shape — a skip exits 0 and ctest counts it as passed, so a release
build reported a pass for a lifetime it never observed. The api prose therefore prescribes the
shape a Founder ruling replaced. It is **not repaired here**: it lives in a different unit, and a
comment-only commit does not reach across unit boundaries. It is repaired when
`core/api/canon.api.cppm` converts, and the claim that lands there is compile-time gating, not
SKIP.

This is also why the unit deliberately did NOT re-home that MUST at the definition site: the
declaration is where a caller contract belongs (`ADR-26.D5`), the declaration is in another unit,
and the claim turned out to be false as stated. Writing it here would have been the conversion
asserting a falsehood in a tagged line.

### Dispositions

**Nothing re-homed.** All ten held claims were carried by the converted code, its tests, the
repo's `CMakeLists.txt` or the ADR a `refs:` now names. No disposition needed a law block.

### Witnesses

Comment-only: the code token stream of `arena_allocator.cpp` is identical to `HEAD`'s.
Grammar: `malf format --check core/src/arena` — 17 comment lines, **0 would-be violations**, post
format. Behaviour: `malf test insight-canon` **809 of 809** on clang-21 and **809 of 809** with
`--profile linux-gcc16-release`, equal to the baseline. Lint: `malf lint --all-files` **21
findings**, equal to the baseline. Count: 39 comment lines at HEAD to 17 as the gate counts them,
a **56 % reduction**.

## Unit 2 — `core/src/identity/` (2 files, 376 lines, 132 would-be violations)

Intent identity and template identity: `canonicalize_intent` (the class), `discriminant_of` (the
complementary instance coordinate), `trimmed_intent_name` (the one trim set all three roles share),
and the SHA-256 template id with its n-gram key. The densest argumentative prose in the source tier
so far — the file header alone carried the closure model, a vocabulary trap, the frozen rule set
and a corpus measurement.

| files | comment lines HEAD → gate | forms written |
|---|---|---|
| `intent_identity.cpp`, `template_id.cpp` | 136 → 34 | post 2 · invariant 8 · note 5 · refs 6 · 9 continuations · 4 tool |

**No `refs:` at the file head.** The ten addresses the old prose carried do not fit one line — a
`refs:` is one line by grammar — so they were distributed to the sites they actually govern:
`BIB:intent_identity, ADR-25.D6, ADR-17, ADR-18` at the header, `SRC-D-TID-1, SRC-D-TID-2,
SRC-II-2, STU-4` at `canonicalize_intent`, `SRC-II-1` at `intent_id_of`, `ADR-18, SRC-II-9,
DN-38.D3` at `discriminant_of`, `DN-38.D1` at the trim set, `SRC-D-TIR-1` at `template_id.cpp`.
That is a better outcome than a header block and it was forced by the budget, not chosen.

### Census (`OPS-8.S4`), with the address leg

`NOLINT` 0, `/*name=*/` 0, namespace closers 4 → 4. **Address sets: nothing LOST.** All six `SRC-`
codes and all three ADRs survive into `refs:` lines. Three deliberate ADDITIONS, each recorded
here as the step requires: `BIB:intent_identity` (the prose carried the path
`bibles/intent_identity.md`, which is not a registry form — `ADR-6.D13` form), and `DN-38.D1` /
`DN-38.D3` and `STU-4`, which name the documents that own the measurements the prose was carrying
inline.

**The CR measurement was deleted from source, and that is correct rather than a loss.** The old
comment carried *"52121/511861 = 10.2% of real banners"*, *"337 distinct step payloads"* and
*"18/34640 same-job same-step pairs (0.052%)"*. `OPS-8.S9`'s new row asks whether the claim can be
re-derived today; the answer here is **yes, from two places outside canon** —
`DN-38` states the 10.2 % and the 18 / 34 640, and `insight-eidos`'s
`sift/tests/phase/test_intent_channel_phantom.cpp` carries all of the figures in full at its ARM 4
block. So this is a citation, not the unsourced-measurement case, and `refs: DN-38.D1` is what
replaces it. The cold reader recovered every figure without the source comment, which is the
measurement that settles it.

### Interrogation

One fresh agent, 13 questions, 43 tool uses, 133 k tokens, 5.9 minutes. No git command; transcript
checked. **13 of 13 recovered, 0 not recovered, 0 wrong.**

Recovery was repeatedly *better* than the prose it replaced:

* Q4 (why R1 before R3) came back with a derivation the comment never gave — with R3 first, `'.'`
  is a non-word byte so `boundary_after` accepts it as a right anchor, and `10.2.3` canonicalizes
  to `N.vX` instead of `vX`, which splits two homologous runs into two classes.
* Q8 (why the envelope and not the first span) came back with `DN-38.D2`'s measurement — nine
  macOS matrix cells collapsing to one class, five of them sharing the coordinate `14` — plus the
  two refused alternatives and *why* each was refused.
* Q10 (what fixes the id at 16 bytes) came back from **`metalog-spec/SPEC.md` §3.2**, which makes
  it a MUST for every producer, and `RATIONALE.md` §R2 for the sizing. The deleted trailing comment
  said only *"spec §3.2"*; the reader found the spec.

### One line THIS conversion wrote was wrong, and the reader caught it before the commit

The header `note:` read *"the appearance ordinal refused below is not the ordinal AXIS SPECIES
**above**"*. The reader's Q3 answer ended: *"the source comment's cross-reference is dangling — it
says 'not the ordinal AXIS SPECIES above', and nothing above it in the current file mentions an
axis species."* Correct: the deleted prose opened *"geometry TREE, axis species POPULATION"* and
the conversion had dropped that clause while keeping a pointer to it. **A dangling reference the
conversion itself created.** Repaired at the claims script and the whole unit re-derived from
`HEAD`, re-stripped, re-placed, re-formatted and re-witnessed: the `invariant:` now opens
*"geometry TREE, axis species POPULATION"* and the `note:` says *"not that axis species"*.

### Two stale claims, one of them created by this unit's own deletion

1. **`core/api/canon.api.cppm` — repaired in this commit.** Above `trimmed_intent_name` it read
   *"the set's own definition in intent_identity.cpp carries the numbers"*. Deleting the numbers
   from `intent_identity.cpp` made that sentence false, so the pointer now names `DN-38.D1`, which
   does carry them. This is a comment-only edit to a file outside the unit, made deliberately: the
   falsehood is one this unit created, and leaving a known-false pointer standing to respect a unit
   boundary would be shipping a defect. It is recorded rather than deferred.
   **This is a general hazard `OPS-8` has no step for** — see the verdict at the end of this file.

2. **`insight-eidos/sift/tests/phase/test_intent_channel_phantom.cpp` — a finding for another
   lane, not fixed here.** Its ARM 4 block states *"canonicalize_intent trims `' '` and `'\t'` —
   NOT `'\r'` (intent_identity.cpp)"*. `is_intent_trim_byte` returns true for `'\r'` today, so the
   premise the block states is contradicted by the code it names. It is in another repo and outside
   this migration; addressee below.

### Dispositions

**Nothing re-homed.** All thirteen held claims were carried by the converted code, its tests, the
owning design note, the studies shelf or the MetaLog specification. No disposition needed a law
block.

### Findings for other lanes — none fixed here

1. **The eidos CR comment contradicts canon — Hephaïstos, with Kleio.** As above. The block's
   numbers are correct and worth keeping; its opening premise is not.
2. **`kTemplateIdBytes` and `TemplateId::bytes`' extent are two independent literals — Hephaïstos.**
   The reader observed that `template_id.cpp`'s `constexpr std::size_t kTemplateIdBytes{16}` and
   `canon.api.cppm`'s `std::array<std::uint8_t, 16>` are both spelled `16` with neither derived
   from the other. `metalog-spec` §3.2 makes 16 a wire MUST, so the value is right; the duplication
   is a code change and did not belong in a comment-only commit.

### Witnesses

Comment-only: the code token stream of both files, and of `canon.api.cppm`, is identical to
`HEAD`'s. Grammar: `malf format --check core/src/identity` — 34 comment lines, **0 would-be
violations**, post format. Count: 136 comment lines at HEAD to 34, a **75 % reduction**.

## Unit 3 — `core/src/transport/` + `core/src/canon.internal.cppm` (2 files, 364 lines, 107 would-be violations)

The transport transform algorithms (stamp peel, bracketed peel, byte-order mark), the fail-closed
declaration resolution, the writer dual, and the module that is canon's single `import std`.
`canon.internal.cppm` joins this unit rather than getting its own: it is 8 violations and 25 lines,
and no question about it is separable from "how does a canon module unit reach std", which the
transport unit answers by importing it.

| files | comment lines HEAD → gate | forms written |
|---|---|---|
| `transport.cpp`, `canon.internal.cppm` | 109 → 40 | pre 1 · post 1 · invariant 12 · assert 1 · note 4 · refs 6 · 13 continuations · 2 tool |

`refs:` targets: `ADR-3.D4`, `ADR-22`, `ADR-23`, `ADR-23.D3`, `ADR-23.D4`, `ADR-23.D6`,
`ADR-23.O2`, `DN-25.D3`.

### Census (`OPS-8.S4`)

`NOLINT` 0, `/*name=*/` 0, namespace closers 2 → 2, `SRC-` codes 0 → 0. **Clean, zero differences
and zero addresses lost** — every ADR and DN address the prose carried is in a `refs:` line.

### The history that went, and the rule that stayed

`has_stamp_at_head` carried nineteen lines describing a defect that had already been fixed: the
predicate *"once validated the invariant 19-byte head and let the declared width cover the
remaining 9"*, and three measured arms of the resulting corruption (a 27-byte serving-API stamp,
a 6-digit-fraction writer, a whole-second syslog line losing the `m` of `myapp`). That is **H —
history of a closed defect** — and `ADR-26.D1` rules a rejected alternative out of source, so it
was deleted rather than converted. What survives is the rule it established, as an `invariant:`:
the declared width is a CLAIM about the bytes, never a promise. The reader recovered the
consequence unaided.

### Interrogation

One fresh agent, 15 questions, 11 tool uses, 79 k tokens, 2.1 minutes. No git command; transcript
checked. **15 of 15 recovered, 0 not recovered, 0 wrong, and no line this conversion wrote was
found false.**

Three answers came back sharper than the claim that prompted them, each from the api module the
`refs:` and the types point at:

* Q1 — the reader added the consequence the prose never stated: trusting the declared width would
  not only cut content head-first, it would then run `strip_separator` and eat the line's leading
  indentation as well, because the shipped row sets `strip_leading_space = true`.
* Q3 — it confirmed the `assert:` (`width == 0` is the live case) and derived why 1–18 are
  unreachable: 19 is the shortest string `rfc3339_datetime_length` can return, so the comparison
  can only be met at 0 or at ≥ 19.
* Q5 — it recovered the enrichment-only contract **with its measurement**, from
  `canon.transport.cppm`: a whole-stream transport stamp covers lines written by different clocks,
  measured on Jenkins at 0 inversions for controller annotations against 7–701 per log for agent
  payload. The unit's own `invariant:` states the prohibition; the number lives where it is owned.

Q4 drew a distinction worth recording because it refines rather than contradicts the written
`invariant:`. The line says the two doors *"share ONE algorithm and differ only in what the
parameter proves and the return type states, never in bytes"*. The reader agreed for identical
input bytes and then noted that on the same **original** line the two can still land differently,
because `peel`'s input has already been ANSI-stripped while `peel_raw`'s has not — an escape ahead
of the transport prefix makes the row decline on one path and not the other. That is
`canon.transport.cppm`'s "TWO DOORS" block, and it is why the claim is scoped to bytes rather than
to lines.

### Dispositions

**Nothing re-homed.** All fifteen held claims were carried by the converted code, the transport
api module, or the ADR a `refs:` names. No disposition needed a law block.

### Witnesses

Comment-only: the code token stream of both files is identical to `HEAD`'s. Grammar:
`malf format --check core/src/transport` and the standalone gate over `canon.internal.cppm` — 40
comment lines, **0 would-be violations**, post format. Count: 109 comment lines at HEAD to 40, a
**63 % reduction**.

## Unit 4 — `core/src/tokenizer/` (1 file, 279 lines, 112 would-be violations)

The `Tokenizer` facade: parse → mask → `CanonicalEvent`, the projection-totality instrument, the
two provenance pairs, and the OTEL span-document doors. The densest `refs:` surface of the run so
far — eleven `refs:` lines carrying eight `SRC-` codes, four ADR slots and four design-note slots.

| files | comment lines HEAD → gate | forms written |
|---|---|---|
| `tokenizer_engine.cpp` | 114 → 48 | invariant 13 · note 7 · refs 11 · 13 continuations · 4 tool |

### Census (`OPS-8.S4`)

Namespace closers 2 → 2, `/*name=*/` 0. **No address lost**: all eight `SRC-` codes
(`SRC-D-OTEL-1`, `SRC-D-OTEL-9`, `SRC-D-OTEL-18`, `SRC-D-PROV-1`, `SRC-D-TID-11`, `SRC-D-W1-3`,
`SRC-II-8`, `SRC-SP-1`), all four ADRs and all four design notes survive into `refs:` lines.

**One difference, and it is not a directive: `NOLINT` 3 → 2.** The third occurrence was the *word*
"NOLINT" inside a prose sentence (*"NOLINT for the same non-owning-ref reason as `arena`"*), which
went with the prose. Both real directives survive; see `OPS-8` verdict finding 7.

### The two suppressions, measured

`clang-tidy-21` over a copy with both directives removed: **2 findings, both
`cppcoreguidelines-avoid-const-or-ref-data-members`, on `arena` and on `composed`. With them in
place: 0.** Both kept, both moved from a trailing position onto their own `NOLINTNEXTLINE` line
under a `note:`, and re-measured at **0 findings** after the conversion — so the re-homed
directives still bind.

**The first attempt at that measurement was contaminated and is worth recording** (verdict finding
9): removing `// NOLINT(...)` with a regex cut the directive out of a trailing comment that
*continues onto a second line*, leaving the continuation text where code is expected. clang-tidy
answered with six `clang-diagnostic-error`s alongside the one real finding. The tell is
`clang-diagnostic-error` in the output — it means the measurement broke its own input.

### Interrogation

One fresh agent, 14 questions, 45 tool uses, 135 k tokens, 4.4 minutes. No git command; transcript
checked. **14 of 14 recovered, 0 not recovered, 0 wrong, and no line this conversion wrote was
found false.**

The recoveries reached instruments the prose never named:

* Q8 (what keeps the timestamp pair together) came back with the mechanism AND its enforcement —
  `EventTime` is structurally unsplittable upstream, `CanonicalEvent` deliberately flattens it back
  to a `bool`, and the one-write-site property is held by a checked lint,
  `scripts/provenance_one_write_site_lint.sh`, which no comment mentions.
* Q11 (what stops dialect knowledge entering core) came back with
  `scripts/sp1_semantic_unawareness_lint.sh` — a deny-list scan over comment-stripped `core/src`
  and `core/api` that also fails a vacuously-green scan. The `invariant:` states the property; the
  gate that holds it was found by reading the tree.
* Q6 (why the empty-projection check is an instrument and not a rule) came back with the pinned
  per-corpus expectation `LogHubProjectionPinGate` at 40 959, and with `ADR-16.D9`'s record that a
  universal checked postcondition is **owed** and blocked on what `ParsedLine` cannot express.
* Q13 (the two OTEL recognisers) came back with the asymmetry the comment only gestured at: L1 runs
  on every JSON line so it is tuned for precision, L3 is the acquisition door where a false
  positive costs one walk returning 0 and a false negative silently drops a conformant export.

### Dispositions

**Nothing re-homed.** All fourteen held claims were carried by the converted code, the api and spi
module interfaces, the owning ADR or design note, or a repo script the reader found. No disposition
needed a law block.

### Witnesses

Comment-only: the code token stream is identical to `HEAD`'s. Grammar:
`malf format --check core/src/tokenizer` — 48 comment lines, **0 would-be violations**, post
format. Behaviour: **809 of 809** on clang-21 and **809 of 809** on gcc-16.2, equal to the
baseline. Count: 114 comment lines at HEAD to 48, a **58 % reduction**.

## Unit 5 — `core/src/parse/` (3 files, 954 lines, 265 would-be violations)

The detection and parsing domain: `FormatDetector` (the candidate heuristic plus the confidence
scan), `LogParser` (arena, sticky routing, the stage-1 site, the level lift and the echoed-source
demotion), and the sealed module interface that carries the `LogParserPasskey` privileged mint.
The first unit of the resumed run, and the first in which the cold reader falsified **four**
lines the conversion had written.

| files | comment lines HEAD → gate | forms written |
|---|---|---|
| `canon.detail.parse.cppm`, `format_detector.cpp`, `log_parser.cpp` | 273 → 103 | pre 5 · post 6 · invariant 22 · note 12 · refs 17 · 29 continuations · 12 tool |

`refs:` targets: `ADR-3.D4`, `ADR-16.D9`, `ADR-17`, `ADR-21.D4`, `ADR-22`, `ADR-22.D3`,
`ADR-22.D6`, `DN-32.D3`, `DN-43.D4`, `DN-43.D14`, `SRC-D-PROV-1`, `SRC-D-TID-11`, `SRC-SP-1`,
`F-SRC-insight-canon:test_normalized_content_doors.cpp`,
`F-SRC-insight-canon:test_transport_peel_equivalence_gate.cpp`.

### Census (`OPS-8.S4`), and the eleven suppressions that silenced nothing

Namespace closers 8 → 8, `/*name=*/` 0, `clang-format off` 0, `wall-clock:` 0, `SPDX` 0. **No
address lost**: every `SRC-` code, ADR and design-note slot the prose carried survives into a
`refs:` line. Two `NOLINT` differences, both decisions:

* **`canon.detail.parse.cppm` 4 → 3.** The fourth occurrence was the WORD "NOLINT" inside a prose
  sentence (*"NOLINT for the same non-owning-ref reason as `arena_`"*), the class the verdict's
  finding 7 named on unit 4. All three real directives survive and were re-homed.
* **`format_detector.cpp` 12 → 1, and this is the unit's suppression finding.** Ten bare trailing
  `// NOLINT` on array-index expressions plus one on a `CandidateList` member write were **measured
  to silence nothing**: `clang-tidy-21` over a copy with every directive removed reports exactly
  **one** finding, `readability-qualified-auto` at `max_score_it`, and the check the ten were
  written for — `cppcoreguidelines-pro-bounds-constant-array-index` — is **disabled in the one
  shared `.clang-tidy`** (`malf/config/.clang-tidy`, the symlink target). They were deleted with
  that evidence. The eleventh, `NOLINTNEXTLINE(readability-qualified-auto)`, silences a real
  diagnostic and was re-homed under its `note:`.

The three real directives in `canon.detail.parse.cppm` were measured the same way: **with them in
place 1 finding, without them 4** — `readability-convert-member-functions-to-static` on `attest`
and `cppcoreguidelines-avoid-const-or-ref-data-members` on `arena_` and on `composed_`. The
remaining finding, `mint` can be made static, is unsuppressed at `HEAD` and stays unsuppressed.
After the conversion the same three files read **1, 0, 0**, identical to before, so both re-homed
`NOLINTNEXTLINE`s still bind.

**One of those two members needed the reflow to bind, and it is worth recording as a shape.**
`composed_`'s declaration was split across two lines *because* its trailing `// NOLINT` pushed it
over the column limit. Moving the directive to its own line above shortened the declaration, so
clang-format rejoined `const insight::semantic::ComposedSemantics& composed_;` onto one line — and
only then does a `NOLINTNEXTLINE` above it cover the line the diagnostic is reported on. Verified
by re-running clang-tidy on the formatted result, not assumed.

### Interrogation

One fresh agent, 35 questions, 56 tool uses, 160 k tokens, 7.6 minutes. No git command was run;
the transcript was checked. **31 of 35 recovered, 0 not recovered, 4 wrong — and all four wrong
verdicts landed on lines THIS conversion wrote.** That is the highest wrong count of the run and
the most valuable reader so far.

Recovery reached instruments and mechanisms the prose never named: the module's seal was recovered
from `core/CMakeLists.txt`'s `PRIVATE FILE_SET cxx_modules_detail` and the `install(TARGETS …)`
rule rather than from any comment; the friend-list-of-one was recovered as the gtest
`NormalizedContentDoors.TheMintKeyHasExactlyOneFriendAndItIsTheParser`, with the reader
establishing by sweep that it is the **only** enforcer; the registration order was recovered from
`compose.cpp`'s `canonical_order`, a byte-wise sort on package name with version as tiebreak; and
the `parse_stable` price was recovered with a number from a test the unit does not mention
(Stage 2's `kKeywordHead{128}` raw-byte cue budget).

### The four lines the conversion wrote that were WRONG, each re-derived at the artifact

1. **`composed_`'s declaration order was never load-bearing.** The old prose said *"Declared BEFORE
   `detector_` so it is constructed first (`detector_` is built from it)"*, and the conversion
   carried it into an `invariant:`. The reader answered that the constructor initializes
   `detector_` from the **parameter**, not from the member: `: arena_(arena), composed_(composed),
   detector_(composed)`. Re-read at `log_parser.cpp`: correct — swapping the two member
   declarations would change nothing observable, and a reference member's binding needs no prior
   construction anyway. **The claim was false in the original prose and the conversion signed it.**
   Disposition: the `invariant:` is deleted, not repaired; the `note:` that `composed_` is borrowed
   and must outlive the parser stands.
2. **"a format absent from the candidate list is never probed" is true only of the BUILTINS.** The
   reader answered that a composed strategy is pushed into `custom_strategies_` as well, and that
   vector is walked on every line regardless of the candidate list. Re-read at
   `format_detector.cpp`: correct. Repaired to *"a BUILTIN absent from this list is never probed …
   a custom strategy is walked on every line regardless."*
3. **"the fallback never becomes the sticky strategy" is false about the assignment.** The reader:
   *"inaccurate about the member assignment and accurate about the behaviour."* Re-read at
   `select_strategy`: `sticky_strategy_ = found` runs whenever `found` is non-null, and `detect()`
   returns `fallback_.get()` on an unclaimed non-empty line — so the fallback **is** latched. What
   it can never do is arm the fast path, because the guard is `confidence(line) > 0.0` and
   `RawTextStrategy::confidence` is a constant `0.0`. Repaired to *"can be LATCHED as sticky but
   never arms the fast path."*
4. **"blank input must not move the rate" is false for a whitespace-only line.** The reader:
   `parse_line` skips only a zero-length `raw_line`; a line of spaces reaches `detect()`, which
   tests `trim_left(line).empty()` and returns `nullptr`, so it lands in `failed_count_`. Re-read
   at both sites: correct, and `core/tests/parse/test_format_detector.cpp`'s
   `ReturnsNullForEmptyLine` asserts `detect("   ") == nullptr` in so many words. The line was
   repaired to state only what holds — *"the denominator is parsed + failed only, so a line counted
   as skipped cannot move the failure rate"* — and the underlying gap became finding 1 below.

A fifth line was **imprecise rather than wrong** and was tightened on the same reading: the
`parse_stable` note said *"a pre-ANSI-stripped stable line carries no wrapper — the demotion is a
no-op"*, which reads as unconditional. The reader pointed out that the door exists precisely for
callers who did **not** strip, where the demotion is live. Now: *"a caller that already stripped
ANSI hands no wrapper here, so this is a no-op."*

### Two stale claims in the OLD prose, deleted with the evidence

1. **`detect_from_batch` is not a majority vote.** Two comments called it one (*"Detect from a
   sample batch (majority vote)"* at the declaration, and the module header's *"strategy registry +
   majority vote"*). The function accumulates `scores[index] += strategy->confidence(line)` over the
   sample and takes `max_element`, so one high-confidence line can outweigh a numerical majority of
   another format's. Deleted; the `post:` now says **CUMULATIVE confidence** and says why that is not
   a count. The reader reached the same verdict independently.
2. **`set_format`'s fallback is immediate, not deferred.** The comment said *"auto-detection is
   re-enabled on the next call"*; `log_parser.cpp` sets `auto_detect_ = true` in that same call,
   before returning. The `post:` at the declaration states the immediate behaviour.

### Two unsourced measurements deleted rather than re-homed (`OPS-8.S9`'s new row)

* **`skipped_count_`'s justification.** The prose carried *"one pass over 4 082 GitHub CI logs:
  523 126 empty lines against 87 643 real strategy failures, i.e. 85.6 % of the counter was
  ordinary input"*, and an effect measurement — bounded failure reports 837 → 1 439, IIS WNC 0 → 47,
  key=value 0 → 1. **Half of it is re-derivable and half is not.** `523'126` is pinned in this
  repo, as the `blank_and_empty` field of the `data/v1/full` slice (4 082 logs) in
  `F-SRC-insight-canon:test_transport_peel_equivalence_gate.cpp`, so that leg has a home and the
  `refs:` now names it. `87 643`, the 85.6 %, and every effect figure appear **nowhere else in the
  workspace** — verified by an allowlisted sweep over every sibling repo and every tracked
  top-level file. They are deleted and become finding 2 below; the RULE they justified survives as
  the `invariant:`.
* **The 57.7 MB stderr figure.** The `parse failed:` WARN carried *"the tokenizer facade used to
  reprint every failure's text at WARN with no rate limit at all, and that unbounded duplicate is
  what turned one corpus pass into 57.7 MB of stderr."* The string `57.7` occurs in no other file
  in the workspace. Deleted. What survives is the live half, and the reader confirmed it at the
  artifact: `tokenizer_engine.cpp` still reprints `parsed.error()`, now at **DEBUG and unbounded**,
  so this WARN really is the only rate-limited record.

### Dispositions

**Nothing re-homed above the comment rung, and no law block minted.** All 31 recovered claims were
carried by the converted code, the api and spi module interfaces, `core/CMakeLists.txt`, the ADR
or design note a `refs:` names, or a test the reader found. The four wrong lines were repaired in
the tree before the commit and the unit was re-derived from `HEAD`, re-stripped, re-placed,
re-formatted and re-witnessed (`OPS-8.S9`'s hand-edit rule, taken as a full regeneration rather
than a hand edit).

### Findings for other lanes — none fixed here

1. **A whitespace-only line is counted as a FAILURE, not a skip — Hephaïstos (canon), with Kleio
   for the witness.** `LogParser::parse_line` short-circuits on `raw_line.empty()` only. A line of
   spaces or tabs is non-empty, survives stage 1 unchanged, reaches `FormatDetector::detect`, which
   returns `nullptr` because it tests `trim_left(line).empty()` — and the caller then does
   `++failed_count_`. `skipped_count_` exists precisely so no-event input cannot dilute the failure
   counter that gates the bounded WARN and feeds the failure-rate stat, and whitespace-only input
   defeats it. This is a **code** change and did not belong in a comment-only commit.
2. **The measurement that justified splitting the two counters has no home — Eqya, to route.**
   87 643 strategy failures over the 4 082-log GitHub CI slice, and the 837 → 1 439 bounded-report
   effect with its two zero-to-nonzero classes, exist only in the comment this unit deleted. The
   population is the same one `test_transport_peel_equivalence_gate.cpp` pins, so the missing half
   is re-derivable by running the measurement again — but it is not knowledge the tree currently
   holds, and re-asserting it in a tagged line would have been the conversion inventing a fact.
3. **A test caption states a rule the code does not implement —
   `F-SRC-insight-canon:test_format_detector.cpp`, for this migration's own test tier.**
   `BatchHandlesMixedFormats` opens with *"Majority JSON (2/3) should win"*. The winner is the
   highest cumulative confidence, not the most-claimed format; the fixture's sum and count happen
   to agree, so the test passes without discriminating the two rules. The caption is repaired when
   `core/tests/parse/` converts, and the test that would discriminate them is Kleio's call.

### Witnesses

Comment-only: the code token stream of all three files is identical to `HEAD`'s. Grammar:
`malf format --check core/src/parse` — 103 comment lines, **0 would-be violations**, post format.
Behaviour: `malf test insight-canon` **809 of 809** on clang-21 and **809 of 809** with
`--profile linux-gcc16-release`, equal to the baseline. Lint: the three files read 1, 0, 0
clang-tidy findings, identical to `HEAD`. Count: 273 comment lines at HEAD to 103, a **62 %
reduction**.

---

# The `OPS-8` verdict — third cold reader, first at scale

`insight-canon` is `OPS-8`'s third run and its first large one. Nine findings, ordered by what
they cost. Items 1 and 5 are the ones that change the runbook; item 5 needs a Founder ruling
before the `core/api/` units can be converted at all.

## 1. `OPS-8.S5`'s cross-check equality is FALSE in any unit that has a suppression

The step says: *"The stripper's REMOVED count must equal the unit's would-be violation count from
`OPS-8.S1`, and its KEPT count the tool-form count."* `strip_to_v1.py` does not behave that way and
says so in its own body: it deliberately **keeps** `suppression-without-why` and `trailing-nolint`,
so the hand pass can re-home them. The true identities are

```
removed == violations − (suppression-without-why + trailing-nolint)
kept    == tool-forms  + (suppression-without-why + trailing-nolint)
```

Measured on both units of this run that carry suppressions. Unit 1: 37 violations, 5 kept-as-
violation, removed 32, kept 7 = 2 tool + 5. Unit 4: 112 violations, 2 trailing-NOLINT, removed 110,
kept 4 = 2 tool + 2. **`insight-twin` had zero suppressions, so the equality as written held
VACUOUSLY there** (644 = 644, 8 = 8) and the defect could not show. An operator who follows the step
literally on a unit with suppressions sees a mismatch, and the step gives him no way to tell a
benign one from the silent deletion the cross-check exists to catch.

## 2. The fixed cold-reader prompt forbids only LogCraft's ledger, by name

`interrogation_prompt.md` names `logcraft/technical_docs/operations/ccc_migration.md`. Every
migrated repo now has its own ledger at the same relative path, and **from unit 2 onward the
migrating repo's own ledger contains the previous units' answers, dispositions and stale-claim
findings** — the exact material the reader must not see. The prompt was adapted for every reader in
this run to forbid any file named `ccc_migration.md` in any repo, plus `OPS-8` itself. The fix
belongs in the committed prompt, not in each lane's memory.

## 3. `OPS-8.S3.3` states the indent-aware byte budget; the worked example does not implement it

The step is explicit that the budget *"must be indent-aware, not the flat 100"* and that the limit
counts bytes. The committed per-unit scripts (`tests15_claims.py` … `tests18_claims.py`) check a
flat `150` / `85` **character** count. This run implemented the step as written — the checker
reproduces `wrap_tagged.py`'s own flow at the site's real indentation and rejects any produced line
over 100 bytes — and it **rejected six claims across units 1–4 that the flat check would have
passed**, every one of them inside a function body at indent 8 to 12. The shared implementation is
`claims_lib.py`; the per-unit scripts carry only claims.

## 4. A deletion inside one unit can falsify prose in another, and no step covers it

Measured, and it fired. Unit 2 deleted the CR prevalence figures from `intent_identity.cpp` —
correctly, because `DN-38` and an `insight-eidos` test both carry them. `canon.api.cppm`, a file in
a much later unit, said *"the set's own definition in intent_identity.cpp carries the numbers"*, and
that sentence became false the instant the deletion landed. The unit-2 cold reader caught it.

`OPS-8.S9`'s *wrong* row assumes the wrong line is inside the unit being converted (*"Fix the line
in the tree"*). The missing step is upstream of it: **before deleting a claim, sweep the repo for
prose that POINTS AT that claim, and repair the pointer in the same commit.** A pointer into a unit
that has not converted yet is invisible to every witness the protocol has — the code-only diff, the
grammar gate and the test suite are all green while the repo now contains a sentence its own author
made false.

## 5. STOP AND REPORT — `SRC-<code>` codes are DECLARED in source prose, and this is the repo that declares them

**This is a Founder ruling, not a lane decision, and it blocks `core/api/`.**

`LEXICON.md` defines the form: `SRC-<code>` is *"a decision code whose **contract is the source** —
the comment at its declaring site IS the statement"*. CCC deletes source prose. The two doctrines
meet head-on for the first time in this repo, and the numbers are:

* **66 distinct `SRC-` codes have their site in `insight-canon` source.**
* **61 of the 66 are cited from OUTSIDE the repo** — `technical_docs`, `insight-eidos`,
  `insight-metalog`, `coderoast-server`. (Verified with an explicit positive control after a first
  measurement returned a false zero: the pattern needs `rg -P`, and the run that lacked it exited 2
  while a discarded stderr made it read as "no citations anywhere".)
* **`registry_grammar_lint` cannot protect this, and the reason is sharper than a weak gate.** Its
  G5 leg does fail per missing code (`return 1 if fails else 0`, one `fails` entry per code with no
  site) — an earlier draft of this verdict said it was only a majority floor and that was **wrong**;
  the `missing * 2 > codes` branch is a separate *instrument-broken* tripwire on top of the per-code
  failure. What the gate actually cannot see is the **content**. `src_codes_present` classes a site
  as a DECLARATION by **position** — any site in a `.cppm`/`.hpp`/`.h`/`.ipp`, or any site in a
  `.cpp`'s first 40 lines — and its own docstring says why: *"checking it by POSITION is what makes
  it checkable at all."* So a bare `refs: SRC-D-TID-6` line in `canon.api.cppm` satisfies G5
  **exactly as the full prose paragraph did**. Carry every code into a `refs:` and the gate stays
  green while the text that `ADR-6.D8` and `LEXICON.md` say IS the statement is gone. The loss is
  invisible to every witness this protocol has.
* **9 codes have exactly ONE site inside canon**: seven in `core/api/canon.api.cppm`
  (`SRC-D-OTEL-8`, `SRC-D-TID-6`, `SRC-D-TID-10`, `SRC-D-W1-2`, `SRC-D-W1-4`, `SRC-D-W1-5`,
  `SRC-D-W1-8`), one in `core/src/utils/time_utils.cpp` (`SRC-D-RNK-2`), one in
  `core/tests/strategy/test_span_unpack.cpp` (`SRC-D-OTEL-23`). The last two are BODY sites, so
  canon declares neither — checked, and both declare in sibling repos
  (`insight-eidos/sift/api/sift.api-config.cppm` and `insight-metalog/api/metalog.api.cppm` for
  `SRC-D-RNK-2`; `logcraft/core/api/core.api-agent.cppm` for `SRC-D-OTEL-23`). The declaration
  surface is WORKSPACE-wide, which is why the gate is green today and why reasoning about it
  one repo at a time is unsafe. The seven in `canon.api.cppm` are declared there and nowhere else.
* The density map says where the problem lives: `canon.api.cppm` 96 occurrences, `mask.cpp` 50,
  `failure_lexicon.cpp` 29, `json.cpp` 25, `canon.spi.cppm` 22, `canon.detail.mask.cppm` 16.

`LEXICON.md` names the successor form itself: *"migration to `LSRC-n` is wanted at the next occasion
that opens the declaring site"*. **A CCC conversion is precisely that occasion, and it opens all 66
at once.** `LSRC-n` is the `D-LSRC-n` law block, whose numbering is workspace-global, append-only and
dense; this lane is instructed not to mint one, and the next free number was ruled to be LogCraft's
start. So the doctrine's own prescribed remedy is the one form this run may not create.

Units 1–4 are unaffected and were converted safely, because they **cite** these codes rather than
declaring them: every code was carried into a `refs:` line and the address census confirms none was
lost. That is not available for `canon.api.cppm`, where the prose beside a code IS the code's text.

**What the Founder has to choose between**, stated so the decision is one reading:

1. Give `insight-canon` a law-number range and convert the statement-bearing codes into
   `D-LSRC-n` blocks at their existing sites. Faithful to `LEXICON`, and it is the largest option.
2. Rule that a `refs:`-carrying site satisfies the `SRC-` declaration, and move each statement into
   the ADR or design note that owns its subject, cited from the site. Smaller, and it converts
   `SRC-` from a source-declared code into an ordinary citation.
3. Rule the codes retired for canon, repointing all 61 external citations to their owning slots.
   Largest blast radius, cleanest end state.

**Nothing here is done on this lane's initiative.** This run stopped at the boundary and converted
only units whose codes are citations.

## 6. `OPS-8.S1.4` is CORRECT for a repo that already has a `technical_docs/` shelf — verified, not assumed

The step's three-part branch (create the directory, a roster README **and** a superproject
`ShelfRuling` row) does **not** apply here, and that was checked rather than taken on the brief's
word. `scripts/docs_lint.py`'s `ShelfRuling("insight-canon", SHELF_TREE_PART, ROSTER, …)` covers the
whole `technical_docs` tree recursively, so a new `operations/` subdirectory holding one file needs
no row — only a roster entry. Proven falsifiable: with the entry removed, `docs_lint` exits 1 and
names the file (*"does not link `operations/ccc_migration.md`, which is tracked under this shelf"*);
with it, exit 0. Tracked-doc population 142 → 143.

**One trap worth adding to the step, met here:** these gates must be run from the workspace ROOT.
Run from inside the repo, `python3 scripts/docs_lint.py` exits 2 for a missing file, and an operator
reading only the exit code sees a red gate rather than a mistyped path.

## 7. The census token is a DIRECTIVE, not the string `NOLINT`

Unit 4's census read `NOLINT 3 → 2`. The third occurrence was the **word** "NOLINT" inside a prose
sentence (*"NOLINT for the same non-owning-ref reason as `arena`"*), which the conversion deleted
along with the prose while both real directives survived and were re-homed. Not a defect in the
run, but `OPS-8.S4` says *"every `NOLINT` (all spellings)"* and an operator counting string
occurrences will chase a difference that is not one.

## 8. `tag-mid-line` is a false-positive class on this repo's subject matter

All 11 `tag-mid-line` sites at the baseline are prose containing the substring `note:`, and nine of
them are describing the **compiler diagnostic marker** `<path>:<line>:<col>: note: ` that canon's
failure lexicon parses (the NOTE register, `SRC-D-NOTE-1`). Not one is an author writing in CCC's
style before it was doctrine. The gate's pattern excludes a preceding word character, backtick,
quote, slash, dot, colon or hyphen — so a space before `note:` matches and the diagnostic marker
trips it.

**This is a live constraint on two units not yet converted** (`core/src/utils/failure_lexicon.cpp`,
`core/api/canon.api.cppm`): a legitimate `note:` or `invariant:` whose text must quote the marker
will be rejected. **The escape is a TIGHT backtick** — `` `note:` `` is excluded by the lookbehind,
while `` `: note: ` `` still matches, because the character immediately before `note` is the space.
Verified against the checker's own compiled pattern.

## 9. Measure a suppression by stripping WHOLE comments, never the directive fragment

A first attempt to measure unit 4's suppressions removed `// NOLINT(...)` with a regex. Two of the
sites are trailing comments that **continue onto a second line**, so the removal left the
continuation text sitting where code is expected, and clang-tidy answered with six
`clang-diagnostic-error`s on top of the one real finding. **The tell is `clang-diagnostic-error` in
the output**: it means the measurement broke its own input, and the finding counts from that run are
worthless. The sound procedure is to strip through `strip_to_v1.py` and then delete the kept
trailing suppressions as whole comments; re-measured that way, unit 4 reads 2 findings without the
suppressions and 0 with them.

## Departures from `OPS-8` in this run, declared

* **Units 2 and 3 landed in ONE commit**, against `OPS-8.S10`'s one-commit-per-unit. Both are
  comment-only, both were in the tree together, and a single `malf test` pass on each toolchain is
  their shared behaviour witness. The ledger keeps a separate entry per unit.
* **`canon.api.cppm` was edited outside its unit**, one comment line, to repair the pointer unit 2's
  deletion falsified (finding 4).

---

# Where this run stopped, and why

**Four units converted, the repo NOT armed.** `malf format --check insight-canon` reads **14 230
comment lines and 13 854 would-be violations** against the baseline's 14 489 and 14 242 — 388
violations converted, **2.7 % of the repo**, in four commits. Arming (`OPS-8.S12`) requires the
whole repo at zero and is not reached, so `comment_contract: true` is NOT set and the CCC phase
still counts this repo rather than failing it.

| unit | surface | violations | comment lines | reader |
|---|---|---|---|---|
| 1 | `core/src/arena/` | 37 | 39 → 17 | 10/10 recovered |
| 2 | `core/src/identity/` | 132 | 136 → 34 | 13/13 recovered |
| 3 | `core/src/transport/` + `canon.internal.cppm` | 107 | 109 → 40 | 15/15 recovered |
| 4 | `core/src/tokenizer/` | 112 | 114 → 48 | 14/14 recovered |
| | **total** | **388** | **398 → 139 (65 %)** | **52/52, 0 wrong** |

Forms standing in the repo: `pre` 1 · `post` 3 · `invariant` 34 · `assert` 2 · `note` 22 ·
`refs` 24 · 36 continuations · 254 tool forms. **Zero law blocks.**

**The run stopped on the verdict's finding 5, deliberately and not for lack of time.** The
`SRC-<code>` question is a Founder ruling, and it does not only block `core/api/` — the answer
decides what a `refs:` line MEANS in every remaining unit. Under the smallest reading (a
`refs:`-carrying site is a declaration) the four units converted here are correct as they stand.
Under either of the other two readings, the `refs:` lines this run wrote in units 1–4 are the
wrong shape and would be rewritten. **Converting ten more units under an assumption that may be
overturned would multiply the rework rather than reduce it**, which is why the lane stopped at a
unit boundary with every witness green instead of pressing on.

The units that remain unblocked once the ruling lands — those that only CITE codes rather than
declaring them — are `core/src/parse`, `core/src/scan`, `core/src/compose`, `core/src/conformance`
and the whole test tier. `core/api/canon.api.cppm` (96 `SRC-` occurrences), `core/src/mask/mask.cpp`
(50), `core/src/utils/failure_lexicon.cpp` (29), `core/src/strategy/json.cpp` (25) and
`core/api/canon.spi.cppm` (22) are where the ruling actually bites.

**Falsifiability of the grammar phase, proven on this run's own output.** The repo cannot be armed,
so the arming proof `OPS-8.S12` asks for is not available. What was proven instead, on a converted
unit: the standalone checker reads 0 violations over `arena_allocator.cpp`; appending one bare
prose line makes it report exactly that line at its coordinate (`bare=1`); restoring the file
returns it to 0, byte-for-byte identical (sha256 checked). The gate sees what it claims to see.

---

## RULED — 2026-09-05, and the answer is wider than the three options this ledger offered

The Founder ruled, verbatim: *"`SRC-<code>` is going out. It is weaker then the others. `F-SRC` if
completely resolvable, same for `D-LSRC`. … each `SRC-<code>` become `ref:LSRC`, and for each
missing `D-LSRC`, add it to the most appropriate place."*

**None of the three options above was taken — the form itself is retired, workspace-wide.** Option 1
proposed law blocks for canon's statement-bearing codes; option 2 proposed keeping `SRC-` as a
citation with the statement moved to a slot; option 3 proposed retiring the codes *for canon*. The
ruling retires `SRC-<code>` **everywhere** and makes `LSRC-n` its successor at every citing site. The
criterion he gave is **resolvability**: `F-SRC-<repo>:<file>` and `LSRC-n` each resolve to exactly
one site by construction; `SRC-<code>` never did, which `LEXICON.md` had always said —
*"form 2, and it is NOT stable"*.

`ADR-26.D5` carries the ruling and its measured scope: **108 distinct codes, 1 577 occurrences over
12 surfaces**, of which `insight-canon` holds 476 — the largest share in the workspace, which is why
the collision surfaced here. The frozen corpus keeps the bare form and is not rewritten.

**WHAT THIS LANE MUST NOT DO WHEN IT RESUMES.** The `SRC-` → `LSRC-` cascade is **not** a
comment-only lane's work and must not be improvised inside a CCC unit: it repoints citations in five
repos and the durable doc tier, and a comment-only commit cannot carry it. It is its own pass, run
by the pilot once this repo is flat. What the lane DOES own at a declaring site is `OPS-8.O5`,
below — the two are not the same act and the split is what keeps every gate green.

**THE FORM QUESTION IS ANSWERED AND UNITS 5+ RESUME.** The same ruling ended *"Donc forget the
multiline comment form for `D-LSRC` -> `/******** D-LSRC-n *********/`"*, which this ledger read as
possibly retiring the block's prose body. The Founder closed it on the same day by pointing at the
already-converted specimen in the programme's design note and ruling *"D-LSRC have multiline
comments"*. **The body stays**: a rule line, the `D-LSRC-n — <title>` line, free prose, a closing
rule — and the frame is the **host language's** comment syntax, not C's, which is what `LSRC-3`
(a `CMakeLists.txt`) and `LSRC-4` (a conan profile) already are in `#`. So a `refs:` line points at
a statement that exists at the site, and units 1–4's `refs:` lines need no reshaping.

**Minting is unheld, but this lane still does not pick a number.** Law numbers are workspace-global,
append-only and checked dense; the next free integer is **5**, and the pilot issues them one at a
time (`OPS-8.O4`). Ask before writing a block.

**The `SRC-` work this lane DOES own is `OPS-8.O5`, and it is narrower than the cascade.** A
**citing** site keeps its `refs: SRC-<code>` form untouched. A **declaring** site — the seven codes
whose statement lives in `core/api/canon.api.cppm` — becomes a law block at that same site naming
the code it absorbs, which is what keeps `registry_grammar_lint` G5 green while every external citer
still resolves. Record each code's citer list; repoint nothing outside this repo.

**Units 1–4 stand either way** and needed no change: they **cite** codes rather than declaring them,
and the address census confirms none was lost.
