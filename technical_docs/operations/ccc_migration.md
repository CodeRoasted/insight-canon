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
