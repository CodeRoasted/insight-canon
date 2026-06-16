# Corpus: `ci-revert`

The **CI-text labelled proxy** — real GitHub Actions build logs, labelled two ways from one crawl,
the only corpus whose labels are *independent of the message* (the teeth the format-relative gate
needs). Registered in [../REGISTRY.md](../REGISTRY.md); governed by
[ADR 0016](../../../../technical_docs/adr/0016-corpus-storage-and-governance.md). The **validity
contract** (what it may and may not claim) is
[ci_corpus_validity_contract.md](../../../../technical_docs/architecture/ci_corpus_validity_contract.md) — read it before using the labels.

## Intent / test-purpose — one crawl, two roles

- **R1 — capture gate** (`ci_outcome` + `failing_steps[]`, intrinsic, *in* the log): the
  format-relative capture fraction. Labels are the run's own CI outcome.
- **R2 — detection value** (`label_reverted_by_t`, revert archaeology, semantic, *after* merge):
  does a structurally-anomalous green run foreshadow a revert (silent-regression-on-green)?
  Decoupled from the merge log (`I(reverted; merge-log) ≈ 0`) — that is *why* the roles split.
- **log_annotated/** (structure-rich GHA) vs **log_stripped/** (`##[…]` + timestamps removed) → the
  lattice-lift experiment.

## Provenance · license · posture

- **Source:** crawled from public GitHub via `scripts/ci_revert_corpus/` over a **pinned 25-repo
  set** (`pinned_repos.txt`). Public CI logs; **privacy-scrubbed**; **Positive-Unlabeled** posture
  (positives reliable, negatives weak); revert ≠ defect (re-applied reverts excluded).
- **Class:** big · **NON-re-acquirable** — GHA logs **expire at 90 days**, so a retrospective crawl
  cannot rebuild a past snapshot. **The pinned `vN` asset is the source of truth, not re-crawl.**

## Version lineage (ADR 0016 §3)

- **`v1`** *(to publish — the current 4.5 GB on-disk snapshot, the interim R1/R2 baseline)*:
  4132 samples (4082 logged) · 403 R1 failures · ~10 R2 revert-positives · 19 repos contributed ·
  measured merge-green-rate 0.807 · `intended_canon_version` 1.5.2 · schema v1. Validity finding
  baked in: survivorship is repo-dependent — **R2 filters to `merge_run_conclusion == "success"`**;
  R1 uses each run's own `ci_outcome` (weak-repo list in `manifest/summary.json`).
- **`v2` (in progress):** the [bugs.md](../../../../technical_docs/bugs.md) R2 fixes — revert-first
  sampling, revert-density repo selection, widened revert recall, uncapped high-volume repos, 50–100
  repos, and the **longitudinal collector** (a standing scheduled crawler that captures merge-run
  logs *fresh* and observes reverts over 3–6 months — the only path past the 90 d retention wall).
  Each publication is a new immutable `ci-revert/vN`; `v1` stays pinned as the fallback.

## Acquisition

`scripts/ci_revert_corpus/` (committed). Validity decisions are pre-registered in `config.py`
(retention 90 d ceiling / 15 d safety → usable window [14, 75] d; T-sweep {14,30,60}; PU posture).
Materializes under the gitignored `insight-canon/data/logs/ci-revert-corpus/`
(`corpus.jsonl`, `log_annotated/`, `log_stripped/`, `manifest/`).

## Smoke slice

`slice/` = a tiny deterministic subset of `corpus.jsonl` + a handful of `log_annotated`/`log_stripped`
pairs (a few positive + negative cases), committed for zero-fetch CI. Extraction recipe: the first N
records of `corpus.jsonl` plus their referenced log pairs, **including ≥1 R2 positive** so the
lattice-lift path is exercised. Regenerate from the current `vN` when the corpus versions.
