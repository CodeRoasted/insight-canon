#!/usr/bin/env bash
###############################################################################
# det_public_proof reporting gate — the DESK-RUNNABLE proof that a failing
# fixture reds `scripts/det_public_proof.sh`, and that its premise assertion
# refuses a vacuous pass.
#
# WHY THIS EXISTS. The determinism proof's failure paths were demonstrated
# exactly once, by hand, by building the four-cell module tower and making a
# corpus file unreadable. That costs minutes of compile per assertion, so in
# practice nothing re-checked them — a gate whose RED is unproven is a gate
# whose green means less than it reads. This runs in well under a second and
# drives the same code the tower run drives.
#
# HOW IT REACHES THE CODE, and why not the obvious way. It SOURCES the proof.
# The proof detects sourcing (`${BASH_SOURCE[0]}" != "$0"`) and returns before
# its first side effect, so this gets the reporting functions and no build.
# The cheap alternative — an environment variable naming the fixture binary,
# so a stub could stand in — is REFUSED: this proof is public and
# outsider-checkable, and a proof whose binary can be swapped by a variable
# stops being one. Sourcing needs no variable, so there is nothing to leak and
# an executed run of the proof can never take that branch.
#
# WHAT IS AND IS NOT COVERED, stated so a green is not over-read. Covered: the
# verdict a failing fixture produces, the stderr excerpt's shape, and the
# -ffp-contract premise assertion — all of them pure functions of files and a
# status. NOT covered: that det_proof itself exits non-zero on an unreadable
# corpus file, that the four cells build, or that their digests agree. Those
# are the tower's to prove and only the tower can.
#
#   bash scripts/tests/det_public_proof_reporting_test.sh
#
# Registered as the ctest gate `det_public_proof_reporting` (core/CMakeLists.txt)
# so it runs under `malf test insight-canon` and cannot rot.
###############################################################################
# SC1090 (non-constant source path) is disabled for the FILE — the directive has to precede the
# first command, which is why it sits here rather than beside the six `source` lines. The path is
# derived from BASH_SOURCE so this gate runs from any working directory, and pointing shellcheck at
# a literal would be a SECOND spelling of where the proof lives — the drift this whole thread is
# about. `shellcheck -S warning` is clean on the proof itself, which is the file that ships.
# shellcheck disable=SC1090
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROOF_SH="$(cd "$SCRIPT_DIR/.." && pwd)/det_public_proof.sh"
[ -f "$PROOF_SH" ] || { echo "error: $PROOF_SH missing — this gate has no subject." >&2; exit 2; }

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
pass=0; fail=0

check() {   # <name> <expected> <actual>
    if [ "$2" = "$3" ]; then
        printf '  ok   %s\n' "$1"; pass=$((pass + 1))
    else
        printf '  FAIL %s\n         expected: %s\n         actual:   %s\n' "$1" "$2" "$3"
        fail=$((fail + 1))
    fi
}

# THE SOURCE SEAM IS ITSELF AN ARM, and it is checked before anything is driven: if sourcing stops
# yielding the functions — or starts running the build — every arm below would pass or fail for the
# wrong reason. Its failure is fatal rather than a count.
#
# BOUNDED, because the interesting mutation does not FAIL, it HANGS. Delete the proof's source guard
# and sourcing runs the four-cell tower: measured here, the gate went from 0.4 s to still running at
# 120 s. A hang is a worse verdict than a red — nothing is reported and a CI job burns its budget —
# so the probe is wrapped in `timeout` and an over-run is named as exactly what it is.
src_rc=0
src_out="$(timeout 30 bash -c 'source "$1" >/dev/null 2>"$2" && declare -F report_fixture_failure surface_fixture_stderr assert_ffp_contract_forced_off' \
                   _ "$PROOF_SH" "$TMP/src.err")" || src_rc=$?
if [ "$src_rc" -eq 124 ]; then
    echo "error: sourcing $PROOF_SH did not RETURN within 30s." >&2
    echo "  A guarded source takes milliseconds. This is the proof running its BUILD on being" >&2
    echo "  sourced, which means the 'SOURCED, not executed' guard is gone or no longer fires" >&2
    echo "  before the first side effect. Restore it — this gate cannot drive anything without it." >&2
    exit 1
fi
if [ "$src_rc" -ne 0 ]; then
    echo "error: sourcing $PROOF_SH did not yield its reporting functions (exit $src_rc)." >&2
    echo "  Its prerequisites (conan, cmake, proof/corpus) are checked before the source guard, so" >&2
    echo "  a missing one lands here. stderr follows:" >&2
    sed 's/^/  | /' "$TMP/src.err" >&2
    exit 2
fi
for fn in report_fixture_failure surface_fixture_stderr assert_ffp_contract_forced_off; do
    check "sourcing defines $fn" "defined" \
          "$(grep -q "^$fn\$" <<< "$src_out" && echo defined || echo MISSING)"
done
check "sourcing runs NO build (the guard fires before the first side effect)" \
      "silent" "$(grep -qE '^(leg |BUILD FAIL|PASS:)' "$TMP/src.err" && echo "RAN THE BUILD" || echo silent)"

# ── report_fixture_failure — the exit-5 verdict ───────────────────────────────
# The measured shape from the 2026-09-04 repair: det_proof aborts partway through the corpus, so
# stdout holds a PREFIX of a digest. The verdict has to say four things or an operator draws the
# wrong conclusion from it.
printf 'section 1 of 7\nsection 2 of 7\n' > "$TMP/trunc.out"
printf 'cannot open /proof/corpus/service-b.log\n'   > "$TMP/trunc.err"
rfr_rc=0
rfr_out="$( ( source "$PROOF_SH"; report_fixture_failure "gpp_O3" 2 "$TMP/trunc.out" "$TMP/trunc.err" 7 ) 2>&1 )" || rfr_rc=$?

check "a failing fixture returns 5 — the proof's OWN status, never a prerequisite's 2" \
      "5" "$rfr_rc"
check "the verdict names the failing cell" "named" \
      "$(grep -q 'cell gpp_O3' <<< "$rfr_out" && echo named || echo "MISSING: $rfr_out")"
check "the verdict names the fixture's exit status and the corpus size" "named" \
      "$(grep -q 'exited 2 over 7 corpus file' <<< "$rfr_out" && echo named || echo "MISSING: $rfr_out")"
check "the verdict reports the truncation point in BYTES (30), not just 'it failed'" \
      "named" "$(grep -q 'stopped at 30 byte' <<< "$rfr_out" && echo named || echo "MISSING: $rfr_out")"
# The one an operator most needs: without it, the natural next move is to re-run the compare over
# the truncated files, find them identical, and read that as determinism.
check "the verdict states the PREFIX hazard — identical truncations are not agreement" \
      "stated" "$(grep -q 'PREFIX of a digest' <<< "$rfr_out" &&
                  grep -q 'agreement between truncated outputs' <<< "$rfr_out" &&
                  echo stated || echo "MISSING: $rfr_out")"
check "the verdict carries the fixture's OWN diagnosis, which names the file" "carried" \
      "$(grep -q 'cannot open /proof/corpus/service-b.log' <<< "$rfr_out" && echo carried || echo "MISSING: $rfr_out")"

# ── surface_fixture_stderr — bounded, and shown from BOTH ENDS ────────────────
: > "$TMP/empty.err"
quiet_out="$( ( source "$PROOF_SH"; surface_fixture_stderr "gpp_O0" "$TMP/empty.err" ) 2>&1 )"
check "a clean run surfaces NOTHING (the cells compile their loggers out)" "" "$quiet_out"

seq 1 9 | sed 's/^/line /' > "$TMP/short.err"
short_out="$( ( source "$PROOF_SH"; surface_fixture_stderr "t" "$TMP/short.err" ) 2>&1 )"
check "a short stderr (9 lines) is shown WHOLE, with no elision" "whole" \
      "$(grep -q 'line 5' <<< "$short_out" && ! grep -q 'elided' <<< "$short_out" &&
         echo whole || echo "GOT: $short_out")"

# THE ARM THIS FUNCTION EXISTS FOR, and the exact shape measured on 2026-09-04 against a build that
# does NOT compile the log macros out: 20 timestamped log records, then the real diagnosis as line
# 21 of 21. A head-only excerpt prints twenty lines of noise and hides the one line that says what
# went wrong.
{ seq 1 20 | sed 's/^/[2026-09-04 00:00:00.000] log record /'; echo 'cannot open /proof/corpus/x.log'; } > "$TMP/long.err"
long_out="$( ( source "$PROOF_SH"; surface_fixture_stderr "gpp_O3" "$TMP/long.err" ) 2>&1 )"
check "a 21-line stderr keeps the LAST line — the diagnosis behind 20 log records" "kept" \
      "$(grep -q 'cannot open /proof/corpus/x.log' <<< "$long_out" && echo kept || echo "LOST: $long_out")"
check "a 21-line stderr keeps the FIRST lines too (the pre-logger-init 'usage:' shape sits there)" \
      "kept" "$(grep -q 'log record 1$' <<< "$long_out" && echo kept || echo "LOST: $long_out")"
check "the middle IS elided (bounded output) and the elision count is exact: 21 - 8 = 13" \
      "13 elided" "$(grep -qE '\.\.\. 13 line\(s\) elided' <<< "$long_out" &&
                     ! grep -q 'log record 10$' <<< "$long_out" &&
                     echo '13 elided' || echo "GOT: $long_out")"
check "the excerpt announces the full size, so a reader knows what was cut from what" "announced" \
      "$(grep -qE 'stderr: 21 line\(s\), [0-9]+ byte' <<< "$long_out" && echo announced || echo "GOT: $long_out")"

# ── assert_ffp_contract_forced_off — the retired axis's premise ───────────────
# Hand-written compile_commands.json in the shape the proof parses (grep '"command":', then the
# text after the LAST `-c `). No build, no compiler.
ROOT="$TMP/canon"
cc_file() {   # <path> <line...>  — writes a minimal compile_commands.json
    local out="$1"; shift
    { echo '['; for l in "$@"; do printf '  {"directory": "/b", "command": "%s"},\n' "$l"; done; echo ']'; } > "$out"
}
ffp() {   # <cc-path> -> rc, output on stdout
    ( source "$PROOF_SH"; assert_ffp_contract_forced_off "$1" "cell" "$ROOT" ) 2>&1
}
ffp_rc() { ( source "$PROOF_SH"; assert_ffp_contract_forced_off "$1" "cell" "$ROOT" ) >/dev/null 2>&1; echo $?; }

cc_file "$TMP/ok.json" \
    "/usr/bin/c++ -O3 -ffp-contract=off -o a.o -c $ROOT/core/src/a.cpp" \
    "/usr/bin/c++ -O3 -ffp-contract=off -o b.o -c $ROOT/semantic/github/src/b.cpp"
check "every first-party TU at -ffp-contract=off PASSES" "0" "$(ffp_rc "$TMP/ok.json")"
check "and the pass STATES its scope — how many TUs it actually looked at" "2 stated" \
      "$(grep -q 'ffp premise: 2 first-party TU' <<< "$(ffp "$TMP/ok.json")" && echo '2 stated' || echo GOT)"

cc_file "$TMP/fast.json" \
    "/usr/bin/c++ -O3 -ffp-contract=off -o a.o -c $ROOT/core/src/a.cpp" \
    "/usr/bin/c++ -O3 -ffp-contract=fast -o b.o -c $ROOT/core/src/b.cpp"
check "one TU at =fast REDS — the axis's retirement premise is broken" "1" "$(ffp_rc "$TMP/fast.json")"
check "and the red NAMES the offending source" "named" \
      "$(grep -q "effective -ffp-contract=fast on $ROOT/core/src/b.cpp" <<< "$(ffp "$TMP/fast.json")" &&
         echo named || echo MISSING)"

# gcc's DEFAULT is `fast`, so a TU carrying no -ffp-contract at all is the same defect wearing an
# absence. An assertion that only looked for the literal `fast` would pass this.
cc_file "$TMP/unset.json" "/usr/bin/c++ -O3 -o a.o -c $ROOT/core/src/a.cpp"
check "a TU carrying NO -ffp-contract reds (gcc's default IS fast — absence is not off)" \
      "1" "$(ffp_rc "$TMP/unset.json")"
check "and the red says <unset> rather than pretending it read a value" "named" \
      "$(grep -q 'effective -ffp-contract=<unset>' <<< "$(ffp "$TMP/unset.json")" && echo named || echo MISSING)"

# LAST FLAG WINS in gcc/clang, and that is the entire reason the axis was retired: canon's PUBLIC
# -ffp-contract=off lands after the harness's directory-scope injection. Both directions, because
# an assertion reading the FIRST flag would pass one and fail the other with the same verdict text.
cc_file "$TMP/lastwins_off.json" \
    "/usr/bin/c++ -O3 -ffp-contract=fast -ffp-contract=off -o a.o -c $ROOT/core/src/a.cpp"
check "last-wins: =fast then =off is OFF (the shipped flag order) — passes" "0" \
      "$(ffp_rc "$TMP/lastwins_off.json")"
cc_file "$TMP/lastwins_fast.json" \
    "/usr/bin/c++ -O3 -ffp-contract=off -ffp-contract=fast -o a.o -c $ROOT/core/src/a.cpp"
check "last-wins: =off then =fast is FAST — reds, so the check reads the LAST flag not the first" \
      "1" "$(ffp_rc "$TMP/lastwins_fast.json")"

check "a missing compile_commands.json reds as UNCHECKABLE, never as clean" "1" \
      "$(ffp_rc "$TMP/does-not-exist.json")"
check "and it says UNCHECKABLE — 'never looked' must not read like 'looked and found nothing'" \
      "said" "$(grep -q 'FFP PREMISE UNCHECKABLE' <<< "$(ffp "$TMP/does-not-exist.json")" && echo said || echo MISSING)"

# THE ANTI-VACUITY ARM. The check is SCOPED to sources under $CANON so it does not judge CMake's
# synthesised `std` module TU. A scope that matches nothing would then pass silently — which is the
# failure mode a scoped assertion always has, and the reason the function counts before it judges.
cc_file "$TMP/foreign.json" \
    "/usr/bin/c++ -O3 -o s.o -c /build/CMakeFiles/__cmake_cxx23/std.cxx" \
    "/usr/bin/c++ -O3 -o t.o -c /elsewhere/other/t.cpp"
check "a compile db with NO first-party TU reds — a scoped check matching nothing is not a pass" \
      "1" "$(ffp_rc "$TMP/foreign.json")"
check "and it says so in those terms" "said" \
      "$(grep -q 'vacuous pass' <<< "$(ffp "$TMP/foreign.json")" && echo said || echo MISSING)"

echo
if [ "$fail" -ne 0 ]; then
    echo "det_public_proof reporting gate: $pass passed, $fail FAILED" >&2
    exit 1
fi
echo "det_public_proof reporting gate: $pass passed, 0 failed"
