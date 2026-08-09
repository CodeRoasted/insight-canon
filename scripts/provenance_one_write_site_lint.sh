#!/usr/bin/env bash
###############################################################################
# DN-29.D14 / DN-29.D16 / DN-32.D3 — the PROVENANCE-PAIR ONE-WRITE-SITE lint
#
#   "A value and its provenance are assigned together, through one site, and are
#    not independently settable."
#
# Takes the field to check as its argument, because canon now carries TWO of these
# pairs on CanonicalEvent and a copy-pasted second script would drift from the first
# the way the brace-init blind spot drifted from the assignment one (see below). One
# script, one argument, one behaviour:
#
#     provenance_one_write_site_lint.sh declared_timestamp
#     provenance_one_write_site_lint.sh declared_level
#
# ── Why this gate exists, and why it is a LINT rather than a test ─────────────
# `ParsedLine::timestamp` is an `EventTime` and `ParsedLine::level` is an
# `EventLevel`: one value carrying the datum AND its provenance, so on that side
# each clause is STRUCTURAL — there is no way to write one without the other, and
# nothing to enforce.
#
# `CanonicalEvent` deliberately carries NEITHER type. For the timestamp it uses the
# `Timestamp{}` sentinel for absence, and that sentinel is load-bearing downstream
# (`resolve_event_time` keys on `parsed_ts != Timestamp{}`). Pushing `EventTime`
# across the seam would either drag `std::optional` into every consumer or give
# `EventTime` TWO representations of absence — worse than two types with one each.
# For the level it is the hot-path POD every downstream package reads by value, and
# `LogLevel::Unknown` already spells absence there. So the seam keeps the plain datum
# + a separate `declared_*` bool, and on THAT side each pairing is held by discipline.
#
# "Correct today, held by discipline" is exactly the sentence that was wrong four
# times in this tail. So the one-ness is made the CHECKED THING rather than a
# consequence of it: a second write site fails on the commit that adds it, months
# before any test would notice, and long before a wrong event time reaches a user.
#
# ── WHAT IT COUNTS: writes, never mentions ───────────────────────────────────
# TWO write forms, because there are two ways to write a member:
#   * ASSIGNMENT through an object — `x.declared_timestamp =`, `x->declared_timestamp =`,
#     and the designated-initialiser form `{.declared_timestamp = true}` (it carries
#     the same leading dot, so one pattern covers all three);
#   * BRACE-INIT — `declared_timestamp{true}`, e.g. a member-init list if
#     CanonicalEvent ever gains a constructor.
#
# The brace form was MISSING from the first version of this lint, and it was found by
# Kleio's sibling sweep rather than by this gate — her pattern is deliberately broader
# because a SWEEP should over-trigger (classify a false positive by hand rather than
# miss a real write), where a GATE should not (a false red costs everyone). Both
# calibrations were right; the bug was that a genuine write construct was absent from
# the tighter one.
#
# Deliberately NOT counted:
#   * the DECLARATION (`bool declared_timestamp{false};`) — brace-init preceded by its
#     TYPE, which is what separates it from a member-init-list write
#   * comparisons (`== / != / >= / <=`) — reads
#   * comments and doc prose — stripped before the scan
#   * this script's own diagnostics — it never scans itself
# Counting mentions instead would fire on the declaration and on the error
# message that explains the failure, which is a gate that cannot pass.
#
# ⚠ ONE BLIND SPOT, DECLARED RATHER THAN IMPLIED: a POSITIONAL aggregate init
# (`CanonicalEvent e{id, ts, true, …}`) writes the field while naming nothing, so no
# textual pattern can see it — neither this gate's nor the broader sweep's. It is not
# reachable today (the struct has 15+ members and no call site initialises it
# positionally), and the honest statement is that this lint checks NAMED writes.
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

if [ "$#" -ne 1 ]; then
  echo "usage: $(basename "$0") <declared_timestamp|declared_level>" >&2
  exit 2
fi
FIELD="$1"
EXPECTED_WRITES=1

# The pairing each field belongs to, named so a failure explains ITSELF rather than
# sending the reader to a doc. Anything else is a typo, and a typo must not scan to a
# green: an unknown field has zero write sites everywhere, which would read as
# "the write site is GONE" on a field that never existed.
case "$FIELD" in
  declared_timestamp) PAIR='the timestamp and its provenance'; PARTNER='EventTime'; SLOT='DN-29.D14' ;;
  declared_level)     PAIR='the level and its provenance';     PARTNER='EventLevel'; SLOT='DN-32.D3' ;;
  *) echo "::error::unknown field '${FIELD}' — this lint covers declared_timestamp and declared_level." >&2
     exit 2 ;;
esac

# Form 1 — assignment through an object (covers `.x =`, `->x =`, and `{.x = …}`),
# and not a comparison: `=` not preceded by [=!<>] and not followed by `=`.
ASSIGN_RE="(\.|->)[[:space:]]*${FIELD}[[:space:]]*[^=!<>[:space:]]?[[:space:]]*=[^=]"
# Form 2 — brace-init (`x{true}`), e.g. a member-init list.
BRACE_RE="${FIELD}[[:space:]]*\{"
# ...minus the DECLARATION, which is the same shape preceded by its type. Excluded at
# LINE granularity: a declaration and a brace-init write never share a line.
DECL_RE="\bbool[[:space:]]+${FIELD}[[:space:]]*\{"

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
  stripped="$(strip_comments "$f")"
  hits="$(
    {
      printf '%s\n' "$stripped" | grep -nE "$ASSIGN_RE" || true
      printf '%s\n' "$stripped" | grep -nE "$BRACE_RE" | grep -vE "$DECL_RE" || true
    } | sort -t: -k1,1n -u
  )"
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

echo "${SLOT} one-write-site lint — field: ${FIELD}"
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
    echo "${SLOT}: ${PAIR} are assigned TOGETHER, through ONE site."
    echo "A second write site is a path that can set a declared value without setting the marker, or"
    echo "set the marker without the value — the two-field failure the ${PARTNER} type exists to"
    echo "prevent on the ParsedLine side, reappearing on the CanonicalEvent side of the seam."
    echo
    echo "IF THIS FIRED BECAUSE A SECOND LEGITIMATE CONSTRUCTION SITE NOW EXISTS — an acquisition"
    echo "path building CanonicalEvents without going through make_event — then this gate has done"
    echo "its real job: it is the trigger telling you the boundary is void. The seam's split"
    echo "representation (${PARTNER} on ParsedLine, the plain datum + a bool on CanonicalEvent) was"
    echo "affordable ONLY because every path funnelled through one site. On that day ${PARTNER} (or"
    echo "a sentinel-tier twin) crosses the seam and the owning slot is rewritten — do not silence"
    echo "this gate to keep the split."
  else
    echo "The write site is GONE. Either make_event no longer sets ${FIELD} — in which case every"
    echo "event now claims the UNDECLARED species, silently collapsing the distinction the marker"
    echo "exists to carry — or the field was renamed and this lint was not updated with it."
  fi
  exit 1
fi

echo "PASS: ${FIELD} is written at exactly one site — provenance cannot be set apart from its value."
exit 0
