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
| `transport.cpp`, `canon.internal.cppm` | 111 → 40 | pre 1 · post 1 · invariant 12 · assert 1 · note 4 · refs 6 · 13 continuations · 2 tool |

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
comment lines, **0 would-be violations**, post format. Count: 111 comment lines at HEAD to 40, a
**64 % reduction**.
