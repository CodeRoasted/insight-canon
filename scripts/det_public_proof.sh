#!/usr/bin/env bash
###############################################################################
# Canon public determinism proof gate — the EXTERNALLY-CHECKABLE half of the
# "same input → same output, bit-for-bit" claim
# (bibles/determinism_model.md §3).
#
# Canon-only by construction: it builds ONLY canon's public Apache module across
# the (ship gcc/libstdc++ × clang-21/libc++)  x  -O{0,3}
# matrix — the cross-compiler AND cross-stdlib DIAGONAL (the modules build forces the
# stdlib per compiler: clang's std module ships with libc++). Strictly stronger than
# the pre-unwrap both-libstdc++ textual matrix, and the pairing the determinism
# contract pins. It replays a canon-local PUBLIC corpus (proof/corpus/), asserts the canonical
# digest is byte-identical across every build. It does NOT compare against a committed
# golden hash: `proof/golden.sha256` was retired with the cross-leg-agreement model (see
# "NEW MODEL" below) and no longer exists in this repo — the sentence that stood here
# claimed an assertion this script had stopped making, 80 lines above the paragraph that
# says why it stopped. No metalog, no eidos, no private surface — an outsider can clone
# the public repo and run this to verify the determinism claim.
#
# ── Approach B (Daidalos ruling 2026-06-06; bibles/determinism_model.md) ──────
# The methodology is unchanged: build N ways → canonical digest → assert identical
# + golden. Only the "N ways" MECHANIC changed. The 1.5.1 unwrap turned canon's
# public surface into a C++20 MODULE (the textual api/*.hpp the old single-shot
# `g++ src/*.cpp det_proof.cpp` recompile #include'd are gone). So "N ways" is now:
# build canon as a MODULE STATIC-LIB per cell via its real CXX_MODULE_STD build
# (proof/CMakeLists.txt: add_subdirectory(canon) + the det_proof fixture importing
# `insight.canon`), and link the CONSTANT fixture per cell. Building the lib IS
# recompiling canon's source under the cell's codegen — only the trivial fixture
# moves from recompile to link, so matrix coverage is unchanged. BMI ordering + the
# std module are delegated to the build system the modules cascade already proves
# green; each cell is one legible cmake build whose compile_commands.json proves the
# flags it applied. The fixture/digest/golden core below is byte-untouched — so a
# green run is proof the swap is byte-equivalent to the retired source-recompile.
#
# Requires conan + a prior canon dep resolution (fmt/spdlog/simdjson in the cache;
# `malf test` / `conan create .` populates it). The compiler×stdlib axis is a conan
# profile (linux-gcc16-release = ship gcc/libstdc++, linux-clang21-release =
# clang-21/libstdc++); -O is appended per cell after the profile.
#
# ── WHY THERE IS NO -ffp-contract AXIS HERE, and how that stays honest ─────────
# This sweep used to run a third axis, -ffp-contract={off,fast}, doubling the cell
# count to four per leg. It was INERT, for two independent reasons, both measured
# at the desk on 2026-09-03:
#
#   1. THE FLAG NEVER REACHED THE COMPILE LINE AS `fast`. canon declares
#      -ffp-contract=off with target_compile_options(... PUBLIC ...) — the
#      determinism requirement itself, and the four semantic packages declare it
#      the same way. proof/CMakeLists.txt injects the cell's flags through
#      add_compile_options at DIRECTORY scope, and CMake emits directory options
#      BEFORE a target's own, so the effective flag on every TU in this harness —
#      canon's, the semantic packages', and det_proof.cpp's, which consumes the
#      INTERFACE options through its link — was `off` in all four cells. Last flag
#      wins in gcc/clang. Reproduced in a two-file CMake project: the compile line
#      came out `-O3 -march=x86-64-v2 -O0 -ffp-contract=fast -ffp-contract=off`.
#      Note the -O axis in that same line: `-O0` DOES follow `-O3`, so the
#      optimization axis was live and is kept.
#   2. AND THE ISA COULD NOT HAVE CONTRACTED ANYWAY. The profiles pin
#      -march=x86-64-v2, which predates FMA3. Measured on both toolchains
#      (g++ 16.2.0, clang 21.1.8): `-march=x86-64-v2 -dM -E` defines __SSE4_2__
#      and defines neither __FMA__ nor __AVX__. With no FMA instruction available
#      there is nothing for `fast` to fuse.
#
# So four cells were two configurations run twice, while the PASS line claimed a
# full -O × -ffp sweep — a false CLAIM over an intact GUARANTEE, paid for with 2x
# the build time on a single self-hosted box.
#
# WHY THE AXIS WAS RETIRED RATHER THAN MADE TO VARY. Making it vary needs canon to
# stop forcing -ffp-contract=off PUBLIC, and that flag IS the invariant (the
# determinism contract: a flag a consumer may unset is not an invariant). Testing an
# invariant by disabling it tests nothing. It would ALSO need the ISA pin moved off
# the ship baseline, so the proof would stop proving the shipped configuration. And
# the coverage the `fast` cell was meant to buy — a tripwire against a future float
# introduction — is bought more strongly by the PUBLIC flag plus the integer-domain
# rule than by a cell that cannot fail.
#
# THE PREMISE IS RE-DERIVED EVERY RUN, never trusted: assert_ffp_contract_forced_off
# below reads each cell's own compile_commands.json and reds if any TU's EFFECTIVE
# (last-wins) -ffp-contract is anything but `off`. If canon ever stops forcing it,
# the retirement stops being justified and this gate says so on the spot instead of
# going quietly uncovered.
#
# NOT TOUCHED, and the difference is the reason: the MSVC leg's /fp:fast dominant
# corner stays live. canon's flag is emitted under a
# $<$<OR:GNU,Clang,AppleClang>:...> generator expression, so MSVC inherits nothing
# and its fp axis genuinely varies.
#
#   det_public_proof.sh                          run the cross-build determinism check (PASS/FAIL)
#   DETERMINISM_OUT=<path> det_public_proof.sh   ALSO emit this leg's digest to <path>, for the
#                                                Determinism-Golden-Proof workflow to cross-compare
#                                                against the other legs (no committed golden to rot)
#
# NEW MODEL — cross-leg agreement, not a committed golden. This script proves the per-leg invariant
# (byte-identity across this leg's compiler × -O × -ffp sweep) and EMITS the leg's digest. The
# workflow runs every leg (gcc/clang × x86/arm64 + msvc) and compares their emitted digests: all
# equal ⇒ cross-toolchain/ISA/OS bit-identity, gating the release. The golden is the agreed digest,
# published per-release as a Release artifact — never committed (so it cannot go stale, the trap the
# old proof/golden.sha256 anchor had).
#
# DETERMINISM_REQUIRE_COMPILERS="g++ clang++" makes each listed compiler mandatory
# (a clang-only break can't pass on the g++ builds alone — a hollow green).
###############################################################################
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CANON="$(cd "$SCRIPT_DIR/.." && pwd)"
PROOF="$CANON/proof"

[ -f "$PROOF/det_proof.cpp" ]   || { echo "error: $PROOF/det_proof.cpp missing." >&2; exit 2; }
[ -f "$PROOF/CMakeLists.txt" ]  || { echo "error: $PROOF/CMakeLists.txt missing (the Approach-B per-cell harness)." >&2; exit 2; }
# LC_ALL=C: this order is the ARGUMENT order handed to det_proof, and det_proof prints one section
# per argument — so corpus order IS digest order, and golden.yaml's MSVC leg sorts the same names
# on the other side of a byte compare. A locale-aware collation treats punctuation as ignorable
# (measured on pwsh 7.4.6: service.log / service_a.log / service-b.log come back in a different
# order than byte order gives), so both sides pin byte order. Today's seven filenames agree under
# either; this keeps that from being luck the first time a name mixes `_`, `.` or case.
CORPUS="$(ls "$PROOF"/corpus/*.log 2>/dev/null | LC_ALL=C sort)"
[ -n "$CORPUS" ] || { echo "error: no corpus under $PROOF/corpus" >&2; exit 2; }

command -v conan >/dev/null || { echo "error: conan not found — the module build needs it for the toolchain + deps." >&2; exit 2; }
command -v cmake >/dev/null || { echo "error: cmake not found." >&2; exit 2; }
# Workspace conan cache (deps + the seeded profiles). Honour an explicit CONAN_HOME.
export CONAN_HOME="${CONAN_HOME:-$(cd "$CANON/.." && pwd)/.conan2}"

# Compiler×stdlib legs → "tag:cxx-bin:cc-bin:conan-profile". The modules build (import
# std) forces the stdlib per compiler — clang-21's std module ships with libc++, NOT
# libstdc++ — so the public matrix is now the cross-stdlib DIAGONAL (ship gcc/libstdc++ ×
# clang-21/libc++): cross-compiler AND cross-stdlib, strictly stronger than the old
# both-libstdc++ textual matrix, and the very pairing the determinism contract pins. The
# compiler is pinned explicitly (the conan toolchain's -stdlib flags would otherwise reach
# a default g++). tag prefix (gpp_/clangpp_) feeds DETERMINISM_REQUIRE_COMPILERS.
# The conan PROFILE per leg is overridable so the SAME proof runs on a second ISA: the arm64
# determinism leg sets DETERMINISM_GCC_PROFILE=linux-gcc16-arm64-release /
# DETERMINISM_CLANG_PROFILE=linux-clang21-libcxx-arm64-release (the only difference vs x86 is the
# profile's arch/-march; the profile-pinned compilers are wired the same way on both
# ISAs). Defaults are the x86 profiles → the x86 gate is byte-unchanged. The golden is shared, so an
# arm64 run matching it IS the cross-ISA bit-identity assertion (the whole point of the 2nd-ISA leg).
GCC_PROFILE="${DETERMINISM_GCC_PROFILE:-linux-gcc16-release}"
CLANG_PROFILE="${DETERMINISM_CLANG_PROFILE:-linux-clang21-libcxx-release}"
# label:profile — the PROFILE is the ONLY compiler authority (its [buildenv] CXX/CC drive the
# harness cmake). WHY (measured 2026-08-16, same defect as metalog's driver): hand-kept bins
# (g++-15) passed as -DCMAKE_CXX_COMPILER beat the profile's compiler — conan_toolchain.cmake
# sets none — so a gcc-16 profile leg silently built with the PATH's g++-15; and a runner
# without that literal binary silently SKIPPED the leg. The profile can never mismatch itself.
LEGS=( "g++:$GCC_PROFILE" "clang++:$CLANG_PROFILE" )

# Optional single-leg selection (DETERMINISM_LEG=gcc|clang) so a per-compiler CI matrix can run
# one leg per parallel job. Default (unset) = the full cross-stdlib diagonal, unchanged. Each leg
# still proves byte-identity across its OWN -O×-ffp sweep AND vs the committed golden; with both
# legs run as separate jobs that each match the SAME committed golden, the cross-stdlib property
# holds transitively (gcc==golden && clang==golden ⟹ gcc==clang).
if [ -n "${DETERMINISM_LEG:-}" ]; then
  case "$DETERMINISM_LEG" in
    gcc)   LEGS=( "g++:$GCC_PROFILE" ) ;;
    clang) LEGS=( "clang++:$CLANG_PROFILE" ) ;;
    *) echo "::error::unknown DETERMINISM_LEG='$DETERMINISM_LEG' (expected gcc|clang)" >&2; exit 2 ;;
  esac
fi

# Every FIRST-PARTY compile line in a cell must end up at -ffp-contract=off (last flag
# wins in gcc/clang) — the premise under which the -ffp-contract axis was retired from
# this sweep (header). A TU at `fast`, or one carrying no -ffp-contract at all (gcc's
# default IS `fast`), means the flag stopped being forced and this gate is no longer
# covering what it stopped claiming to cover.
#
# SCOPED TO SOURCES UNDER $CANON, and the scoping is load-bearing rather than tidy. The
# harness's compile_commands.json also lists translation units this repo does not own —
# CMake synthesises one for the `std` module under CXX_MODULE_STD, in the BUILD tree, and
# it links no first-party target. Judging those would put a red on this gate for a flag
# nobody here declares. The anti-vacuity cost of scoping is paid below: a filter that
# matched NOTHING would pass silently, so an empty first-party set is itself a red.
#
# Pure grep + bash on the cell's own compile_commands.json: no python, no jq — an outsider
# must be able to run this proof with conan and cmake and nothing else.
assert_ffp_contract_forced_off() {
  local cc="$1" tag="$2" root="$3" total=0 bad=0 line src eff
  if [ ! -f "$cc" ]; then
    echo "FFP PREMISE UNCHECKABLE: $tag emitted no compile_commands.json at $cc" >&2
    echo "  — CMAKE_EXPORT_COMPILE_COMMANDS is set in proof/CMakeLists.txt; its absence is a harness break." >&2
    return 1
  fi
  while IFS= read -r line; do
    src="${line##*-c }"; src="${src%\",}"
    case "$src" in "$root"/*) ;; *) continue ;; esac
    total=$((total + 1))
    case "$line" in
      *-ffp-contract=*) eff="${line##*-ffp-contract=}"; eff="${eff%%[^a-z]*}" ;;
      *)                eff="<unset>" ;;
    esac
    if [ "$eff" != "off" ]; then
      bad=$((bad + 1))
      [ "$bad" -le 5 ] && echo "   effective -ffp-contract=$eff on $src" >&2
    fi
  done < <(grep '"command":' "$cc")
  if [ "$total" -eq 0 ]; then
    echo "FFP PREMISE UNCHECKABLE: $tag's compile_commands.json lists no compile line under $root." >&2
    echo "  A scoped assertion that matches nothing is a vacuous pass, so it is a red instead." >&2
    return 1
  fi
  if [ "$bad" -ne 0 ]; then
    echo "FFP PREMISE BROKEN: $bad of $total first-party compile line(s) in cell $tag are NOT at -ffp-contract=off." >&2
    echo "  This sweep dropped its -ffp-contract={off,fast} axis because canon forces the flag PUBLIC," >&2
    echo "  making the axis inert (see this script's header). That is no longer true. Either restore the" >&2
    echo "  PUBLIC declaration in the package that lost it, or restore the axis here — not neither." >&2
    return 1
  fi
  echo "  ffp premise: $total first-party TU(s) in $tag at -ffp-contract=off" >&2
  return 0
}

# det_proof's stderr, bounded and shown from BOTH ENDS. The two ends are load-bearing rather
# than tidy: the fixture's two error exits sit on opposite sides of its logger init, so `usage:`
# (written before init_logging) is necessarily the FIRST line, and `cannot open <path>` (written
# after it) is necessarily the LAST. Measured 2026-09-04 on this repo's own
# proof/build-inventory-gcc16-release/det_proof, a build that does NOT compile the log macros out:
# `cannot open` came back as line 21 of 21, behind 20 log records. A head-only excerpt would have
# printed twenty lines of noise and hidden the one line that says what went wrong.
surface_fixture_stderr() {
  local tag="$1" err="$2" lines bytes
  [ -s "$err" ] || return 0
  lines="$(wc -l < "$err")"; bytes="$(wc -c < "$err")"
  echo "  --- $tag stderr: $lines line(s), $bytes byte(s) ---" >&2
  if [ "$lines" -le 12 ]; then
    sed 's/^/  | /' "$err" >&2
  else
    head -3 "$err" | sed 's/^/  | /' >&2
    echo "  | ... $((lines - 8)) line(s) elided ..." >&2
    tail -5 "$err" | sed 's/^/  | /' >&2
  fi
}

WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
echo "canon=$CANON  conan_home=$CONAN_HOME  corpus=$(echo "$CORPUS" | wc -l) files" >&2

# ── Build the matrix: canon module-lib + det_proof, N ways ────────────────────
builds=()        # tags
declare -A BIN   # tag -> det_proof binary path
for leg in "${LEGS[@]}"; do
  IFS=: read -r cxx profile <<< "$leg"
  [ -f "$CONAN_HOME/profiles/$profile" ] || { echo "skip $cxx ($profile not in $CONAN_HOME/profiles)" >&2; continue; }
  # Compiler = the profile's [buildenv] CXX/CC, resolved and PRINTED — a wrong-compiler or
  # missing-compiler leg must be impossible to miss (it was silently skipped before).
  prof_cxx="$(sed -nE 's/^[[:space:]]*CXX[[:space:]]*=[[:space:]]*//p' "$CONAN_HOME/profiles/$profile" | tail -1)"
  prof_cc="$(sed -nE 's/^[[:space:]]*CC[[:space:]]*=[[:space:]]*//p' "$CONAN_HOME/profiles/$profile" | tail -1)"
  [ -n "$prof_cc" ] || prof_cc="$prof_cxx"
  if [ -z "$prof_cxx" ] || ! { [ -x "$prof_cxx" ] || command -v "$prof_cxx" >/dev/null 2>&1; }; then
    echo "skip $cxx (profile '$profile' CXX '${prof_cxx:-<unset>}' not present on this runner)" >&2; continue
  fi
  cxx_abs="$(command -v "$prof_cxx")" || cxx_abs="$prof_cxx"
  cc_abs="$(command -v "$prof_cc")" || cc_abs="$prof_cc"
  echo "leg $cxx: CXX=$cxx_abs ($("$cxx_abs" --version 2>/dev/null | head -1))" >&2

  # One conan install per leg → toolchain + dep configs for building canon from source.
  # $CANON = the repo root; the core recipe lives at core/ (the multi-package layout).
  legdir="$WORK/conan-${cxx//+/p}"
  if ! conan install "$CANON/core" --profile:host="$profile" --profile:build="$profile" \
        --build=missing -of "$legdir" >"$legdir.install.log" 2>&1; then
    echo "CONAN INSTALL FAIL: $cxx ($profile)" >&2; tail -4 "$legdir.install.log" | sed 's/^/   /' >&2; continue
  fi
  toolchain="$(find "$legdir" -name conan_toolchain.cmake 2>/dev/null | head -1)"
  [ -f "$toolchain" ] || { echo "CONAN INSTALL FAIL: $cxx — no conan_toolchain.cmake under $legdir" >&2; continue; }

  for opt in -O0 -O3; do
    tag="${cxx//+/p}_${opt#-}"
    bdir="$WORK/$tag"
    # SPDLOG OFF: engine/debug logs carry a wall-clock stamp; keep them out of the digest.
    # -ffp-contract=off is stated rather than inherited: canon forces it PUBLIC anyway (the
    # header's retirement argument), and a cell that names the flag it is built under is one
    # the assertion below can be read against.
    cell_flags="$opt -ffp-contract=off -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF"
    if cmake -S "$PROOF" -B "$bdir" -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_C_COMPILER="$cc_abs" -DCMAKE_CXX_COMPILER="$cxx_abs" \
          -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
          -DCANON_ROOT="$CANON" \
          -DCELL_FLAGS="$cell_flags" >"$bdir.cfg.log" 2>&1 \
       && cmake --build "$bdir" --target det_proof >"$bdir.build.log" 2>&1; then
      # The retired -ffp-contract axis's premise, re-derived from THIS cell's own compile
      # lines. A hard exit, not a BUILD FAIL: a broken premise means the sweep silently lost
      # coverage it stopped claiming, which is not a condition to carry on through.
      assert_ffp_contract_forced_off "$bdir/compile_commands.json" "$tag" "$CANON" || exit 4
      bin="$(find "$bdir" -name det_proof -type f -perm -u+x 2>/dev/null | head -1)"
      if [ -x "$bin" ]; then builds+=("$tag"); BIN["$tag"]="$bin";
      else echo "BUILD FAIL: $tag (no det_proof binary)" >&2; fi
    else
      echo "BUILD FAIL: $tag" >&2
      { tail -4 "$bdir.build.log" "$bdir.cfg.log" 2>/dev/null | sed 's/^/   /' >&2; } || true
    fi
  done
done
[ "${#builds[@]}" -gt 0 ] || { echo "no builds succeeded" >&2; exit 1; }

# ── Gate integrity: every REQUIRED compiler must have produced a build ─────────
for req in ${DETERMINISM_REQUIRE_COMPILERS:-}; do
  pfx="${req//+/p}_"
  printf '%s\n' "${builds[@]}" | grep -q "^$pfx" || {
    echo "GATE INTEGRITY FAIL: required compiler '$req' produced no successful build" >&2
    echo "  — the cross-compiler property is unverified; the gate would be hollow." >&2
    exit 3
  }
done

# ── Each build replays the whole corpus in one process; the digest is its stdout ─
#
# THE STATUS IS READ EXPLICITLY AND THE STDERR IS KEPT. What `2>/dev/null` cost here was never a
# false green: this script runs under `set -e`, so a non-zero fixture DID abort the loop before the
# compare (measured 2026-09-04 — the loop dies carrying the fixture's own status, and the PASS line
# below is never reached). What it cost was the DIAGNOSIS and the ATTRIBUTION. Nothing named the
# cell, nothing said why, and the fixture's error exit is 2 — the same 2 this script returns for a
# missing prerequisite (no conan, no cmake, no corpus) — so an operator could not tell "a corpus
# file is unreadable" from "conan is not installed" without reproducing the run by hand.
# det_proof has exactly two orderly error exits, both `return 2`, and both diagnose ONLY on the
# stream that went to /dev/null: no corpus arguments ("usage: det_proof <corpus-file> ..."), and a
# corpus file it cannot open ("cannot open <path>", which names the file). A third failure shape is
# not a return at all — its main is NOLINT(bugprone-exception-escape), so an escaping exception
# aborts the process and the shell reports 134. Exit 5 is this script's own and is distinct from the
# 1/2/3/4 used above, so a failing fixture can never again be read as a missing prerequisite.
#
# THE STDERR GOES TO A FILE AND IS NEVER MERGED INTO STDOUT, and that is a determinism requirement
# rather than hygiene. Canon's module loggers carry a wall-clock `[%Y-%m-%d %H:%M:%S.%e]` pattern.
# These cells compile them out (-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF above), so their stderr is
# empty — but a build without that flag emits them, measured 2026-09-04 on this repo's
# proof/build-inventory-gcc16-release/det_proof over ONE corpus file: 20 records, 1679 bytes, every
# line carrying the clock. A `2>&1` here would hash the operator's clock into the digest and make
# two runs of one binary on one input differ, which is precisely the defect this gate exists to
# catch. stdout stays the digest and nothing else.
for tag in "${builds[@]}"; do
  run_rc=0
  # shellcheck disable=SC2086
  "${BIN[$tag]}" $CORPUS > "$WORK/$tag.out" 2>"$WORK/$tag.err" || run_rc=$?
  if [ "$run_rc" -ne 0 ]; then
    echo "FIXTURE FAIL: cell $tag -- det_proof exited $run_rc over $(echo "$CORPUS" | wc -l) corpus file(s)." >&2
    echo "  Its stdout stopped at $(wc -c < "$WORK/$tag.out") byte(s), so that file is a PREFIX of a digest and" >&2
    echo "  not a digest. Every cell fails on the same input at the same point, so a compare run over these" >&2
    echo "  files would find the prefixes IDENTICAL and report agreement between truncated outputs as" >&2
    echo "  determinism. The walk stops here instead." >&2
    surface_fixture_stderr "$tag" "$WORK/$tag.err"
    exit 5
  fi
  surface_fixture_stderr "$tag" "$WORK/$tag.err"
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
echo "PASS: canon public digest byte-identical across ${#builds[@]} builds (compiler×stdlib × -O{0,3};" >&2
echo "      -ffp-contract is NOT an axis of this sweep and is asserted forced-off per cell — header)." >&2

digest_sha="$(sha256sum "$ref" | awk '{print $1}')"
echo "canon public digest sha256=$digest_sha" >&2

# Emit this leg's digest for the workflow to cross-compare against the other legs (gcc/clang ×
# x86/arm64 + msvc). The per-leg sweep-invariance above is already asserted; the cross-toolchain/
# ISA/OS bit-identity — and the published per-release golden — come from the workflow's compare.
if [ -n "${DETERMINISM_OUT:-}" ]; then
  cp "$ref" "$DETERMINISM_OUT"
  echo "emitted digest → $DETERMINISM_OUT" >&2
fi
exit 0
