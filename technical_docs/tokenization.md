# Tokenization (insight-canon)

Status: shipped. Repo: insight-canon (single package). The public surface is the `insight.canon` module facade (`api/insight/canon.cppm` — the `Tokenizer`); the tokenization internals live in the sealed `insight.canon.detail.{scan,strategy,mask,parse}` shards under `src/insight/`.

Tokenization turns raw log lines into `CanonicalEvent` records that downstream layers can process without understanding log-format syntax. It owns format detection, structured parsing, stateless per-line template masking, wildcard parameter extraction, and arena-backed string lifetime.

## Input

The phase accepts one raw log line at a time as `std::string_view`.

Callers can use `Tokenizer::process_line()` for ordinary input or `Tokenizer::process_stable_line()` when the caller guarantees the line storage remains valid for the duration of the call. The engine package normally wraps this through `InsightPipeline::ingest_line()` or `InsightPipeline::ingest_stable_line()`.

## Output

The output is `insight::tokenization::CanonicalEvent`:

| Field | Meaning |
|---|---|
| `id` | Monotonic `EventID` assigned in tokenizer input order. |
| `timestamp` | Parsed UTC timestamp when present; otherwise a fallback timestamp. |
| `level` | Normalized `LogLevel` (`Trace` through `Fatal`, or `Unknown`). |
| `component` | Parsed service, process, host, or source component when available. |
| `template_str` | Stable template string with variable tokens replaced by wildcards. |
| `params` | Variable tokens extracted from the raw line. |
| `session_key` | Optional session hint for later MetaLog session-aware behavior. |

All string views in a `CanonicalEvent` point into the tokenizer's `ArenaAllocator`. They remain valid until the arena is reset or destroyed.

## Flow

```text
raw line
  -> FormatDetector selects the highest-confidence strategy
  -> LogParser produces ParsedLine metadata and content
  -> the stateless masker forms the per-line masked template + params
  -> Tokenizer returns CanonicalEvent
```

## Supported Formats

The tokenizer currently covers JSON, BSD/RFC3339/RFC5424 syslog, CLF/Combined access logs, key-value/logfmt, Log4j/Python style logs, Spark/HDFS, BGL/Thunderbird, Android Logcat, Apache error, Windows CBS, HealthApp, Proxifier, HPC, Nginx error, IIS W3C, CloudWatch, systemd journal, plain text, and fallback parsing.

## Configuration

The important knobs live below the tokenizer facade:

| Area | Knob | Effect |
|---|---|---|
| Arena | arena capacity | Bounds string storage for one tokenizer lifetime/window. |
| Masker | `MaskConfig` (mask IPv4 / hex address tokens) | Which token classes are masked before the template is formed. |
| Format strategies | registered strategy set | Controls which log formats participate in detection. |

The default `InsightPipeline` constructs the tokenizer with the shipped defaults. Direct package users may pass a `MaskConfig` to the `Tokenizer` when they need different masking behavior.

## Contracts

- Tokenization is deterministic for the same ordered input stream.
- `Tokenizer`, `LogParser`, and `ArenaAllocator` are not thread-safe; use one instance per worker or source.
- Output-affecting paths must not depend on unordered-map iteration order.
- Raw input lines must not be logged by diagnostics.
- Downstream phases must not retain `CanonicalEvent` string views beyond the arena lifetime.

## Consumers

- insight-canon sequence layer consumes `CanonicalEvent::id` ordering to build transitions and n-grams.
- insight-metalog consumes `CanonicalEvent::template_str`, `params`, level, timestamp, and optional session key to build MetaLog windows.
- The insight-playground package (in coderoast-server) uses LogCraft fixtures and scenarios to keep parser coverage and deterministic behavior regression-testable.

## Validation

Unit coverage lives under `tests/<domain>/` (the per-domain mirror of `src/insight/` — strategy/mask/parse/tokenizer/arena/utils/math). Loghub regression coverage uses fixtures under `data/logs/loghub/` when present; missing fixtures are reported as skipped so local builds remain portable. Benchmarks live under `benchmarks/` and are included in the aggregate pipeline performance reports.
