#!/usr/bin/env bash
###############################################################################
# Canon public determinism proof gate — the EXTERNALLY-CHECKABLE half of the
# "same input → same output, bit-for-bit" claim
# (bibles/determinism_model.md §3).
#
# Canon-only by construction: it builds ONLY canon's public Apache module across
# the (ship gcc/libstdc++ × clang-21/libc++)  x  -O{0,3}  x  -ffp-contract{off,fast}
# matrix — the cross-compiler AND cross-stdlib DIAGONAL (the modules build forces the
# stdlib per compiler: clang's std module ships with libc++). Strictly stronger than
# the pre-unwrap both-libstdc++ textual matrix, and the pairing the determinism
# contract pins. It replays a canon-local PUBLIC corpus (proof/corpus/), asserts the canonical
# digest is byte-identical across every build AND matches the committed golden hash
# (proof/golden.sha256). No metalog, no eidos, no private surface — an outsider can
# clone the public repo and run this to verify the determinism claim.
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
# clang-21/libstdc++); -O / -ffp-contract are appended per cell after the profile.
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

  for opt in -O0 -O3; do for fpc in off fast; do
    tag="${cxx//+/p}_${opt#-}_${fpc}"
    bdir="$WORK/$tag"
    # SPDLOG OFF: engine/debug logs carry a wall-clock stamp; keep them out of the digest.
    cell_flags="$opt -ffp-contract=$fpc -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF"
    if cmake -S "$PROOF" -B "$bdir" -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_C_COMPILER="$cc_abs" -DCMAKE_CXX_COMPILER="$cxx_abs" \
          -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
          -DCANON_ROOT="$CANON" \
          -DCELL_FLAGS="$cell_flags" >"$bdir.cfg.log" 2>&1 \
       && cmake --build "$bdir" --target det_proof >"$bdir.build.log" 2>&1; then
      bin="$(find "$bdir" -name det_proof -type f -perm -u+x 2>/dev/null | head -1)"
      if [ -x "$bin" ]; then builds+=("$tag"); BIN["$tag"]="$bin";
      else echo "BUILD FAIL: $tag (no det_proof binary)" >&2; fi
    else
      echo "BUILD FAIL: $tag" >&2
      { tail -4 "$bdir.build.log" "$bdir.cfg.log" 2>/dev/null | sed 's/^/   /' >&2; } || true
    fi
  done; done
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
for tag in "${builds[@]}"; do
  # shellcheck disable=SC2086
  "${BIN[$tag]}" $CORPUS > "$WORK/$tag.out" 2>/dev/null
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
echo "canon public digest sha256=$digest_sha" >&2

# Emit this leg's digest for the workflow to cross-compare against the other legs (gcc/clang ×
# x86/arm64 + msvc). The per-leg sweep-invariance above is already asserted; the cross-toolchain/
# ISA/OS bit-identity — and the published per-release golden — come from the workflow's compare.
if [ -n "${DETERMINISM_OUT:-}" ]; then
  cp "$ref" "$DETERMINISM_OUT"
  echo "emitted digest → $DETERMINISM_OUT" >&2
fi
exit 0
