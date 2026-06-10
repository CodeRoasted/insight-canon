# Sequence (insight-canon)

Status: shipped. Repo: insight-canon (single package — the former `sequence/` package is folded into `insight_canon`). The sequence surface ships in the `insight.canon` module facade alongside the Tokenizer.

Sequence converts an ordered stream of canonical events into bounded temporal summaries. It preserves the order signal from tokenization without storing an unbounded event history.

## Input

The phase accepts `insight::tokenization::CanonicalEvent` values through `SequenceEngine::ingest()`.

The event's `id` is the sequence token. Sequence does not parse log text, inspect template strings, or own string storage.

## Output

`SequenceEngine` exposes read-only summaries of the ingested stream:

| Output | API | Meaning |
|---|---|---|
| Event count | `size()` | Total events ingested since the last clear. |
| Unique events | `unique_events()` | Number of distinct `EventID` values observed. |
| Transition graph | `transitions()` | Sorted `(from, to, count, probability)` edges. |
| Top n-grams | `top_ngrams(order, k)` | Most frequent bigrams/trigrams with probabilities. |
| Dominant path | `reconstruct_dominant_path(max_steps)` | Greedy most-likely path from the highest-count node. |
| Branching | `branching(top_k)` | Per-node fanout and entropy, sorted deterministically. |

## Flow

```text
CanonicalEvent stream
  -> account current EventID
  -> account previous -> current transition
  -> account bounded bigram/trigram keys
  -> expose graph, n-gram, path, and branching views
```

The hot path keeps a small rolling ring for n-gram construction and flat hash maps for edge and n-gram counts. Public query methods sort their results before returning them.

## Configuration

`SequenceConfig` controls memory and detail:

| Field | Default | Effect |
|---|---:|---|
| `max_ngram_size` | `3` | Tracks bigrams and trigrams. Values above 3 are not supported by the current fixed key layout. |
| `max_transitions` | `100000` | Soft cap on distinct graph edges. Existing edges keep updating after the cap; new edges are dropped. |
| `max_ngram_keys` | `50000` | Soft cap on distinct n-gram keys per tracked order. |

## Contracts

- Sequence is streaming and bounded by its configured caps.
- It is single-threaded; use one `SequenceEngine` per source/worker.
- Result ordering is deterministic: count descending, then key order where applicable.
- Caps trade long-tail accuracy for memory safety; they must not affect already-tracked keys.

## Consumers

insight-metalog uses these summaries to fill MetaLog behavior fields: top n-grams, dominant path, graph edge count, and branching entropy. Detection does not consume sequence directly; it reads the structured MetaLog document emitted by insight-metalog.

## Validation

Unit tests live under `tests/` (the per-domain mirror). The insight-playground package (in coderoast-server) also checks that the package still consumes LogCraft-backed tokenized streams through the full pipeline.
