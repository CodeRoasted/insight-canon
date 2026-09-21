#!/usr/bin/env bash
###############################################################################
# OPS-7.O9 disclosure-set lint — the accepted historic exposure is EXACT and CLOSED
#
# DISCLOSURE.md (Founder-signed, 2026-08-22; set corrected 2026-09-04) accepts
# that fifteen historic tags of this repository carry the 17-file LogHub
# `corpora/loghub/slice/` tree, and rules the boundary: no other ref carries any of it, and the set can never
# silently grow. Under OPS-7.O9, "disclosed" = that signed record in-tree
# MATCHED EXACT-SET by a gate sitting in CI on every publication run. This
# script is the gate half; the record is THE source.
#
# ── The predicate ──────────────────────────────────────────────────────────────
# { refs whose tree contains a path matching (^|/)corpora/loghub/slice/ }
#   == the declared set below, in BOTH directions:
#   * a ref carrying the slice outside the set  → FAIL (the exposure grew)
#   * a declared ref no longer carrying it      → FAIL (record ≠ reality: a
#     retag or rewrite happened; the record must be re-derived and re-signed)
# and each declared ref carries EXACTLY the recorded 17-file tree.
# The path is matched as a SUFFIX: the historic refs hold it under
# `data/corpora/loghub/slice/`; a re-introduction at any prefix must trip.
#
# ── Why an enumeration, not a range expansion ──────────────────────────────────
# The declared set is a closed LIST, deliberately never "all tags between
# v1.5.4 and v1.7.5": a range is satisfied by a newly minted in-range tag, so
# an attacker (or an accident) could grow the exposure inside the range without
# reddening a range-based check. The enumeration cannot be grown by minting.
#
# ── Record↔check accord ────────────────────────────────────────────────────────
# The list here is a derived cache of the record. Before checking refs, the
# script proves the cache still agrees with DISCLOSURE.md: the signature line
# exists (unsigned = no disclosure, OPS-7.O9), and the record's stated
# cardinality, range bounds, stray set, boundary tag and file count all match
# the enumeration. A stray (a declared ref outside the `v…` series) is
# OPTIONAL in the record: a record naming none requires an enumeration holding
# none, and a record naming one requires exactly that one — the stray set is
# compared both ways, so neither side can carry a ref the other does not.
# The record's file count must also add up on its own: N `*_2k.log` files +
# the attribution == the stated M-file tree. If the Founder re-signs a different boundary, this accord fails
# loudly until the enumeration is re-derived — the record stays the source.
#
# ── Fetch non-vacuity ──────────────────────────────────────────────────────────
# A shallow or tag-less clone would see an empty carrying set and could only
# fail closed — but a clone holding ONLY the declared tags would pass while
# blind to newer refs. Two guards: every declared ref must resolve, and at
# least one enumerated tag must be version-newer than the boundary (the
# boundary must be TESTABLE). CI must check out with fetch-depth: 0 +
# fetch-tags: true.
#
# ── Selftest (always on) ───────────────────────────────────────────────────────
# Every run first proves all failure arms on a synthetic repo: green on a
# conforming tag set, red on an out-of-set carrier, red on a declared ref gone
# clean, red on a vanished declared ref, red on a record↔cache disaccord (in
# cardinality, in the stray set, and in the record's own file arithmetic).
# A gate whose red arms are never exercised is a gate that can't FAIL
# (MEM:synthetic-gate-vacuity-vs-judgment) — here they fire on every run.
###############################################################################
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CANON="$(cd "$SCRIPT_DIR/.." && pwd)"

SLICE_RE='(^|/)corpora/loghub/slice/'

# The declared set — derived from DISCLOSURE.md § The exact set, accord-checked below.
DECLARED=(
  v1.5.4 v1.5.5 v1.5.6
  v1.6.0 v1.6.1 v1.6.2 v1.6.3 v1.6.4 v1.6.5
  v1.7.0 v1.7.1 v1.7.2 v1.7.3 v1.7.4 v1.7.5
)

# The declared refs outside the `v…` series, one per line, sorted (empty when none).
declared_strays() {
  printf '%s\n' "${DECLARED[@]}" | grep -v '^v' | sort || true
}

in_declared() {
  local ref="$1" d
  for d in "${DECLARED[@]}"; do [ "$ref" = "$d" ] && return 0; done
  return 1
}

# ── Accord: the enumeration above still says what the signed record says ──────
# Prints nothing on success; on failure prints the disagreement and returns 2.
accord_check() {
  local disclosure="$1"

  if [ ! -f "$disclosure" ]; then
    echo "::error::OPS-7.O9 — disclosure record not found at ${disclosure}."
    echo "No signed record in-tree = no disclosure; the gate cannot pass without it."
    return 2
  fi
  if ! grep -q '^\*\*Ruling:\*\* Founder' "$disclosure"; then
    echo "::error::OPS-7.O9 — the record at ${disclosure} carries no Founder ruling line."
    echo "An unsigned inventory is below the disclosure bar (OPS-7.O9); the gate fails until the record is signed."
    return 2
  fi

  # The exact-set sentence: "The <N> tags \`first\` … \`last\` [plus the stray \`s\`] each carrying
  # the <M>-file \`corpora/loghub/slice/\` tree (<K> \`*_2k.log\` files + attribution)." Every fact
  # is read by its own phrase, never by backtick position: the slice path is backticked too, and a
  # positional read took it for the stray the moment the record stopped naming one.
  local setline
  setline="$(grep -E 'The [0-9]+ tags' "$disclosure" | head -1 || true)"
  local rec_count rec_first rec_last rec_strays rec_files rec_logs rec_boundary
  rec_count="$(grep -oE 'The [0-9]+ tags' <<<"$setline" | grep -oE '[0-9]+' || true)"
  rec_first="$(grep -oE 'tags `[^`]+` … `[^`]+`' <<<"$setline" | cut -d'`' -f2 || true)"
  rec_last="$(grep -oE 'tags `[^`]+` … `[^`]+`' <<<"$setline" | cut -d'`' -f4 || true)"
  rec_strays="$(grep -oE 'plus the stray `[^`]+`' <<<"$setline" | cut -d'`' -f2 | sort || true)"
  rec_files="$(grep -oE '[0-9]+-file' <<<"$setline" | head -1 | grep -oE '[0-9]+' || true)"
  rec_logs="$(grep -oE '[0-9]+ `\*_2k\.log` files' <<<"$setline" | head -1 | grep -oE '^[0-9]+' || true)"
  rec_boundary="$(grep -oE 'newer than `v?[0-9][0-9.]*`' "$disclosure" | head -1 | tr -d '`' | awk '{print $3}' || true)"

  if [ -z "$rec_count" ] || [ -z "$rec_first" ] || [ -z "$rec_last" ] \
     || [ -z "$rec_files" ] || [ -z "$rec_logs" ] || [ -z "$rec_boundary" ]; then
    echo "::error::OPS-7.O9 accord — could not parse the exact-set facts out of ${disclosure}."
    echo "The record's shape changed (count/range/file-count/log-count/boundary sentence)."
    echo "Re-derive DECLARED in scripts/disclosure_set_lint.sh from the re-signed record, then re-accord."
    return 2
  fi

  local vmin vmax
  vmin="$(printf '%s\n' "${DECLARED[@]}" | grep '^v' | sort -V | head -1)"
  vmax="$(printf '%s\n' "${DECLARED[@]}" | grep '^v' | sort -V | tail -1)"

  local bad=0
  [ "${#DECLARED[@]}" -eq "$rec_count" ] \
    || { echo "accord: record says ${rec_count} tags, enumeration holds ${#DECLARED[@]}"; bad=1; }
  [ "$vmin" = "$rec_first" ] \
    || { echo "accord: record range starts at ${rec_first}, enumeration starts at ${vmin}"; bad=1; }
  [ "$vmax" = "$rec_last" ] \
    || { echo "accord: record range ends at ${rec_last}, enumeration ends at ${vmax}"; bad=1; }
  local enum_strays
  enum_strays="$(declared_strays)"
  [ "$enum_strays" = "$rec_strays" ] \
    || { echo "accord: record's stray set is [${rec_strays//$'\n'/ }], enumeration's is [${enum_strays//$'\n'/ }]"; bad=1; }
  [ "$((rec_logs + 1))" -eq "$rec_files" ] \
    || { echo "accord: record's tree is ${rec_files} files, but ${rec_logs} \`*_2k.log\` files + attribution is $((rec_logs + 1))"; bad=1; }
  [ "$vmax" = "$rec_boundary" ] \
    || { echo "accord: record's mechanical boundary is ${rec_boundary}, enumeration's newest is ${vmax}"; bad=1; }

  if [ "$bad" -ne 0 ]; then
    echo "::error::OPS-7.O9 accord BROKEN — the enumeration in scripts/disclosure_set_lint.sh no longer matches ${disclosure}."
    echo "The record is the source: re-derive the enumeration from the signed record (never the reverse) and re-accord."
    return 2
  fi

  EXPECTED_FILES="$rec_files"
  return 0
}

# ── The ref check: exact-set match over every tag and branch of a repo ────────
# run_check <repo-dir> <disclosure-path>; prints a verbose verdict, returns
# 0 = exact match, 1 = set mismatch, 2 = accord/vacuity error.
run_check() {
  local repo="$1" disclosure="$2"
  cd "$repo"

  EXPECTED_FILES=""
  accord_check "$disclosure" || return 2

  # Every declared ref must exist — an absent one means the clone is blind, not clean.
  local d missing_refs=()
  for d in "${DECLARED[@]}"; do
    git rev-parse -q --verify "refs/tags/${d}" >/dev/null || missing_refs+=("$d")
  done
  if [ "${#missing_refs[@]}" -ne 0 ]; then
    echo "::error::OPS-7.O9 — declared ref(s) absent from this clone: ${missing_refs[*]}"
    echo "Either the clone is incomplete (CI checkout needs fetch-depth: 0 + fetch-tags: true)"
    echo "or a declared tag was DELETED — deletion removes no published bytes and breaks the record; re-derive and re-sign."
    return 2
  fi

  # The boundary must be testable: at least one tag newer than the enumeration's max.
  local vmax
  vmax="$(printf '%s\n' "${DECLARED[@]}" | grep '^v' | sort -V | tail -1)"
  local newest
  newest="$(git tag | grep '^v' | sort -V | tail -1)"
  if [ "$(printf '%s\n%s\n' "$vmax" "$newest" | sort -V | tail -1)" = "$vmax" ]; then
    echo "::error::OPS-7.O9 — no tag newer than the boundary ${vmax} is visible; the boundary is untestable."
    echo "The clone's tag set is incomplete (fetch-depth: 0 + fetch-tags: true)."
    return 2
  fi

  # Enumerate every ref: tags, local heads, remote heads (never a remote's symbolic HEAD).
  local -a refs=()
  mapfile -t refs < <(git for-each-ref --format='%(refname)' refs/tags refs/heads refs/remotes \
                      | grep -vE '^refs/remotes/[^/]+/HEAD$')

  local ref name tree cnt
  local -a carriers=() extra=() gone_clean=() shape_drift=()
  for ref in "${refs[@]}"; do
    name="${ref#refs/tags/}"
    tree="$(git ls-tree -r "$ref")"
    cnt="$(grep -cE "$SLICE_RE" <<<"$tree" || true)"
    if [ "$cnt" -gt 0 ]; then
      carriers+=("${name} (${cnt} files)")
      if in_declared "$name"; then
        [ "$cnt" -eq "$EXPECTED_FILES" ] || shape_drift+=("${name} carries ${cnt} slice files, record says ${EXPECTED_FILES}")
      else
        extra+=("$name")
      fi
    fi
  done
  for d in "${DECLARED[@]}"; do
    tree="$(git ls-tree -r "refs/tags/${d}")"
    cnt="$(grep -cE "$SLICE_RE" <<<"$tree" || true)"
    [ "$cnt" -gt 0 ] || gone_clean+=("$d")
  done

  echo "OPS-7.O9 disclosure-set lint — ${#refs[@]} refs enumerated, ${#carriers[@]} carry a '…corpora/loghub/slice/' tree:"
  printf '    %s\n' "${carriers[@]:-"(none)"}"

  local rc=0
  if [ "${#extra[@]}" -ne 0 ]; then
    echo "::error::OPS-7.O9 VIOLATION — ref(s) OUTSIDE the signed set carry the slice tree: ${extra[*]}"
    echo "The accepted exposure is the closed set in DISCLOSURE.md and can never grow."
    echo "Do NOT extend the set to green this gate — that decision is the Founder's, by a new signed ruling."
    rc=1
  fi
  if [ "${#gone_clean[@]}" -ne 0 ]; then
    echo "::error::OPS-7.O9 VIOLATION — declared ref(s) no longer carry the slice tree: ${gone_clean[*]}"
    echo "The signed record no longer matches reality (a retag or history rewrite happened)."
    echo "Re-derive the set and have the Founder re-sign the record; never edit the gate to match silently."
    rc=1
  fi
  if [ "${#shape_drift[@]}" -ne 0 ]; then
    echo "::error::OPS-7.O9 VIOLATION — declared ref(s) drifted from the recorded ${EXPECTED_FILES}-file tree:"
    printf '    %s\n' "${shape_drift[@]}"
    rc=1
  fi

  if [ "$rc" -eq 0 ]; then
    echo "PASS: carrying refs == the signed set (both directions); no ref newer than ${vmax} carries the slice."
  fi
  return "$rc"
}

# ── Selftest: prove every arm on a synthetic repo before judging the real one ──
selftest() {
  local T
  T="$(mktemp -d "${TMPDIR:-/tmp}/disclosure_selftest.XXXXXX")"
  # shellcheck disable=SC2064
  trap "rm -rf '$T'" RETURN

  local R="$T/repo" GITC=(-c user.name=selftest -c user.email=selftest@none -c tag.gpgSign=false -c commit.gpgsign=false)
  git "${GITC[@]}" init -q -b main "$R"
  mkdir -p "$R/data/corpora/loghub/slice"
  echo attribution > "$R/data/corpora/loghub/slice/ATTRIBUTION.md"
  local i
  for i in $(seq 1 16); do echo "log $i" > "$R/data/corpora/loghub/slice/L${i}_2k.log"; done
  git -C "$R" "${GITC[@]}" add data
  git -C "$R" "${GITC[@]}" commit -qm carrying
  local dirty; dirty="$(git -C "$R" rev-parse HEAD)"
  local d; for d in "${DECLARED[@]}"; do git -C "$R" "${GITC[@]}" tag "$d"; done
  git -C "$R" "${GITC[@]}" rm -qr data
  git -C "$R" "${GITC[@]}" commit -qm clean
  git -C "$R" "${GITC[@]}" tag v1.9.6

  local out
  # Arm 0 — conforming synthetic repo is GREEN.
  if ! out="$( (run_check "$R" "$CANON/DISCLOSURE.md") 2>&1 )"; then
    echo "selftest FAIL: conforming synthetic repo did not pass:"; echo "$out"; return 3
  fi
  # Arm 1 — an out-of-set carrier is RED and NAMED.
  git -C "$R" "${GITC[@]}" tag v9.9.9 "$dirty"
  if out="$( (run_check "$R" "$CANON/DISCLOSURE.md") 2>&1 )"; then
    echo "selftest FAIL: out-of-set carrier v9.9.9 stayed green:"; echo "$out"; return 3
  fi
  grep -q 'OUTSIDE the signed set.*v9\.9\.9' <<<"$out" \
    || { echo "selftest FAIL: v9.9.9 red did not name the ref:"; echo "$out"; return 3; }
  git -C "$R" "${GITC[@]}" tag -d v9.9.9 >/dev/null
  # Arm 2 — a declared ref gone clean is RED and NAMED.
  local probe="${DECLARED[$(( ${#DECLARED[@]} / 2 ))]}"
  local probe_re="${probe//./\\.}"
  local orig_probe; orig_probe="$(git -C "$R" rev-parse "refs/tags/${probe}")"
  git -C "$R" update-ref "refs/tags/${probe}" "$(git -C "$R" rev-parse 'v1.9.6^{commit}')"
  if out="$( (run_check "$R" "$CANON/DISCLOSURE.md") 2>&1 )"; then
    echo "selftest FAIL: declared ref ${probe} gone clean stayed green:"; echo "$out"; return 3
  fi
  grep -q "no longer carry the slice tree.*${probe_re}" <<<"$out" \
    || { echo "selftest FAIL: gone-clean red did not name ${probe}:"; echo "$out"; return 3; }
  git -C "$R" update-ref "refs/tags/${probe}" "$orig_probe"
  # Arm 3 — a vanished declared ref is RED as blindness, never green.
  git -C "$R" "${GITC[@]}" tag -d "$probe" >/dev/null
  if out="$( (run_check "$R" "$CANON/DISCLOSURE.md") 2>&1 )"; then
    echo "selftest FAIL: vanished declared ref ${probe} stayed green:"; echo "$out"; return 3
  fi
  grep -q "absent from this clone.*${probe_re}" <<<"$out" \
    || { echo "selftest FAIL: vanished-ref red did not name ${probe}:"; echo "$out"; return 3; }
  git -C "$R" update-ref "refs/tags/${probe}" "$orig_probe"
  # Arm 4 — a cardinality disaccord is RED (the record is the source).
  local n="${#DECLARED[@]}"
  sed "s/The ${n} tags/The $((n + 1)) tags/" "$CANON/DISCLOSURE.md" > "$T/disaccord.md"
  if out="$( (run_check "$R" "$T/disaccord.md") 2>&1 )"; then
    echo "selftest FAIL: record↔check cardinality disaccord stayed green:"; echo "$out"; return 3
  fi
  grep -q "accord: record says $((n + 1)) tags" <<<"$out" \
    || { echo "selftest FAIL: cardinality disaccord red did not name the count:"; echo "$out"; return 3; }
  # Arm 5 — a stray the record names and the enumeration lacks is RED: the stray set is compared,
  # so a record that gains a stray cannot pass against an enumeration that has none (and the
  # reverse is the same comparison).
  sed 's/` each carrying/` plus the stray `0.0.0` each carrying/' "$CANON/DISCLOSURE.md" > "$T/stray.md"
  if out="$( (run_check "$R" "$T/stray.md") 2>&1 )"; then
    echo "selftest FAIL: a record naming stray 0.0.0 stayed green:"; echo "$out"; return 3
  fi
  grep -q "accord: record's stray set is \[0\.0\.0\]" <<<"$out" \
    || { echo "selftest FAIL: stray disaccord red did not name the stray:"; echo "$out"; return 3; }
  # Arm 6 — a record whose own file arithmetic does not add up is RED.
  sed -E 's/\(([0-9]+) `\*_2k\.log` files/(1\1 `*_2k.log` files/' "$CANON/DISCLOSURE.md" > "$T/arith.md"
  if out="$( (run_check "$R" "$T/arith.md") 2>&1 )"; then
    echo "selftest FAIL: a record whose log count does not add up stayed green:"; echo "$out"; return 3
  fi
  grep -q 'accord: record.s tree is' <<<"$out" \
    || { echo "selftest FAIL: file-arithmetic disaccord red did not name the arithmetic:"; echo "$out"; return 3; }

  echo "selftest PASS: green arm + 6 red arms (out-of-set carrier, gone-clean, vanished ref, cardinality, stray set, file arithmetic) all fire and name their subject."
  return 0
}

selftest
echo
run_check "$CANON" "${DISCLOSURE_PATH:-$CANON/DISCLOSURE.md}"
