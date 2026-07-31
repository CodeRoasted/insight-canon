module;

module insight.canon.api;
import insight.canon.internal;

namespace insight::utils
{
namespace
{
    [[nodiscard]] constexpr bool is_alpha(char chr) noexcept
    {
        return ((static_cast<unsigned>(static_cast<unsigned char>(chr)) | detail::kAsciiCaseBit) -
                'a') < detail::kAlphabetLen;
    }
    [[nodiscard]] constexpr char lower(char chr) noexcept
    {
        return chr >= 'A' && chr <= 'Z' ? static_cast<char>(chr - 'A' + 'a') : chr;
    }
    [[nodiscard]] bool iequals(std::string_view lhs, std::string_view rhs) noexcept
    {
        if (lhs.size() != rhs.size())
            return false;
        for (std::size_t idx{0}; idx < lhs.size(); ++idx)
            if (lower(lhs[idx]) != lower(rhs[idx]))
                return false;
        return true;
    }

    // First byte is an ASCII digit ("5", "500", "5ms", "2026-01-15"). Used to detect a numeric/
    // temporal CHAIN element (so a timestamp's `:SS` seconds, whose neighbour is the `:MM` minutes,
    // is recognised as part of a chain, not a standalone count).
    [[nodiscard]] bool is_digit_leading_numeric(std::string_view tok) noexcept
    {
        return !tok.empty() && tok.front() >= '0' && tok.front() <= '9';
    }

    // A BARE INTEGER ("1", "5", "500") — every byte an ASCII digit. A genuine failure COUNT is a
    // bare integer, never "5ms" / "2026-01-15" / a hash; the trimmed token (for_each_token strips
    // surrounding non-alnum) makes this exact.
    [[nodiscard]] bool is_bare_integer(std::string_view tok) noexcept
    {
        return !tok.empty() &&
               std::ranges::all_of(tok, [](char chr) noexcept { return chr >= '0' && chr <= '9'; });
    }

    // SRC-D-CNT-1 (count register): is a failure word in COUNT register, given its two preceding
    // tokens? True iff the IMMEDIATELY-preceding token is a bare-integer count ("1 failure", "5
    // failed", "HTTP 500 error") that is NOT part of a numeric/temporal chain — i.e. the token
    // before the count is NOT itself digit-leading. The chain guard is load-bearing: a leading ISO
    // timestamp "2026-…T11:00:01 ERROR" tokenises to …,`00`,`01`,ERROR, so ERROR's predecessor `01`
    // is a bare integer — but `01`'s predecessor `00` IS digit-leading (the `:MM` minutes), so it
    // is a timestamp second, not a failure count. One source of truth, shared by is_count_register
    // (the predicate) and contains_failure_cue / contains_failure_summary_cue (their in-loop prev
    // pair).
    [[nodiscard]] bool is_count_preceded(std::string_view prev, std::string_view prev2) noexcept
    {
        return is_bare_integer(prev) && !is_digit_leading_numeric(prev2);
    }

    // Negation segments that INVERT an "…Error"/"…Exception" suffix: the CamelCase
    // word right before the suffix. "…NotError"/"…NoError" is not an error type — the
    // textbook false match is a test named "…IsNotError", whose ctest/gtest result
    // line would otherwise read as a HIGH "New error", i.e. a false regression on a
    // PASSING test (the cardinal sin). This is a property of the token itself,
    // independent of the surrounding line.
    constexpr std::array<std::string_view, 3U> kNegationSegments{"no", "not", "non"};

    // The CamelCase word ending just before `suffix_start`: the maximal lowercase run
    // ending at suffix_start-1, plus its leading uppercase letter if present. For
    // "EvidenceNotError" with the "Error" suffix at index 12, this returns "Not".
    // substr(begin, suffix_start - begin): begin <= suffix_start, and the sole caller
    // (is_camel_error_type) guards token.size() > suffix.size(), so
    // suffix_start = token.size() - suffix.size() < token.size() — pos in-bounds, cannot throw.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] std::string_view preceding_camel_word(std::string_view token,
                                                        std::size_t suffix_start) noexcept
    {
        std::size_t begin{suffix_start};
        while (begin > 0U && token[begin - 1U] >= 'a' && token[begin - 1U] <= 'z')
            --begin;
        if (begin > 0U && token[begin - 1U] >= 'A' && token[begin - 1U] <= 'Z')
            --begin;
        return token.substr(begin, suffix_start - begin);
    }

    // A CamelCase error-type name: ends in "Error"/"Exception" (exact case) preceded
    // by an ASCII letter (OperationalError, ValueError, IOError, RuntimeException).
    // This recovers compound type names a standalone-word match would miss, while
    // staying immune to a lowercase "error" buried in a path/identifier (which is
    // never capitalised, so it can never form this suffix). A negated type name
    // ("…NotError"/"…NoError"/"…NonError") is EXCLUDED — see kNegationSegments.
    [[nodiscard]] bool is_camel_error_type(std::string_view token) noexcept
    {
        constexpr std::array<std::string_view, 2U> kSuffixes{"Error", "Exception"};
        return std::ranges::any_of(
            kSuffixes,
            [&token](std::string_view suffix)
            {
                if (token.size() <= suffix.size() || !token.ends_with(suffix))
                    return false;
                const std::size_t suffix_start{token.size() - suffix.size()};
                if (!is_alpha(token[suffix_start - 1U]))
                    return false;
                const std::string_view preceding{preceding_camel_word(token, suffix_start)};
                return std::ranges::none_of(kNegationSegments,
                                            [preceding](std::string_view neg) noexcept
                                            { return iequals(preceding, neg); });
            });
    }

    // The unified failure lexicon — token forms (base + the inflections that occur in
    // real CI / app logs), partitioned by BENIGN-COLLISION-PRONENESS (SRC-D-OUT-4, refined by
    // the Daidalos Q1 ruling — the criterion is collision-proneness, NOT grammatical role):
    //   • SelfAnchoring — a token that essentially NEVER appears benignly, so it fires bare
    //     with no surrounding register. Two kinds: inflected outcome verbs whose morphology
    //     IS the verdict ("build failed", "connection refused"), AND zero-collision failure-
    //     event nouns ("segfault", "traceback", "unhandled" — "unhandled exception" /
    //     "unhandled promise rejection" are real failures, lowercase, and register-gating
    //     them only suppressed recall). No benign use exists to guard against.
    //   • RegisterAnchored — a token with a real benign sense ("error rate", "crash course",
    //     "timeout=30", "fatal flaw", "panic button", "fail-safe"). It fires ONLY in verdict
    //     register (is_verdict_anchored: caps / `:` / bracket / leading ✗ / CamelCase type /
    //     phrase), exactly as is_camel_error_type distinguishes "ValueError" from a path's
    //     lowercase "error". A declared partition — NOT reactive per-FP tuning (we do not
    //     remove "crash"; we say the collision-prone noun needs context).
    // Plural "errors" is omitted ON PURPOSE so a negation like "no errors found" /
    // "0 errors" is not read as an error (count-dependent, a numeric-rule concern, not
    // the lexicon's); CamelCase `…Error`/`…Exception` types are caught structurally by
    // is_camel_error_type, not enumerated here. (If a bare "Kernel panic - not syncing:"
    // miss ever surfaces, add a `kernel panic` phrase like segmentation fault — do NOT
    // promote the collision-prone bare "panic".)
    enum class FailureRole : unsigned char
    {
        RegisterAnchored, // benign-collision-prone — fires ONLY when verdict-anchored
        SelfAnchoring,    // zero benign collision — fires bare (outcome verb or unique noun)
    };
    struct FailureWord
    {
        std::string_view word;
        FailureRole role;
    };
    constexpr std::array<FailureWord, 18U> kFailureLexicon{{
        {.word = "error", .role = FailureRole::RegisterAnchored},
        {.word = "exception", .role = FailureRole::RegisterAnchored},
        {.word = "fatal", .role = FailureRole::RegisterAnchored},
        {.word = "panic", .role = FailureRole::RegisterAnchored},
        {.word = "panicked", .role = FailureRole::SelfAnchoring},
        {.word = "refused", .role = FailureRole::SelfAnchoring},
        {.word = "timeout", .role = FailureRole::RegisterAnchored},
        {.word = "traceback", .role = FailureRole::SelfAnchoring},
        {.word = "fail", .role = FailureRole::RegisterAnchored},
        {.word = "failed", .role = FailureRole::SelfAnchoring},
        {.word = "failure", .role = FailureRole::RegisterAnchored},
        {.word = "segfault", .role = FailureRole::SelfAnchoring},
        {.word = "denied", .role = FailureRole::SelfAnchoring},
        {.word = "unhandled", .role = FailureRole::SelfAnchoring},
        {.word = "abort", .role = FailureRole::RegisterAnchored},
        {.word = "aborted", .role = FailureRole::SelfAnchoring},
        {.word = "crash", .role = FailureRole::RegisterAnchored},
        {.word = "crashed", .role = FailureRole::SelfAnchoring},
    }};
    constexpr std::array<std::string_view, 2U> kWarningWords{"warn", "warning"};

    // One token's verdict against the lexicon. `matched` says the token IS lexicon vocabulary
    // (whether or not it fired) — the CamelCase error-TYPE check is skipped for those, since a
    // lexicon word that declined its register must not be re-admitted through the weaker signal.
    // Extracted from contains_failure_cue's token lambda so each carries one decision; the
    // lexicon walk still runs exactly once per token.
    struct LexiconHit
    {
        bool fired{false};
        bool matched{false};
    };

    // Defined below the register kernels it consults (is_verdict_anchored / is_count_preceded);
    // declared here so contains_failure_cue's lambda can name it.
    [[nodiscard]] LexiconHit lexicon_hit(std::string_view text, std::string_view token,
                                         std::string_view prev, std::string_view prev_prev) noexcept;

    // Caps register (SRC-D-OUT-4 anchor #1): the token's raw bytes are ALL-UPPERCASE ASCII
    // letters, ≥2 of them — the decoration CI/test tooling uses to mark an outcome
    // (ERROR, FAILED, FATAL, PANIC). A pre-casefold byte fact; the matched token keeps
    // the source case (for_each_token trims surrounding non-alnum but never folds), so
    // this reads the raw word. Any lowercase letter disqualifies it; non-letters are
    // ignored, but a real failure word is all-alpha so this resolves to "all uppercase".
    [[nodiscard]] bool is_caps_register(std::string_view token) noexcept
    {
        std::size_t letters{0U};
        for (const char chr : token)
        {
            if (chr >= 'a' && chr <= 'z')
                return false;
            if (chr >= 'A' && chr <= 'Z')
                ++letters;
        }
        return letters >= 2U;
    }

    // Multi-word cues that are only precision-safe as an ADJACENT token pair. A bare
    // "segmentation"/"fault" collides with benign uses (image/network segmentation,
    // page fault), so the crash signal needs the two-token adjacency — and the OS/
    // shell form "Segmentation fault (core dumped)" carries no level keyword, so the
    // lexicon is the only thing that flags it.
    using Phrase = std::array<std::string_view, 2U>;
    constexpr std::array<Phrase, 1U> kFailurePhrases{{{"segmentation", "fault"}}};

    // Explicit positive test/CI verdicts (success WORDS). A line that DECLARES a pass
    // ("… Passed", gtest "[ OK ]", pytest "PASSED") must not be promoted to a failure on
    // the strength of a CamelCase error-TYPE name alone — a test named
    // "…RaisesValueError" that PASSED is not a regression. A success WORD overrides ONLY
    // that weak, name-based signal; it NEVER demotes an explicit failure WORD
    // ("error"/"failed"/…), because a leading pass WORD would false-demote a genuine
    // failure summary ("25 passed, 5 failed"). The strong failure WORD is demoted only by
    // an unambiguous leading pass GLYPH (D-OUT-1, leading_outcome_is_pass), never a word.
    constexpr std::array<std::string_view, 4U> kSuccessVerdicts{"passed", "ok", "success",
                                                                "succeeded"};

    // Only throw path is for_each_token's substr(begin, ...) with begin <= limit <= text.size()
    // (see token_scan.hpp); the noexcept body cannot throw.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] bool any_standalone_word(std::string_view text,
                                           std::span<const std::string_view> words,
                                           std::size_t scan_limit) noexcept
    {
        return for_each_token(text, scan_limit,
                              [words](std::string_view token) noexcept
                              {
                                  return std::ranges::any_of(words,
                                                             [token](std::string_view word) noexcept
                                                             { return iequals(token, word); });
                              });
    }

    // ── Outcome-aware demotion: a leading PASS GLYPH (D-OUT-1) ───────────────────────
    // Per-test PASS glyphs (3-byte UTF-8, lead byte 0xE2) are unambiguous verdicts: they
    // appear in CI output as a result marker and NOWHERE else, unlike the words
    // "passed"/"ok" (which double as summary counts, prose, and test names). They are
    // also INVISIBLE to for_each_token — a standalone glyph is all-non-alnum, so the
    // tokenizer trims it to an empty token and the verdict is destroyed before any
    // lexicon sees it — so they are byte-matched directly at a token start.
    //   ✓ U+2713 · ✔ U+2714 · ✅ U+2705 · √ U+221A (the mocha-on-Windows pass mark)
    using Glyph = std::array<unsigned char, 3U>;
    constexpr std::array<Glyph, 4U> kPassGlyphs{{
        {0xE2U, 0x9CU, 0x93U}, // ✓ U+2713 CHECK MARK
        {0xE2U, 0x9CU, 0x94U}, // ✔ U+2714 HEAVY CHECK MARK
        {0xE2U, 0x9CU, 0x85U}, // ✅ U+2705 WHITE HEAVY CHECK MARK
        {0xE2U, 0x88U, 0x9AU}, // √ U+221A SQUARE ROOT — mocha's Windows pass mark
    }};
    // Per-test FAIL glyphs — the SRC-D-OUT-4a set (contract: canon.api.cppm). The ballot-X
    // family + the ❌ cross-mark emoji, all 3-byte UTF-8; × U+00D7 is the 2-byte one the rule
    // excludes. ❌ U+274C earned its place on evidence: the 1.6.5 dogfood found ❌-led fail
    // lines anchoring nothing where ✗-led ones did.
    constexpr std::array<Glyph, 5U> kFailGlyphs{{
        {0xE2U, 0x9CU, 0x95U}, // ✕ U+2715 MULTIPLICATION X
        {0xE2U, 0x9CU, 0x96U}, // ✖ U+2716 HEAVY MULTIPLICATION X
        {0xE2U, 0x9CU, 0x97U}, // ✗ U+2717 BALLOT X
        {0xE2U, 0x9CU, 0x98U}, // ✘ U+2718 HEAVY BALLOT X
        {0xE2U, 0x9DU, 0x8CU}, // ❌ U+274C CROSS MARK — the emoji fail mark jest/vitest/mocha emit
    }};
    [[nodiscard]] bool starts_with_glyph(std::string_view text, std::size_t pos,
                                         std::span<const Glyph> glyphs) noexcept
    {
        constexpr std::size_t kGlyphLen{3U};
        if (pos + kGlyphLen > text.size())
            return false;
        const Glyph head{static_cast<unsigned char>(text[pos]),
                         static_cast<unsigned char>(text[pos + 1U]),
                         static_cast<unsigned char>(text[pos + 2U])};
        return std::ranges::find(glyphs, head) != glyphs.end();
    }
    [[nodiscard]] bool starts_with_pass_glyph(std::string_view text, std::size_t pos) noexcept
    {
        return starts_with_glyph(text, pos, kPassGlyphs);
    }
    [[nodiscard]] bool starts_with_fail_glyph(std::string_view text, std::size_t pos) noexcept
    {
        return starts_with_glyph(text, pos, kFailGlyphs);
    }
    // Advance past ANSI escapes + token delimiters; returns the next token-start index
    // (or text.size() if the rest of the line is delimiters). Mirrors for_each_token's
    // inter-token skip so the two agree on token boundaries.
    [[nodiscard]] std::size_t skip_to_token_start(std::string_view text, std::size_t pos) noexcept
    {
        for (;;)
        {
            if (const std::size_t esc{detail::ansi_escape_len(text, pos)}; esc != 0U)
                pos += esc;
            else if (pos < text.size() && detail::is_token_delimiter(text[pos]))
                ++pos;
            else
                return pos;
        }
    }

    // The trimmed token starting at `pos` (caller guarantees a non-delimiter, non-ANSI
    // start); advances `pos` past it. Mirrors for_each_token's token extraction + the
    // surrounding-non-alnum trim. substr begin <= pos <= size → noexcept body cannot throw.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] std::string_view take_trimmed_token(std::string_view text,
                                                      std::size_t& pos) noexcept
    {
        const std::size_t begin{pos};
        while (pos < text.size() && !detail::is_token_delimiter(text[pos]) &&
               detail::ansi_escape_len(text, pos) == 0U)
            ++pos;
        std::string_view token{text.substr(begin, pos - begin)};
        while (!token.empty() && !detail::is_token_alnum(token.front()))
            token.remove_prefix(1U);
        while (!token.empty() && !detail::is_token_alnum(token.back()))
            token.remove_suffix(1U);
        return token;
    }

    // ── SRC-D-OUT-4c — the KIND SLOT: anchor #2 is a POSITION claim, not an adjacency ─────
    // PREFIX MATERIAL: a token that may PRECEDE the line's kind slot without displacing it. Two
    // declared, closed classes — the same shape SRC-D-OUT-4's role partition uses, and the reason
    // this is a rule rather than per-shape tuning:
    //   1. colon-terminated — the byte past its span is `:` (`ld:`, `src/main.rs:`, `357:`, `14:`)
    //   2. bracket-enclosed — its span is bounded by `[…]`, `(…)` or `<…>` (`[main]`, `(none)`,
    //      `<WORKSPACE>`); the closing bracket may itself carry a `:`, which class 1 then covers
    //      for whatever token follows.
    // Both read the RAW span — the untrimmed extent take_trimmed_token walked — because the byte
    // that carries the structure is the one just past it, and the trim can hide it ("foo-:" trims
    // to "foo", whose next byte is `-`, not `:`).
    [[nodiscard]] bool is_prefix_material(std::string_view line, std::size_t raw_begin,
                                          std::size_t raw_end) noexcept
    {
        const char before{raw_begin > 0U ? line[raw_begin - 1U] : '\0'};
        const char after{raw_end < line.size() ? line[raw_end] : '\0'};
        if (after == ':')
            return true;
        return (before == '[' && after == ']') || (before == '(' && after == ')') ||
               (before == '<' && after == '>');
    }

    // SRC-D-OUT-4c — true iff `token` occupies the line's KIND SLOT: every token preceding it on
    // the line is prefix material.
    //
    // WHY A WALK IS NEEDED AT ALL. `after == ':'` is an adjacency test standing in for a position
    // claim, and it cannot separate
    //     error: connection refused   (a verdict)
    //     error: string               (a struct field declaration)
    //     err:   &str                 (a named parameter in a code frame)
    // — all three are byte-identical in the ±1 neighbourhood of the token, so no WIDENING of that
    // neighbourhood discriminates: the information is positional, not local. SRC-D-NOTE-1 already
    // validates its own marker BACKWARDS for exactly this reason ("structural rather than
    // lexical"); this is that same treatment for anchor #2. It also subsumes the compiler-frame
    // shape for free — `<path>:<line>:<col>:` is not a special case, it IS a run of
    // colon-terminated tokens.
    //
    // WHOLE-LINE, never head-bounded, and that is load-bearing: a register is a claim about a
    // line's structure and a bounded head is a cost control, so gating the claim on a byte budget
    // makes it change silently with presentation — the one property of a log line no producer
    // guarantees (ADR-20). Cost is bounded instead by the walk being SELF-TERMINATING: it stops
    // at the first non-prefix token, which on prose is token index 1, and it is reached only after
    // a `:` was already found.
    //
    // PRECONDITION: `token` is a sub-view of `line`, as for every register kernel here. A token
    // never reached is treated as absent and DEMOTES — the conservative direction.
    [[nodiscard]] bool token_in_kind_slot(std::string_view line, std::string_view token) noexcept
    {
        std::size_t pos{0};
        for (;;)
        {
            pos = skip_to_token_start(line, pos);
            if (pos >= line.size())
                return false; // walked off the line without reaching `token`
            const std::size_t raw_begin{pos};
            const std::string_view current{take_trimmed_token(line, pos)};
            if (current.data() == token.data() && current.size() == token.size())
                return true; // reached it with nothing but prefix material behind it
            // An empty trimmed token is pure punctuation ("##", "--") and is invisible to
            // for_each_token, so it is invisible here too — the two must agree on what a token is.
            if (!current.empty() && !is_prefix_material(line, raw_begin, pos))
                return false;
        }
    }

    // ── SRC-D-OUT-4a — a leading FAIL glyph CONFIRMS a failure word (the ✗-anchor) ────────
    // Mirror of leading_outcome_is_pass: does the line's FIRST outcome-bearing token mark a
    // FAIL? Walk the head; the first token that is a ballot-X glyph ⇒ true; a PASS glyph ⇒
    // false (a pass leads — not a fail line); a failure WORD ⇒ false (it self-handles via
    // its own role/anchor — this predicate only adds the GLYPH register). End-of-head ⇒
    // false. Used ONLY to ANCHOR an already-matched failure word, never to create a cue, so
    // a glyph-only line ("✗ 1920×1080") with no failure word stays silent — provably safe
    // against the deferred D-OUT-3 ×-risk. Pure byte-compare ⇒ MSVC bit-identical (F5).
    [[nodiscard]] bool leading_outcome_is_fail(std::string_view line) noexcept
    {
        static constexpr std::size_t kOutcomeHead{128U}; // matches leading_outcome_is_pass
        const std::size_t limit{line.size() < kOutcomeHead ? line.size() : kOutcomeHead};
        std::size_t pos{0};
        while ((pos = skip_to_token_start(line, pos)) < limit)
        {
            if (starts_with_fail_glyph(line, pos)) // first outcome token is a fail glyph
                return true;
            if (starts_with_pass_glyph(line, pos)) // a pass leads — not a fail line
                return false;
            const std::string_view token{take_trimmed_token(line, pos)};
            for (const FailureWord& entry : kFailureLexicon)
                if (iequals(token, entry.word)) // a failure word leads — it self-handles
                    return false;
            // otherwise a non-outcome token (scope segment / name / number) — continue
        }
        return false; // no leading fail glyph within the head
    }

    // SRC-D-OUT-4b (D-MSK-4 cut, 2026-07-21 ruling): a CamelCase error-TYPE token anchors a failure
    // cue ONLY when it is verdict-anchored — the register/position discriminator, stated literally.
    // A thrown verdict (`TypeError:`, `[Error]`, CAPS) fires; a type NAMED but not thrown does not.
    // The prior `|| !leads_with_descriptive_glyph(line)` fallback was a one-entry denylist built
    // for a single node:test `▶`-suite FP — it fired on every producer shape it never saw (ctest's
    // `Start NN: FooErrorTest.Bar` here, gtest/pytest next). This IS the check's own partition
    // criterion (benign-collision-proneness): a CamelCase type ending in `Error` is maximally
    // collision-prone in a test suite, so it belongs in RegisterAnchored ("fires only in verdict
    // register"). Recall is protected by POSITION not luck: the caller runs this only on lines with
    // NO failure word at all (`!matched && …`), and a genuinely thrown error brings register or a
    // fail glyph that fires via a separate, stronger cue. See bugs.md 2026-07-09 row.
    [[nodiscard]] bool error_type_anchors(std::string_view line, std::string_view token) noexcept
    {
        return detail::is_verdict_anchored(line, token);
    }

} // namespace

// The shared outcome predicate (SRC-D-OUT-1b) — declared on the canon-internal detail surface in
// canon.api.cppm so infer_leading_log_level (a SEPARATE TU) consults the SAME predicate as the
// cue lexicon, not a TU-local copy. Defined here, with the failure lexicon, reusing the TU-local
// glyph/scan helpers above (single-responsibility). Does the line's FIRST outcome-bearing token
// declare a pass? Walk the head left-to-right (ANSI- and scope-prefix-tolerant, tokenising exactly
// as for_each_token): the first token that is a pass GLYPH ⇒ true (pass leads); the first token
// that is a failure WORD ⇒ false (failure leads — stop, so a pathological "ERROR … ✓" still fires
// and the "ERROR teardown failed though setup was ok" guard holds); any other token (scope segment
// "@cline/core", a name, a number) is skipped. End-of-head ⇒ false. Glyph-gated AND (SRC-D-OUT-2)
// first-significant-token-word-gated: a leading pass GLYPH demotes anywhere in the head; a pass
// WORD (passed/ok/success/succeeded) demotes ONLY as the FIRST significant token ("ok 1 - should
// return error" → demote), so a summary count "25 passed, 5 failed" (a number leads, not "passed")
// and a prose "passed" mid-line never false-demote a real failure. Pure byte-compare + ASCII
// case-fold ⇒ cross-stdlib and MSVC bit-identical (F5).
namespace detail
{
    [[nodiscard]] bool leading_outcome_is_pass(std::string_view line) noexcept
    {
        static constexpr std::size_t kOutcomeHead{128U}; // generous monorepo scope-prefix bound
        const std::size_t limit{line.size() < kOutcomeHead ? line.size() : kOutcomeHead};
        std::size_t pos{0};
        bool first_significant{true};
        while ((pos = skip_to_token_start(line, pos)) < limit)
        {
            if (starts_with_pass_glyph(line, pos)) // an outcome token is a pass glyph
                return true;
            const std::string_view token{take_trimmed_token(line, pos)};
            if (token.empty())
                continue; // a pure-punctuation token (trimmed to nothing) — not significant
            for (const FailureWord& entry : kFailureLexicon)
                if (iequals(token, entry.word)) // a failure word leads → not a pass
                    return false;
            // SRC-D-OUT-2 — see canon.api.cppm for the contract. WHY THE GUARD IS THE LOOP'S
            // `first_significant` FLAG and not a position compare: the pass WORD must LEAD, and
            // "leading" is defined over SIGNIFICANT tokens, so a pure-punctuation prefix is
            // skipped (above) without consuming the slot. This is also what makes "not ok 1 …"
            // come out right with no special case — `not` takes the slot, so no demote fires and
            // the TAP failure survives.
            if (first_significant)
            {
                for (const std::string_view verdict : kSuccessVerdicts)
                    if (iequals(token, verdict))
                        return true;
                first_significant = false;
            }
            // otherwise a non-outcome token (scope segment / name / number) — continue
        }
        return false; // no leading outcome token within the head
    }

    // SRC-D-OUT-4 — see canon.api.cppm for the contract. anchor #1 (caps) is a pure token
    // test; anchors #2 (delimiter) need the surrounding bytes, recovered from `token`'s
    // position within `line` (precondition: `token` is a sub-view of `line`).
    [[nodiscard]] bool is_verdict_anchored(std::string_view line, std::string_view token) noexcept
    {
        if (is_caps_register(token)) // anchor #1 — ERROR / FAILED / FATAL …
            return true;
        const std::size_t start{static_cast<std::size_t>(token.data() - line.data())};
        const std::size_t end{start + token.size()};
        const char before{start > 0U ? line[start - 1U] : '\0'};
        const char after{end < line.size() ? line[end] : '\0'};
        // anchor #2 — a verdict colon ("error:") IN THE LINE'S KIND SLOT (SRC-D-OUT-4c), or the
        // token enclosed in brackets/parens ("[error]", "##[error]", "(FAILED)"): the separators
        // CI/test runners frame an outcome with. A leading "##" is its own (empty-trimmed) token,
        // so the byte immediately before "error" is the '[' — bracket-bound. Only the colon half
        // carries the kind-slot precondition, and the asymmetry is the design: a trailing colon is
        // a ONE-SIDED adjacency that every `key: value` in every config dump, source frame and
        // quoted string satisfies, while a bracket pair is already a two-sided enclosure that prose
        // does not produce by accident.
        if (after == ':' && token_in_kind_slot(line, token))
            return true;
        if ((before == '[' && after == ']') || (before == '(' && after == ')'))
            return true;
        // anchor #3 (SRC-D-OUT-4a) — a leading FAIL glyph (✗/✕/✖/✘) marks the line a failed
        // verdict, confirming this failure word. Line-level, so it applies to every register-
        // anchored token on a ✗-led line; never creates a cue (no failure word ⇒ never called).
        return leading_outcome_is_fail(line);
    }

    // SRC-D-CNT-1 — see canon.api.cppm for the contract. true iff `token`'s IMMEDIATELY-PRECEDING
    // token (under the shared canon tokenization) is a digit-leading numeric — count register
    // ("1 failure", "5 failed"). PRECONDITION: `token` is a sub-view of `line` (a for_each_token
    // token). Forward-scans `line` tracking the previous non-empty token until it reaches `token`
    // (pointer identity, the occurrence not just an equal string); the predecessor's
    // digit-leading-ness decides. Cold — consulted only once a level word matched. Mirrors
    // for_each_token's boundaries exactly (ANSI- and delimiter-aware via the shared helpers).
    // Pure byte/case test, order-independent ⇒ cross-stdlib + MSVC bit-identical (F5).
    [[nodiscard]] bool is_count_register(std::string_view line, std::string_view token) noexcept
    {
        std::string_view prev{};
        std::string_view prev_prev{};
        std::size_t pos{0};
        for (;;)
        {
            pos = skip_to_token_start(line, pos);
            if (pos >= line.size())
                return false; // token not found (not a valid sub-view) — conservative
            const std::string_view cur{take_trimmed_token(line, pos)};
            if (cur.data() == token.data() && cur.size() == token.size())
                return is_count_preceded(prev, prev_prev);
            if (!cur.empty())
            {
                prev_prev = prev;
                prev = cur;
            }
        }
    }

    // SRC-D-NOTE-1 — the note-register kernel's single implementation; see canon.api.cppm for the
    // contract. Returns the offset of the first byte of a NOTE diagnostic's MESSAGE — one past the
    // structural `<path>:<line>:<col>: note: ` marker — or npos when the line carries no such
    // marker. FIRST occurrence wins: it is the line's diagnostic-kind slot, and anything after it
    // belongs to the note's own claim.
    //
    // The shape is validated BACKWARDS from the marker so the scan is one forward find plus a
    // bounded walk: locate `": note: "`, then require `<digits>` `:` `<digits>` immediately to its
    // left, itself preceded by the path's own `:`. That is what makes it structural rather than
    // lexical — `"Note: the deploy failed"` and `"see note: below"` both fail it, and must, or a
    // labelling fix becomes a detection defect.
    //
    // Pure byte walk, alloc-free, noexcept, no case folding (the compilers emit lowercase `note:`;
    // matching case-insensitively would start admitting prose). F5-bit-identical by construction.
    [[nodiscard]] std::size_t note_register_begin(std::string_view line) noexcept
    {
        constexpr std::string_view kNoteMarker{": note: "};
        const std::size_t marker{line.find(kNoteMarker)};
        if (marker == std::string_view::npos)
            return std::string_view::npos;
        // Walk left over the column digits, then its ':', then the line digits, then the path's
        // ':'. Every step is required — a missing one means this is not the diagnostic-kind slot.
        std::size_t pos{marker};
        const auto take_digits{[line, &pos]() noexcept
                               {
                                   const std::size_t end{pos};
                                   while (pos > 0U && line[pos - 1U] >= '0' && line[pos - 1U] <= '9')
                                       --pos;
                                   return end != pos; // at least one digit consumed
                               }};
        const auto take_colon{[line, &pos]() noexcept
                              {
                                  if (pos == 0U || line[pos - 1U] != ':')
                                      return false;
                                  --pos;
                                  return true;
                              }};
        if (!take_digits() || !take_colon() || !take_digits() || !take_colon())
            return std::string_view::npos;
        return marker + kNoteMarker.size();
    }

    // SRC-D-NOTE-1 — the register, expressed over ONE token: is this token inside the note's
    // message? A thin view on note_register_begin so the per-token predicate and the once-per-line
    // offset can never drift into two implementations of one property. PRECONDITION: `token` is a
    // sub-view of `line` (a for_each_token token), as for every register kernel here.
    [[nodiscard]] bool token_in_note_message(std::string_view line, std::string_view token,
                                             std::size_t message_at) noexcept
    {
        return message_at != std::string_view::npos &&
               static_cast<std::size_t>(token.data() - line.data()) >= message_at;
    }

    // SRC-D-CNT-1 — the DUAL of contains_failure_cue: true iff the head carries a failure-lexicon
    // word in COUNT register (immediately preceded by a digit-leading numeric — "1 failure",
    // "5 failed"): an aggregate SUMMARY, not a per-item verdict. contains_failure_cue treats
    // such words as non-firing; this reports their presence so infer_leading_log_level caps a
    // count-summary line at Warn (surfaced, below per-item verdicts — demote, never suppress).
    // Cold path (only reached when contains_failure_cue is false). Single bounded pass;
    // alloc-free, noexcept. Pure byte/case test ⇒ cross-stdlib + MSVC bit-identical (F5).
    // Only throw path is for_each_token's substr (begin <= size); the noexcept body cannot throw.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] bool contains_failure_summary_cue(std::string_view text,
                                                    std::size_t scan_limit) noexcept
    {
        std::string_view prev{};
        std::string_view prev_prev{};
        // SRC-D-NOTE-1 applies to the dual as well: a counted failure word inside a note's message
        // ("note: 5 candidates failed") is still the NOTE's word, and the note asserts nothing.
        // Without this the demotion would only move the line from Error to Warn, and the ruling's
        // target level is Unknown.
        const std::size_t note_message_at{note_register_begin(text)};
        return for_each_token(text, scan_limit,
                              [&](std::string_view token) noexcept
                              {
                                  bool summary{false};
                                  if (!token_in_note_message(text, token, note_message_at))
                                      for (const FailureWord& entry : kFailureLexicon)
                                          if (iequals(token, entry.word))
                                          {
                                              summary = is_count_preceded(prev, prev_prev);
                                              break;
                                          }
                                  prev_prev = prev;
                                  prev = token;
                                  return summary;
                              });
    }
} // namespace detail

namespace
{
    LexiconHit lexicon_hit(std::string_view text, std::string_view token, std::string_view prev,
                           std::string_view prev_prev) noexcept
    {
        for (const FailureWord& entry : kFailureLexicon)
        {
            if (!iequals(token, entry.word))
                continue;
            // SRC-D-CNT-1: a count-register failure word (immediately preceded by a bare-integer
            // count — "1 failure", "5 failed" — that is not a timestamp chain) is a SUMMARY, not
            // a per-item verdict, so it does NOT fire as a cue (checked BEFORE the verdict
            // anchors: a counted noun is a summary even with a trailing colon — "1 failure:").
            // The line still surfaces, demoted to Warn by the level path; it just stops
            // outranking the specific verdicts it summarizes (the "25 passed, 5 failed" dual). On
            // a masked TEMPLATE the digit is `<*>` (not a bare integer) so the metalog salience
            // path is unaffected.
            if (is_count_preceded(prev, prev_prev))
                return {.fired = false, .matched = true};
            // zero-collision token self-anchors; collision-prone token needs verdict register
            return {.fired = entry.role == FailureRole::SelfAnchoring ||
                             detail::is_verdict_anchored(text, token),
                    .matched = true};
        }
        return {};
    }
} // namespace

// Only throw path is for_each_token / any_standalone_word, whose substr has begin <= text.size()
// (see token_scan.hpp); the noexcept body cannot throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
bool contains_failure_cue(std::string_view text, std::size_t scan_limit) noexcept
{
    // One head-bounded pass. The "segmentation fault" phrase, or an OUTCOME verb (a
    // self-anchoring inflected verdict), is a strong cue (short-circuit true). A TERM
    // noun fires only in verdict register (SRC-D-OUT-4 is_verdict_anchored) — a bare noun in
    // prose is not a verdict. A CamelCase error-TYPE name is a WEAKER signal — recorded,
    // but the scan continues.
    bool saw_error_type{false};
    std::string_view prev{};
    std::string_view prev_prev{};
    // SRC-D-NOTE-1 (the fourth register): resolved ONCE per line, not per token — the offset where
    // a compiler NOTE diagnostic's message begins, or npos. Every cue form below (phrase, lexicon
    // word, CamelCase error TYPE) is demoted uniformly inside that message, because they are all
    // the note's own words and a note asserts no verdict. Tokens BEFORE the marker keep their
    // authority: an `##[error]` wrapper or a CI prefix on the same line is not the note's claim.
    const std::size_t note_message_at{detail::note_register_begin(text)};
    const bool saw_failure_word{for_each_token(
        text, scan_limit,
        [&](std::string_view token) noexcept
        {
            if (detail::token_in_note_message(text, token, note_message_at))
            {
                prev_prev = prev; // the window still shifts: the token exists, it just does not fire
                prev = token;
                return false;
            }
            for (const Phrase& phrase : kFailurePhrases)
                if (iequals(prev, phrase[0]) && iequals(token, phrase[1]))
                    return true; // phrase completes — a strong cue
            const LexiconHit hit{lexicon_hit(text, token, prev, prev_prev)};
            // SRC-D-OUT-4b — see canon.api.cppm for the contract. Cold: reached only
            // when the lexicon missed, so the extra full-line scan
            // error_type_anchors costs is paid on non-matching lines only.
            if (!hit.matched && is_camel_error_type(token) && error_type_anchors(text, token))
                saw_error_type = true;
            prev_prev = prev; // shift the two-token window for the next adjacency
            prev = token;     // check (count register needs prev AND prev-prev)
            return hit.fired;
        })};
    // A strong failure WORD fires unconditionally; a weak error-TYPE name (no failure
    // word) is demoted by any success WORD anywhere — scanned across the WHOLE text, as a
    // ctest verdict trails a long test name well past the keyword head. The success-word
    // pass runs ONLY on the rare error-type-without-failure-word line (`||` short-circuit),
    // so the hot path is unaffected.
    const bool result{saw_failure_word ||
                      (saw_error_type && !any_standalone_word(text, kSuccessVerdicts,
                                                              /*scan_limit=*/0U))};
    // Outcome guard (D-OUT-1): a line that carries a failure cue but is LED by an
    // unambiguous pass GLYPH (✓/✔/✅/√) is a passing test whose NAME embeds failure
    // vocabulary ("✓ marks runs failed …"), not a regression. Only a leading pass GLYPH
    // demotes a failure WORD; a leading pass WORD ("25 passed, 5 failed") must not, so
    // words do not. Paid only on the would-be-positive minority (short-circuit), and the
    // walk stops at the first outcome token.
    if (result && detail::leading_outcome_is_pass(text))
        return false;
    return result;
}

bool contains_warning_cue(std::string_view text, std::size_t scan_limit) noexcept
{
    return any_standalone_word(text, kWarningWords, scan_limit);
}

} // namespace insight::utils
