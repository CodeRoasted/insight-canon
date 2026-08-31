# insight-canon — tokenization + the determinism core (public, Apache-2.0)

The ingest layer of InSight: raw log line → format detection → stateless
masking → `CanonicalEvent`, plus the bit-identical math core (`det_math`) every
deterministic number downstream is computed with. Public and auditable by
design — this repo is the verifiable half of the determinism claim.

## Arrival

- Build/test: `malf build` / `malf test` from the repo root (dependency-ordered:
  core → semantic/* → benchmarks), or per package dir.
- Layout: `core/` = the `insight_canon` package (the language);
  `semantic/<dialect>/` = per-dialect vocabulary packages (rule rows + code
  tier); `benchmarks/` = the composed perf harness (application package, never
  released); `proof/` = the determinism-proof harness. Packages and deps:
  `packages.yml`.
- Docs: `technical_docs/README.md` — masking, classification, formats,
  determinism.
- `core/data/corpora/REGISTRY.md` — the public corpus registry of record (pins
  by content-anchor sha256; the bytes live elsewhere).

## Local traps

- PUBLIC repo: third-party or unscrubbed corpus bytes never land here — they
  live in `coderoast-corpora` (private); the registry only pins them.
- Semantic packages are one module interface unit each, NO partitions — the
  cross-package BMI closure must not deepen (`packages.yml` states the rule).
- `core` never links a vocabulary package; anything needing composed semantics
  (the perf gate included) lives in `benchmarks/` or downstream.
- Onboarding a new dialect touches ~30 points across 6 repos — follow the
  superproject's `OPS-2` (present only in a full workspace checkout), never
  memory.
- The determinism contract (no libm, integer fixed-point, `-ffp-contract=off`
  consumers) is owned by `technical_docs/determinism.md` — point, don't restate.
- **Hot-path performance is a first-class requirement here — act, don't flag.**
  When already editing tokenizer / Drain / masking hot-path code, look for
  vectorizable scans (whitespace splitting, digit/hex classification, prefix
  checks) and implement the SIMD/branchless version in the same change,
  measured, with determinism intact.
