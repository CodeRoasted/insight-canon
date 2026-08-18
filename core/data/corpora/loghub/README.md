# Corpus: `loghub`

Real-world log datasets from the academic **LogHub** collection — the structural ground truth for
cube measurement, the cross-stdlib determinism measurement, and the rich-format AMI re-measure
(decision #1). Registered in [../REGISTRY.md](../REGISTRY.md); governed by ADR-7, corpus storage &
governance (an internal CodeRoast record, not shipped here).

## Intent / test-purpose

- **Cube measurement** (concluded): the `b_native` oracle (BGL/Thunderbird alert classes), the
  all-format structural sweep, the template-lattice / format-relative gate. *This arc is closed*
  (the disposable `insight_cube` pkg was deleted at the 1.6.0 verdict, adr/0018) — LogHub is
  retained for what follows.
- **Cross-stdlib determinism measurement**: a large, messy, real input for the gcc-16.2/libstdc++ ≡
  clang-21/libc++ diagonal.
- **Rich-format AMI re-measure**: base-vs-lattice AMI on a genuinely rich format. The cube itself
  is no longer conditioned on it — it ships always-on since 1.7.2 (no opt-in flag).

## Provenance · license

- **Source:** Zenodo record [`8196385`](https://zenodo.org/records/8196385) (LogHub).
- **License:** **CC-BY-4.0** — attribute LogHub in any published artifact derived from it.
- **Pins** (the full corpus under the warehouse's gitignored `coderoast-corpora/zenodo_corpora/loghub/data/loghub-full/`; sha256 = data-input anchor):
  - `BGL.zip` — 57,489,019 B · sha256 `d67fd82a711aea0157a9b83175892c6ee60e384a2ddf5bc51f39118453816da8` **(verified 2026-06-17)**; extracts to →
  - `BGL.log` — 743,185,031 B · 4,747,963 lines · sha256 `666130b15ef44eb32fd02bd053e6c6e007c37696b5e7e8b9d8e45b729876a5d2` (4.40 M normal + ~348 k alerts / ~30 classes, labels intact).
  - `Thunderbird_5M.log` — 868,147,617 B · 5,000,000 lines · sha256 `6e0f52d45d639c76fc2f430e6ef609a915072c2328b862cb097dc59ac5694580` (a 5 M-line head-extract of LogHub Thunderbird — extraction recipe under Acquisition below).
- **Class:** big · **re-acquirable** (CC-BY, Zenodo `8196385`) — we store **zero bytes in git**; the pins make any (re)download verifiable.

> Lesson baked into the pin: a "full" academic corpus can be silently reprocessed (labels dropped)
> or a partial download — **verify col-1 labels + byte-count + checksum** before trusting it
> (the Zenodo-18522101 "LogTrie" dead-end: truncated + label-stripped).

## Acquisition

All tooling lives in the private warehouse **`coderoast-corpora`** (ADR 0016 §2a).

- **`_2k` samples + structured-JSON mix:** `coderoast-corpora/zenodo_corpora/loghub/scripts/download_logs.sh` fetches the 16
  `_2k` samples (logpai/loghub GitHub) under the warehouse's `data/logs/loghub/` and a structured-JSON
  archive. (The published sample slice is the `_2k` set — see below.)
- **Full corpus (`coderoast-corpora/zenodo_corpora/loghub/data/loghub-full/`):** `download_logs.sh` →
  `fetch_loghub_full` re-acquires it from Zenodo `8196385`: `BGL.zip` → verify → extract → `BGL.log`
  (verify); `Thunderbird.tar.gz` streamed through `tar -xzO Thunderbird.log | head -n 5000000` →
  `Thunderbird_5M.log` (verify) — the head-extract is taken without materializing the full ~30 GB log.
  Each step checks its sha256 pin inline. **Verified 2026-06-17** to reproduce all three pins from
  scratch (BGL.zip / BGL.log / Thunderbird_5M.log all `OK`). No private backup needed — re-acquirable
  CC-BY on Zenodo, unlike the non-re-acquirable `ci-revert`.

## Ground truth / labelling

BGL / Thunderbird carry an **alert-label column 1** (`-` = normal; `KERNDTLB`/`APPSEV`/… = alert
class). The loader strips col-1 **only** where the sentinel-rate detector confirms it exists (the 14
message-leading formats keep col-1 as real message).

## Public-safe sample slice (ADR 0016 §5)

The LogHub per-format **`*_2k.log`** set (16 files, 2000 lines each) — the canonical small,
deterministic, all-format input — plus `ATTRIBUTION.md` (CC-BY-4.0 credit + the "no changes / full
corpus not committed" notice) lives at **`coderoast-corpora/zenodo_corpora/loghub/samples/`** and
publishes to the **public hub** via the corpora Sample Release workflow. Extraction: the LogHub `_2k`
distribution verbatim (or `head -n 2000` of each full format file, label column preserved).

**Consumed by:** the **canon Samples Showcase** — `insight-canon/proof/det_proof` run over the hub
samples for a client-facing "what Canon extracts" render (a showcase, **not** a gate: the determinism
gate uses `proof/corpus/`, and the end-to-end is owned by Eidos + the playground e2e). No canon test
resolves an in-git slice, so the LogHub `_2k` bytes were removed from canon git (ADR 0016 §5, 2026-07-09).
