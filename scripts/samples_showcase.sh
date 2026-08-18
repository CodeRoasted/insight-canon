#!/usr/bin/env bash
###############################################################################
# samples_showcase — run canon's PUBLIC det_proof over our PUBLIC sample logs and
# render a human-legible "what Canon does over our samples" showcase.
#
# CLIENT-FACING TRANSPARENCY, NOT A COVERAGE GATE. Eidos + Sift own the end-to-end;
# this exists so an external reader can SEE, over the exact sample logs we publish,
# what the open (Apache-2.0) canon core extracts: the templates it collapses lines
# into, each event's level / failure / warning / structural-role, and the
# deterministic det_math entropy term. It never asserts, never blocks a release.
#
#   samples_showcase.sh <det_proof-binary> <samples-root> <out-dir>
#
# <samples-root> is a coderoast-hub checkout's `samples/` tree, laid out as
# `samples/<corpus>/samples/**/*.log` — the public, synthetic-or-CC-BY slices only
# (the §2a real crawled bytes are private and never reach the hub). One det_proof
# invocation per corpus; its canonical stdout (templates + events + det_math per
# file) IS the showcase. A short README frames it for a non-engineer reader.
###############################################################################
set -euo pipefail

DET="${1:?usage: samples_showcase.sh <det_proof-binary> <samples-root> <out-dir>}"
SAMPLES="${2:?usage: samples_showcase.sh <det_proof-binary> <samples-root> <out-dir>}"
OUT="${3:?usage: samples_showcase.sh <det_proof-binary> <samples-root> <out-dir>}"

[ -x "$DET" ]      || { echo "error: det_proof '$DET' is not an executable" >&2; exit 2; }
[ -d "$SAMPLES" ]  || { echo "error: samples-root '$SAMPLES' is not a directory" >&2; exit 2; }
mkdir -p "$OUT"

# THE REPLAY CHECK IS NOT CEREMONY. What this script publishes is a PUBLIC artifact whose whole
# claim is "same input → same bytes", and a stale-but-reproducible page is strictly better than a
# fresh one that moves under the reader. The failure this catches is silent by nature: anything
# that leaks a wall clock, a pid, an address or a hash-order into det_proof's stdout still renders
# a page that LOOKS right, and only a byte compare of two runs can see it. It is a fact-shaped
# check (byte equality), not a heuristic, so it has no false-positive mode. Measured cost: one
# extra pass per corpus over ~30 small logs.
replay="$(mktemp)"
trap 'rm -f "$replay"' EXIT

# One det_proof run per corpus (all its logs in a single process, sorted for a stable order).
corpora=()   # "corpus:count"
for cdir in "$SAMPLES"/*/samples; do
  [ -d "$cdir" ] || continue
  corpus="$(basename "$(dirname "$cdir")")"
  mapfile -t logs < <(find "$cdir" -type f -name '*.log' | LC_ALL=C sort)
  [ "${#logs[@]}" -gt 0 ] || { echo "skip $corpus (no *.log under $cdir)" >&2; continue; }
  echo "showcase: $corpus (${#logs[@]} logs)" >&2
  "$DET" "${logs[@]}" > "$OUT/$corpus.canon.txt"
  "$DET" "${logs[@]}" > "$replay"
  if ! cmp -s "$OUT/$corpus.canon.txt" "$replay"; then
    {
      echo "error: det_proof is NOT deterministic on corpus '$corpus' — refusing to publish."
      echo "  two runs of the SAME binary on the SAME ${#logs[@]} logs disagree:"
      cmp "$OUT/$corpus.canon.txt" "$replay" || true
      echo "  first differing hunk (run 1 '<' vs run 2 '>'):"
      diff "$OUT/$corpus.canon.txt" "$replay" | head -6 || true
    } >&2
    exit 3
  fi
  corpora+=("$corpus:${#logs[@]}")
done

[ "${#corpora[@]}" -gt 0 ] || { echo "error: no corpora with *.log under $SAMPLES" >&2; exit 1; }

# canon's composed-ruleset identity hash + package list are the first lines of any det_proof
# output — lift them from the first corpus so the index states which canon vocabulary produced this.
first_out="$OUT/${corpora[0]%%:*}.canon.txt"
identity="$(grep -m1 '^# semantic_identity ' "$first_out"  | sed 's/^# semantic_identity /semantic_identity /' || true)"
packages="$(grep -m1 '^# semantic_packages ' "$first_out" | sed 's/^# semantic_packages /packages: /' || true)"

# The client-facing framing. No engine internals, no moat — this is the give-away language layer.
{
  echo "# Canon over our public samples"
  echo
  echo "**Canon** is the open (Apache-2.0) core of the CodeRoast log engine — the"
  echo "tokenizer, the stateless masker, the failure/warning lexicon, and the integer-domain"
  echo "\`det_math\`. This page is that exact core run over the **public sample logs** we ship"
  echo "in [coderoast-hub](https://github.com/CodeRoasted/coderoast-hub) under \`samples/\`."
  echo
  echo "It is a **transparency showcase, not a test result**: nothing here passes or fails."
  echo "It lets you read, line by line, what Canon does to a log before any of our proprietary"
  echo "analysis runs. The end-to-end detection is exercised elsewhere (Eidos + Sift)."
  echo
  [ -n "$identity" ] && echo "- \`$identity\`"
  [ -n "$packages" ] && echo "- \`$packages\`"
  echo
  echo "## What each \`*.canon.txt\` shows"
  echo
  echo "Per source log, Canon emits three sections:"
  echo
  echo "- **templates** — the distinct line shapes Canon collapsed the log into (variable"
  echo "  parts masked to \`<*>\`), with how many lines matched each."
  echo "- **events** — one row per line: its severity level, a two-char \`failure/warning\`"
  echo "  lexicon flag (\`F\`/\`W\`, \`-\` when absent), its structural role, and the template."
  echo "- **det_math** — the deterministic entropy term over the template distribution"
  echo "  (integer domain; identical on every compiler / OS / CPU — that is the whole point)."
  echo
  echo "## Corpora in this showcase"
  echo
  echo "| corpus | source logs | canon output |"
  echo "| --- | --- | --- |"
  for entry in "${corpora[@]}"; do
    c="${entry%%:*}"; n="${entry##*:}"
    echo "| \`$c\` | $n | [\`$c.canon.txt\`]($c.canon.txt) |"
  done
  echo
  # The publication claim states the GATE, never a safety verdict. `samples_safety_lint.py`
  # judges two independent axes — the right to redistribute, and a scan for declared
  # identifying-content classes — and its own bound is that a pass means "no declared class
  # fired", never "these bytes are safe". Wording that outran that bound is what published a
  # real third-party corpus behind the phrase "public-safe by construction".
  echo "> Every sample tree published here clears two independent checks: the **right** to"
  echo "> redistribute (a \`SLICE.json\` declaring it fully synthetic, or an \`ATTRIBUTION.md\`"
  echo "> naming a redistribution licence) **and** a scan that refuses declared"
  echo "> identifying-content classes. That scan matches byte shapes, so it is a floor and not"
  echo "> a certificate. Our real third-party crawl corpora stay private."
} > "$OUT/README.md"

echo "showcase rendered → $OUT (${#corpora[@]} corpora)" >&2
