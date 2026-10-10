#!/usr/bin/env bash
###############################################################################
# det_proof digest memory gate — the proof that `det_proof --digest` streams:
# its peak resident memory does not grow with the size of the file it renders
# (DN-108.D24, the cut's canon generation gate, B3b of `./pharos build`).
#
# WHY THIS EXISTS. B3b renders the WHOLE corpus byte root, LogHub's 2 GB
# archives included. Until 2026-10-10 the digest mode read the whole file into
# memory, copied it once per arm for the peel, and never rewound the arena, so
# a file held 4 to 6 times its size: step 0 of 1.10.7 drove WSL to 0 GB free
# over Thunderbird.tar.gz (2.0 GB), Windows.tar.gz (1.7 GB) and
# Thunderbird_5M.log (0.9 GB), and its 15 det_proof processes were killed.
#
# THE ARM, red before the streaming digest:
#   M1  the peak RSS of `det_proof --digest` over a generated file nine times
#       larger than a small one grows by less than GROWTH_BOUND_KIB. A
#       whole-file read grows by several times the 32 MiB difference.
#
# WHY A GROWTH AND NOT A CAP. The property is "independent of file size", and a
# difference of two runs of the same binary states it whatever the binary's
# baseline is: a sanitizer build reserves terabytes of address space, so a
# `prlimit --as` cap cannot run there, and its resident baseline differs from a
# release build's. ASan's quarantine is held to 1 MiB, or freed memory it keeps
# would grow with the line count and red a streaming binary.
#
# THE MEASUREMENT'S FLOOR. The peak is the child's ru_maxrss, and Linux folds
# the forking interpreter's own resident size (about 11 MiB) into it at exec,
# so a binary below that floor reads AT the floor. The arm stays sound: a
# growth the floor hides is a growth below the floor, itself below the bound.
#
# WHAT IS NOT COVERED: the digest's bytes (the generation gate compares them),
# and the MSVC leg, whose golden.yaml path is PowerShell and does not run bash.
###############################################################################
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <det_proof>" >&2
    exit 2
fi
DET_PROOF=$1

# The two inputs: SMALL_LINES and nine times as many, about 4 MiB and 36 MiB.
SMALL_LINES=50000
LARGE_FACTOR=9
GROWTH_BOUND_KIB=16384

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

generate() {
    python3 -I - "$1" "$2" <<'PY'
import sys
lines, path = int(sys.argv[1]), sys.argv[2]
with open(path, "w", encoding="ascii", newline="\n") as out:
    for idx in range(lines):
        out.write(f"2026-10-10T12:{idx // 60 % 60:02}:{idx % 60:02}.{idx % 1000:03}Z INFO "
                  f"worker-{idx % 7} request {idx} took {idx % 997} ms from "
                  f"10.0.{idx % 251}.{idx % 253} status={'ok' if idx % 5 else 'error'}\n")
PY
}

# Prints "<exit code> <peak RSS in KiB>" of one det_proof run, its stdout discarded.
peak_rss_kib() {
    ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}quarantine_size_mb=1" python3 -I - "$@" <<'PY'
import resource, subprocess, sys
ran = subprocess.run(sys.argv[1:], stdout=subprocess.DEVNULL, check=False)
print(ran.returncode, resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)
PY
}

generate "$SMALL_LINES" "$WORK/small.log"
generate "$((SMALL_LINES * LARGE_FACTOR))" "$WORK/large.log"
small_bytes=$(stat -c %s "$WORK/small.log")
large_bytes=$(stat -c %s "$WORK/large.log")

read -r small_code small_kib < <(peak_rss_kib "$DET_PROOF" --digest "$WORK/small.log")
read -r large_code large_kib < <(peak_rss_kib "$DET_PROOF" --digest "$WORK/large.log")

echo "M1: small ${small_bytes} bytes -> exit ${small_code}, peak ${small_kib} KiB"
echo "M1: large ${large_bytes} bytes -> exit ${large_code}, peak ${large_kib} KiB"
if [[ $small_code -ne 0 || $large_code -ne 0 ]]; then
    echo "FAIL M1: det_proof --digest exited ${small_code} (small) and ${large_code} (large), expected 0 and 0" >&2
    exit 1
fi
growth=$((large_kib - small_kib))
if ((growth >= GROWTH_BOUND_KIB)); then
    echo "FAIL M1: peak RSS grew by ${growth} KiB for $((large_bytes - small_bytes)) more input bytes;" \
         "expected < ${GROWTH_BOUND_KIB} KiB — the digest holds the file, not a line" >&2
    exit 1
fi
echo "PASS M1: peak RSS grew by ${growth} KiB (< ${GROWTH_BOUND_KIB} KiB) for $((large_bytes - small_bytes)) more input bytes"
