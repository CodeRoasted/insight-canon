// test_mask_rule_golden.cpp — the arm ROADMAP N74 asks for: a golden that pins MASKED OUTPUT for a
// named line population, so a masking-rule change reds WITHOUT a `kCanonicalizationVersion` change.
//
// ═══ THE DEFECT THIS EXISTS FOR ════════════════════════════════════════════════════════════════
// `kCanonicalizationVersion` is the declared identity of the masking rule set. Consumers pin it,
// `insight-metalog`'s wire vectors carry it, and `insight-eidos`'s sift report gate
// `static_assert`s on its literal value. Every one of those fires when the VERSION moves and none
// of them fires when the RULES move — so the bump is enforced only by whoever remembers to make it,
// and a rule change that silently forgets it is invisible. Measured: applying a new masking arm
// left `insight-canon` and `insight-eidos` fully green, and THAT GREEN WAS THE FINDING.
//
// ═══ WHY THE EXISTING ARMS DID NOT CATCH IT — the shape, not the count ════════════════════════
// `test_stateless_template.cpp` is not thin: it carries dozens of assertions and many DO pin
// literal template bytes. But its coverage is RELATIONAL where it is not literal —
// `EXPECT_EQ(masked(a), masked(b))`, "these two lines share a template" — and a relational
// assertion is INVARIANT under any rule change that moves both sides the same way. `zlib/1.3` and
// `zlib/1.2.11` still share a template if `versioned_ref` starts masking the whole token; `built in
// 6.2s` and `built in 11.9s` still share one if the wildcard spelling changes. The literal pins
// that do exist are concentrated on the arms most recently REPAIRED (bracket_timestamp, the
// wrapper shell, the compact-UTC instant, the hash floor) because each was written to close its own
// incident — nothing derived them from the rule set, so an arm nobody had just repaired had no
// literal pin at all. That is the hole, and it is a hole in DERIVATION, not in effort.
//
// ═══ WHAT THIS FILE PINS, AND THE TWO LIMBS ═══════════════════════════════════════════════════
// `mask_rules.golden` holds one row per witness: `rule_id | subject_token | input_line | template`.
// Two limbs, and NEITHER substitutes for the other — the split is stated in `kCompositeRules`'s own
// comment in mask.cpp, which is where the gap was named before it was closed:
//
//   LIMB 1 — a rule's ACCEPTANCE SET widens or narrows in place (a new arm inside a normalizer, a
//     moved threshold). Every table in mask.cpp stays byte-identical; the masked OUTPUT of some
//     witness moves. `GoldenTemplatesAreByteIdentical` is that limb.
//   LIMB 2 — a rule or a catalog entry is ADDED, REMOVED or REORDERED. Output may not move at all
//     for any existing witness, because the new rule's class has no witness yet. The coverage arms
//     are that limb: they read the DECLARED catalogs through `rule_catalog::*` and red when the
//     rule set contains something the population does not.
//
// Limb 2 is what stops this file from becoming the defect one level up. A golden over a hand-picked
// handful pins whatever those lines happen to exercise, and stays green while a new arm ships
// unwitnessed — which is exactly how the incident above happened at the level of the whole suite.
//
// ═══ WHAT IT DOES NOT COVER — stated, not left implicit ═══════════════════════════════════════
//   * `params`. The golden pins `template_str` only. A masked position contributes both a `<*>` to
//     the template and its raw token to `params`, so which positions masked is already visible in
//     the template; params add no sensitivity and would double the file's width.
//   * `template_id`. It is SHA-256 of `template_str` (canon.api.cppm), so pinning the template
//     pins the id up to a hash change, and a hash change is not a masking change.
//   * THE COMPOSED PATH — and this is the boundary the homing call BUYS. The population goes
//     straight to `stateless_template()`, never through `Tokenizer::process_line`. A golden taken
//     through the composed pipeline reds on either stage and cannot say WHICH one moved, so this
//     fixture controls exactly one thing — the token bytes the masker sees — and a red here names
//     the MASKER. The price is that anything manifesting only THROUGH the composition is invisible
//     here: format detection selecting a different strategy, a strategy handing the masker a
//     different `content` slice, a semantic package's rows changing what a line projects to. None
//     of those is a masking-rule change, and each has its own home — the strategy suites, the
//     conformance kit, and `insight-metalog`'s committed wire vectors, which DO re-derive through
//     the full shipped tokenizer. This gate is deliberately the narrow half of that pair.
//   * `MaskConfig` beyond `mask_ip_addresses`. That is the only knob gating a rule.
//   * Any rule class whose witness is absent. Limb 2 makes that condition VISIBLE for the declared
//     catalogs (composite rules, ephemeral roots, wrapper pairs, status keywords, currency
//     markers); it cannot see a class that no catalog declares.
//
// ═══ REGENERATION, AND THE GUARD THAT STOPS IT BEING A RUBBER STAMP ═══════════════════════════
// `MaskRuleGolden.DISABLED_RegenerateGolden` rewrites the file. It is `DISABLED_` because it is a
// maintenance ACTION, not a property — gtest prints a "YOU HAVE 1 DISABLED TEST" banner, so it is
// visible rather than silently skipped, and running it from the same binary the gate runs in is
// what guarantees the regenerated bytes come from the same compiled masker the gate reads.
//
//   <build dir>/core/insight_canon_tests \
//       --gtest_also_run_disabled_tests --gtest_filter='MaskRuleGolden.DISABLED_RegenerateGolden'
//
// THE GUARD: it REFUSES to rewrite a row whose expected value is present and differs, unless
// `kCanonicalizationVersion` already differs from the version recorded in the golden's header. So
// the only way to make a moved row green again is to BUMP THE TOKEN FIRST — which is the obligation
// this whole file exists to enforce, and it means "just regenerate it" cannot be the silent repair.
// Rows whose expected column is EMPTY are always filled: that is how a new witness is added to the
// population, and it can never mask a moved row, because a moved row is by definition not empty.
#include <gtest/gtest.h>

import insight.canon.test;

using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::stateless_template;
namespace catalog = insight::tokenization::rule_catalog;

namespace
{

// The five TOP-LEVEL dispositions of the SRC-D-TID-12 precedence that are not the composite layer.
// Rule 2 IS the composite layer and is named by `catalog::composite_rule_ids()` instead, so this
// list plus that catalog is the complete rule-id namespace. It is hand-held because the dispatcher
// states these as an `if`/disjunction chain rather than a table —
// `MaskRuleGolden.EveryGoldenRowNamesADeclaredRule` is what keeps a typo here from silently minting
// a sixth id.
constexpr std::array<std::string_view, 5> kTopLevelRuleIds{
    {std::string_view{"status_keep"},    // rule 1 — a short numeric after a status keyword
     std::string_view{"uuid_or_hash"},   // rule 3 — a standalone UUID or a hex run at the floor
     std::string_view{"ipv4"},           // rule 4 — the only knob-gated rule
     std::string_view{"digit_leading"},  // rule 5 — the discriminator the whole model rests on
     std::string_view{"literal_keep"}}}; // rule 6 — the boundary: what canon must NOT mask

struct Row
{
    std::string rule_id;
    std::string subject;  // the token under test, a whitespace token of `input`
    std::string input;    // the line fed to stateless_template
    std::string expected; // the pinned template_str; empty = "fill me" (regeneration only)
    std::size_t line_no{0};
};

struct Golden
{
    std::string version; // the canonicalization_version recorded in the header
    std::vector<Row> rows;
    // EVERY raw line, in file order. Regeneration walks THIS and substitutes the rebuilt text for
    // the lines that are witness rows, so the section comments stay where their section is. An
    // earlier shape kept comments and rows in two separate lists and would have rewritten the file
    // with every comment hoisted to the top — a regenerator that destroys the document is a
    // regenerator nobody runs twice.
    std::vector<std::string> lines;
    // lines[i] is a witness row iff row_at[i] is not npos, in which case it indexes `rows`.
    std::vector<std::size_t> row_at;
};

constexpr std::string_view kVersionKey{"canonicalization_version = "};
constexpr std::string_view kFieldSep{" | "};

[[nodiscard]] std::string trim(std::string_view str)
{
    const std::size_t first{str.find_first_not_of(" \t")};
    if (first == std::string_view::npos)
        return {};
    const std::size_t last{str.find_last_not_of(" \t")};
    return std::string{str.substr(first, last - first + 1)};
}

// Split on " | ". Exactly 3 or 4 fields; a 3-field row is a witness awaiting its expected value.
// A field count outside that range is a malformed row and is reported as such rather than guessed
// at — a `|` inside a witness line would silently shift every column, so the population may not
// contain one.
[[nodiscard]] bool split_row(std::string_view text, std::size_t line_no, Row& out, std::string& why)
{
    std::vector<std::string> fields;
    std::size_t cursor{0};
    while (true)
    {
        const std::size_t hit{text.find(kFieldSep, cursor)};
        if (hit == std::string_view::npos)
        {
            fields.emplace_back(text.substr(cursor));
            break;
        }
        fields.emplace_back(text.substr(cursor, hit - cursor));
        cursor = hit + kFieldSep.size();
    }
    if (fields.size() < 3 || fields.size() > 4)
    {
        why = "expected 3 or 4 ` | `-separated fields, found " + std::to_string(fields.size()) +
              " — a witness line may not contain the byte sequence ` | `";
        return false;
    }
    out.rule_id = trim(fields[0]);
    out.subject = fields[1];
    out.input = fields[2];
    out.expected = fields.size() == 4 ? fields[3] : std::string{};
    out.line_no = line_no;
    return true;
}

[[nodiscard]] Golden load_golden(std::string& why)
{
    Golden golden;
    std::ifstream file{INSIGHT_CANON_MASK_GOLDEN_PATH};
    if (!file)
    {
        why = std::string{"cannot open the golden at "} + INSIGHT_CANON_MASK_GOLDEN_PATH;
        return golden;
    }
    std::string text;
    std::size_t line_no{0};
    while (std::getline(file, text))
    {
        ++line_no;
        golden.lines.push_back(text);
        golden.row_at.push_back(std::string::npos);
        if (text.starts_with(kVersionKey))
        {
            golden.version = trim(text.substr(kVersionKey.size()));
            continue;
        }
        if (text.empty() || text.starts_with('#'))
            continue;
        Row row;
        std::string row_why;
        if (!split_row(text, line_no, row, row_why))
        {
            why = "golden line " + std::to_string(line_no) + ": " + row_why + "\n  line: " + text;
            return golden;
        }
        golden.row_at.back() = golden.rows.size();
        golden.rows.push_back(std::move(row));
    }
    if (golden.version.empty())
        why = std::string{"the golden carries no `"} + std::string{kVersionKey} + "` header line";
    return golden;
}

[[nodiscard]] std::string mask_line(std::string_view content, const MaskConfig& config)
{
    ArenaAllocator arena{256U * 1024U};
    return std::string{stateless_template(content, arena, config).template_str};
}

[[nodiscard]] std::string mask_line(std::string_view content)
{
    return mask_line(content, {});
}

// The whitespace tokens of `line` — the same split the masker performs, so a token index in one is
// a token index in the other.
[[nodiscard]] std::vector<std::string> tokens_of(std::string_view line)
{
    std::vector<std::string> out;
    std::size_t cursor{0};
    while (cursor < line.size())
    {
        while (cursor < line.size() && line[cursor] == ' ')
            ++cursor;
        if (cursor >= line.size())
            break;
        const std::size_t start{cursor};
        while (cursor < line.size() && line[cursor] != ' ')
            ++cursor;
        out.emplace_back(line.substr(start, cursor - start));
    }
    return out;
}

// Index of `subject` among `line`'s whitespace tokens, or npos. The first occurrence: a witness
// whose subject appears twice is testing an ambiguous thing, and `SubjectTokenIsPresentExactlyOnce`
// rejects it rather than picking one.
[[nodiscard]] std::size_t subject_index(const Row& row, std::size_t& occurrences)
{
    const std::vector<std::string> toks{tokens_of(row.input)};
    std::size_t found{std::string::npos};
    occurrences = 0;
    for (std::size_t pos{0}; pos < toks.size(); ++pos)
        if (toks[pos] == row.subject)
        {
            if (occurrences == 0)
                found = pos;
            ++occurrences;
        }
    return found;
}

// The row's subject rendered through the masker IN ITS LINE — the masked token at the subject's
// index, which is what a rule's disposition is actually about. Reading the whole template instead
// would fold every other token's behaviour into the discriminator.
[[nodiscard]] std::string masked_subject(const Row& row, const MaskConfig& config, bool& okay)
{
    std::size_t occurrences{0};
    const std::size_t index{subject_index(row, occurrences)};
    okay = index != std::string::npos && occurrences == 1;
    if (!okay)
        return {};
    const std::vector<std::string> masked{tokens_of(mask_line(row.input, config))};
    // The masker emits exactly one output token per input token (it joins on a single space and a
    // normalized token never gains one), so the indices correspond.
    if (index >= masked.size())
    {
        okay = false;
        return {};
    }
    return masked[index];
}

[[nodiscard]] MaskConfig without_ip_masking()
{
    MaskConfig cfg;
    cfg.mask_ip_addresses = false;
    return cfg;
}

[[nodiscard]] bool is_top_level_id(std::string_view rule_id)
{
    return std::ranges::find(kTopLevelRuleIds, rule_id) != kTopLevelRuleIds.end();
}

[[nodiscard]] bool is_composite_id(std::string_view rule_id)
{
    const auto declared{catalog::composite_rule_ids()};
    return std::ranges::find(declared, rule_id) != declared.end();
}

[[nodiscard]] std::string join_root(std::span<const std::string_view> segments)
{
    std::string out;
    for (const std::string_view seg : segments)
    {
        if (!out.empty())
            out.push_back('/');
        out.append(seg);
    }
    return out;
}

// The loaded golden, once. A parse failure is reported by every arm rather than crashing one.
struct Loaded
{
    Golden golden;
    std::string why;
};

const Loaded& loaded()
{
    static const Loaded kLoaded{[]
                                {
                                    Loaded out;
                                    out.golden = load_golden(out.why);
                                    return out;
                                }()};
    return kLoaded;
}

// Every arm calls this first: an unparsable golden must fail ONCE with the parse error, never as
// forty confusing coverage failures about a population that was never read.
[[nodiscard]] bool golden_is_readable()
{
    if (loaded().why.empty())
        return true;
    ADD_FAILURE() << "the masking golden could not be read, so nothing below is a statement about "
                     "the masker:\n  "
                  << loaded().why;
    return false;
}

// Which token first differs, so a failure names the moved token instead of handing back two lines
// to eyeball. Returns npos when the two templates tokenize identically.
[[nodiscard]] std::size_t first_differing_token(std::string_view lhs, std::string_view rhs)
{
    const std::vector<std::string> left{tokens_of(lhs)};
    const std::vector<std::string> right{tokens_of(rhs)};
    for (std::size_t pos{0}; pos < std::max(left.size(), right.size()); ++pos)
    {
        const bool left_short{pos >= left.size()};
        const bool right_short{pos >= right.size()};
        if (left_short || right_short || left[pos] != right[pos])
            return pos;
    }
    return std::string::npos;
}

[[nodiscard]] std::string token_at(std::string_view line, std::size_t index)
{
    const std::vector<std::string> toks{tokens_of(line)};
    return index < toks.size() ? toks[index] : std::string{"<no token at this index>"};
}

constexpr std::string_view kRegenCommand{
    "<build dir>/core/insight_canon_tests --gtest_also_run_disabled_tests "
    "--gtest_filter='MaskRuleGolden.DISABLED_RegenerateGolden'"};

} // namespace

// ═══ LIMB 1 — the masked output itself ════════════════════════════════════════════════════════

TEST(MaskRuleGolden, GoldenTemplatesAreByteIdentical)
{
    if (!golden_is_readable())
        return;
    const Golden& golden{loaded().golden};
    ASSERT_FALSE(golden.rows.empty()) << "the golden holds no witness rows — an empty population "
                                         "pins nothing and would pass vacuously";

    std::size_t moved{0};
    for (const Row& row : golden.rows)
    {
        if (row.expected.empty())
        {
            ADD_FAILURE() << "golden line " << row.line_no << " (" << row.rule_id
                          << ") has NO pinned template. A witness without an expected value pins "
                             "nothing. Fill it by running:\n  "
                          << kRegenCommand;
            continue;
        }
        const std::string actual{mask_line(row.input)};
        if (actual == row.expected)
            continue;
        ++moved;
        const std::size_t diff_at{first_differing_token(row.expected, actual)};
        ADD_FAILURE()
            << "MASKED OUTPUT MOVED — golden line " << row.line_no << "\n"
            << "  rule       : " << row.rule_id << "\n"
            << "  input      : " << row.input << "\n"
            << "  expected   : " << row.expected << "\n"
            << "  actual     : " << actual << "\n"
            << (diff_at == std::string::npos
                    ? "  (the two templates tokenize identically — they differ in WHITESPACE)\n"
                    : "  first token that differs: index ")
            << (diff_at == std::string::npos ? std::string{} : std::to_string(diff_at)) << "\n"
            << "      expected token: " << token_at(row.expected, diff_at) << "\n"
            << "      actual token  : " << token_at(actual, diff_at);
    }

    if (moved != 0)
        ADD_FAILURE()
            << "\n"
            << moved << " of " << golden.rows.size()
            << " witness rows changed their masked output.\n"
            << "  kCanonicalizationVersion (source): " << insight::kCanonicalizationVersion << "\n"
            << "  canonicalization_version (golden): " << golden.version << "\n"
            << (golden.version == insight::kCanonicalizationVersion
                    ? "  THE RULES MOVED AND THE VERSION DID NOT. This is the defect this "
                      "gate exists for.\n"
                      "  Either restore the rule, or bump kCanonicalizationVersion in "
                      "core/api/canon.api.cppm\n"
                      "  (with its ledger entry) and THEN regenerate:\n    "
                    : "  The version was already bumped; the golden is stale. "
                      "Regenerate and review the diff:\n    ")
            << kRegenCommand;
}

TEST(MaskRuleGolden, GoldenHeaderRecordsTheLiveCanonicalizationVersion)
{
    if (!golden_is_readable())
        return;
    EXPECT_EQ(loaded().golden.version, insight::kCanonicalizationVersion)
        << "the golden's header names the rule generation its pinned bytes belong to. A bump "
           "without a regeneration leaves a golden addressed to a contract that no longer exists — "
           "and the regeneration is where a human sees the diff the bump was made for.\n"
        << "  source: " << insight::kCanonicalizationVersion << "\n"
        << "  golden: " << loaded().golden.version << "\n"
        << "  regenerate with:\n    " << kRegenCommand;
}

// ═══ LIMB 2 — the population's coverage of the DECLARED rule set ══════════════════════════════

TEST(MaskRuleGolden, EveryDeclaredCompositeRuleHasAWitness)
{
    if (!golden_is_readable())
        return;
    for (const std::string_view declared_id : catalog::composite_rule_ids())
    {
        const bool covered{std::ranges::any_of(loaded().golden.rows, [declared_id](const Row& row)
                                               { return row.rule_id == declared_id; })};
        EXPECT_TRUE(covered)
            << "composite rule `" << declared_id
            << "` is DECLARED in kCompositeRules and has no witness row in the golden.\n"
               "  A rule with no witness is a rule whose change this gate cannot see, which is the "
               "exact defect\n"
               "  the golden exists to close — one level up. Add a row naming it to "
               "core/tests/mask/mask_rules.golden.";
    }
}

TEST(MaskRuleGolden, EveryGoldenRowNamesADeclaredRule)
{
    if (!golden_is_readable())
        return;
    for (const Row& row : loaded().golden.rows)
        EXPECT_TRUE(is_composite_id(row.rule_id) || is_top_level_id(row.rule_id))
            << "golden line " << row.line_no << " names rule `" << row.rule_id
            << "`, which is neither a declared composite rule nor one of the five top-level "
               "dispositions.\n"
               "  Either it is a typo, or a rule was RENAMED or REMOVED and this witness was left "
               "addressed to it.";
}

TEST(MaskRuleGolden, EveryCompositeRowIsClaimedByTheRuleItNames)
{
    if (!golden_is_readable())
        return;
    for (const Row& row : loaded().golden.rows)
    {
        if (!is_composite_id(row.rule_id))
            continue;
        const std::string_view claimed{catalog::composite_rule_claiming(row.subject)};
        EXPECT_EQ(claimed, row.rule_id)
            << "golden line " << row.line_no << " claims to witness composite rule `" << row.rule_id
            << "`, but the composite layer routes its subject elsewhere.\n"
            << "  subject       : " << row.subject << "\n"
            << "  claimed by    : "
            << (claimed.empty() ? "<no composite rule — the pre-gate "
                                  "skipped the catalog, or every rule "
                                  "declined>"
                                : std::string{claimed})
            << "\n"
            << "  Without this arm the row would still be green while `" << row.rule_id
            << "` sat unwitnessed behind\n"
               "  whichever rule actually claims the token — a coverage claim that is asserted "
               "rather than checked.";
    }
}

TEST(MaskRuleGolden, SubjectTokenIsPresentExactlyOnce)
{
    if (!golden_is_readable())
        return;
    for (const Row& row : loaded().golden.rows)
    {
        std::size_t occurrences{0};
        const std::size_t index{subject_index(row, occurrences)};
        EXPECT_NE(index, std::string::npos)
            << "golden line " << row.line_no << ": subject `" << row.subject
            << "` is not a whitespace token of the input line.\n  input: " << row.input;
        EXPECT_EQ(occurrences, 1U)
            << "golden line " << row.line_no << ": subject `" << row.subject << "` occurs "
            << occurrences
            << " times in the input. A discriminator that cannot say WHICH token it is about "
               "proves nothing about either.\n  input: "
            << row.input;
    }
}

// ═══ LIMB 2, the top-level rules — each row proves the rule it NAMES is the reason ════════════
// A composite row is discriminated by `composite_rule_claiming`. The five top-level dispositions
// have no such table, so each carries its own discriminator, and each is a statement about WHY the
// token got its disposition rather than merely THAT it did.

TEST(MaskRuleGolden, StatusKeepRowsAreKeptBecauseOfTheKeywordBeforeThem)
{
    if (!golden_is_readable())
        return;
    for (const Row& row : loaded().golden.rows)
    {
        if (row.rule_id != "status_keep")
            continue;
        bool okay{false};
        const std::string kept{masked_subject(row, MaskConfig{}, okay)};
        if (!okay)
            continue; // SubjectTokenIsPresentExactlyOnce owns that failure
        EXPECT_EQ(kept, row.subject)
            << "golden line " << row.line_no << ": a status_keep witness must survive VERBATIM.\n"
            << "  subject: " << row.subject << "\n  masked to: " << kept;

        // NON-VACUITY, and it is the whole arm: replace the preceding token with a non-keyword and
        // the same value must MASK. Without this leg a row could be green because the value is a
        // letter-leading word rule 6 would have kept anyway, and rule 1 would sit unwitnessed.
        std::size_t occurrences{0};
        const std::size_t index{subject_index(row, occurrences)};
        if (index == 0)
        {
            ADD_FAILURE() << "golden line " << row.line_no
                          << ": a status_keep witness needs a status keyword token BEFORE its "
                             "subject — rule 1 reads the PREVIOUS token, so a line-leading subject "
                             "cannot be witnessing it.\n  input: "
                          << row.input;
            continue;
        }
        std::vector<std::string> toks{tokens_of(row.input)};
        toks[index - 1] = "zz";
        std::string rebuilt;
        for (const std::string& tok : toks)
        {
            if (!rebuilt.empty())
                rebuilt.push_back(' ');
            rebuilt.append(tok);
        }
        const std::vector<std::string> rebuilt_masked{tokens_of(mask_line(rebuilt))};
        ASSERT_LT(index, rebuilt_masked.size())
            << "golden line " << row.line_no
            << ": the masker returned fewer tokens than the line has. It emits exactly one output "
               "token per input token, so this is a masker invariant break, not a golden problem.";
        const std::string without_keyword{rebuilt_masked[index]};
        EXPECT_EQ(without_keyword, "<*>")
            << "golden line " << row.line_no
            << ": with the status keyword removed the value must MASK — otherwise this row is not "
               "witnessing rule 1 at all, it is witnessing whatever keeps the token anyway.\n"
            << "  original : " << row.input << "\n  rebuilt  : " << rebuilt
            << "\n  subject masked to: " << without_keyword;
    }
}

TEST(MaskRuleGolden, Ipv4RowsMaskOnlyBecauseOfTheKnobbedRule)
{
    if (!golden_is_readable())
        return;
    for (const Row& row : loaded().golden.rows)
    {
        if (row.rule_id != "ipv4")
            continue;
        bool knob_on_okay{false};
        bool knob_off_okay{false};
        const std::string with_knob{masked_subject(row, MaskConfig{}, knob_on_okay)};
        const std::string without_knob{masked_subject(row, without_ip_masking(), knob_off_okay)};
        if (!knob_on_okay || !knob_off_okay)
            continue;
        EXPECT_EQ(with_knob, "<*>")
            << "golden line " << row.line_no
            << ": an ipv4 witness must mask with mask_ip_addresses ON.\n"
            << "  subject: " << row.subject << "\n  masked to: " << with_knob;
        // `mask_ip_addresses` gates rule 4 and nothing else, so a subject that still masks with the
        // knob OFF is being claimed by some OTHER rule and rule 4 is unwitnessed by this row. That
        // is why every ipv4 witness is OPENER-LED: a bare address is digit-leading, so rule 5 masks
        // it whatever the knob says, and a bare-address row would silently witness rule 5.
        EXPECT_EQ(without_knob, row.subject)
            << "golden line " << row.line_no
            << ": with mask_ip_addresses OFF an ipv4 witness must stay LITERAL — otherwise "
               "something upstream of rule 4 claims it and this row names the wrong rule.\n"
            << "  subject: " << row.subject << "\n  masked to: " << without_knob;
    }
}

TEST(MaskRuleGolden, DigitLeadingRowsMaskWithoutAnyOtherRuleReachingThem)
{
    if (!golden_is_readable())
        return;
    for (const Row& row : loaded().golden.rows)
    {
        if (row.rule_id != "digit_leading")
            continue;
        bool knob_on_okay{false};
        bool knob_off_okay{false};
        const std::string with_knob{masked_subject(row, MaskConfig{}, knob_on_okay)};
        const std::string without_knob{masked_subject(row, without_ip_masking(), knob_off_okay)};
        if (!knob_on_okay || !knob_off_okay)
            continue;
        EXPECT_EQ(with_knob, "<*>")
            << "golden line " << row.line_no << ": a digit_leading witness must mask.\n"
            << "  subject: " << row.subject << "\n  masked to: " << with_knob;
        EXPECT_EQ(without_knob, "<*>")
            << "golden line " << row.line_no
            << ": it must mask with the IPv4 knob OFF too — otherwise rule 4, not rule 5, is what "
               "masks it.\n  subject: "
            << row.subject;
        EXPECT_EQ(catalog::composite_rule_claiming(row.subject), std::string_view{})
            << "golden line " << row.line_no
            << ": no composite rule may claim a digit_leading witness — rule 2 runs first, so a "
               "claimed token witnesses rule 2.\n  subject: "
            << row.subject;
        // Rule 3 also masks to `<*>`, so the row must be out of its reach to name rule 5 honestly.
        // The floor is read from the catalog rather than written as a second `16` here.
        EXPECT_LT(row.subject.size(), catalog::min_hash_length())
            << "golden line " << row.line_no
            << ": a digit_leading witness at or above the hash floor could be claimed by rule 3 "
               "instead.\n  subject: "
            << row.subject << " (" << row.subject.size() << " bytes, floor "
            << catalog::min_hash_length() << ")";
    }
}

TEST(MaskRuleGolden, UuidOrHashRowsAreOutOfEveryOtherRulesReach)
{
    if (!golden_is_readable())
        return;
    for (const Row& row : loaded().golden.rows)
    {
        if (row.rule_id != "uuid_or_hash")
            continue;
        bool okay{false};
        const std::string got{masked_subject(row, MaskConfig{}, okay)};
        if (!okay)
            continue;
        EXPECT_EQ(got, "<*>") << "golden line " << row.line_no
                              << ": a uuid_or_hash witness must mask to the bare wildcard.\n"
                              << "  subject: " << row.subject << "\n  masked to: " << got;
        EXPECT_EQ(catalog::composite_rule_claiming(row.subject), std::string_view{})
            << "golden line " << row.line_no
            << ": no composite rule may claim it — rule 2 runs before rule 3.\n  subject: "
            << row.subject;
        // Rule 5 also masks to `<*>`. A letter-leading subject is out of its reach, so the
        // disposition can only have come from rule 3.
        EXPECT_FALSE(insight::tokenization::is_digit(row.subject.front()))
            << "golden line " << row.line_no
            << ": a DIGIT-leading uuid_or_hash witness would be masked by rule 5 anyway and names "
               "the wrong rule.\n  subject: "
            << row.subject;
    }
}

TEST(MaskRuleGolden, LiteralKeepRowsSurviveEveryRule)
{
    if (!golden_is_readable())
        return;
    for (const Row& row : loaded().golden.rows)
    {
        if (row.rule_id != "literal_keep")
            continue;
        bool okay{false};
        const std::string got{masked_subject(row, MaskConfig{}, okay)};
        if (!okay)
            continue;
        EXPECT_EQ(got, row.subject)
            << "golden line " << row.line_no
            << ": a literal_keep witness must survive VERBATIM. These rows are the OVER-MASKING "
               "boundary — a rule that widened too far shows up here and nowhere else.\n"
            << "  subject: " << row.subject << "\n  masked to: " << got;
        EXPECT_EQ(catalog::composite_rule_claiming(row.subject), std::string_view{})
            << "golden line " << row.line_no
            << ": a composite rule claims a literal_keep witness. Even if it normalizes to the "
               "same bytes today, the row is witnessing rule 2, not rule 6.\n  subject: "
            << row.subject;
    }
}

// ═══ LIMB 2, the CATALOGS — a new entry with no witness is itself detectable ══════════════════
// Each arm reads the declared table, never a list typed beside it. This is the difference between
// "covers the entries I remembered" and "covers the entries that exist", and the first shape is
// live in this very tree: `Ipv4MasksInsideEveryDeclaredWrapperPair` (test_stateless_template.cpp)
// names catalog completeness over a hand-typed list a seventh pair would not touch.

TEST(MaskRuleGolden, EveryDeclaredWrapperPairHasAWitness)
{
    if (!golden_is_readable())
        return;
    for (const auto& pair : insight::tokenization::kWrapperPairs)
    {
        const bool covered{std::ranges::any_of(
            loaded().golden.rows, [&pair](const Row& row)
            { return !row.subject.empty() && row.subject.front() == pair.open; })};
        EXPECT_TRUE(covered) << "declared wrapper pair `" << pair.open << pair.close
                             << "` has no witness whose subject is led by its OPENING byte.\n"
                                "  The opener is the byte that destroys digit-leading and leaves "
                                "rule 4 the only rule that can see the token,\n"
                                "  which is why a shell that shipped without one leaked a real "
                                "address into a published render.";
    }
}

TEST(MaskRuleGolden, EveryDeclaredStatusKeywordHasAWitness)
{
    if (!golden_is_readable())
        return;
    for (const std::string_view keyword : catalog::status_keywords())
    {
        const bool covered{std::ranges::any_of(
            loaded().golden.rows,
            [keyword](const Row& row)
            {
                if (row.rule_id != "status_keep")
                    return false;
                const std::vector<std::string> toks{tokens_of(row.input)};
                return std::ranges::find(toks, std::string{keyword}) != toks.end();
            })};
        EXPECT_TRUE(covered) << "declared status keyword `" << keyword
                             << "` has no status_keep witness.\n"
                                "  The lexicon grows on calibration evidence, and a keyword added "
                                "without a witness joins the\n"
                                "  green->red split silently — which is the one collapse masking "
                                "is forbidden to make.";
    }
}

TEST(MaskRuleGolden, EveryDeclaredCurrencyMarkerHasAWitness)
{
    if (!golden_is_readable())
        return;
    for (const std::string_view marker : catalog::currency_markers())
    {
        const bool covered{std::ranges::any_of(loaded().golden.rows, [marker](const Row& row)
                                               { return row.subject.starts_with(marker); })};
        EXPECT_TRUE(covered)
            << "declared currency marker `" << marker
            << "` has no witness whose subject is prefixed by it.\n"
               "  The catalog is structured to grow (the euro/pound/yen byte sequences); a marker "
               "added without a\n  witness extends the mask with nothing pinning what it does.";
    }
}

TEST(MaskRuleGolden, EveryDeclaredEphemeralRootHasAWitness)
{
    if (!golden_is_readable())
        return;
    for (const auto segments : catalog::ephemeral_root_segments())
    {
        const std::string root{join_root(segments)};
        const bool covered{
            std::ranges::any_of(loaded().golden.rows, [&root](const Row& row)
                                { return row.subject.find(root) != std::string::npos; })};
        EXPECT_TRUE(covered) << "declared ephemeral root `" << root
                             << "` has no witness whose subject contains it.\n"
                                "  Adding a root is an output-affecting core masking change that "
                                "owes a version bump, and a\n  mis-declared root OVER-masks — "
                                "over-masking destroys signal irrecoverably, so an unwitnessed "
                                "root\n  is the most expensive kind to add blind.";
    }
}

// ═══ The regeneration ACTION — DISABLED_ on purpose; see the file header ══════════════════════

TEST(MaskRuleGolden, DISABLED_RegenerateGolden)
{
    ASSERT_TRUE(loaded().why.empty()) << loaded().why;
    const Golden& golden{loaded().golden};
    const bool version_moved{golden.version != insight::kCanonicalizationVersion};

    std::vector<std::string> refused;
    std::vector<std::string> filled;
    std::vector<std::string> rewritten;
    std::vector<std::string> rebuilt_rows;
    rebuilt_rows.reserve(golden.rows.size());

    for (const Row& row : golden.rows)
    {
        const std::string actual{mask_line(row.input)};
        if (row.expected.empty())
            filled.push_back(row.rule_id + " -> " + actual);
        else if (row.expected != actual)
        {
            if (!version_moved)
                refused.push_back("line " + std::to_string(row.line_no) + " (" + row.rule_id +
                                  "): " + row.expected + "  ->  " + actual);
            else
                rewritten.push_back("line " + std::to_string(row.line_no) + " (" + row.rule_id +
                                    "): " + row.expected + "  ->  " + actual);
        }
        rebuilt_rows.push_back(row.rule_id + std::string{kFieldSep} + row.subject +
                               std::string{kFieldSep} + row.input + std::string{kFieldSep} +
                               actual);
    }

    // THE GUARD. A moved row may only be rewritten AFTER the version token has been bumped, so
    // regeneration cannot be the silent repair for a rule change that forgot its bump — which is
    // the entire failure mode this gate was built for.
    if (!refused.empty())
    {
        std::string detail;
        for (const std::string& item : refused)
            detail += "\n    " + item;
        FAIL() << "REFUSING TO REGENERATE.\n"
               << refused.size()
               << " witness row(s) changed their masked output while "
                  "kCanonicalizationVersion did NOT move.\n"
               << "  kCanonicalizationVersion: " << insight::kCanonicalizationVersion << "\n"
               << "  golden header           : " << golden.version << "\n"
               << "  moved:" << detail << "\n\n"
               << "  A masking rule change is an output-affecting canonicalization change: "
                  "every stored comparison\n"
                  "  made before it stops being comparable with one made after.\n\n"
                  "  DO ONE OF TWO THINGS:\n"
                  "  (a) The change was INTENDED. Bump kCanonicalizationVersion in "
                  "core/api/canon.api.cppm,\n"
                  "      add a ledger entry above it naming what moved and why, then re-run "
                  "this action:\n        "
               << kRegenCommand
               << "\n      and review the resulting golden diff line by line — it is the "
                  "human-readable\n"
                  "      statement of exactly which line populations this generation "
                  "re-canonicalises.\n"
                  "  (b) The change was NOT intended. Restore the rule in "
                  "core/src/mask/mask.cpp. The rows\n"
                  "      listed above name which classes moved, and "
                  "MaskRuleGolden.GoldenTemplatesAreByteIdentical\n"
                  "      prints the first differing TOKEN for each.";
    }

    std::ofstream out{INSIGHT_CANON_MASK_GOLDEN_PATH, std::ios::binary | std::ios::trunc};
    ASSERT_TRUE(out.is_open()) << "cannot write " << INSIGHT_CANON_MASK_GOLDEN_PATH;
    for (std::size_t pos{0}; pos < golden.lines.size(); ++pos)
    {
        if (golden.row_at[pos] != std::string::npos)
            out << rebuilt_rows[golden.row_at[pos]] << "\n";
        else if (golden.lines[pos].starts_with(kVersionKey))
            out << kVersionKey << insight::kCanonicalizationVersion << "\n";
        else
            out << golden.lines[pos] << "\n";
    }
    out.flush();
    ASSERT_TRUE(out.good()) << "write to " << INSIGHT_CANON_MASK_GOLDEN_PATH << " failed";

    std::printf("regenerated %s\n  version : %s\n  rows    : %zu\n  filled  : %zu\n  rewritten: "
                "%zu\n",
                INSIGHT_CANON_MASK_GOLDEN_PATH,
                std::string{insight::kCanonicalizationVersion}.c_str(), rebuilt_rows.size(),
                filled.size(), rewritten.size());
    for (const std::string& item : filled)
        std::printf("  filled    %s\n", item.c_str());
    for (const std::string& item : rewritten)
        std::printf("  rewritten %s\n", item.c_str());
}
