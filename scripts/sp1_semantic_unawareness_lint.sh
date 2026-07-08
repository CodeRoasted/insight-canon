#!/usr/bin/env bash
###############################################################################
# SP-1 semantic-unawareness lint (ADR 0024 §9.1; insight_canon_semantic_packages.md §9)
#
#   "Core is semantic-unaware. No ecosystem literal (marker, dialect prefix,
#    framework suffix) exists in canon core sources. Enforced by a lint (the
#    scrub-lint pattern: a greppable deny-list of dialect literals over core src/),
#    not by review vigilance."
#
# The vocabulary — GitHub-Actions workflow-command markers, intent-marker prefixes,
# test-framework file-naming suffixes — lives in the semantic PACKAGES
# (semantic/github, semantic/test_frameworks). This gate asserts none of it has
# leaked back into the core mechanism as a BEHAVIORAL string literal.
#
# ── Scope ──────────────────────────────────────────────────────────────────────
# Core only: src/ + api/ (the mechanism + its public/provider surface). Excluded:
#   * semantic/          — the packages; they OWN the vocabulary (that is the point)
#   * tests/ proof/ benchmarks/ conformance harnesses' test bodies — not the mechanism
#   * build*/            — generated
#
# ── What is a violation vs what is allowed ─────────────────────────────────────
# We scan COMMENT-STRIPPED code. Comments legitimately NAME the literals to document
# the grammar (e.g. spi.cppm: "`##[group]` → GroupBegin") — that is the contract's
# documentation, not knowledge fused into the mechanism, so it is NOT a violation.
# The `LogFormat` enum (incl. its `to_string` render "GitHubActions") is the closed,
# wire-stable identifier registry that §1.3 KEEPS in core — so format NAMES are NOT
# on the deny-list; only behavioral markers/suffixes/dialect identifiers are.
#
# A hit in comment-stripped code = an ecosystem literal fused into the mechanism =
# an SP-1 regression = FAIL. Verbose on failure (file:line of every hit).
###############################################################################
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CANON="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CANON"

# Core scan roots (the mechanism + its contract surface). semantic/ is deliberately absent.
SCAN_ROOTS=(src api)

# The deny-list: BEHAVIORAL ecosystem literals as they appear in CODE. Deliberately NOT the
# LogFormat enum names (GitHubActions/…) — those are the closed identifier registry §1.3 keeps core.
#   - GHA workflow-command markers (`##[error]`, `::group::`, `::set-output`, …)
#   - GHA intent-marker prefixes (`Complete job name: ` → Job)
#   - test-framework file-naming suffixes (jest/vitest/playwright `.test.`/`.spec.`; pytest `test_*.py`;
#     go/ruby `_test.go`/`_spec.rb`) — the naming vocabulary, package data
#   - dialect identifiers that named the migrated code (the GHA strategy TU, framework tokens)
DENY=(
  '##\['                                              # GHA workflow-command bracket
  '::(group|endgroup|error|warning|notice|debug|set-output|save-state|add-mask|echo|add-matcher)::'
  'Complete job name'                                 # GHA Job intent marker
  '_test\.(go|py|rb)'                                 # go/pytest/ruby suffix set
  '_spec\.rb'                                         # ruby spec suffix
  '\.(test|spec)\.(ts|tsx|js|jsx|mjs|cjs)'            # jest/vitest/playwright spec-extension family
  '\.cy\.'                                            # cypress
  'github_actions'                                    # the migrated GHA strategy TU identifier
  '\b(pytest|vitest|playwright|jenkins)\b'            # framework/dialect identifiers in code
)

# Build one alternation.
PATTERN="$(IFS='|'; echo "${DENY[*]}")"

# Comment-stripping: remove /* … */ (incl. multi-line) then // … to EOL, per file, before matching.
# rg reports the ORIGINAL line number via a stripped mirror kept line-aligned (block comments become
# blank lines, preserving numbering).
strip_comments() { perl -0777 -pe 's{/\*.*?\*/}{ $& =~ tr/\n//cdr }ges; s{//[^\n]*}{}g' "$1"; }

violations=0
report=""
while IFS= read -r -d '' f; do
  # Match comment-stripped content, but recover the real line numbers by grepping the stripped text.
  hits="$(strip_comments "$f" | grep -nEi "$PATTERN" || true)"
  if [ -n "$hits" ]; then
    while IFS= read -r line; do
      report+="  ${f}:${line}"$'\n'
      violations=$((violations + 1))
    done <<< "$hits"
  fi
done < <(
  for root in "${SCAN_ROOTS[@]}"; do
    [ -d "$root" ] || continue
    find "$root" -type f \( -name '*.cpp' -o -name '*.cppm' -o -name '*.hpp' -o -name '*.h' \) \
      -not -path '*/build*/*' -print0
  done
)

echo "SP-1 semantic-unawareness lint — scanned: ${SCAN_ROOTS[*]} (core mechanism + contract surface)"
echo "deny-list (behavioral ecosystem literals; LogFormat enum names deliberately excluded):"
printf '    %s\n' "${DENY[@]}"
echo

if [ "$violations" -ne 0 ]; then
  echo "::error::SP-1 VIOLATION — ${violations} ecosystem literal(s) fused into canon CORE (must live in a semantic package):"
  printf '%s' "$report"
  echo
  echo "Core is semantic-unaware (ADR 0024 §9.1). Move the marker/prefix/suffix into semantic/github"
  echo "or semantic/test_frameworks as a rule row (the closed grammar), or — if it is a documentation"
  echo "reference — keep it in a comment, not a code string literal."
  exit 1
fi

echo "PASS: canon core carries no behavioral ecosystem literal — the vocabulary lives in the packages."
exit 0
