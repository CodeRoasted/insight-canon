# Formats — ingest normalization, detection & field extraction

How a raw line becomes structured fields. Three steps, in order: **normalize** (strip presentation escapes),
**detect** (pick a format strategy), **extract** (the strategy fills `ParsedLine`). The result feeds masking
([masking.md](masking.md)) and classification ([classification.md](classification.md)). Between extraction and
masking, one normal form runs on the extracted `content`: a whole-line JSON value has its object members put in
name order (§5).

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
| **Syslog** | BSD (yearless, see below) or RFC3339 prefix | inferred from the message body | daemon/tag (`tag[pid]:`) | — | Two prefix shapes. Claims a line only on the full syslog HEADER (`TIMESTAMP HOST TAG:`), never on the timestamp alone; the tag search is bounded to ONE token. |
| **RFC5424** | RFC3339 | PRI value → level | APP-NAME | HOSTNAME | Structured syslog. |
| **Log4j** | `YYYY-MM-DD HH:MM:SS,mmm` | explicit level word | thread/component (variant) | — | Hadoop/Zookeeper/OpenStack variants. |
| **SparkHDFS** | `YY/MM/DD` or `YYMMDD HHMMSS` | explicit level word | component | — | Spark + HDFS. |
| **BGL** | decimal epoch | explicit level (else inferred) | subsystem (low-card) | node (high-card) | Splits low-card `component` from high-card `host`. |
| **CLF** | `10/Oct/2000:13:55:36 -0700` | HTTP status → level | (empty) | client IP / hostname | Common/Combined access logs. The client IP is a NODE identity, so it is `host`, never a cube dimension; the layout declares no functional source and says so. |
| **IIS W3C** | `YYYY-MM-DD HH:MM:SS` | HTTP status → level | — | — | IIS extended format. |
| **NginxError** | `YYYY/MM/DD HH:MM:SS` | `[level]` bracket | — | — | nginx error log. |
| **ApacheError** | `[Wkd Mon DD HH:MM:SS YYYY]`, or 2.4's `[Wkd Mon DD HH:MM:SS.f YYYY]` (1–9 fraction digits, checked and never read) | the level seat, the bracket right after the clock: `[level]` (2.2) or `[module:level]` (2.4, split at the LAST colon); Apache's `trace1`–`trace8` read Trace in this seat only | the seat's module; `"httpd"` when the seat holds none | — | Apache httpd error log, 2.2 and 2.4 (`DN-43.D21`). A line with no clock bracket (httpd's startup `AH00558`) is not claimed. |
| **AndroidLogcat** | `MM-DD HH:MM:SS.mmm` (yearless, see below) | priority letter → level | tag | — | Zero-copy fast scan. |
| **WindowsCBS** | `YYYY-MM-DD HH:MM:SS` | explicit level word | component | — | Windows Component-Based Servicing. |
| **SystemdJournal** | `__REALTIME_TIMESTAMP` (µs) | `PRIORITY` | `_COMM` | — | journal export (JSON-shaped). |
| **CloudWatch** | millis field | (JSON path) | (JSON path) | — | AWS CloudWatch JSON. |
| **HealthApp** | `YYYYMMDD-HH:MM:SS:mmm` | — | pipe-delimited field | — | |
| **HPC** | decimal epoch | — | space-delimited field | — | |
| **Proxifier** | `[MM.DD HH:MM:SS]` (yearless, see below) | — | process name | — | No level column → `Unknown`. |
| **Rfc3339Text** | RFC3339 prefix token | inferred from the message body | (empty) | — | The leading-RFC-3339 LAYOUT: a stamp then free text, no vocabulary. Claims exactly the lines Syslog's header predicate rejects, so the two are disjoint. Keeps the event time; names no functional source. |
| **RawText** | — | inferred from content | (empty) | — | Fallback; confidence always `0.0`. |

> **Yearless stamps take their year from the stream, or have no time.** BSD syslog, logcat and
> Proxifier write a month, a day and a clock but no year, and so does a BSD value in a JSON or KeyValue
> timestamp field. The strategy never builds an instant from one: it returns a *yearless* `EventTime`, and the
> `Tokenizer` resolves it against the event time of the stream's last line that had one — the year among that
> line's year −1, +0 and +1 whose instant is nearest, the later year on an exact tie, a Feb 29 only in a leap
> year — and the resolved instant becomes the next line's reference, so a chain crosses New Year with no new
> anchor. Before any timed line, and after every `declare_context`, the time is **absent**: canon never writes a
> year it did not read (no fixed year, no wall clock). A resolved time is parsed, never declared. A date inside
> the message body is content and never an anchor. `CanonicalEvent::timestamp` is `std::optional<Timestamp>`,
> so a real 1970-01-01T00:00:00Z is a time and absence is the empty optional; the projection prints both as
> `timestamp_ns` `0`.

`component` is the **low-cardinality functional source** (a subsystem/daemon, a small stable set — the useful
grouping dimension); `host` is the **high-cardinality node identity**, kept separate so it never explodes the
grouping. Only BGL and RFC5424 populate `host` today.

> **The DECLARED level lift is not a strategy's read.** A semantic package may declare `LevelLiftRow`s —
> a prefix that lifts the line's level (`##[error]` → Error). Those rows are **data**; the walk is canon's
> (`insight::tokenization::lift_level` over `ComposedSemantics::level_lifts()`), and `LogParser` applies it
> to every parsed line right after the strategy returns, gated on the routed format. So the lift **overrides**
> whatever the strategy put in `level`, and the table column above describes only what the strategy itself
> reads. Exactly one rule outranks the lift in turn: the echoed-source demotion (`ADR-20.D5`), which drives an
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

## 5. Between extraction and masking — the JSON member-order normal form

RFC 8259 § 4 makes an object an unordered collection, so the ORDER of its members is presentation, as an escape
sequence is (§1). A producer that prints the same object from a map prints it in a different order run to run,
and without a normal form each order is its own template. So when the `content` a strategy extracted is, whole,
one RFC 8259 object or array (blanks around it allowed, nothing else), the masker's entry
(`stateless_template`) first gives it its **member-order normal form**:

- **What moves.** The members of every object, recursively, are **permuted** into ascending order of their
  names' bytes, each name compared **after unescaping** (`\uXXXX` pairs as UTF-8, a lone surrogate as its 3-byte
  form). A member's text (name, blanks, colon, value) moves whole; the separator text between the i-th and the
  (i+1)-th member stays in that position; **array elements never move**. Permuting rather than re-serializing
  keeps every whitespace token the masker reads, so no masking decision inside the line moves.
- **What is left as it is, byte for byte.** A line already in order; a line that is not, whole, one strict RFC
  8259 value (trailing text, a raw control byte or an undeclared escape in a string, a trailing comma); an
  object that repeats a member name; a value nested past the declared bound (`kMaxDepth`, 128 levels), so no
  line drives the parse without limit.
- **Where it reaches.** The `content`, never the raw line: a line the JSON strategy claims gives the masker its
  message field, so J reaches that field only when the field itself is a JSON value. A rewritten `content` is
  stored in the event's arena and the masked values' `params` are views into it.

It is a function of one line's bytes and holds no state, so it is part of the masking generation
(`kCanonicalizationVersion`) and moves `template_str`, `template_id` and the order of `params`, never a field
this document extracts.

---

*See also: [masking.md](masking.md) (what happens to `content` next) · [classification.md](classification.md)
(how `level` is inferred when a strategy leaves it `Unknown`) · [determinism.md](determinism.md).*
