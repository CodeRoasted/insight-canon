#!/usr/bin/env bash
###############################################################################
# SRC-SP-1 semantic-unawareness lint (ADR 0024 §9.1; insight_canon_semantic_packages.md §9)
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
# Core only: core/src + core/api (the mechanism + its public/provider surface). Excluded:
#   * semantic/          — the packages; they OWN the vocabulary (that is the point)
#   * bench/ proof/      — composition consumers BY DESIGN (they name package manifests)
#   * core/tests/        — test bodies exercise universal mechanisms with ecosystem-shaped
#                          inputs (incl. absence assertions) — not the mechanism
#   * build*/            — generated
#
# ── What is a violation vs what is allowed ─────────────────────────────────────
# We scan COMMENT-STRIPPED code, STRING-LITERALS PRESERVED: a deny literal inside a
# string is behavioral (a matcher fused into the mechanism) even when it hides behind
# a `//` sequence inside that string ("http://…::group::…"); a deny literal in a
# comment legitimately documents the grammar (spi.cppm: "`##[group]` → GroupBegin").
# The `LogFormat` enum (incl. its `to_string` render and every qualified `LogFormat::<Ident>`
# reference) is the closed, wire-stable identifier registry that §1.3 KEEPS in core. It is
# exempted EXPLICITLY — `strip_logformat_registry` blanks the enum body, the `to_string(LogFormat)`
# render, and qualified refs before the scan — so a single-word dialect name (Jenkins, and any
# future Splunk/Datadog/…) is exempt on principle, not by the accident that the compound
# "GitHubActions" carries no word boundary after "github" (that never generalized to bare names).
#
# A hit in comment-stripped code = an ecosystem literal fused into the mechanism =
# an SRC-SP-1 regression = FAIL. Verbose on failure (file:line of every hit).
# NON-VACUITY: scanning zero files is a FAIL (a moved/renamed scan root must never
# turn this gate silently green).
###############################################################################
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CANON="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CANON"

# Core scan roots (the mechanism + its contract surface). semantic/, bench/, proof/ deliberately absent.
SCAN_ROOTS=(core/src core/api)

# The deny-list: BEHAVIORAL ecosystem literals as they appear in CODE. Deliberately NOT the
# LogFormat enum names (GitHubActions/…) — those are the closed identifier registry §1.3 keeps core.
#   - GHA workflow-command markers (`##[error]`, `::group::`, `::set-output`, …)
#   - GHA intent-marker prefixes (`"Complete job name: "`, the exact `"Run "` step-marker literal)
#   - the GHA discriminant source (`Requested labels`) + capture-section marker (`SIFT_CAPTURE`)
#   - the echoed-source SGR params (`36;1`/`1;36`) — the GHA runner's command-echo styling
#   - test-framework file-naming vocabulary (jest/vitest/playwright `.test.`/`.spec.`; the exact
#     pytest `"test_"` prefix literal; go/ruby `_test.go`/`_spec.rb`; cypress `.cy.`)
#   - dialect/framework identifiers in code (`github_actions`, bare `github`/`gha`, jest/mocha/
#     pytest/vitest/playwright/jenkins/gitlab, the retired `intent-gha` registry tag)
DENY=(
  '##\['                                              # GHA workflow-command bracket
  '::(group|endgroup|error|warning|notice|debug|set-output|save-state|add-mask|echo|add-matcher)::'
  'Complete job name'                                 # GHA Job intent marker
  '"Run "'                                            # GHA Step intent marker (exact code literal)
  'Requested labels'                                  # GHA declared runs-on discriminant source
  'SIFT_CAPTURE'                                      # sift-action capture-section marker
  '36;1|1;36'                                         # GHA echoed-source SGR params
  '_test\.(go|py|rb)'                                 # go/pytest/ruby suffix set
  '_spec\.rb'                                         # ruby spec suffix
  '"test_"'                                           # pytest basename prefix (exact code literal)
  '\.(test|spec)\.(ts|tsx|js|jsx|mjs|cjs)'            # jest/vitest/playwright spec-extension family
  '\.cy\.'                                            # cypress
  'github_actions'                                    # the migrated GHA strategy TU identifier
  'intent-gha'                                        # the retired dialect registry tag
  '\b(github|gha)\b'                                  # bare dialect identifiers ("GitHubActions" has no \b after "github")
  '\b(jest|mocha|pytest|vitest|playwright|jenkins|gitlab)\b' # framework/dialect identifiers in code
  'section_start:|section_end:'                       # GitLab section-marker prefixes
)

# Build one alternation.
PATTERN="$(IFS='|'; echo "${DENY[*]}")"

# Comment-stripping, STRING-AWARE: string/char literals are matched FIRST and kept verbatim
# (a deny literal inside a string must stay scannable — the old `s{//…}{}` dropped everything
# after a `//` inside a string, a false-negative vector); block comments become newline-preserving
# blanks (line numbers stay aligned); line comments drop. Earliest-match-wins alternation gives
# correct precedence (a quote inside a comment never opens a string, because the comment matched
# first). Known conservative edge: C++ raw strings R"x(…)x" with embedded quotes are parsed as
# plain strings — content stays scannable, which errs toward detection, never suppression.
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

# Exempt the LogFormat identifier registry (ADR 0024 §1.3). The closed enum, its
# `to_string` render, and any qualified `LogFormat::<Ident>` reference are the sanctioned
# home of dialect *identifiers* in core — an identifier registry, never a behavioral literal
# fused into the mechanism. Blank them (newline-preserving) BEFORE the deny scan so a
# single-word dialect name (Jenkins, and any future Splunk/Datadog/…) is exempt on principle,
# not by the fragile accident that the compound "GitHubActions" carries no word boundary after
# "github". A dialect literal ANYWHERE ELSE (a bare "jenkins" behavior in a recognizer, a marker
# string) is untouched here and still trips the gate — this narrows the registry, not the scan.
strip_logformat_registry() {
  # `!`-delimited s/// throughout: the patterns carry bare `{`/`}` in their character classes,
  # which collide with perl's brace-counting when `{…}` is the delimiter (a crashed scrubber
  # emits nothing → a silent green); `~` collides with the `=~` in the replacement code. `!`
  # appears in neither pattern nor replacement.
  perl -0777 -pe '
    # (a) the enum body: `enum class LogFormat … { … }` (flat — enum values carry no nested braces)
    s! (enum \s+ class \s+ LogFormat \b [^{]*) (\{ [^{}]* \}) ! $1 . ($2 =~ tr/\n//cdr) !gesx;
    # (b) the `to_string(LogFormat …) … { … }` body — the switch nests braces, so match balanced
    s! (\b to_string \s* \( [^)]*\b LogFormat \b [^)]* \) [^{;]*) (\{ (?: [^{}]++ | (?2) )* \}) ! $1 . ($2 =~ tr/\n//cdr) !gesx;
    # (c) qualified enum references elsewhere (recognizers, conformance probe arrays)
    s! \b LogFormat \s* :: \s* [A-Za-z_]\w* !LogFormat::_!gx;
  '
}

violations=0
scanned=0
report=""
while IFS= read -r -d '' f; do
  scanned=$((scanned + 1))
  # Match comment-stripped content, but recover the real line numbers by grepping the stripped text.
  hits="$(strip_comments "$f" | strip_logformat_registry | grep -nEi "$PATTERN" || true)"
  if [ -n "$hits" ]; then
    while IFS= read -r line; do
      report+="  ${f}:${line}"$'\n'
      violations=$((violations + 1))
    done <<< "$hits"
  fi
done < <(
  for root in "${SCAN_ROOTS[@]}"; do
    [ -d "$root" ] || continue
    find "$root" -type f \( -name '*.cpp' -o -name '*.cppm' -o -name '*.hpp' -o -name '*.h' -o -name '*.cc' \) \
      -not -path '*/build*/*' -print0
  done
)

echo "SRC-SP-1 semantic-unawareness lint — scanned: ${scanned} files under ${SCAN_ROOTS[*]} (core mechanism + contract surface)"
echo "deny-list (behavioral ecosystem literals; LogFormat enum names deliberately excluded):"
printf '    %s\n' "${DENY[@]}"
echo

if [ "$scanned" -eq 0 ]; then
  echo "::error::SRC-SP-1 lint scanned ZERO files — the scan roots (${SCAN_ROOTS[*]}) are missing or empty."
  echo "A relocated core must update SCAN_ROOTS; a vacuous scan is a silent green, so this FAILS."
  exit 2
fi

if [ "$violations" -ne 0 ]; then
  echo "::error::SRC-SP-1 VIOLATION — ${violations} ecosystem literal(s) fused into canon CORE (must live in a semantic package):"
  printf '%s' "$report"
  echo
  echo "Core is semantic-unaware: the mechanism carries no ecosystem literal. Move the marker/prefix/suffix into semantic/github"
  echo "or semantic/test_frameworks as a rule row (the closed grammar), or — if it is a documentation"
  echo "reference — keep it in a comment, not a code string literal."
  exit 1
fi

echo "PASS: canon core carries no behavioral ecosystem literal — the vocabulary lives in the packages."
exit 0
