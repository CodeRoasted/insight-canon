# Corpus: `loghub`

Real-world log datasets from the academic **LogHub** collection — the structural ground truth for
cube measurement, the cross-stdlib determinism measurement, and the rich-format AMI re-measure
(decision #1). Registered in [../REGISTRY.md](../REGISTRY.md); governed by
[ADR 0016](../../../../technical_docs/adr/0016-corpus-storage-and-governance.md).

## Intent / test-purpose

- **Cube measurement** (concluded): the `b_native` oracle (BGL/Thunderbird alert classes), the
  all-format structural sweep, the template-lattice / format-relative gate. *This arc is closed*
  (the disposable `insight_cube` pkg retires at 1.6.0) — LogHub is retained for what follows.
- **Cross-stdlib determinism measurement**: a large, messy, real input for the gcc-15/libstdc++ ≡
  clang-21/libc++ diagonal.
- **Rich-format AMI re-measure (decision #1)**: base-vs-lattice AMI on a genuinely rich format,
  the still-open number that conditions where the cube ships.

## Provenance · license

- **Source:** Zenodo record [`8196385`](https://zenodo.org/records/8196385) (LogHub).
- **License:** **CC-BY-4.0** — attribute LogHub in any published artifact derived from it.
- **Pin:** `BGL.zip` — 57,489,019 B, sha256
  `d67fd82a711aea0157a9b83175892c6ee60e384a2ddf5bc51f39118453816da8` →
  `BGL.log` 4,747,963 lines (4.40 M normal + ~348 k alerts / ~30 classes, labels intact).
- **Class:** big · **re-acquirable** — we store **zero bytes**; the pin makes any download verifiable.

> Lesson baked into the pin: a "full" academic corpus can be silently reprocessed (labels dropped)
> or a partial download — **verify col-1 labels + byte-count + checksum** before trusting it
> (the Zenodo-18522101 "LogTrie" dead-end: truncated + label-stripped).

## Acquisition

`insight-canon/scripts/download_logs.sh` → materializes under the gitignored
`insight-canon/data/logs/loghub-full/` (`BGL.log`, `BGL.zip`, `Thunderbird_5M.log`) and the
per-format `_2k` samples under `data/logs/loghub/`. Verify against the pin above before use.

## Ground truth / labelling

BGL / Thunderbird carry an **alert-label column 1** (`-` = normal; `KERNDTLB`/`APPSEV`/… = alert
class). The loader strips col-1 **only** where the sentinel-rate detector confirms it exists (the 14
message-leading formats keep col-1 as real message).

## Smoke slice

`slice/` = the standard LogHub per-format **`*_2k.log`** samples (2000 lines each) — the canonical
small, deterministic, all-format input. Extraction: the LogHub `_2k` distribution verbatim (or
`head -n 2000` of each full format file, label column preserved). Committed for zero-fetch CI /
determinism smoke; the full `BGL.log` / `Thunderbird_5M.log` are fetched only for deep runs.
