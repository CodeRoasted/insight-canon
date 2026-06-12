# insight-canon Technical Documentation

Technical reference for the insight-canon library: tokenization and canonical events.

## Read Order

1. [tokenization.md](tokenization.md) — raw log line to `CanonicalEvent`: format detection, Drain clustering, arena allocator, and the public `Tokenizer` API.

## Pipeline Position

```text
Raw log line
  -> insight-canon tokenization  ->  CanonicalEvent
  -> insight-metalog             ->  MetaLogDocument
  -> insight-eidos               ->  DetectionReport + Insight
```

insight-canon is the upstream layer of the pipeline. It emits the `CanonicalEvent` stream consumed by **insight-metalog**, the reference implementation of the open [MetaLog specification](https://github.com/CodeRoasted/metalog-spec); downstream, **insight-eidos** performs detection and explanation.
