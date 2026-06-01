# insight-canon Technical Documentation

Technical reference for the insight-canon library: tokenization, canonical events, and sequence summaries.

## Read Order

1. [tokenization.md](phases/tokenization.md) — raw log line to `CanonicalEvent`: format detection, Drain clustering, arena allocator, and the public `Tokenizer` API.
2. [sequence.md](phases/sequence.md) — event ordering, transition graph, bounded n-gram counters, dominant-path reconstruction, and the `SequenceEngine` API.

## Pipeline Position

```text
Raw log line
  -> insight-canon tokenization  ->  CanonicalEvent
  -> insight-canon sequence      ->  SequenceEngine summaries
  -> insight-metalog             ->  MetaLogDocument
  -> insight-eidos               ->  DetectionReport + Insight
```

insight-canon is the upstream layer of the pipeline. It emits the `CanonicalEvent` stream consumed by **insight-metalog**, the reference implementation of the open [MetaLog specification](https://github.com/CodeRoasted/metalog-spec); downstream, **insight-eidos** performs detection and explanation.
