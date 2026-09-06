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

    [[nodiscard]] bool is_digit_leading_numeric(std::string_view tok) noexcept
    {
        return !tok.empty() && tok.front() >= '0' && tok.front() <= '9';
    }

    // pre: `tok` is a trimmed token - for_each_token strips surrounding non-alnum.
    [[nodiscard]] bool is_bare_integer(std::string_view tok) noexcept
    {
        return !tok.empty() &&
               std::ranges::all_of(tok, [](char chr) noexcept { return chr >= '0' && chr <= '9'; });
    }

    // post: true iff `prev` is a bare integer whose own predecessor is not digit-leading.
    // refs: SRC-D-CNT-1
    [[nodiscard]] bool is_count_preceded(std::string_view prev, std::string_view prev2) noexcept
    {
        return is_bare_integer(prev) && !is_digit_leading_numeric(prev2);
    }

    // invariant: negation is a property of the token alone, independent of its line.
    constexpr std::array<std::string_view, 3U> kNegationSegments{"no", "not", "non"};

    // pre: `suffix_start` <= token.size(), guaranteed by the sole caller is_camel_error_type.
    // note: that bound makes the substr in-bounds, so the noexcept body has no throw path.
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

    // invariant: the partition axis is benign-collision-proneness, never grammatical role.
    // refs: SRC-D-OUT-4
    enum class FailureRole : unsigned char
    {
        // note: a benign-collision-prone word - it fires only when verdict-anchored.
        RegisterAnchored,
        // note: zero benign collision - it fires bare, with no surrounding register.
        SelfAnchoring,
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

    // invariant: `matched` means the token is lexicon vocabulary, whether or not it fired.
    // note: a matched word that declined its register is never re-admitted as a CamelCase type.
    struct LexiconHit
    {
        bool fired{false};
        bool matched{false};
    };

    [[nodiscard]] LexiconHit lexicon_hit(std::string_view text, std::string_view token,
                                         std::string_view prev,
                                         std::string_view prev_prev) noexcept;

    // pre: `token` carries its source case - for_each_token trims but never folds.
    // refs: SRC-D-OUT-4
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

    using Phrase = std::array<std::string_view, 2U>;
    constexpr std::array<Phrase, 1U> kFailurePhrases{{{"segmentation", "fault"}}};

    // invariant: a success word demotes only the CamelCase-type signal, never a failure word.
    // refs: SRC-D-OUT-1
    constexpr std::array<std::string_view, 4U> kSuccessVerdicts{"passed", "ok", "success",
                                                                "succeeded"};

    // note: for_each_token's substr is the only throw path and its bound is checked.
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

    // note: a bare glyph trims to an empty token, so it is byte-matched at a token start.
    // refs: SRC-D-OUT-1
    using Glyph = std::array<unsigned char, 3U>;
    constexpr std::array<Glyph, 4U> kPassGlyphs{{
        // note: in order U+2713, U+2714, U+2705, U+221A - the last is mocha's Windows pass mark.
        {0xE2U, 0x9CU, 0x93U},
        {0xE2U, 0x9CU, 0x94U},
        {0xE2U, 0x9CU, 0x85U},
        {0xE2U, 0x88U, 0x9AU},
    }};
    // invariant: every entry is 3-byte UTF-8; U+00D7 is two bytes and the rule excludes it.
    // note: in order U+2715, U+2716, U+2717, U+2718, U+274C - the last is the emoji fail mark.
    // refs: SRC-D-OUT-4a
    constexpr std::array<Glyph, 5U> kFailGlyphs{{
        {0xE2U, 0x9CU, 0x95U},
        {0xE2U, 0x9CU, 0x96U},
        {0xE2U, 0x9CU, 0x97U},
        {0xE2U, 0x9CU, 0x98U},
        {0xE2U, 0x9DU, 0x8CU},
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
    // post: the next token-start index, or text.size() when only delimiters remain.
    // invariant: mirrors for_each_token's inter-token skip, so both agree on token boundaries.
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

    // pre: `pos` is at a non-delimiter, non-ANSI byte of `text`.
    // post: the trimmed token, with `pos` advanced past its raw span.
    // invariant: mirrors for_each_token's extraction and its surrounding-non-alnum trim.
    // note: the substr bound is begin <= pos <= size, so the noexcept body cannot throw.
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

    // pre: `raw_begin`/`raw_end` bound the UNTRIMMED span - the trim can hide the byte that carries
    // the structure.
    // invariant: the prefix classes are a closed declared set, never per-shape tuning.
    // refs: SRC-D-OUT-4c
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

    // pre: `token` is a sub-view of `line`.
    // post: true iff every token before `token` is prefix material; a token never reached is
    // treated as absent, which demotes.
    // invariant: derived over the WHOLE line - a register is a claim, never a byte budget.
    // refs: ADR-20.D3, SRC-D-OUT-4c
    [[nodiscard]] bool token_in_kind_slot(std::string_view line, std::string_view token) noexcept
    {
        std::size_t pos{0};
        for (;;)
        {
            pos = skip_to_token_start(line, pos);
            if (pos >= line.size())
                return false;
            const std::size_t raw_begin{pos};
            const std::string_view current{take_trimmed_token(line, pos)};
            if (current.data() == token.data() && current.size() == token.size())
                return true;
            // assert: an empty trimmed token is pure punctuation and is invisible to for_each_token
            // too.
            if (!current.empty() && !is_prefix_material(line, raw_begin, pos))
                return false;
        }
    }

    // post: true iff the first outcome-bearing token in the head is a fail glyph.
    // invariant: it only ANCHORS an already-matched failure word and never creates a cue, so a
    // glyph-only line stays silent.
    // refs: SRC-D-OUT-4a
    [[nodiscard]] bool leading_outcome_is_fail(std::string_view line) noexcept
    {
        // invariant: equal to leading_outcome_is_pass's kOutcomeHead - the two heads must agree.
        static constexpr std::size_t kOutcomeHead{128U};
        const std::size_t limit{line.size() < kOutcomeHead ? line.size() : kOutcomeHead};
        std::size_t pos{0};
        while ((pos = skip_to_token_start(line, pos)) < limit)
        {
            if (starts_with_fail_glyph(line, pos))
                return true;
            if (starts_with_pass_glyph(line, pos))
                return false;
            const std::string_view token{take_trimmed_token(line, pos)};
            for (const FailureWord& entry : kFailureLexicon)
                if (iequals(token, entry.word))
                    return false;
        }
        return false;
    }

    // pre: the caller reached this only on a line carrying no failure-lexicon word at all.
    // refs: SRC-D-OUT-4b
    [[nodiscard]] bool error_type_anchors(std::string_view line, std::string_view token) noexcept
    {
        return detail::is_verdict_anchored(line, token);
    }

} // namespace

namespace detail
{
    // post: true iff the token is in kFailureLexicon - membership, never firing, and role-blind.
    // note: exposed so an instrument never re-lists the vocabulary and goes stale when it grows.
    // refs: DN-37.D20
    [[nodiscard]] bool is_failure_lexicon_word(std::string_view token) noexcept
    {
        for (const FailureWord& entry : kFailureLexicon)
            if (iequals(token, entry.word))
                return true;
        return false;
    }

    // post: true iff a pass glyph leads the head, or a pass word is its first significant token.
    // invariant: a failure word met first stops the walk and returns false.
    // refs: SRC-D-OUT-1b, SRC-D-OUT-2
    [[nodiscard]] bool leading_outcome_is_pass(std::string_view line) noexcept
    {
        // invariant: equal to leading_outcome_is_fail's kOutcomeHead - the two heads must agree.
        static constexpr std::size_t kOutcomeHead{128U};
        const std::size_t limit{line.size() < kOutcomeHead ? line.size() : kOutcomeHead};
        std::size_t pos{0};
        bool first_significant{true};
        while ((pos = skip_to_token_start(line, pos)) < limit)
        {
            if (starts_with_pass_glyph(line, pos))
                return true;
            const std::string_view token{take_trimmed_token(line, pos)};
            if (token.empty())
                continue;
            for (const FailureWord& entry : kFailureLexicon)
                if (iequals(token, entry.word))
                    return false;
            // assert: leading is defined over SIGNIFICANT tokens, so punctuation never spends the
            // slot.
            // refs: SRC-D-OUT-2
            if (first_significant)
            {
                for (const std::string_view verdict : kSuccessVerdicts)
                    if (iequals(token, verdict))
                        return true;
                first_significant = false;
            }
        }
        return false;
    }

    // pre: `token` is a sub-view of `line` - anchor #2 reads the bytes around it.
    // refs: SRC-D-OUT-4
    [[nodiscard]] bool is_verdict_anchored(std::string_view line, std::string_view token) noexcept
    {
        if (is_caps_register(token))
            return true;
        const std::size_t start{static_cast<std::size_t>(token.data() - line.data())};
        const std::size_t end{start + token.size()};
        const char before{start > 0U ? line[start - 1U] : '\0'};
        const char after{end < line.size() ? line[end] : '\0'};
        // assert: only the colon anchor carries the kind-slot precondition, and that asymmetry is
        // the design - a bracket pair is already two-sided, a trailing colon is not.
        // refs: SRC-D-OUT-4c
        if (after == ':' && token_in_kind_slot(line, token))
            return true;
        if ((before == '[' && after == ']') || (before == '(' && after == ')'))
            return true;
        // assert: anchor #3 is line-level and never creates a cue - no failure word, never called.
        // refs: SRC-D-OUT-4a
        return leading_outcome_is_fail(line);
    }

    // pre: `token` is a sub-view of `line` - the occurrence, matched by pointer identity.
    // post: true iff `token`'s immediately preceding token is a digit-leading numeric.
    // refs: SRC-D-CNT-1
    [[nodiscard]] bool is_count_register(std::string_view line, std::string_view token) noexcept
    {
        std::string_view prev{};
        std::string_view prev_prev{};
        std::size_t pos{0};
        for (;;)
        {
            pos = skip_to_token_start(line, pos);
            if (pos >= line.size())
                return false;
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

    // post: the offset of the first message byte past a diagnostic's note marker, else npos.
    // invariant: the first occurrence wins - it is the line's diagnostic-kind slot.
    // refs: SRC-D-NOTE-1
    [[nodiscard]] std::size_t note_register_begin(std::string_view line) noexcept
    {
        constexpr std::string_view kNoteMarker{": note: "};
        const std::size_t marker{line.find(kNoteMarker)};
        if (marker == std::string_view::npos)
            return std::string_view::npos;
        // assert: every step is required - a missing one means this is not the diagnostic-kind
        // slot.
        std::size_t pos{marker};
        const auto take_digits{
            [line, &pos]() noexcept
            {
                const std::size_t end{pos};
                while (pos > 0U && line[pos - 1U] >= '0' && line[pos - 1U] <= '9')
                    --pos;
                return end != pos;
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

    // pre: `token` is a sub-view of `line`, and `message_at` came from note_register_begin.
    // invariant: a thin view on note_register_begin - one property, never two implementations.
    // refs: SRC-D-NOTE-1
    [[nodiscard]] bool token_in_note_message(std::string_view line, std::string_view token,
                                             std::size_t message_at) noexcept
    {
        return message_at != std::string_view::npos &&
               static_cast<std::size_t>(token.data() - line.data()) >= message_at;
    }

    // pre: cold path - reached only once contains_failure_cue has returned false.
    // post: true iff the head carries a failure word in count register - a summary, not a per-item
    // verdict.
    // invariant: the caller demotes such a line to Warn; it is never suppressed.
    // refs: SRC-D-CNT-1
    // note: for_each_token's substr is the only throw path and its bound is checked.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] bool contains_failure_summary_cue(std::string_view text,
                                                    std::size_t scan_limit) noexcept
    {
        std::string_view prev{};
        std::string_view prev_prev{};
        // assert: a counted failure word inside a diagnostic's message is that diagnostic's word,
        // so it asserts no verdict.
        // refs: SRC-D-NOTE-1
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
            // assert: the count check runs BEFORE the verdict anchors - a counted noun is a summary
            // even with a trailing colon.
            // refs: SRC-D-CNT-1
            if (is_count_preceded(prev, prev_prev))
                return {.fired = false, .matched = true};
            return {.fired = entry.role == FailureRole::SelfAnchoring ||
                             detail::is_verdict_anchored(text, token),
                    .matched = true};
        }
        return {};
    }
} // namespace

// note: for_each_token and any_standalone_word's substr are the only throw paths, both bounded.
// NOLINTNEXTLINE(bugprone-exception-escape)
bool contains_failure_cue(std::string_view text, std::size_t scan_limit) noexcept
{
    // assert: a phrase or a self-anchoring verb short-circuits true; a CamelCase type is only
    // recorded, so the scan continues.
    bool saw_error_type{false};
    std::string_view prev{};
    std::string_view prev_prev{};
    // assert: resolved once per line; every cue inside the message is demoted, while tokens before
    // the marker keep their authority.
    // refs: SRC-D-NOTE-1
    const std::size_t note_message_at{detail::note_register_begin(text)};
    const bool saw_failure_word{for_each_token(
        text, scan_limit,
        [&](std::string_view token) noexcept
        {
            if (detail::token_in_note_message(text, token, note_message_at))
            {
                // assert: the two-token window still shifts - the token exists, it just does not
                // fire.
                prev_prev = prev;
                prev = token;
                return false;
            }
            for (const Phrase& phrase : kFailurePhrases)
                if (iequals(prev, phrase[0]) && iequals(token, phrase[1]))
                    return true;
            const LexiconHit hit{lexicon_hit(text, token, prev, prev_prev)};
            // assert: cold - reached only when the lexicon missed, so the extra full-line scan is
            // paid on non-matching lines only.
            // refs: SRC-D-OUT-4b
            if (!hit.matched && is_camel_error_type(token) && error_type_anchors(text, token))
                saw_error_type = true;
            prev_prev = prev;
            prev = token;
            return hit.fired;
        })};
    // assert: the success-word scan covers the WHOLE text, never the head - a ctest verdict trails
    // a long test name - and it runs only on the error-type-only line.
    const bool result{saw_failure_word ||
                      (saw_error_type && !any_standalone_word(text, kSuccessVerdicts,
                                                              /*scan_limit=*/0U))};
    // assert: only a leading pass GLYPH demotes a failure word; a leading pass WORD never does.
    // refs: SRC-D-OUT-1
    if (result && detail::leading_outcome_is_pass(text))
        return false;
    return result;
}

bool contains_warning_cue(std::string_view text, std::size_t scan_limit) noexcept
{
    return any_standalone_word(text, kWarningWords, scan_limit);
}

} // namespace insight::utils
