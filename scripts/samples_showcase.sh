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

# THE RIGHT-AXIS VERDICT IS COMPUTED HERE, IN THE RUN THAT PRODUCES THE BYTES, and the render
# cannot be emitted without it. That ordering is the whole repair: `rendered but undeclared` is not
# a reachable state of this script, in the same way `rendered but unguarded` is not a reachable
# state of publish_hub_evidence.py (its scrub_guard lives INSIDE the render functions). A check the
# renderer performs is a property of the artifact; a check a workflow performs beside it is a
# property of nobody.
#
# WHAT IT READS. Exactly the two declarations the warehouse gate reads on its RIGHT axis: a
# `SLICE.json` with `"synthetic": true`, or an `ATTRIBUTION.md` naming a redistribution licence.
# It does NOT re-implement the CONTENT axis — that predicate lives in the private warehouse and
# this repo is public and anonymous, so it cannot run here. The README says so in as many words
# rather than implying a scan nobody performed.
#
# A DERIVATIVE OWES ATTRIBUTION. CC-BY 4.0 §3(a) conditions redistribution on naming the source,
# the licence and — for a modified work — the fact of modification. A render is a modified work:
# it is a new arrangement we authored, and the declaration cannot simply travel with it, because
# the source's own claim ("Changes: none — verbatim 2k samples") is FALSE of the render. So the
# attribution is REBUILT here, per act, with the derivation stated. Until 2026-08-20 the render
# carried none of it — measured: zero occurrences of `loghub`, `zenodo`, `CC-BY`, `attribution` or
# `licence` in the 14 MB artifact, published at three public locations.
right_of() {   # $1 = a <corpus>/samples dir -> prints "SYNTHETIC" | "REDISTRIBUTABLE|<attrib>"
  local cdir="$1" slice="$1/SLICE.json" attrib
  if [ -f "$slice" ] && grep -q '"synthetic"[[:space:]]*:[[:space:]]*true' "$slice"; then
    echo "SYNTHETIC"; return 0
  fi
  attrib="$(find "$cdir" -type f -name 'ATTRIBUTION.md' | LC_ALL=C sort | head -1)"
  # The same licence vocabulary the warehouse gate accepts. Kept in step deliberately: a render
  # that claimed a right its source's gate would refuse is the failure one level down.
  if [ -n "$attrib" ] && grep -qiE 'CC-?BY|CC0|public[ -]?domain|MIT|Apache|BSD|permissive' "$attrib"; then
    echo "REDISTRIBUTABLE|$attrib"; return 0
  fi
  return 1
}

# One det_proof run per corpus (all its logs in a single process, sorted for a stable order).
corpora=()   # "corpus:count"
rights=()    # "corpus:SYNTHETIC" | "corpus:REDISTRIBUTABLE|<path to the source ATTRIBUTION.md>"
for cdir in "$SAMPLES"/*/samples; do
  [ -d "$cdir" ] || continue
  corpus="$(basename "$(dirname "$cdir")")"
  mapfile -t logs < <(find "$cdir" -type f -name '*.log' | LC_ALL=C sort)
  [ "${#logs[@]}" -gt 0 ] || { echo "skip $corpus (no *.log under $cdir)" >&2; continue; }
  if ! right="$(right_of "$cdir")"; then
    {
      echo "error: corpus '$corpus' declares no right to redistribute — refusing to publish."
      echo "  $cdir carries neither a SLICE.json with \"synthetic\": true nor an ATTRIBUTION.md"
      echo "  naming a redistribution licence. A render of undeclared bytes is a publication we"
      echo "  cannot stand behind, and this script will not emit one."
    } >&2
    exit 4
  fi
  rights+=("$corpus:$right")
  echo "showcase: $corpus (${#logs[@]} logs, RIGHT=${right%%|*})" >&2
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

# ── The PINS: what a re-runner needs to land on THESE bytes ─────────────────────────────────────
# OWED BY THE FOUNDER'S SIGNATURE, not by taste. ADR-33.D5 clause 2 grounds the derived-artifact
# entry on "the reader re-runs the tool". The replay check above proves det_proof deterministic
# FOR FIXED INPUTS AND A FIXED BINARY — it says nothing across builds, and the measurement that
# proves it matters is on the record: two acts of this same producer published `loghub.canon.txt`
# at 14 053 806 B / 155 212 lines and 13 997 882 B / 155 184 lines — identical class profile,
# different bytes — while `marker_corpus.canon.txt` differed at the SAME size. Name, size and
# provenance are not identity.
#
# THE FAILURE THIS PREVENTS is specific and it is self-inflicted: a reader who re-runs the tool
# against different inputs or a different build measures a different class set, and under
# DN-39.D4's exact-set rule (declared must equal fired, in BOTH directions) that reader REFUSES
# OUR DISCLOSURE WITH OUR OWN INSTRUMENT. The pins are what make the re-run land where the
# disclosure says it will.
#
# Every value below is a digest or a count — no wall clock, nothing that moves between two runs
# over the same inputs, because this file rides into an artifact whose whole claim is byte
# reproducibility and the replay check would be a lie if its own manifest churned.
det_sha="$(sha256sum "$DET" | cut -d' ' -f1)"
{
  echo "# Pins — what a re-run needs to reproduce these bytes"
  echo
  echo "These renders are deterministic **for a fixed tool and fixed inputs**. That is narrower than"
  echo "\"deterministic\", and the difference is the reason this file exists: the same tool over the"
  echo "same-named inputs has already produced different bytes at two different acts. If you re-run"
  echo "and your class set differs from the one declared beside these artifacts, check these pins"
  echo "first — you are almost certainly not holding the same inputs or the same build."
  echo
  echo "## Tool"
  echo
  echo "- \`det_proof\` sha256: \`$det_sha\`"
  [ -n "$identity" ] && echo "- canon ruleset: \`$identity\`"
  [ -n "$packages" ] && echo "- canon $packages"
  echo
  echo "## Inputs"
  echo
  # The samples root's commit, when it is a checkout. ABSENT IS STATED, never omitted: a missing
  # pin that simply is not printed reads exactly like a pin that was never needed.
  if samples_ref="$(git -C "$SAMPLES" rev-parse HEAD 2>/dev/null)"; then
    echo "- source repository commit: \`$samples_ref\`"
  else
    echo "- source repository commit: **unavailable** — this render was produced from a"
    echo "  non-checkout input tree, so the inputs cannot be pinned by ref. The per-file digests"
    echo "  below still identify them exactly."
  fi
  echo
  echo "Per corpus, the SHA-256 of every input log actually read by this run, in the order it was"
  echo "read. A corpus anchor is the SHA-256 of that list, so one line identifies the whole input set."
  for entry in "${corpora[@]}"; do
    c="${entry%%:*}"; n="${entry##*:}"
    cdir="$SAMPLES/$c/samples"
    echo
    echo "### \`$c\` — $n log(s)"
    echo
    echo '```'
    ( cd "$cdir" && find . -type f -name '*.log' | LC_ALL=C sort | xargs sha256sum )
    echo '```'
    anchor="$( ( cd "$cdir" && find . -type f -name '*.log' | LC_ALL=C sort | xargs sha256sum ) \
               | sha256sum | cut -d' ' -f1 )"
    echo
    echo "corpus anchor: \`$anchor\`"
  done
} > "$OUT/PINS.md"
echo "pins: det_proof $det_sha, ${#corpora[@]} corpus anchor(s) → $OUT/PINS.md" >&2

# ── The derived work's OWN attribution file ──────────────────────────────────────────────────
# One section per corpus whose right is a LICENCE (a synthetic corpus owes nobody a notice). The
# source declaration is quoted verbatim so the reader can check it against the upstream record,
# and the derivation is stated separately, because the source's "Changes: none" is a claim about
# the SOURCE and is false of what sits beside this file.
attributed=0
{
  echo "# Attribution for the rendered artifacts in this folder"
  echo
  echo "Each \`*.canon.txt\` here is a **derived work**: the open (Apache-2.0) canon core run over a"
  echo "sample corpus published at [coderoast-hub](https://github.com/CodeRoasted/coderoast-hub)"
  echo "under \`samples/\`. Where the source corpus is third-party material under a redistribution"
  echo "licence, that licence conditions this derivative too, and its notice is below."
  echo
  echo "The renders are **not** verbatim copies. Canon collapses each line to a template, emits one"
  echo "event row per line, and computes an integer entropy term; the output is a new arrangement of"
  echo "the source data, authored by CodeRoast. Line counts, ordering and content all differ from the"
  echo "source logs. Treat every section below as *\"Changes: yes — rendered through canon\"*, whatever"
  echo "the upstream declaration says about its own copy."
  for entry in "${rights[@]}"; do
    c="${entry%%:*}"; r="${entry#*:}"
    case "$r" in
      REDISTRIBUTABLE\|*)
        src="${r#REDISTRIBUTABLE|}"
        attributed=$((attributed + 1))
        echo
        echo "## \`$c.canon.txt\` — derived from the \`$c\` corpus"
        echo
        echo "Source declaration, quoted verbatim from \`${src#"$SAMPLES/"}\` in the corpus this render"
        echo "was produced from:"
        echo
        # Quote it as a blockquote so it cannot be mistaken for this file's own prose.
        sed 's/^/> /' "$src"
        echo
        echo "**Derivation:** rendered by \`insight-canon/scripts/samples_showcase.sh\` using canon's"
        echo "\`det_proof\`. Deterministic and reproducible from the published inputs above with the"
        echo "published Apache-2.0 canon core; this script byte-compares two runs and refuses to"
        echo "publish if they differ."
        ;;
      SYNTHETIC)
        echo
        echo "## \`$c.canon.txt\` — derived from the \`$c\` corpus"
        echo
        echo "The source corpus is a **fabricated fixture** (\`SLICE.json \"synthetic\": true\`) with no"
        echo "third-party bytes, so no third-party notice is owed. Read as of this run, not asserted."
        ;;
    esac
  done
} > "$OUT/ATTRIBUTION.md"
echo "attribution: $attributed licensed corpus/corpora declared → $OUT/ATTRIBUTION.md" >&2

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
  # THE PAGE MUST SAY WHICH RUN IT IS, and it cannot date itself: this script renders on demand,
  # has no release version to stamp, and a wall clock in a published artifact is exactly what the
  # replay check above exists to forbid. So it says what the two bullets ARE — one run's vocabulary
  # — and points at the sibling folder that carries a release stamp. Measured cost of not having
  # this: the committed render sat fifteen cuts old, publishing a three-package set at github@1.3.0
  # while the shipped engine composed four at github@1.4.0, and no reader could have known.
  # NOTE the pointer is deliberately NOT "determinism/ is newer" — it is not always: on 2026-08-18
  # this page re-rendered at stateless-masks-11 while determinism/ still held the v1.9.5 snapshot.
  # What is durably true is only that one of the two carries a version stamp and this one does not.
  echo "The two lines above are the canon vocabulary of **this run** — not a standing version claim."
  echo "This page is rendered on demand, not at every release cut, so read it as a snapshot of one"
  echo "run. The release-stamped evidence lives beside it in [\`determinism/\`](../../determinism/),"
  echo "which is regenerated at each cut and names the version it belongs to."
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
  echo "| corpus | source logs | right to redistribute | canon output |"
  echo "| --- | --- | --- | --- |"
  for entry in "${corpora[@]}"; do
    c="${entry%%:*}"; n="${entry##*:}"
    r=""
    for re in "${rights[@]}"; do [ "${re%%:*}" = "$c" ] && r="${re#*:}"; done
    case "$r" in
      SYNTHETIC)          rcell="synthetic fixture (\`SLICE.json\`)" ;;
      REDISTRIBUTABLE\|*) rcell="licensed — see [\`ATTRIBUTION.md\`](ATTRIBUTION.md)" ;;
      *)                  rcell="—" ;;
    esac
    echo "| \`$c\` | $n | $rcell | [\`$c.canon.txt\`]($c.canon.txt) |"
  done
  echo
  # A PUBLISHED VERDICT MUST BE AN OUTPUT OF THE RUN THAT PRODUCED THE ARTIFACT.
  #
  # This block used to be a constant. It asserted, unconditionally, that "every sample tree
  # published here clears two independent checks", naming the right-to-redistribute axis AND a
  # scan for identifying-content classes. Measured 2026-08-20 with the shipped predicate: the
  # `loghub` tree it named does NOT clear that scan (4 279 line-hits over 6 classes), and the
  # artifact the sentence physically sat beside — `loghub.canon.txt` — had never been scanned by
  # anything (5 916 line-hits over 4 classes). The sentence had no failing mode.
  #
  # The repair that PRODUCED it was already reasoning correctly and simply did not reach far
  # enough: it retired an unearned SAFETY claim ("public-safe by construction") and replaced it
  # with an unearned GATE claim — which is worse in one specific way, because naming a mechanism
  # makes it more credible to the reader it misleads.
  #
  # So: the RIGHT axis is stated because this run computed it, per corpus, above — and refuses to
  # render at all when it cannot. The CONTENT axis is stated as NOT COMPUTED, because it is not:
  # that predicate lives in the private warehouse and this workflow is public and anonymous. An
  # instrument that cannot reach a surface must say so where the human reads, not only where the
  # machine prints.
  echo "> **What this run checked, and what it did not.** The *right to redistribute* in the table"
  echo "> above was read from each source corpus during this run — a \`SLICE.json\` declaring it"
  echo "> fabricated, or an \`ATTRIBUTION.md\` naming a licence — and this page is not rendered at"
  echo "> all when a corpus declares neither. **No identifying-content scan was computed for these"
  echo "> rendered artifacts.** That scan is a separate axis, it runs outside this repository, and"
  echo "> a redistribution licence says nothing about what is *in* the bytes. Read the table as a"
  echo "> statement about our right to publish these renders, never as a statement about their"
  echo "> contents. Our real third-party crawl corpora stay private."
} > "$OUT/README.md"

echo "showcase rendered → $OUT (${#corpora[@]} corpora)" >&2
