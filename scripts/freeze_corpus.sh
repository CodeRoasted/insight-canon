#!/bin/bash
# scripts/freeze_corpus.sh
#
# Freeze a big, NON-re-acquirable corpus snapshot into an immutable, checksummed
# release artifact (ADR 0016 §3/§5 + the §"Consequences" split-archive host).
#
# Produces, for <corpus>/<version>, under <out>/:
#   <name>-<version>.manifest.sha256   per-file sha256 over the corpus tree, LC_ALL=C
#                                      sorted, paths relative to the corpus root. This is
#                                      the DATA-INPUT COHERENCE ANCHOR (ADR 0016 §3):
#                                      compression-independent, reproducible, and the
#                                      thing a consumer verifies the *extracted* tree
#                                      against. The registry `pin` is sha256(this file).
#   <name>-<version>.tar.zst           deterministic tar (--sort=name, fixed --mtime,
#                                      numeric owner 0) piped through zstd. The transport
#                                      artifact / release asset.
#   <name>-<version>.tar.zst.sha256    sha256 of the asset (download-integrity check).
#   <name>-<version>.tar.zst.partNN +  ONLY when the asset exceeds the GitHub 2 GiB
#   <name>-<version>.parts.sha256      per-asset limit: split parts + their sha256s.
#                                      Reassemble: `cat *.partNN > <name>-<version>.tar.zst`.
#   <name>-<version>.pin.txt           the one-line registry pin (manifest sha256) + the
#                                      asset sha256 + sizes, for pasting into REGISTRY.md.
#
# Verify recipe (records in the corpus README): fetch the asset, check it against
# .tar.zst.sha256, `zstd -dc <asset> | tar -x`, then `sha256sum -c <name>-<version>.manifest.sha256`
# from the extracted corpus root. Re-crawl CANNOT recover an expired snapshot — this
# artifact is the source of truth (ADR 0016).
#
# Usage:
#   bash freeze_corpus.sh <corpus_dir> <name> <version> [out_dir] [zstd_level]
# Example:
#   bash freeze_corpus.sh data/logs/ci-revert-corpus ci-revert v1

set -euo pipefail

CORPUS_DIR="${1:?usage: freeze_corpus.sh <corpus_dir> <name> <version> [out_dir] [zstd_level]}"
NAME="${2:?missing <name>}"
VERSION="${3:?missing <version>}"
OUT_DIR="${4:-$(dirname "$CORPUS_DIR")/${NAME}-${VERSION}.frozen}"
ZSTD_LEVEL="${5:-19}"

# GitHub release-asset hard limit is 2 GiB; stay safely under it per part.
readonly SPLIT_BYTES=$((1900 * 1024 * 1024))

[[ -d "$CORPUS_DIR" ]] || { echo "❌ corpus dir not found: $CORPUS_DIR" >&2; exit 1; }
command -v zstd >/dev/null    || { echo "❌ zstd not found" >&2; exit 1; }
command -v sha256sum >/dev/null || { echo "❌ sha256sum not found" >&2; exit 1; }

CORPUS_DIR="$(cd "$CORPUS_DIR" && pwd)"
PARENT_DIR="$(dirname "$CORPUS_DIR")"
BASENAME="$(basename "$CORPUS_DIR")"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
STEM="${NAME}-${VERSION}"

echo "👉 Freezing $CORPUS_DIR → $OUT_DIR/$STEM.*"

# 1. Per-file content manifest — the data-input anchor. Paths relative to the corpus
#    root, deterministic order (LC_ALL=C, NUL-safe), so the digest is reproducible.
echo "  [1/4] per-file content manifest (sha256 over the tree)…"
MANIFEST="$OUT_DIR/$STEM.manifest.sha256"
( cd "$CORPUS_DIR" && find . -type f -print0 | LC_ALL=C sort -z \
    | xargs -0 sha256sum ) > "$MANIFEST"
FILE_COUNT="$(wc -l < "$MANIFEST")"
PIN="$(sha256sum "$MANIFEST" | cut -d' ' -f1)"
echo "        $FILE_COUNT files · manifest sha256 (= registry pin) = $PIN"

# 2. Deterministic tar → zstd. tar metadata is normalized so the archive is a pure
#    function of file contents + paths (ownership / mtime stripped).
echo "  [2/4] deterministic tar | zstd -$ZSTD_LEVEL …"
ASSET="$OUT_DIR/$STEM.tar.zst"
tar --sort=name \
    --mtime='2026-06-13 16:14:00 UTC' \
    --owner=0 --group=0 --numeric-owner \
    --format=gnu \
    -C "$PARENT_DIR" -cf - "$BASENAME" \
  | zstd -q -"$ZSTD_LEVEL" -T0 -o "$ASSET" -f

ASSET_SHA="$(sha256sum "$ASSET" | cut -d' ' -f1)"
echo "$ASSET_SHA  $STEM.tar.zst" > "$ASSET.sha256"
ASSET_BYTES="$(stat -c%s "$ASSET")"
CORPUS_BYTES="$(du -sb "$CORPUS_DIR" | cut -f1)"
echo "        asset $(numfmt --to=iec "$ASSET_BYTES") (from $(numfmt --to=iec "$CORPUS_BYTES")) · sha256 = $ASSET_SHA"

# 3. Split ONLY if the single asset would exceed GitHub's per-asset ceiling.
echo "  [3/4] split check (ceiling $(numfmt --to=iec "$SPLIT_BYTES"))…"
if (( ASSET_BYTES > SPLIT_BYTES )); then
    echo "        asset > ceiling → splitting into .partNN"
    split -b "$SPLIT_BYTES" -d -a 2 "$ASSET" "$ASSET.part"
    ( cd "$OUT_DIR" && sha256sum "$STEM.tar.zst.part"* ) > "$OUT_DIR/$STEM.parts.sha256"
    echo "        $(ls "$ASSET.part"* | wc -l) parts · reassemble: cat $STEM.tar.zst.part* > $STEM.tar.zst"
else
    echo "        single asset (no split needed)"
fi

# 4. The registry pin block.
echo "  [4/4] pin block → $OUT_DIR/$STEM.pin.txt"
{
  echo "corpus:        $NAME/$VERSION"
  echo "files:         $FILE_COUNT"
  echo "corpus_bytes:  $CORPUS_BYTES ($(numfmt --to=iec "$CORPUS_BYTES"))"
  echo "asset:         $STEM.tar.zst"
  echo "asset_bytes:   $ASSET_BYTES ($(numfmt --to=iec "$ASSET_BYTES"))"
  echo "asset_sha256:  $ASSET_SHA"
  echo "PIN (manifest sha256, the data-input anchor): $PIN"
} | tee "$OUT_DIR/$STEM.pin.txt"

echo "✅ Frozen. The manifest sha256 is the registry pin; the .tar.zst is the release asset."
