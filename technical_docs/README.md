# insight-canon Technical Documentation

Technical reference for the insight-canon library: tokenization, canonical events, and sequence summaries.

Cross-repo status, package pins, and planning live in the parent docs: [../../technical_docs/README.md](../../technical_docs/README.md) and [../../technical_docs/ROADMAP.md](../../technical_docs/ROADMAP.md).

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

## Cross-Project Map

| Project | Role | Start here |
|---|---|---|
| insight-canon | This repo: tokenization, sequence, shared core types | This folder |
| insight-metalog | MetaLog producer — consumes `CanonicalEvent` and sequence summaries | [../../insight-metalog/README.md](../../insight-metalog/README.md) |
| insight-eidos | Detection, explain, engine, CLI, and full product reference | [../../insight-eidos/technical_docs/README.md](../../insight-eidos/technical_docs/README.md) |
| CodeRoast parent docs | Cross-repo status, compatibility matrix, and roadmap | [../../technical_docs/README.md](../../technical_docs/README.md) |

## Key Cross-References

- Parent compatibility matrix: [../../technical_docs/compatibility_matrix.md](../../technical_docs/compatibility_matrix.md)
- insight-metalog phase reference: [../../insight-metalog/technical_docs/phases/metalog.md](../../insight-metalog/technical_docs/phases/metalog.md)
- InSight full pipeline: [../../insight-eidos/technical_docs/README.md](../../insight-eidos/technical_docs/README.md)
