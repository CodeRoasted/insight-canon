# Classification — level, failure/outcome cues & structural roles

What canon decides *about* a line, beyond its template. Three signals, all computed per-line on the
escape-stripped bytes: the **log level**, whether the line is a **failure / warning outcome**, and its
**structural role**. These drive how downstream layers weigh the line — so the central discipline is
**precision**: a line must not be called a failure unless it is one.

The tokenizer here is the **structural** one (splits on whitespace *and* structural punctuation
`[ ] ( ) { } < > " ' \` , : ; | =`, trims surrounding non-alphanumerics, skips ANSI runs) — distinct from the
whitespace-only tokenizer used by masking.

---

## 1. Log level

`LogLevel` is `Trace < Debug < Info < Warn < Error < Fatal`, plus `Unknown`. A strategy may read an explicit
level from structured input ([formats.md](formats.md)); when it does not, canon infers one from the line head.

### 1.1 Two-stage inference

`infer_leading_log_level` scans the line **head** (≈ first 40 chars for the explicit token, ≈ 64 for the cue
path) in two stages:

- **Stage 1 — explicit level token.** `parse_log_level` matches a level word:

  | Words | Level |
  |---|---|
  | `trace` | Trace |
  | `debug`, `dbg` | Debug |
  | `info`, `information` | Info |
  | `warn`, `warning` | Warn |
  | `error`, `err`, `severe` | Error |
  | `fatal`, `failure`, `critical`, `crit` | Fatal |

  A leading level word is **authoritative only when it is anchored** — in verdict register (caps / `:` /
  bracket, see §2.2) **or** it is the terminal/sole significant token in the head. An unanchored level word
  buried mid-sentence (`failure modes documented`) does **not** set the level by itself; it falls through to
  Stage 2. This stops a descriptive sentence that merely contains a level word from being classified as that
  level.

- **Stage 2 — failure / warning cue.** If no authoritative explicit level, the failure lexicon (§2) decides:
  a failure cue → `Error`; a warning cue → `Warn`; otherwise `Unknown`.

### 1.2 The outcome guard (passing tests are not failures)

Both stages are gated by an **outcome guard**: when the inferred level is *alerting* (Warn/Error/Fatal) **and**
the line is led by an unambiguous **pass glyph** (`✓`/`✔`/`✅`/`√`), the level is demoted to `Unknown`. This is
why a passing test whose **name** embeds failure vocabulary —
`✔ write_bash failure returns a non-empty error message` — is not flagged as a failure. The guard is paid only
on would-be-alerting lines, and it fires on a leading pass *glyph* (an unambiguous per-test verdict marker),
**not** a pass *word*: a leading word like `passed` would false-demote a real summary such as
`25 passed, 5 failed` (where "passed" is a count, not a verdict).

> A declared OTEL `severityNumber` ([formats.md](formats.md) §4.1) is stronger than any inferred level and
> overrides it.

---

## 2. The failure / warning lexicon

The core precision question: *does a failure word on this line mean the line is a failure?* Not always — test
names, prose, and config all carry failure vocabulary. canon answers with a **verdict-register** model.

### 2.1 The lexicon, partitioned by collision-proneness

Failure words are partitioned not by grammar but by how often they appear **benignly**:

- **Self-anchoring** (zero benign collision → fires bare, anywhere): `failed`, `refused`, `aborted`,
  `crashed`, `panicked`, `denied`, `segfault`, `traceback`, `unhandled`. These are either inflected outcome
  verbs whose morphology *is* the verdict ("build **failed**", "connection **refused**") or unique failure
  nouns that essentially never modify benignly ("**segfault**", "**traceback**", "**unhandled** exception").
- **Register-anchored** (real benign senses → fires **only** in verdict register): `error`, `exception`,
  `fatal`, `panic`, `timeout`, `fail`, `failure`, `abort`, `crash`. Each has an everyday non-failure use
  ("error rate", "crash course", "timeout=30", "fatal flaw", "panic button"), so a bare lowercase occurrence in
  prose does **not** classify — it needs an outcome decoration to count.

Plural `errors` is intentionally omitted (so "no errors found" / "0 errors" is not read as an error). Warning
words are `warn`, `warning`.

### 2.2 Verdict register — the anchors

A register-anchored word fires only when one of these structural **anchors** holds — the same decoration CI/test
tooling uses to mark an outcome:

1. **Caps register** — the raw token is ALL-UPPERCASE, ≥ 2 letters (`ERROR`, `FAILED`, `FATAL`, `PANIC`).
2. **Delimiter-bound** — immediately followed by `:` (`error:`, `fatal:`), or enclosed `[…]` / `(…)`
   (`[error]`, `(FAILED)`, `##[error]`).
3. **Leading fail glyph** — the line is led by a ballot-X glyph (`✗`/`✕`/`✖`/`✘`), which *confirms* an
   already-matched failure word on that line. (It only confirms an existing word; a glyph-only line with no
   failure word stays silent. `×` U+00D7 is excluded — it doubles as a dimension separator, `1920×1080`.)

Two more structural cues complete the picture:

- **CamelCase error-type** — a name ending in `Error`/`Exception` preceded by a letter (`ValueError`,
  `RuntimeException`) is a *weak* failure signal, **negation-aware** (`IsNotError` does not count). A weak
  type-name signal is demoted by **any** success word anywhere on the line (`passed`/`ok`/`success`/
  `succeeded`) — so a passing test named `…RaisesValueError` is not a regression.
- **Phrase** — `segmentation fault` fires only as the adjacent pair (a bare `segmentation` or `fault` collides
  with benign uses).

### 2.3 The outcome predicates

Two small predicates encode the verdict polarity, both walking the head left-to-right and deciding on the
**first outcome-bearing token**:

- `leading_outcome_is_pass` — first outcome token is a pass glyph → true (drives the §1.2 demotion). A failure
  word first → false (a failure leads).
- `leading_outcome_is_fail` — first outcome token is a ballot-X fail glyph → true (drives anchor #3 above). Used
  only to confirm an existing failure word, never to invent one.

`contains_failure_cue` / `contains_warning_cue` are the public entry points; they apply the lexicon, the
anchors, the phrase, the type-name rule, and the pass-glyph demotion in one head-bounded pass. They run pure
byte-compare + ASCII case-fold — no allocation, no float.

### 2.4 Why this is a contract, not a denylist

The precision criterion is **benign-collision-proneness**, declared per word — not a reactive list of phrases
to suppress. A collision-prone word like `crash` is never *removed* from the lexicon; it is declared as needing
verdict register, uniformly, exactly as capitalization already distinguishes a verdict-bearing `ValueError`
from a lowercase `error` in a path. This is the same discipline masking follows: decidable structural rules,
not data-tuned exceptions.

---

## 3. Structural roles

`StructuralRole` marks a line's role in the log's section structure, assigned from **announced markers only**
(never positional or inferred):

| Role | Marker |
|---|---|
| `GroupBegin` | `##[group]` / `::group::` |
| `GroupEnd` | `##[endgroup]` / `::endgroup::` |
| `Terminator` | `##[error]` / `::error::` |
| `None` | (the common case) |

The role registry is a declared seed catalog (marker-based today; it grows only when a scenario surfaces a new
announced marker that earns its place).

---

*See also: [masking.md](masking.md) (template identity for the same line) · [formats.md](formats.md) (explicit
level/severity from structured input) · [determinism.md](determinism.md) (every predicate here is byte-exact and
order-independent).*
