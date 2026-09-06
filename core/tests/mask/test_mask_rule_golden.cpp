// invariant: a golden that pins MASKED OUTPUT for a named line population, so a masking-rule change
// reds WITHOUT the canonicalization version token moving.
// invariant: THE DEFECT THIS EXISTS FOR — that token is the declared identity of the masking rule
// set, and consumers pin it, wire vectors carry it and a downstream gate asserts its literal.
// invariant: every one of those fires when the VERSION moves and NONE fires when the RULES move, so
// the bump is enforced only by whoever remembers to make it.
// invariant: MEASURED — applying a new masking arm left this repo and the downstream one fully
// green, AND THAT GREEN WAS THE FINDING.
// invariant: WHY THE EXISTING ARMS DID NOT CATCH IT IS THE SHAPE, NOT THE COUNT.
// invariant: the sibling suite carries dozens of assertions and many pin literal bytes, but its
// coverage is RELATIONAL where it is not literal.
// invariant: a relational assertion is INVARIANT under any rule change that moves both sides the
// same way, so two lines still share a template when the rule that masks them widens.
// invariant: the literal pins that do exist are concentrated on the arms most recently REPAIRED,
// each written to close its own incident, so an arm nobody had just repaired had no pin at all.
// invariant: that is a hole in DERIVATION, not in effort.
// invariant: the golden holds one row per witness — rule id, subject token, input line, template.
// invariant: LIMB 1 is a rule's ACCEPTANCE SET widening or narrowing in place, where every table
// stays byte-identical and the masked OUTPUT of some witness moves.
// invariant: LIMB 2 is a rule or catalog entry being ADDED, REMOVED or REORDERED, where output may
// not move at all because the new rule's class has no witness yet.
// invariant: the coverage arms read the DECLARED catalogs and red when the rule set contains
// something the population does not, and NEITHER limb substitutes for the other.
// invariant: LIMB 2 IS WHAT STOPS THIS FILE BECOMING THE DEFECT ONE LEVEL UP — a golden over a
// hand-picked handful stays green while a new arm ships unwitnessed.
// invariant: WHAT IT DOES NOT COVER IS STATED, NOT LEFT IMPLICIT.
// invariant: params are not pinned — a masked position contributes both a wildcard to the
// template and its raw token to params, so which positions masked is already visible.
// invariant: the template id is not pinned — it is a digest of the template string, so pinning
// the template pins the id up to a hash change, which is not a masking change.
// invariant: THE COMPOSED PATH is not covered, and that boundary is what the homing call BUYS —
// the population goes straight to the masker and never through the tokenizer.
// invariant: a golden taken through the composed pipeline reds on either stage and cannot say WHICH
// one moved, so this fixture controls exactly one thing and a red here names the MASKER.
// invariant: the price is that anything manifesting only THROUGH the composition is invisible here
// — detection picking another strategy, a different content slice, a semantic row change.
// invariant: none of those is a masking-rule change, and each has its own home, including the
// committed wire vectors downstream that DO re-derive through the full shipped tokenizer.
// invariant: the only config knob covered is the IP one, because it is the only knob gating a rule.
// invariant: a rule class whose witness is absent is made VISIBLE by limb 2 for the declared
// catalogs, but it cannot see a class that no catalog declares.
// invariant: REGENERATION IS AN ACTION, NOT A PROPERTY, which is why it is a disabled test — the
// harness prints a disabled-test banner, so it is visible rather than silently skipped.
// invariant: running it from the same binary the gate runs in is what guarantees the regenerated
// bytes come from the same compiled masker the gate reads.
// invariant: THE GUARD — it REFUSES to rewrite a row whose expected value is present and differs,
// unless the version token already differs from the version recorded in the golden's header.
// invariant: so the only way to make a moved row green again is to BUMP THE TOKEN FIRST, which
// means `just regenerate it` cannot be the silent repair.
// invariant: rows whose expected column is EMPTY are always filled — that is how a new witness
// joins the population, and it can never mask a moved row, which is by definition not empty.
#include <gtest/gtest.h>

import insight.canon.test;

using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::stateless_template;
namespace catalog = insight::tokenization::rule_catalog;

namespace
{

// invariant: the five TOP-LEVEL dispositions of the masking precedence that are not the composite
// layer, which is named by its own catalog instead.
// invariant: this list plus that catalog is the complete rule-id namespace.
// invariant: it is hand-held because the dispatcher states these as a disjunction chain rather than
// a table, and a sibling arm is what keeps a typo here from silently minting a sixth id.
// refs: SRC-D-TID-12
constexpr std::array<std::string_view, 5> kTopLevelRuleIds{
    {std::string_view{"status_keep"}, std::string_view{"uuid_or_hash"}, std::string_view{"ipv4"},
     std::string_view{"digit_leading"}, std::string_view{"literal_keep"}}};

struct Row
{
    std::string rule_id;
    // invariant: an EMPTY expected column means fill-me and is honoured only by regeneration.
    std::string subject;
    std::string input;
    std::string expected;
    std::size_t line_no{0};
};

struct Golden
{
    std::string version;
    std::vector<Row> rows;
    // invariant: regeneration walks EVERY raw line in file order and substitutes rebuilt text only
    // for witness rows, so the section comments stay where their section is.
    // invariant: an earlier shape kept comments and rows in two separate lists and would have
    // rewritten the file with every comment hoisted to the top.
    // invariant: a regenerator that destroys the document is a regenerator nobody runs twice.
    std::vector<std::string> lines;
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

// invariant: exactly 3 or 4 fields, a 3-field row being a witness awaiting its expected value.
// invariant: a field count outside that range is MALFORMED and is reported rather than guessed at,
// because a separator inside a witness line would silently shift every column.
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

// invariant: the same split the masker performs, so a token index in one is a token index in the
// other.
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

// invariant: the FIRST occurrence — a witness whose subject appears twice is testing an ambiguous
// thing, and a sibling arm rejects it rather than picking one.
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

// invariant: the masked token AT THE SUBJECT'S INDEX, which is what a rule's disposition is
// actually about.
// invariant: reading the whole template instead would fold every other token's behaviour into the
// discriminator.
[[nodiscard]] std::string masked_subject(const Row& row, const MaskConfig& config, bool& okay)
{
    std::size_t occurrences{0};
    const std::size_t index{subject_index(row, occurrences)};
    okay = index != std::string::npos && occurrences == 1;
    if (!okay)
        return {};
    const std::vector<std::string> masked{tokens_of(mask_line(row.input, config))};
    // invariant: the masker emits exactly one output token per input token, joining on a single
    // space and never gaining one, so the indices correspond.
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

// invariant: loaded once, and a parse failure is reported by every arm rather than crashing one.
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

// invariant: an unparsable golden must fail ONCE with the parse error, never as forty confusing
// coverage failures about a population that was never read.
[[nodiscard]] bool golden_is_readable()
{
    if (loaded().why.empty())
        return true;
    ADD_FAILURE() << "the masking golden could not be read, so nothing below is a statement about "
                     "the masker:\n  "
                  << loaded().why;
    return false;
}

// invariant: a failure names the moved TOKEN instead of handing back two lines to eyeball.
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

// invariant: each top-level row proves the rule it NAMES is the REASON for the disposition, not
// merely that the disposition happened.
// invariant: a composite row is discriminated by its catalog lookup; the five top-level
// dispositions have no such table, so each carries its own discriminator.
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
            continue;
        EXPECT_EQ(kept, row.subject)
            << "golden line " << row.line_no << ": a status_keep witness must survive VERBATIM.\n"
            << "  subject: " << row.subject << "\n  masked to: " << kept;

        // invariant: NON-VACUITY, and it is the whole arm — replace the preceding token with a
        // non-keyword and the same value must MASK.
        // invariant: without this leg a row could be green because the value is a letter-leading
        // word the boundary rule would have kept anyway, leaving the status rule unwitnessed.
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
        // invariant: the IP knob gates its rule and nothing else.
        // invariant: a subject that still masks with the knob OFF is being claimed by some OTHER
        // rule and this rule is unwitnessed by that row.
        // invariant: that is why every IP witness is OPENER-LED.
        // invariant: a bare address is digit-leading, so the digit-leading rule masks it whatever
        // the knob says, and a bare-address row would witness it.
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
        // invariant: the hash rule also masks to a wildcard, so the row must be out of its reach to
        // name the digit-leading rule honestly.
        // invariant: the floor is read from the catalog rather than written as a second literal
        // here.
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
        // invariant: the digit-leading rule also masks to a wildcard, so a LETTER-leading subject
        // is out of its reach and the disposition can only have come from the hash rule.
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

// invariant: each catalog arm reads the DECLARED table, never a list typed beside it.
// invariant: that is the difference between covering the entries someone remembered and covering
// the entries that EXIST.
// invariant: the instance this file used to name — the wrapper-pair arm in the sibling masker
// suite — has since been REPAIRED to build its shells from the catalog.
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

    // invariant: THE GUARD — a moved row may only be rewritten AFTER the version token has been
    // bumped.
    // invariant: so regeneration cannot be the silent repair for a rule change that forgot its
    // bump, which is the entire failure mode this gate was built for.
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
