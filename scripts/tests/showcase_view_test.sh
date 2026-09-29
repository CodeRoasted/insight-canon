#!/usr/bin/env bash
###############################################################################
# showcase view gate — the proof that the public canon showcase is a declared
# VIEW of canon's projection, selected by the published tool and never filtered
# after it (ADR-33.D5, D2, D6; the arms H1–H5 of DN-121.D7).
#
# WHY THIS EXISTS. `det_proof` prints the whole CanonicalEvent, and its `params`
# member holds the very values canon's templates mask, so a render of the whole
# projection publishes them; nothing in canon said which columns a publication may
# show. The view now lives in `det_proof --showcase` — an allowlist compiled into
# the tool, checked at compile time — and this gate holds what the compiler
# cannot: that the rows it prints are the whole rows with the omitted columns cut,
# that a masked value never reaches a render, and that the pins a reader re-runs
# from name the source, the invocation and the view.
#
# THE ARMS, each red before the view existed:
#   H1  --showcase over proof/corpus/ prints no `params` column; `# columns`
#       names `cues` and 15 members; `# omits params`.
#   H2  the selection property: the showcase body equals the whole-mode body with
#       the `# omits` columns cut from every events row, header lines aside.
#   H3  --showcase with --digest, in either order, is a usage error: exit 2, no
#       stdout. The digest is the whole projection the generation gate compares.
#   H4  samples_showcase.sh over a synthetic tree whose lines carry
#       documentation-range addresses (RFC 5737) that canon masks: the render
#       shows `<*>` and no file of the output carries an address.
#   H5  PINS.md names the insight-canon commit, the invocation and ONE view line
#       equal to the render's version, view, arms and columns header lines; the
#       README's column list equals the `# columns` line.
#
# WHAT IS AND IS NOT COVERED. Covered: the binary it is handed, over the public
# proof corpus and one synthetic tree. NOT covered: that the binary is
# deterministic across builds (the tower in det_public_proof.sh, which runs this
# gate on every cell it builds), and the MSVC leg, whose golden.yaml path is
# PowerShell and does not run bash.
#
#   bash scripts/tests/showcase_view_test.sh <det_proof-binary>
#
# At a desk the binary is the inventory build `malf build` leaves at
# proof/build-inventory-<profile>/det_proof.
###############################################################################
set -uo pipefail

DET="${1:?usage: showcase_view_test.sh <det_proof-binary>}"
[ -x "$DET" ] || { echo "error: det_proof '$DET' is not an executable" >&2; exit 2; }
DET="$(cd "$(dirname "$DET")" && pwd)/$(basename "$DET")"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CANON="$(cd "$SCRIPT_DIR/../.." && pwd)"
SHOWCASE_SH="$CANON/scripts/samples_showcase.sh"
[ -f "$SHOWCASE_SH" ] || { echo "error: $SHOWCASE_SH missing — this gate has no subject." >&2; exit 2; }
# LC_ALL=C: the argument order det_public_proof.sh and golden.yaml hand det_proof.
mapfile -t CORPUS < <(find "$CANON/proof/corpus" -maxdepth 1 -type f -name '*.log' | LC_ALL=C sort)
[ "${#CORPUS[@]}" -gt 0 ] || { echo "error: no *.log under $CANON/proof/corpus" >&2; exit 2; }

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

exited() {   # <name> <expected status> <actual status> <stderr file> — a red names the last stderr line
    check "$1" "$2" "$3"
    [ "$2" = "$3" ] || printf '         stderr:   %s\n' "$(tail -1 "$4")"
}

header_value() {   # <det_proof text output> <key> -> the value of its `# <key> <value>` header line
    sed -n '/^## /q;p' "$1" | sed -n "s/^# $2 //p"
}

# ── H1 ────────────────────────────────────────────────────────────────────────
SHOWCASE_OUT="$TMP/showcase.txt"
rc=0; "$DET" --showcase "${CORPUS[@]}" > "$SHOWCASE_OUT" 2>"$TMP/showcase.err" || rc=$?
exited "H1 det_proof --showcase over proof/corpus exits 0" 0 "$rc" "$TMP/showcase.err"
check "H1 the header names the showcase view" "showcase" "$(header_value "$SHOWCASE_OUT" view)"
read -ra shown <<<"$(header_value "$SHOWCASE_OUT" columns)"
check "H1 the columns line opens with the cues column" "cues" "${shown[0]:-<no columns line>}"
check "H1 the columns line names cues plus 15 members" 16 "${#shown[@]}"
check "H1 no column is params" "absent" \
    "$(grep -qx params <<<"$(printf '%s\n' "${shown[@]}")" && echo present || echo absent)"
check "H1 the omits line names params" "params" "$(header_value "$SHOWCASE_OUT" omits)"
events_rows() {   # <file> <fields> -> "<rows> <rows of another width>"
    awk -F'\t' -v want="$2" '
        /^### events$/ { inside = 1; next }
        /^#/           { inside = 0 }
        inside         { rows++; if (NF != want) other++ }
        END            { printf "%d %d", rows, other }' "$1"
}
read -r rows other <<<"$(events_rows "$SHOWCASE_OUT" 16)"
check "H1 the showcase prints events rows ($rows)" "yes" "$([ "$rows" -gt 0 ] && echo yes || echo no)"
check "H1 every events row carries the cues and the 15 members" 0 "$other"

# ── H2 ────────────────────────────────────────────────────────────────────────
WHOLE_OUT="$TMP/whole.txt"
rc=0; "$DET" "${CORPUS[@]}" > "$WHOLE_OUT" 2>"$TMP/whole.err" || rc=$?
exited "H2 whole mode over proof/corpus exits 0" 0 "$rc" "$TMP/whole.err"
check "H2 the whole header names the whole view" "whole" "$(header_value "$WHOLE_OUT" view)"
check "H2 whole mode prints no omits line" "" "$(header_value "$WHOLE_OUT" omits)"
read -ra whole_columns <<<"$(header_value "$WHOLE_OUT" columns)"
read -ra omitted <<<"$(header_value "$SHOWCASE_OUT" omits)"
kept=()
cut_fields=()
for index in "${!whole_columns[@]}"; do
    name="${whole_columns[$index]}"
    if grep -qx -- "$name" <<<"$(printf '%s\n' "${omitted[@]}")"; then
        cut_fields+=("$((index + 1))")
    else
        kept+=("$name")
    fi
done
check "H2 the showcase columns are the whole columns less the omits, in order" \
    "${kept[*]}" "${shown[*]}"
check "H2 every omitted member is a whole-mode column" "${#omitted[@]}" "${#cut_fields[@]}"
# The whole body with the omitted fields cut from every events row; every other line verbatim.
awk -F'\t' -v OFS='\t' -v cut="${cut_fields[*]}" '
    BEGIN          { n = split(cut, list, " "); for (i = 1; i <= n; i++) drop[list[i]] = 1 }
    /^## /         { body = 1 }
    !body          { next }
    /^### events$/ { inside = 1; print; next }
    /^#/           { inside = 0 }
    !inside        { print; next }
    {
        row = ""; first = 1
        for (i = 1; i <= NF; i++) if (!(i in drop)) { row = first ? $i : row OFS $i; first = 0 }
        print row
    }' "$WHOLE_OUT" > "$TMP/expected-body.txt"
sed -n '/^## /,$p' "$SHOWCASE_OUT" > "$TMP/showcase-body.txt"
if cmp -s "$TMP/expected-body.txt" "$TMP/showcase-body.txt"; then
    selection="identical"
else
    selection="differs: $(diff "$TMP/expected-body.txt" "$TMP/showcase-body.txt" | sed -n 1,3p | tr '\n' ' ')"
fi
check "H2 the showcase body is the whole body with the omits columns cut ($(wc -l < "$TMP/showcase-body.txt") lines)" \
    "identical" "$selection"

# ── H3 ────────────────────────────────────────────────────────────────────────
for order in "--showcase --digest" "--digest --showcase"; do
    read -ra flags <<<"$order"
    rc=0; "$DET" "${flags[@]}" "${CORPUS[0]}" > "$TMP/h3.out" 2>"$TMP/h3.err" || rc=$?
    check "H3 det_proof $order exits 2" 2 "$rc"
    check "H3 det_proof $order writes no stdout" 0 "$(wc -c < "$TMP/h3.out")"
    check "H3 det_proof $order refuses with its usage line" "usage: det_proof [--digest | --showcase]" \
        "$(grep -o '^usage: det_proof \[--digest | --showcase\]' "$TMP/h3.err" || head -1 "$TMP/h3.err")"
done

# ── H4 ────────────────────────────────────────────────────────────────────────
# Documentation-range addresses (RFC 5737): what canon masks, and what no real host owns.
ADDRESSES=(203.0.113.7 198.51.100.23)
TREE="$TMP/samples/synthetic_probe/samples"
mkdir -p "$TREE"
printf '{\n  "synthetic": true\n}\n' > "$TREE/SLICE.json"
printf 'worker 12 accepted connection from %s on port 8443\n' "${ADDRESSES[@]}" > "$TREE/connections.log"
OUT="$TMP/out"
rc=0; bash "$SHOWCASE_SH" "$DET" "$TMP/samples" "$OUT" > "$TMP/h4.log" 2>&1 || rc=$?
exited "H4 samples_showcase.sh renders the synthetic tree" 0 "$rc" "$TMP/h4.log"
RENDER="$OUT/synthetic_probe.canon.txt"
check "H4 the render shows the masked template" "worker <*> accepted connection from <*> on port <*>" \
    "$(grep -m1 -o 'worker <\*> accepted connection from <\*> on port <\*>' "$RENDER" || echo '<absent>')"
for address in "${ADDRESSES[@]}"; do
    check "H4 no output file carries $address" "" "$(grep -rlF -- "$address" "$OUT" | tr '\n' ' ')"
done

# ── H5 ────────────────────────────────────────────────────────────────────────
PINS="$OUT/PINS.md"
canon_commit="$(git -C "$CANON" rev-parse HEAD 2>/dev/null || echo '<not a checkout>')"
check "H5 PINS.md names the insight-canon commit the script ran from" "$canon_commit" \
    "$(sed -n 's/^- insight-canon commit: `\([0-9a-f]*\)`.*$/\1/p' "$PINS")"
check "H5 PINS.md names the invocation" "det_proof --showcase" \
    "$(sed -n 's/^- invocation: `\(det_proof --showcase\) .*$/\1/p' "$PINS")"
check "H5 PINS.md carries exactly one view line" 1 "$(grep -c '^- view:' "$PINS")"
expected_view="$(sed -n '1s/^# //p' "$RENDER") · view $(header_value "$RENDER" view) · arms $(header_value "$RENDER" arms) · columns $(header_value "$RENDER" columns)"
check "H5 the view pin is the render's version, view, arms and columns lines" "$expected_view" \
    "$(sed -n 's/^- view: `\([^`]*\)`$/\1/p' "$PINS")"
render_columns="$(header_value "$RENDER" columns)"
check "H5 the README's column list is the render's columns line" \
    "${render_columns:-<the render has no columns line>}" \
    "$(sed -n 's/^  `\(cues [^`]*\)`\.$/\1/p' "$OUT/README.md")"

echo "showcase view gate: $pass passed, $fail failed — $(basename "$DET") over ${#CORPUS[@]} proof corpus file(s) and one synthetic sample tree"
[ "$fail" -eq 0 ]
