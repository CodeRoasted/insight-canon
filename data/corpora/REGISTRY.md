# Corpus registry

The authoritative index of every corpus the workspace tests or measures against. Governed by
[ADR 0016 — corpus storage & governance](../../../technical_docs/adr/0016-corpus-storage-and-governance.md).

**Rules (ADR 0016):**
- A committed test/gate references a corpus **by the `id` below**, resolving to in-git bytes (a
  fixture or the smoke `slice`) or a **pinned + checksummed** asset — **never** an untracked
  `data/logs/` path.
- Big bytes are **not** in git (no LFS); they materialize under the gitignored `data/logs/` via the
  acquisition recipe and are warmed by content-hash `actions/cache`. Only this `data/corpora/` tree
  (registry + READMEs + smoke slices) is tracked.
- Corpora are **versioned datasets**: each materialization is an immutable `<id>/<version>` + sha256;
  the `pin` column is the current version, old versions stay reproducible.

## Big external corpora (defined here — canon-tokenizer-facing)

| id | intent / test-purpose | class | provenance · license | pin (current) | recipe | consumed by | lifecycle |
|---|---|---|---|---|---|---|---|
| **loghub** | real-log structural ground for cube measurement, the cross-stdlib determinism measurement, and the rich-format AMI re-measure (decision #1) | big · **re-acquirable** | Zenodo record `8196385` · **CC-BY-4.0** (attribute) | `BGL.zip` sha256 `d67fd82a711aea0157a9b83175892c6ee60e384a2ddf5bc51f39118453816da8` (57,489,019 B → `BGL.log` 4,747,963 lines) | `insight-canon/scripts/download_logs.sh` | cube gates (concluded), determinism measurement | **measurement — mostly concluded** (cube arc closed); retained for AMI #1 + determinism. See [loghub/README.md](loghub/README.md) |
| **ci-revert** | the CI-text labelled proxy: **R1** format-relative capture gate + **R2** silent-regression-on-green detection | big · **NON-re-acquirable** (90 d GHA log expiry) · versioned | crawled public GitHub (pinned 25-repo set) · public logs, **PU posture**, privacy-scrubbed | `ci-revert/v1` (to publish) — 4132 samples / 4082 logged / 403 R1 failures / ~10 R2 positives / 19 repos / merge-green 0.807 / canon 1.5.2 | `scripts/ci_revert_corpus/` (crawler — re-crawl **cannot** recover expired logs) | R1 gate, R2 detection | **LIVE — actively regrowing** (longitudinal collector + revert-first re-crawl, [bugs.md](../../../technical_docs/bugs.md)). Contract: [ci_corpus_validity_contract.md](../../../technical_docs/architecture/ci_corpus_validity_contract.md). See [ci-revert/README.md](ci-revert/README.md) |

## Repo-local fixtures (defined where their tests own them — listed for completeness)

These are **small permanent fixtures**, committed in git lock-step with their tests (ADR 0016 §2).
Do not relocate them here.

| id | what | home | consumed by |
|---|---|---|---|
| **canon-goldens** | determinism golden-hash corpus (byte-identity oracle) | `insight-canon/` tests + `insight-canon/proof/` | the F5 cross-stdlib determinism gate |
| **eidos-fuzz** | parse-path fuzz replay set (minimized) + curated seeds | `insight-eidos/fuzz/corpus/` | `parse_fuzzer` (libFuzzer/ASAN gate) |
| **playground-red** | cube REDs + e2e scenario fixtures (1:1 scenario↔test) | `coderoast-server/insight-playground/` | the e2e regression suite |

## Smoke slices

Each big corpus commits a small, deterministic `slice.*` under its directory (ADR 0016 §5) so CI
and the determinism gates have a **zero-fetch, reproducible** input; the full corpus is fetched only
for deep runs. The slice's extraction recipe is in the corpus README so it regenerates when the
corpus versions.
