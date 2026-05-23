# Insight-Canon — CLAUDE.md

See root CLAUDE.md for global rules.

## Insight-Canon — Tokenizer

- What: high-throughput tokenizer for streaming input; emits tokens + offsets.
- Contract: deterministic tokenization — same bytes => same tokens (versioned).
- Perf: zero-copy where possible; no heap alloc in hot path; vectorized parsing allowed.
- Docs: insight-canon/README.md
