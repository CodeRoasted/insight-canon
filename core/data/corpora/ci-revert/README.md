# Corpus: `ci-revert`

The **CI-text labelled proxy** — real GitHub Actions build logs, labelled two ways from one crawl,
the only corpus whose labels are *independent of the message* (the teeth the format-relative gate
needs). Registered in [../REGISTRY.md](../REGISTRY.md); governed by ADR-7, corpus storage &
governance. The **validity contract** — what this corpus may and may not claim — is ADR-8, corpus
gates, oracles & measurement; read it before using the labels. Both are internal CodeRoast records
and are not shipped with this repo.

## Intent / test-purpose — one crawl, two roles

- **R1 — capture gate** (`ci_outcome` + `failing_steps[]`, intrinsic, *in* the log): the
  format-relative capture fraction. Labels are the run's own CI outcome.
- **R2 — detection value** (`label_reverted_by_t`, revert archaeology, semantic, *after* merge):
  does a structurally-anomalous green run foreshadow a revert (silent-regression-on-green)?
  Decoupled from the merge log (`I(reverted; merge-log) ≈ 0`) — that is *why* the roles split.
- **log_annotated/** (structure-rich GHA) vs **log_stripped/** (`##[…]` markers removed, **timestamp
  kept** — stripping it would break canon's `<ts> <msg>` parse, contract §4) → the lattice-lift
  experiment.

## Provenance · license · posture

- **Source:** crawled from public GitHub via `coderoast-corpora/github_corpora/revert_corpus/scripts/ci_revert_corpus/` (v1: a
  **pinned 25-repo set**, `pinned_repos.txt`; v2: ~60 repos, **revert-density-gated** selection).
  **Positive-Unlabeled** posture (positives reliable, negatives weak); revert ≠ defect (re-applied
  *and* relanded reverts excluded).
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

- **`v2` (policy `d11-ci-revert-v2`, 2026-06-17 — tooling IMPLEMENTED; awaits the official crawl):**
  the internal defect register's R2-yield fixes are all built + offline-tested —
  widened revert recall (rebase-merge sha + PR-number + rollback/back-out/reland lexicon; `revert.method`),
  revert-first sampling (guarantee-crawl every reverted PR + matched controls; `sampling_role`),
  revert-density repo selection, uncapped high-volume repos, ~60 repos (schema v2). The **longitudinal
  collector** (`coderoast-corpora/.../longitudinal/`) — a standing scheduled crawler that captures
  merge-run logs *fresh* and observes reverts over 3–6 months — is the path past the 90 d retention
  wall to a representative R2 gate. Publishing `ci-revert/v2` is a live crawl on a trusted runner (the
  density-gated selection differs from v1's pin → re-run + Founder review); `v1` stays pinned as the
  fallback. Each publication is a new immutable `ci-revert/vN`.

## Fetch / verify / freeze (reproducibility — private, trusted-runner only)

All corpus tooling lives in the private warehouse **`coderoast-corpora`** (ADR 0016 §2a). The frozen
artifact is produced by `coderoast-corpora/_shared/freeze_corpus.sh` (deterministic tar → `zstd -19`;
emits the content manifest, the asset, its sha256, and a `.pin.txt`). To re-freeze a future version:
`bash scripts/freeze_corpus.sh data/ci-revert/v1/full ci-revert vN`. `v1` was frozen and
**round-trip-verified** (asset hash OK; all 8171 files byte-exact on extract).

Consuming `v1` happens **only on a trusted runner with the Argos credential** (§2a) — the public CI
never fetches it. Fetch from the private store, verify the asset, extract, then verify the extracted
tree against the content manifest — never trust an un-verified materialization.

**The recipe below is the MEASURED one** (re-materialized and re-verified 2026-07-26, Argos). The
previous text claimed `tar -x -C data/` yields `data/ci-revert/v1/full/`; it does not — **v1's archive
carries a single top-level `ci-revert-corpus/`**, frozen before the `data/vN/full` layout existed, so the
rename is a required step, not a cosmetic one. Following the old text left the tree at a path nothing
reads and then `cd`'d into one that does not exist.

```bash
# (trusted runner) fetch the three assets from the PRIVATE store — never a public Release (§2a)
gh release download ci-revert-v1 -R CodeRoasted/coderoast-corpora
sha256sum -c ci-revert-v1.tar.zst.sha256                        # asset bytes: 26d3d3d6…
sha256sum ci-revert-v1.manifest.sha256                          # content anchor = the REGISTRY pin 3054b158…

# extract (top-level dir is `ci-revert-corpus/`) then rename to the layout every consumer reads
ROOT=<warehouse>/github_corpora/revert_corpus/data/v1           # → /home/windows/corpora-data/github/revert/v1
zstd -dc ci-revert-v1.tar.zst | tar -x -C "$ROOT" && mv "$ROOT/ci-revert-corpus" "$ROOT/full"

cd "$ROOT/full" && sha256sum -c /path/to/ci-revert-v1.manifest.sha256 --quiet   # 8171 files vs the anchor
```

**`data/v1/sample` is NOT a subsample of `data/v1/full`** — verified by name and by content hash, and
load-bearing for anything comparing a sample measurement to a full-corpus one. They are **two separate
crawls 44 minutes apart** on the same seed (`0xD11C15E1EC7`): the sample queried at `08:28:52Z` over
**3 repos** (react/react, ollama/ollama, microsoft/markitdown — 250 API requests, 60 logs), the frozen
full at `09:12:08Z` over **25 queried repos** (12 734 requests, 4 082 logs). **Queried and contributing
are different denominators, and only the second is the population:** `manifest/repos_used.txt` and
`summary.json`'s `repos: 25` count the *pinned query set* (`repo_errors: 0`), while **19 repos actually
carry rows** in `corpus.jsonl` — six of the pinned 25 yielded zero samples under the selection policy
(termux/termux-app, harry0703/MoneyPrinterTurbo, realworld-apps/realworld, Genymobile/scrcpy,
doocs/advanced-java, ChatGPTNextWeb/NextChat) and are absent from `summary.json`'s
`per_repo_diagnostics` (19 entries). Quote **19** whenever the sentence is about population; quote 25
only about the pin. On the sample side the two denominators coincide — 3 queried, 3 contributing.
**38 of the sample's 60 logs are absent from `full/` by name and 37 of those by content**; of the
sample's three repos only `react/react` is in the frozen population at all. A "same measurement at
larger n" reading of a full-corpus re-run is therefore wrong — the population changes with the scale.

**Row counts vs log-file counts** (both corpora internally consistent, cross-checked against disk):
`corpus.jsonl` carries one row per sampled run whether or not its log survived, and the discriminant is
the row's **`log_status`** field — `full` 4 132 rows = 4 082 `ok` + 48 `log_expired` + 2 `log_error`,
`sample` 63 rows = 60 `ok` + 3 `log_expired`. Every non-`ok` row has a null `log_annotated`/`log_stripped`
reference, and **zero rows reference a file that is not on disk** in either corpus.

## Acquisition

`coderoast-corpora/github_corpora/revert_corpus/scripts/ci_revert_corpus/` (the crawler). Validity decisions are pre-registered in
`config.py` (retention 90 d ceiling / 15 d safety → usable window [14, 75] d; T-sweep {14,30,60}; PU
posture). Materializes under the warehouse's gitignored `coderoast-corpora/github_corpora/revert_corpus/data/v1/full/`
(`corpus.jsonl`, `log_annotated/`, `log_stripped/`, `manifest/`).

## Public-safe sample slice — SYNTHETIC (public-safe by construction, ADR 0016 §2a + §5)

Because ci-revert is third-party/private, its public slice is a **fully synthetic, clearly-labelled
*shape* fixture** — fabricated repos/content, **zero third-party bytes** — living at
**`coderoast-corpora/github_corpora/revert_corpus/samples/`** (and publishing to the **public hub**
via the corpora Sample Release workflow), **never** in canon git. It mirrors the on-disk layout
(`corpus.jsonl` + `log_annotated/` & `log_stripped/` pairs + `SLICE.json` with `"synthetic": true`)
and the schema, so it exercises canon's `<ts> <msg>` parse and the lattice-lift path; the stripped
form is produced by the **real** pipeline `degrade()` so degradation matches production exactly.
**5 fabricated samples** (1 R2-positive, 2 R1-failure, 2 clean).

**Generated** (deterministic — regenerates byte-identically; writes the warehouse `samples/`):

```bash
# from coderoast-corpora/github_corpora/revert_corpus/scripts/  (writes ../samples by default)
python3 -m ci_revert_corpus.make_synthetic_slice
```

The **real-data** R1/R2/lattice-lift validity gates do **not** use this synthetic slice — they run
**private, on a trusted runner**, against the real corpus extracted from the private `vN` asset. That
private real-data slice is produced by `coderoast-corpora/github_corpora/revert_corpus/scripts/ci_revert_corpus/extract_slice.py`
(non-empty log pairs; smallest-`(bytes, sample_id)` per label category; ≥1 R2 positive) —
trusted-runner context only, **never** committed to public git:

```bash
# trusted runner only — produces the PRIVATE real-data slice (warehouse data/), never public git
python3 -m ci_revert_corpus.extract_slice \
  --corpus coderoast-corpora/github_corpora/revert_corpus/data/v1/full \
  --parent-version ci-revert/v1 \
  --parent-pin 3054b158382c333301d986ad0472e2208078ee87a92edf42b33dd32a4660b059
```
