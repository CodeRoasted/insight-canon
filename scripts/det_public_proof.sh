#!/usr/bin/env bash
###############################################################################
# Canon public determinism proof gate — the EXTERNALLY-CHECKABLE half of the
# "same input → same output, bit-for-bit" claim
# (insight_determinism_model.md § "Public proof-gate (canon, Apache)").
#
# Canon-only by construction: it compiles ONLY canon's public Apache sources +
# proof/det_proof.cpp across the gcc × clang × -O{0,2,3} × -ffp-contract{off,fast}
# matrix, replays a canon-local PUBLIC corpus (proof/corpus/), and asserts the
# canonical digest is byte-identical across every build AND matches the committed
# golden hash (proof/golden.sha256). No metalog, no eidos, no private surface — an
# outsider can clone the public repo and run this to verify the determinism claim.
#
# It is the public twin of the superproject's private determinism_bitidentity.sh:
# ONE methodology (build N ways → canonical digest → assert identical), scoped to
# canon's public entry (tokenizer/Drain → template set, failure_lexicon, det_math).
#
# Requires a prior canon build for the dependency include/lib paths
# (build/compile_commands.json — `conan create .` / `malf test` generates it).
#
#   det_public_proof.sh            run the gate (PASS/FAIL vs the golden)
#   det_public_proof.sh --freeze   (re-)derive and write proof/golden.sha256
#
# DETERMINISM_REQUIRE_COMPILERS="g++ clang++" makes each listed compiler mandatory
# (a clang-only break can't pass on the g++ builds alone — a hollow green).
###############################################################################
set -euo pipefail

FREEZE=0
[ "${1:-}" = "--freeze" ] && FREEZE=1

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CANON="$(cd "$SCRIPT_DIR/.." && pwd)"
PROOF="$CANON/proof"
GOLDEN="$PROOF/golden.sha256"
CC="$CANON/build/compile_commands.json"
[ -f "$CC" ] || CC="$CANON/build/Release/compile_commands.json"  # multi-config layout fallback

[ -f "$CC" ] || { echo "error: compile_commands.json missing under $CANON/build — build canon first ('conan install' + cmake configure, or 'malf test')." >&2; exit 2; }
[ -f "$PROOF/det_proof.cpp" ] || { echo "error: $PROOF/det_proof.cpp missing." >&2; exit 2; }
CORPUS="$(ls "$PROOF"/corpus/*.log 2>/dev/null | sort)"
[ -n "$CORPUS" ] || { echo "error: no corpus under $PROOF/corpus" >&2; exit 2; }

# DEFS + INCS from canon's own compile_commands (a representative TU). SPDLOG off
# so engine init/debug logs (which carry a wall-clock timestamp) never enter the
# digest — the determinism-relevant content is the only output.
{ read -r DEFS; read -r INCS; } < <(python3 - "$CC" <<'PY'
import json, re, sys
cmds = json.load(open(sys.argv[1]))
e = next((c for c in cmds if c['file'].endswith(('tokenizer_engine.cpp', 'log_parser.cpp', 'drain.cpp'))), cmds[0])
cmd = e.get('command') or ' '.join(e.get('arguments', []))
defs = [d for d in re.findall(r'-D\S+', cmd) if not d.startswith('-DSPDLOG_ACTIVE_LEVEL')]
incs = [a or b for a, b in re.findall(r'-I *([^ ]+)|-isystem *([^ ]+)', cmd)]
print(' '.join(defs + ['-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF']))
print(' '.join('-isystem ' + i for i in incs))
PY
)
INCS="-I$CANON/api -I$CANON/src $INCS"

# Static libs: the sibling lib/ of each fmt/spdlog/simdjson include dir — derived
# from THIS build's package dirs (never a blind glob, which can grab an ASan variant).
LIBS=""
for dep in spdlo fmt simdj; do
  for inc in $(echo "$INCS" | tr ' ' '\n' | grep -E "/${dep}[^/]*/p/include$"); do
    a="$(ls "${inc%/include}"/lib/lib*.a 2>/dev/null | head -1)"
    [ -n "$a" ] && LIBS="$LIBS $a"
  done
done
LIBS="-Wl,--start-group $LIBS -Wl,--end-group -pthread"

SRCS="$(find "$CANON/src" -name '*.cpp')"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
echo "canon=$CANON  libs=$(echo "$LIBS" | grep -o '/[^ ]*\.a' | wc -l)  corpus=$(echo "$CORPUS" | wc -l) files" >&2

builds=()
for cxx in g++ clang++; do
  command -v "$cxx" >/dev/null || { echo "skip $cxx (not installed)" >&2; continue; }
  for opt in -O0 -O2 -O3; do for fpc in off fast; do
    tag="${cxx//+/p}_${opt#-}_${fpc}"; extra=""
    # Old clang (<21) under libstdc++ needs __cpp_concepts asserted for std::expected;
    # clang-21 defines it natively and ignores the redefine with -Wno-builtin-macro-redefined.
    [ "$cxx" = clang++ ] && extra="-D__cpp_concepts=202002L -Wno-builtin-macro-redefined"
    # shellcheck disable=SC2086
    if $cxx -std=c++23 "$opt" -ffp-contract="$fpc" $extra $DEFS $INCS $SRCS \
        "$PROOF/det_proof.cpp" $LIBS -o "$WORK/$tag" 2>"$WORK/$tag.log"; then
      builds+=("$tag")
    else echo "BUILD FAIL: $tag" >&2; tail -3 "$WORK/$tag.log" | sed 's/^/   /' >&2; fi
  done; done
done
[ "${#builds[@]}" -gt 0 ] || { echo "no builds succeeded" >&2; exit 1; }

for req in ${DETERMINISM_REQUIRE_COMPILERS:-}; do
  pfx="${req//+/p}_"
  printf '%s\n' "${builds[@]}" | grep -q "^$pfx" || {
    echo "GATE INTEGRITY FAIL: required compiler '$req' produced no successful build" >&2
    echo "  — the cross-compiler property is unverified; the gate would be hollow." >&2
    exit 3
  }
done

# Each build replays the whole corpus in one process; the digest is its stdout.
for tag in "${builds[@]}"; do
  # shellcheck disable=SC2086
  "$WORK/$tag" $CORPUS > "$WORK/$tag.out" 2>/dev/null
done
ref="$WORK/${builds[0]}.out"; rc=0
echo "reference: ${builds[0]}  digest-sha=$(sha256sum "$ref" | cut -c1-16)…" >&2
for tag in "${builds[@]}"; do
  if cmp -s "$ref" "$WORK/$tag.out"; then st=IDENTICAL; else st=DIVERGENT; rc=1; fi
  printf "  %-26s %s\n" "$tag" "$st" >&2
done
if [ $rc -ne 0 ]; then
  echo "FAIL: cross-build divergence over canon's public entry — a determinism regression." >&2
  exit 1
fi
echo "PASS: canon public digest byte-identical across ${#builds[@]} builds." >&2

digest_sha="$(sha256sum "$ref" | awk '{print $1}')"
if [ "$FREEZE" -eq 1 ]; then
  echo "$digest_sha" > "$GOLDEN"
  echo "FROZEN golden → $GOLDEN : $digest_sha" >&2
  exit 0
fi
[ -f "$GOLDEN" ] || { echo "no golden at $GOLDEN — run with --freeze to commit one." >&2; exit 2; }
want="$(tr -d '[:space:]' < "$GOLDEN")"
if [ "$digest_sha" = "$want" ]; then
  echo "GOLDEN MATCH: $digest_sha" >&2
  exit 0
fi
echo "GOLDEN MISMATCH: got $digest_sha  want $want" >&2
echo "  → a determinism regression, OR an intentional change needing 'det_public_proof.sh --freeze' + review." >&2
exit 4
