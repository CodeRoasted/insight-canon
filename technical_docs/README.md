# insight-canon — Technical Reference

`insight-canon` is the **ingest layer** of the log-analysis pipeline: it turns a raw log line into a
normalized, format-agnostic **`CanonicalEvent`** that downstream layers process without understanding any log
syntax. It is a self-contained C++23 static library (Apache-2.0), consumed via the single CMake target
`insight::canon`.

This reference describes the **current shipped state** of the engine (the rules generation named by
`kCanonicalizationVersion` in `core/api/canon.api.cppm`, which owns the value). It is split by *kind of work* so each part stays maintainable on its own — when one
subsystem's rules change, exactly one doc moves with it.

## The doc map

| Doc | The kind of work it owns |
|---|---|
| **[formats.md](formats.md)** | Ingest normalization (escape stripping), format detection, and the per-format **field extraction** roster — how a raw line becomes structured fields (`timestamp`, `level`, `component`, `host`, `content`, trace, ordinals). |
| **[masking.md](masking.md)** | The per-token **masking** rule set that turns `content` into a stable `template_str` / `template_id` — every keep-class / mask-instance rule, every declared marker catalog, and the boundary of what canon deliberately does **not** mask. |
| **[classification.md](classification.md)** | The **semantic classification** canon emits: log **level** (two-stage inference), the **failure / warning / outcome** lexicon (verdict-register awareness, pass/fail glyphs), and **structural roles**. |
| **[determinism.md](determinism.md)** | The cross-cutting **determinism contract** — why every rule above is byte-exact and order-independent, and what the `canonicalization_version` gate guarantees. |

## Pipeline position

```text
Raw log line
  → insight-canon            → CanonicalEvent   (this library)
  → insight-metalog          → MetaLogDocument  (the reference impl of the open MetaLog spec)
  → insight-eidos            → detection + explanation
```

canon is the upstream layer. It emits the `CanonicalEvent` stream consumed by **insight-metalog**
([MetaLog specification](https://github.com/CodeRoasted/metalog-spec)); downstream, **insight-eidos** performs
detection and explanation. canon's responsibility **ends** at the `CanonicalEvent` — it classifies and
canonicalizes single lines; it never decides significance, diffs windows, or detects regressions (those are
metalog/eidos concerns).

## The output contract — `CanonicalEvent`

One raw line in (`Tokenizer::process_line` / `process_stable_line`), one `CanonicalEvent` out:

| Field | Type | Meaning |
|---|---|---|
| `id` | `EventID` (`u64`) | Monotonic id, assigned in tokenizer input order. |
| `timestamp` | `Timestamp` | Parsed event time when present; otherwise a fallback. |
| `level` | `LogLevel` | Normalized severity (`Trace`…`Fatal`, or `Unknown`) — see [classification.md](classification.md). |
| `format` | `LogFormat` | The routed strategy winner for this line — **per-line observability metadata, not deterministic content.** |
| `component` | `string_view` | Low-cardinality functional source (subsystem / daemon / service) — see [formats.md](formats.md). |
| `host` | `string_view` | High-cardinality node / host identity (kept out of low-card grouping). |
| `template_str` | `string_view` | The masked template (`"Connection from <*> port <*>"`) — see [masking.md](masking.md). |
| `params` | `span<string_view>` | The raw values masked out of the template, in order. |
| `structural_role` | `StructuralRole` | Announced section/outcome marker (`GroupBegin`/`GroupEnd`/`Terminator`/`None`). |
| `trace` | `OtelTraceContext` | OTEL trace/span context when the input carried it — **in-memory only, never serialized.** |
| `ordinals` | `span<OrdinalObservation>` | Declared numeric observations (latency/size) — **consumed by metalog, never tokenized into the template.** |

All `string_view`s point into the tokenizer's `ArenaAllocator` and are valid until the arena is reset or
destroyed. `ParsedLine` is the per-strategy intermediate that feeds the masker; it carries the same field set
minus the masked `template_str`/`params`/`id`.

## Cross-cutting contracts

These hold for every subsystem (the detail is in [determinism.md](determinism.md)):

- **Deterministic** for the same ordered input stream — bit-identical across compilers/stdlibs/OSes. No
  floating point in any path that feeds `template_str`/`level`/`component`; no dependence on unordered-map
  iteration order in any output-affecting path.
- **Not thread-safe.** `Tokenizer`, `LogParser`, and `ArenaAllocator` are single-threaded — use one instance
  per worker / source.
- **Arena-scoped lifetime.** Downstream must not retain `CanonicalEvent` string views beyond the arena
  lifetime.
- **Privacy.** Raw input lines are never written by diagnostics.

## Source & test layout

```text
api/            canon.api.cppm (public types + contracts), canon.cppm (the Tokenizer facade)
src/
  parse/        log_parser (orchestration), format_detector
  scan/         escape stripping + token-scan helpers
  strategy/     one IFormatStrategy per supported format
  mask/         the stateless masker (token → template)
  identity/     template_id (SHA-256 of template_str)
  tokenizer/    the Tokenizer engine
  utils/        failure_lexicon, time_utils (level inference), logger
tests/<domain>/ per-domain mirror of src/  ·  proof/  determinism corpus  ·  benchmarks/
```

Unit coverage mirrors `src/` under `tests/<domain>/`; LogHub regression fixtures live under
`data/logs/loghub/` (missing fixtures are reported skipped so local builds stay portable).
