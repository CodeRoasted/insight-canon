# Formats — ingest normalization, detection & field extraction

How a raw line becomes structured fields. Three steps, in order: **normalize** (strip presentation escapes),
**detect** (pick a format strategy), **extract** (the strategy fills `ParsedLine`). The result feeds masking
([masking.md](masking.md)) and classification ([classification.md](classification.md)).

---

## 1. Ingest normalization — escape stripping (before everything)

The **first** thing canon does to a line, *before* format detection and *before* tokenization, is strip
terminal escape sequences (`LogParser::parse_line` → `normalize`, the stage-1 factory returning a
`NormalizedLine` — the type that carries the proof stage 1 ran, and the only road to the
`NormalizedContent` the recognition walkers accept):

- **What is stripped:** CSI / SGR colour sequences (`ESC [ … m`), OSC sequences (`ESC ] … BEL`/`ST`), and bare
  two-byte `ESC` sequences. A pure byte state machine.
- **Why here, unconditionally:** colour is **presentation, never content**. The escapes interleave within and
  between tokens (`\x1b[31mERROR\x1b[0m: foo`), so a per-token mask downstream could not reach them — they must
  die at ingest. Stripping first means format detection, level inference, and `component` extraction all see
  colour-free bytes.
- **Consequence (load-bearing):** any signal that lives **only** in the escape bytes is gone before any
  strategy or classifier runs. A format strategy never sees the original SGR colours — only the cleaned text.

A line that is entirely escape bytes is dropped (empty after stripping).

---

## 2. Format detection

`FormatDetector` picks the strategy by **confidence vote**: each registered strategy exposes an O(1)
`confidence(line) → [0,1]`; the highest score above `0.0` wins. `LogParser` keeps a **sticky** winner — once a
stream's format is known, the sticky strategy is tried first and re-confirmed cheaply before a full re-detect.

- **Fallback:** `RawTextStrategy` always returns confidence `0.0` (so it never greedily captures a structured
  line) and is used only when no structured strategy matches a non-empty line.
- **Forcing a format:** a caller may pin a `LogFormat` (disables per-line auto-detect); an unknown format falls
  back to auto-detect.
- `CanonicalEvent.format` reports the routed winner per line — **observability only**, never part of
  deterministic content.

---

## 3. The strategy roster & field extraction

Each strategy implements `IFormatStrategy` (`parse` / `format` / `confidence`) and fills `ParsedLine`:
`timestamp`, `level`, `component`, `host`, `content` (the message body fed to the masker), and — for JSON only
today — `trace` and `ordinals`. `level` here is the strategy's *explicit* read; lines with no explicit level
fall through to the level-inference path in [classification.md](classification.md).

| Strategy (`LogFormat`) | Timestamp | `level` source | `component` source | `host` | Notes |
|---|---|---|---|---|---|
| **JSON** | `kTimestampKeys` | `kLevelKeys` (or OTEL `severityNumber`, which **overrides**) | `kComponentKeys` | — | OTEL- and ordinal-aware (§4). Fast path for escape-free input, simdjson slow path otherwise. |
| **KeyValue** | per-key `timestamp` value | per-key `level` value | first matched key value | — | `key=value` / logfmt. |
| **Syslog** | BSD or RFC3339 prefix | — | daemon/tag (`tag[pid]:`) | — | Two prefix shapes. |
| **RFC5424** | RFC3339 | PRI value → level | APP-NAME | HOSTNAME | Structured syslog. |
| **Log4j** | `YYYY-MM-DD HH:MM:SS,mmm` | explicit level word | thread/component (variant) | — | Hadoop/Zookeeper/OpenStack variants. |
| **SparkHDFS** | `YY/MM/DD` or `YYMMDD HHMMSS` | explicit level word | component | — | Spark + HDFS. |
| **BGL** | decimal epoch | explicit level (else inferred) | subsystem (low-card) | node (high-card) | Splits low-card `component` from high-card `host`. |
| **CLF** | `10/Oct/2000:13:55:36 -0700` | HTTP status → level | host field | — | Common/Combined access logs. |
| **IIS W3C** | `YYYY-MM-DD HH:MM:SS` | HTTP status → level | — | — | IIS extended format. |
| **NginxError** | `YYYY/MM/DD HH:MM:SS` | `[level]` bracket | — | — | nginx error log. |
| **ApacheError** | `[Wkd Mon DD HH:MM:SS YYYY]` | `[level]` bracket | `"httpd"` (constant) | — | apache error log. |
| **AndroidLogcat** | `MM-DD HH:MM:SS.mmm` | priority letter → level | tag | — | Zero-copy fast scan. |
| **GitHubActions** | RFC3339 (100-ns / `Z`) | inferred from content (the `##[error]` / `::error::` LIFT is applied above the strategy — see below) | — | — | Strips the leading timestamp; the workflow-command marker is KEPT in the content. (Note: ANSI colour already removed at ingest, §1.) |
| **WindowsCBS** | `YYYY-MM-DD HH:MM:SS` | explicit level word | component | — | Windows Component-Based Servicing. |
| **SystemdJournal** | `__REALTIME_TIMESTAMP` (µs) | `PRIORITY` | `_COMM` | — | journal export (JSON-shaped). |
| **CloudWatch** | millis field | (JSON path) | (JSON path) | — | AWS CloudWatch JSON. |
| **HealthApp** | `YYYYMMDD-HH:MM:SS:mmm` | — | pipe-delimited field | — | |
| **HPC** | decimal epoch | — | space-delimited field | — | |
| **Proxifier** | (coarse → none) | — | process name | — | Timestamp too coarse → `Unknown`. |
| **RawText** | — | inferred from content | (empty) | — | Fallback; confidence always `0.0`. |

`component` is the **low-cardinality functional source** (a subsystem/daemon, a small stable set — the useful
grouping dimension); `host` is the **high-cardinality node identity**, kept separate so it never explodes the
grouping. Only BGL and RFC5424 populate `host` today.

> **The DECLARED level lift is not a strategy's read.** A semantic package may declare `LevelLiftRow`s —
> a prefix that lifts the line's level (`##[error]` → Error). Those rows are **data**; the walk is canon's
> (`insight::tokenization::lift_level` over `ComposedSemantics::level_lifts()`), and `LogParser` applies it
> to every parsed line right after the strategy returns, gated on the routed format. So the lift **overrides**
> whatever the strategy put in `level`, and the table column above describes only what the strategy itself
> reads. Exactly one rule outranks the lift in turn: the echoed-source demotion (SRC-D-PROV-1), which drives an
> echoed script line to `Unknown` whatever any earlier stage decided.

> **Known gap (JSON nested fields):** the JSON strategy reads `component`/`level` only at the **top level**.
> Loggers that nest custom fields under a `"fields": { … }` object leave `component` empty. The descent
> pattern already exists for OTEL bodies (`body.stringValue`); the component path does not yet use it.

---

## 4. Declared structured-field catalogs (JSON)

canon recognizes a small set of **schema-declared** structured fields by **exact top-level key** — declared,
never data-learned, so they need no registry. Two families:

### 4.1 OTEL trace context & severity

| Class | Key | Routes to | Use |
|---|---|---|---|
| `TraceId` | `traceId` | `OtelTraceContext.trace_id` | trace grouping |
| `SpanId` | `spanId` | `OtelTraceContext.span_id` | causal vertex |
| `ParentSpanId` | `parentSpanId` | `OtelTraceContext.parent_span_id` | causal edge |
| `SeverityNumber` | `severityNumber` | `LogLevel` band (declared **>** inferred) | severity |

Trace/span ids are content-hashed (FNV-1a-64 of the hex; zero = absent) — same hex → same id, byte-only,
deterministic. `trace` is consumed downstream for grouping/DAG and is **never serialized** (it would be a
cardinality bomb). `severityNumber` maps `1–24 → Trace…Fatal` by integer band (clamped; the raw number is
discarded) and **overrides** any text-inferred level.

### 4.2 Ordinal observations (numeric drift carrier)

A declared catalog of numeric fields (by exact top-level key) is parsed into a **canonical integer** unit and
carried as `OrdinalObservation { field_name, schedule, value }`:

- **Duration** fields (`latency_ms`, `duration_ms`, `elapsed_ms`, `response_time_ms`, `latency_us`,
  `duration_us`, `latency_ns`, `duration_ns`, `duration_seconds`, `elapsed_seconds`) → canonical **nanoseconds**.
- **Size** fields (`response_bytes`, `request_bytes`, `size_bytes`, `payload_bytes`) → canonical **bytes**.

Parsing is **decimal-text → int64** scaled by a power of ten (exact fixed-point, **no float**); negative /
overflow / exponent → omitted. Ordinals are **consumed by metalog** (distribution-drift binning) and are
**never tokenized into the template** — so a varying latency value never fragments template identity.

---

*See also: [masking.md](masking.md) (what happens to `content` next) · [classification.md](classification.md)
(how `level` is inferred when a strategy leaves it `Unknown`) · [determinism.md](determinism.md).*
