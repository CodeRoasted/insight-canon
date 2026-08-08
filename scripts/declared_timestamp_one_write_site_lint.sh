#!/usr/bin/env bash
###############################################################################
# DN-29.D14 / DN-29.D16 — the `declared_timestamp` ONE-WRITE-SITE lint
#
#   "The timestamp and its provenance are assigned together, through one site,
#    and are not independently settable."
#
# ── Why this gate exists, and why it is a LINT rather than a test ─────────────
# `ParsedLine::timestamp` is an `EventTime`: one value carrying the timestamp AND
# its provenance, so on that side the clause is STRUCTURAL — there is no way to
# write one without the other, and nothing to enforce.
#
# `CanonicalEvent` deliberately does NOT carry `EventTime`. It uses the
# `Timestamp{}` sentinel for absence, and that sentinel is load-bearing
# downstream (`resolve_event_time` keys on `parsed_ts != Timestamp{}`). Pushing
# `EventTime` across the seam would either drag `std::optional` into every
# consumer or give `EventTime` TWO representations of absence — worse than two
# types with one each. So the seam keeps `Timestamp` + a separate
# `declared_timestamp` bool, and on THAT side the pairing is held by discipline.
#
# "Correct today, held by discipline" is exactly the sentence that was wrong four
# times in this tail. So the one-ness is made the CHECKED THING rather than a
# consequence of it: a second write site fails on the commit that adds it, months
# before any test would notice, and long before a wrong event time reaches a user.
#
# ── WHAT IT COUNTS: writes, never mentions ───────────────────────────────────
# A write is an ASSIGNMENT through an object — `x.declared_timestamp =` or
# `x->declared_timestamp =`. Deliberately NOT counted:
#   * the DECLARATION (`bool declared_timestamp{false};`) — no object, no `=`
#   * comparisons (`== / != / >= / <=`) — reads
#   * comments and doc prose — stripped before the scan
#   * this script's own diagnostics — it never scans itself
# Counting mentions instead would fire on the declaration and on the error
# message that explains the failure, which is a gate that cannot pass.
#
# ── SCOPE, and its declared boundary ─────────────────────────────────────────
# canon PRODUCTION code only: core/src, core/api, semantic/*/src. Tests are
# excluded on purpose — a test may construct any event shape it needs, and a gate
# that forbade that would be preventing the very arms that prove this field works.
#
# ⚠ IT DOES NOT SCAN SIBLING REPOS. A consumer (insight-eidos, insight-metalog)
# writing `declared_timestamp` would not be caught here, because canon may not
# depend on its siblings' trees. Today the field is written nowhere else — the
# measured state is one write, one declaration, one read (in eidos) — and a
# workspace-wide sweep belongs to Argos's gates, not to canon's own lint.
###############################################################################
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CANON="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CANON"

FIELD='declared_timestamp'
EXPECTED_WRITES=1

# An assignment THROUGH AN OBJECT, and not a comparison: `= ` not preceded by
# [=!<>] and not followed by `=`.
WRITE_RE="(\.|->)[[:space:]]*${FIELD}[[:space:]]*[^=!<>[:space:]]?[[:space:]]*=[^=]"

# Comment-stripping, string-literals preserved, newline-preserving for block
# comments so reported line numbers stay true. Same helper as the SRC-SP-1 lint.
strip_comments() {
  perl -0777 -pe '
    s{
        ( " (?: \\. | [^"\\] )* " )
      | ( '\'' (?: \\. | [^'\''\\] )* '\'' )
      | ( /\* .*? \*/ )
      | ( //[^\n]* )
    }{
      defined $1 ? $1 : defined $2 ? $2 : defined $3 ? ($3 =~ tr/\n//cdr) : ""
    }gesx' "$1"
}

writes=0
scanned=0
report=""
while IFS= read -r -d '' f; do
  scanned=$((scanned + 1))
  hits="$(strip_comments "$f" | grep -nE "$WRITE_RE" || true)"
  if [ -n "$hits" ]; then
    while IFS= read -r line; do
      report+="  ${f}:${line}"$'\n'
      writes=$((writes + 1))
    done <<< "$hits"
  fi
done < <(
  for root in core/src core/api semantic; do
    [ -d "$root" ] || continue
    find "$root" -type f \( -name '*.cpp' -o -name '*.cppm' -o -name '*.hpp' -o -name '*.h' \) \
      -not -path '*/build*/*' -not -path '*/tests/*' -print0
  done
)

echo "DN-29.D14 one-write-site lint — field: ${FIELD}"
echo "scanned: ${scanned} production file(s) under core/src, core/api, semantic (tests excluded by design)"
echo "write sites found: ${writes} (expected exactly ${EXPECTED_WRITES})"
[ -n "$report" ] && printf '%s' "$report"
echo

# NON-VACUITY: a scan that reached no files is a silent green and must fail.
if [ "$scanned" -eq 0 ]; then
  echo "::error::${FIELD} lint scanned ZERO files — the scan roots are missing or empty."
  echo "A relocated core must update the roots; a vacuous scan is a silent green, so this FAILS."
  exit 2
fi

if [ "$writes" -ne "$EXPECTED_WRITES" ]; then
  echo "::error::${FIELD} has ${writes} write site(s) in canon production code; the contract is exactly ${EXPECTED_WRITES}."
  if [ "$writes" -gt "$EXPECTED_WRITES" ]; then
    printf '%s' "$report"
    echo
    echo "DN-29.D14: the timestamp and its provenance are assigned TOGETHER, through ONE site."
    echo "A second write site is a path that can set a declared time without setting the marker, or"
    echo "set the marker without the time — the two-field failure the EventTime type exists to"
    echo "prevent on the ParsedLine side, reappearing on the CanonicalEvent side of the seam."
    echo
    echo "IF THIS FIRED BECAUSE A SECOND LEGITIMATE CONSTRUCTION SITE NOW EXISTS — an acquisition"
    echo "path building CanonicalEvents without going through make_event — then this gate has done"
    echo "its real job: it is the trigger telling you the boundary is void. The seam's split"
    echo "representation (EventTime on ParsedLine, Timestamp + bool on CanonicalEvent) was"
    echo "affordable ONLY because every path funnelled through one site. On that day EventTime (or"
    echo "a sentinel-tier twin) crosses the seam and DN-29.D16's slot is rewritten — do not silence"
    echo "this gate to keep the split."
  else
    echo "The write site is GONE. Either make_event no longer sets ${FIELD} — in which case every"
    echo "event now claims its time was parsed, and the DN-29.D12 ladder silently loses rung 1 —"
    echo "or the field was renamed and this lint was not updated with it."
  fi
  exit 1
fi

echo "PASS: ${FIELD} is written at exactly one site — provenance cannot be set apart from its timestamp."
exit 0
