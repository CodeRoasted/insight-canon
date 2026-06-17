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
  set** (`pinned_repos.txt`). **Positive-Unlabeled** posture (positives reliable, negatives weak);
  revert ≠ defect (re-applied reverts excluded).
- **Distribution class: third-party / PRIVATE (ADR 0016 §2a).** These are **third-party CI logs**
  with **no redistribution licence**, and the scrub is **heuristic** with **IPv4 redaction
  deliberately dropped** (`ips: 0` in `summary.json` means *not redacted*, **not** *none present*).
  So the real bytes are **private-only**: never a public Release, **never committed to this public
  repo's git — including any smoke slice.** The public smoke runs on a **synthetic** stand-in (below);
  the real-data R1/R2/lattice-lift gates run **private, on a trusted runner**.
- **Storage class:** big · **NON-re-acquirable** — GHA logs **expire at 90 days**, so a retrospective
  crawl cannot rebuild a past snapshot. **The pinned `vN` private asset is the source of truth.**

## Version lineage (ADR 0016 §3)

- **`v1`** *(FROZEN 2026-06-17 — the interim R1/R2 baseline)*: 4132 samples (4082 logged) · 403 R1
  failures · ~10 R2 revert-positives · 19 repos contributed · measured merge-green-rate 0.807 ·
  `intended_canon_version` 1.5.2 · schema v1. Validity finding baked in: survivorship is
  repo-dependent — **R2 filters to `merge_run_conclusion == "success"`**; R1 uses each run's own
  `ci_outcome` (weak-repo list in `manifest/summary.json`).
  - **Content anchor (the registry pin, ADR 0016 §3):** sha256
    `3054b158382c333301d986ad0472e2208078ee87a92edf42b33dd32a4660b059` — a per-file manifest over the
    8171-file tree (compression-independent; verify the *extracted* corpus against it).
  - **Frozen asset:** `ci-revert-v1.tar.zst` · 274,069,224 B (262 MB, 5.8 % of the 4.5 GB tree) ·
    sha256 `26d3d3d6af4e461f4ab955b91346a7ce9c458469671007d33f7c63856ee8dd49` (single asset, under the
    2 GiB ceiling). Backed up to the **private store** (§2a, Argos credential) — **never** a public
    Release. The asset is the only source of truth (re-crawl cannot rebuild it).

- **`v2` (in progress):** the [bugs.md](../../../../technical_docs/bugs.md) R2 fixes — revert-first
  sampling, revert-density repo selection, widened revert recall, uncapped high-volume repos, 50–100
  repos, and the **longitudinal collector** (a standing scheduled crawler that captures merge-run
  logs *fresh* and observes reverts over 3–6 months — the only path past the 90 d retention wall).
  Each publication is a new immutable `ci-revert/vN`; `v1` stays pinned as the fallback.

## Fetch / verify / freeze (reproducibility — private, trusted-runner only)

The frozen artifact is produced by `insight-canon/scripts/freeze_corpus.sh` (deterministic tar →
`zstd -19`; emits the content manifest, the asset, its sha256, and a `.pin.txt`). To re-freeze a
future version: `bash scripts/freeze_corpus.sh data/logs/ci-revert-corpus ci-revert vN`. `v1` was
frozen from `data/logs/ci-revert-corpus/` and **round-trip-verified** (asset hash OK; all 8171 files
byte-exact on extract).

Consuming `v1` happens **only on a trusted runner with the Argos credential** (§2a) — the public CI
never fetches it. Fetch from the private store, verify the asset, extract, then verify the extracted
tree against the content manifest — never trust an un-verified materialization.

```bash
# (trusted runner, Argos credential) fetch ci-revert-v1.tar.zst + .sha256 + .manifest.sha256
sha256sum -c ci-revert-v1.tar.zst.sha256                  # verify the asset bytes
zstd -dc ci-revert-v1.tar.zst | tar -x -C data/logs/      # re-materialize data/logs/ci-revert-corpus/
cd data/logs/ci-revert-corpus \
  && sha256sum -c /path/to/ci-revert-v1.manifest.sha256 --quiet   # verify every file vs the anchor
```

## Acquisition

`scripts/ci_revert_corpus/` (committed). Validity decisions are pre-registered in `config.py`
(retention 90 d ceiling / 15 d safety → usable window [14, 75] d; T-sweep {14,30,60}; PU posture).
Materializes under the gitignored `insight-canon/data/logs/ci-revert-corpus/`
(`corpus.jsonl`, `log_annotated/`, `log_stripped/`, `manifest/`).

## Smoke slice — SYNTHETIC (public-safe by construction, ADR 0016 §2a + §5)

Because ci-revert is third-party/private, the committed public slice is a **synthetic,
clearly-labelled *shape* fixture** — never real crawled logs. It mirrors the on-disk layout
(`slice/corpus.jsonl` + `log_annotated/` & `log_stripped/` pairs + `SLICE.json`) and the schema, so
it exercises canon's JSONL + annotated/stripped parse and the lattice-lift path, while owning **zero
third-party bytes**. **Authored by Heph/Kleio** (the fixture must exercise canon's tokenization
correctly, like the other REDs) — *pending*; until it lands, the public determinism smoke leans on
the **LogHub** slice (CC-BY-4.0, redistributable).

The **real-data** R1/R2/lattice-lift validity gates do **not** use this synthetic slice — they run
**private, on a trusted runner**, against the real corpus extracted from the private `vN` asset. That
private real-data slice is produced deterministically by
`scripts/ci_revert_corpus/extract_slice.py` (non-empty log pairs; smallest-`(bytes, sample_id)` per
label category; ≥1 R2 positive) — for the trusted-runner context only, **not** committed here:

```bash
# trusted runner only — produces the PRIVATE real-data slice, never committed to public git
python3 scripts/ci_revert_corpus/extract_slice.py \
  --corpus insight-canon/data/logs/ci-revert-corpus \
  --parent-version ci-revert/v1 \
  --parent-pin 3054b158382c333301d986ad0472e2208078ee87a92edf42b33dd32a4660b059
```
