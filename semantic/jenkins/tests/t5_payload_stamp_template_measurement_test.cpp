// NOLINTBEGIN — measurement harness: long literals, wide reports and raw loops are the point.
// t5_payload_stamp_template_measurement_test.cpp — the pre-registered measurement owed by
// adr/0046 Part 2 clause 2 (and the evidence that settles clause 3).
//
// THE QUESTION. adr/0044 §1 rules the Jenkins `payload-stamped` class NOT declarable as transport
// (the stamp is a payload-determined subset, not a stream property), so on that class the
// timestamper stamps stay in CONTENT and those lines lose their Jenkins claim → they re-route to
// RawText and template WITH the stamp. The templates change; that is settled. What is NOT settled
// is whether the template COUNT changes with them, and that difference is the difference between a
// cosmetic re-baseline and a precision-first regression (adr/0013).
//
// THE ARMS, and why arm B is constructed the way it is.
//   A (+strip, the shipped world) — Tokenizer composed WITH insight.semantic.jenkins. A stamped
//       line is claimed by JenkinsStrategy, the prefix is stripped, the template comes off the
//       stripped content.
//   B (−strip, the adr/0044 §1 world) — Tokenizer composed with NO semantic package. A stamped
//       line is claimed by nobody and falls to the RawText floor with the stamp in content.
// B is the right vehicle for the §1 world ONLY IF removing the whole package changes nothing for
// the lines that are not supposed to move. That is not assumed: `TemplateCountUnderTheStrip`
// asserts the moving set is EXACTLY the stamped set (an unstamped line must template identically
// in both arms — the "byte-identical RawText fallback" the strategy comment claims, verified on
// real bytes rather than trusted). If an unstamped line ever moved, arm B would be measuring the
// package removal instead of the strip, and the assertion says so loudly.
//
// PRE-REGISTERED DECISION RULE (fixed here, in the commit that precedes the numbers — the commit
// order is the audit trail, studies/010 §2 discipline). Let `distinct_A` / `distinct_B` be the
// distinct template counts over the whole payload-stamped slice, and `ceiling` the distinct RAW
// stamped-line count (what "no collapse at all" would score):
//   * STABLE   ⇒ distinct_B == distinct_A. adr/0046 branch 1 (honesty-only + re-baseline; the move
//                rides T5 as specified) — the id SETS still move, which is why the id counts are
//                reported alongside.
//   * EXPLODES ⇒ distinct_B >= 2 × distinct_A, or the stamped-subset arm-B distinct count reaches
//                >= 0.5 × ceiling (the token defeating collapse toward per-line uniqueness).
//                adr/0046 branch 2 (precision-first regression under adr/0013; T5 blocked until the
//                masker claims the token to a stable normal form).
//   * Anything strictly between is a THIRD outcome and is reported as such — it is never rounded
//     into a branch.
//
// THE VACUITY GUARD. A gate that cannot return the bad answer is not a gate
// ([[synthetic-gate-vacuity-vs-judgment]]). `CounterCanReportAnExplosion` feeds the SAME counting
// path a block of lines carrying a token the masker keeps verbatim and asserts the count tracks the
// line count — so a green "count stable" on the corpus is a fact about the corpus, not a saturated
// instrument.
//
// CORPUS-GATED. The Jenkins marker corpus is §2a-private and out-of-tree, so this SKIPS cleanly
// when the manifest env is unset/missing — green in CI and on every clone.
//   JENKINS_T5_MANIFEST  one absolute log path per line (the 19 payload-stamped logs of
//                        jenkins-markers/v2; the class definition is studies/010 §6.2's triage,
//                        reused via coderoast-corpora .../scripts/t0_transport.py — never
//                        re-invented)
//   JENKINS_T5_OUT       (optional) directory for a per-line TSV dump, for the offline cross-check
//
// BYTE FIDELITY. Every read is binary; lines are split on '\n' ONLY and no '\r' is trimmed
// ([[corpus-scrub-freeze-byte-fidelity]] — a CR-folding read has already fabricated a gate score in
// this workspace). Both arms see the identical bytes, so the comparison is fair by construction.
//
// Determinism: pure byte functions, no RNG, no clock, no float in any counted quantity.
#include <gtest/gtest.h>

import std;
import insight.canon; // Tokenizer / ArenaAllocator / MaskConfig / compose / template_id_of
import insight.semantic.jenkins; // kManifest + make_strategy

namespace
{
using insight::TemplateId;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

constexpr std::size_t kArenaBlockBytes{4U * 1024U * 1024U};
constexpr std::size_t kSampleTemplatesPrinted{12};
constexpr double kExplosionCountRatio{2.0};
constexpr double kExplosionCeilingShare{0.5};

[[nodiscard]] std::optional<std::string> env_value(const char* key)
{
    if (const char* value = std::getenv(key); value != nullptr && *value != '\0')
        return std::string{value};
    return std::nullopt;
}

// Raw bytes → lines. Split on '\n' only; a '\r' stays in the line (it is content, not a delimiter).
[[nodiscard]] std::vector<std::string> read_raw_lines(const std::string& path)
{
    std::ifstream input{path, std::ios::binary};
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string bytes{buffer.str()};

    std::vector<std::string> lines;
    std::size_t start{0};
    while (start <= bytes.size())
    {
        const std::size_t stop{bytes.find('\n', start)};
        if (stop == std::string::npos)
        {
            if (start < bytes.size())
                lines.emplace_back(bytes.substr(start));
            break;
        }
        lines.emplace_back(bytes.substr(start, stop - start));
        start = stop + 1;
    }
    return lines;
}

[[nodiscard]] std::vector<std::string> read_manifest(const std::string& path)
{
    std::vector<std::string> paths;
    for (std::string& line : read_raw_lines(path))
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (!line.empty())
            paths.push_back(line);
    }
    return paths;
}

// One arm's outcome for one line: the template it produced, or nullopt when the line was declined
// (empty / blank-after-strip — `make_event` drops it, never an empty template).
struct LineOutcome
{
    bool produced{false};
    std::string template_str;
    insight::LogFormat format{insight::LogFormat::Unknown};
};

[[nodiscard]] std::vector<LineOutcome> run_arm(const std::vector<std::string>& lines,
                                               const insight::semantic::ComposedSemantics& composed)
{
    ArenaAllocator arena{kArenaBlockBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, composed};
    std::vector<LineOutcome> outcomes;
    outcomes.reserve(lines.size());
    for (const std::string& line : lines)
    {
        const auto event{tokenizer.process_line(line)};
        LineOutcome outcome;
        if (event.has_value())
        {
            outcome.produced = true;
            outcome.template_str = std::string{event->template_str};
            outcome.format = event->format;
        }
        outcomes.push_back(std::move(outcome));
    }
    return outcomes;
}

// Distinct-set accumulator: the two counts adr/0046 clause 2 asks for, kept side by side so their
// AGREEMENT is itself observable (template_id is SHA-256 of the template string — a divergence
// would be a collision and is reported, not assumed away).
struct DistinctCounter
{
    std::set<std::string> templates;
    std::set<TemplateId> ids;

    void add(const std::string& template_str)
    {
        templates.insert(template_str);
        ids.insert(insight::template_id_of(template_str));
    }
};

[[nodiscard]] std::string escape_field(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (const char chr : text)
    {
        if (chr == '\t')
            out += "\\t";
        else if (chr == '\r')
            out += "\\r";
        else if (chr == '\\')
            out += "\\\\";
        else
            out += chr;
    }
    return out;
}
} // namespace

// The instrument's positive control: with a token the masker keeps verbatim, the distinct count
// must track the line count. If this ever collapsed, a "count stable" verdict on the corpus would
// be an artifact of the counter, not a property of the masker.
TEST(JenkinsPayloadStampMeasurement, CounterCanReportAnExplosion)
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose(manifests)};

    const std::array<std::string, 5> distinct_lines{
        "starting stage alpha now", "starting stage bravo now", "starting stage charlie now",
        "starting stage delta now", "starting stage echo now"};
    std::vector<std::string> lines{distinct_lines.begin(), distinct_lines.end()};
    DistinctCounter distinct;
    for (const LineOutcome& outcome : run_arm(lines, composed))
    {
        ASSERT_TRUE(outcome.produced);
        distinct.add(outcome.template_str);
    }
    EXPECT_EQ(distinct.templates.size(), distinct_lines.size())
        << "the counter collapsed lines it must not collapse — an 'explosion' verdict would be "
           "unreachable and the gate vacuous";
    EXPECT_EQ(distinct.ids.size(), distinct.templates.size())
        << "template_id count must track the distinct template count";

    std::vector<std::string> repeated(distinct_lines.size(), std::string{distinct_lines.front()});
    DistinctCounter collapsed;
    for (const LineOutcome& outcome : run_arm(repeated, composed))
        collapsed.add(outcome.template_str);
    EXPECT_EQ(collapsed.templates.size(), 1U)
        << "identical lines must collapse to one template — the counter is not merely counting "
           "lines";
}

// What the real masker does to the real token, run through the real chain — MEASURED, and the
// answer is the one adr/0046 clause 3 routes into the shipped comment.
//
// It is NOT the one clause (c)'s re-derivation predicted. That re-derivation had
// `normalize_diagnostic_composite` (kCompositeRules rule #1, first-claim-wins) claiming the token
// because `15:11` satisfies its `':'`-then-digit TRIGGER. It stops one gate short: D-MSK-1 also
// requires an ANCHOR — at least one LETTER-LEADING sub-segment (mask.cpp, the rule's own contract:
// "Returns true … only when ≥1 segment was masked AND an anchor exists"). Splitting
// `[2026-06-23T15:11:09.020Z]` on `:`/`/` yields `[2026-06-23T15`, `11`, `09.020Z]` — not one of
// them letter-leading — so rule #1 DECLINES. `normalize_bracket_index` declines too (digits then
// `-`, not `]`), and the whole-token digit mask (dispatch rule 5) never sees it because the leading
// byte is `[`, not a digit. The token therefore reaches dispatch rule 6: LITERAL KEEP, verbatim,
// stamp and all.
//
// This is a CHARACTERIZATION test: it asserts what the masker does today, not what it should do.
// It is deliberately the tripwire for the T5 masker work — the day the masker claims `[<RFC3339>]`
// to a stable normal form, this test goes RED and must be rewritten to the new normal form. That is
// the intended failure, not a regression.
TEST(JenkinsPayloadStampMeasurement, TheMaskerKeepsTheTimestamperTokenVerbatim)
{
    const insight::semantic::ComposedSemantics none{
        insight::semantic::compose(std::span<const insight::semantic::SemanticPackageManifest>{})};
    const std::array<std::string, 3> stamped{"[2026-06-23T15:11:09.020Z] + git fetch --tags",
                                             "[2026-06-23T15:11:10.884Z] + git fetch --tags",
                                             "[2026-06-24T09:02:44.001Z] + git fetch --tags"};
    std::vector<std::string> lines{stamped.begin(), stamped.end()};
    const auto outcomes{run_arm(lines, none)};
    ASSERT_EQ(outcomes.size(), stamped.size());
    for (std::size_t index{0}; index < outcomes.size(); ++index)
    {
        ASSERT_TRUE(outcomes[index].produced) << "line " << index << " produced no event";
        std::cout << "  unstripped[" << index << "] = \"" << outcomes[index].template_str << "\"\n";
        EXPECT_EQ(outcomes[index].template_str, stamped[index])
            << "the bracketed RFC3339 token survives the masker VERBATIM — no rule claims it";
    }
    EXPECT_NE(outcomes[0].template_str, outcomes[1].template_str)
        << "two lines differing ONLY in the stamp's milliseconds template APART: this is the "
           "collapse loss the strip prevents, and it is why adr/0046's branch 2 fires";
    EXPECT_NE(outcomes[0].template_str, outcomes[2].template_str)
        << "a different day forks the template too";

    // The mechanism, pinned AT THE MASK LAYER: the same token WITHOUT its brackets is
    // digit-leading, so dispatch rule 5 masks it and the two lines collapse to one template. The
    // token sits mid-line in both probes so neither line's strategy routing can change — the ONLY
    // difference reaching the masker is the pair of brackets. That is the whole of it, and it gives
    // a T5 masker fix a short, named target.
    const std::array<std::string, 2> unbracketed{"fetched at 2026-06-23T15:11:09.020Z ok",
                                                 "fetched at 2026-06-24T09:02:44.001Z ok"};
    std::vector<std::string> bare{unbracketed.begin(), unbracketed.end()};
    const auto bare_outcomes{run_arm(bare, none)};
    ASSERT_EQ(bare_outcomes.size(), unbracketed.size());
    ASSERT_TRUE(bare_outcomes[0].produced);
    ASSERT_TRUE(bare_outcomes[1].produced);
    std::cout << "  unbracketed[0] = \"" << bare_outcomes[0].template_str << "\"\n";
    EXPECT_EQ(bare_outcomes[0].template_str, bare_outcomes[1].template_str)
        << "the same timestamp WITHOUT brackets is digit-leading → masked → collapses; the "
           "bracket is the whole difference";
    const std::array<std::string, 2> bracketed{"fetched at [2026-06-23T15:11:09.020Z] ok",
                                               "fetched at [2026-06-24T09:02:44.001Z] ok"};
    std::vector<std::string> kept{bracketed.begin(), bracketed.end()};
    const auto kept_outcomes{run_arm(kept, none)};
    ASSERT_TRUE(kept_outcomes[0].produced);
    ASSERT_TRUE(kept_outcomes[1].produced);
    std::cout << "  bracketed[0]   = \"" << kept_outcomes[0].template_str << "\"\n";
    EXPECT_NE(kept_outcomes[0].template_str, kept_outcomes[1].template_str)
        << "re-bracket the SAME token, same position, same routing — and collapse is lost again";
}

// The measurement. Corpus-gated: skips cleanly without the private corpus.
TEST(JenkinsPayloadStampMeasurement, TemplateCountUnderTheStrip)
{
    const auto manifest_path{env_value("JENKINS_T5_MANIFEST")};
    if (!manifest_path.has_value())
        GTEST_SKIP() << "JENKINS_T5_MANIFEST unset — the payload-stamped slice is §2a-private and "
                        "out-of-tree";
    const std::vector<std::string> logs{read_manifest(*manifest_path)};
    ASSERT_FALSE(logs.empty()) << "manifest " << *manifest_path << " listed no logs";

    const std::array manifests{insight::semantic::jenkins::kManifest};
    const insight::semantic::ComposedSemantics with_jenkins{insight::semantic::compose(manifests)};
    const insight::semantic::ComposedSemantics without_jenkins{
        insight::semantic::compose(std::span<const insight::semantic::SemanticPackageManifest>{})};

    const auto strategy{insight::semantic::jenkins::make_strategy()};
    ArenaAllocator probe_arena{kArenaBlockBytes};

    DistinctCounter all_a;
    DistinctCounter all_b;
    DistinctCounter stamped_a;
    DistinctCounter stamped_b;
    std::set<std::string> stamped_raw; // the no-collapse ceiling
    std::size_t total_lines{0};
    std::size_t total_events_a{0};
    std::size_t total_events_b{0};
    std::size_t stamped_lines{0};
    std::size_t moved_lines{0};
    std::size_t unstamped_moved{0};
    std::size_t declined_a{0};
    std::size_t declined_b{0};

    const auto out_dir{env_value("JENKINS_T5_OUT")};

    std::cout << "\n=== adr/0046 Part 2 clause 2 — per-log ===\n"
              << std::format("{:>7} {:>7} {:>7} {:>7} {:>7} {:>7}  {}\n", "lines", "stamped",
                             "tmplA", "tmplB", "idA", "idB", "log");

    for (const std::string& log_path : logs)
    {
        const std::vector<std::string> lines{read_raw_lines(log_path)};
        ASSERT_FALSE(lines.empty()) << "empty or unreadable log: " << log_path;

        const auto arm_a{run_arm(lines, with_jenkins)};
        const auto arm_b{run_arm(lines, without_jenkins)};
        ASSERT_EQ(arm_a.size(), arm_b.size());

        DistinctCounter log_a;
        DistinctCounter log_b;
        std::size_t log_stamped{0};

        std::ofstream dump;
        if (out_dir.has_value())
        {
            std::string name{log_path};
            std::ranges::replace(name, '/', '_');
            dump.open(*out_dir + "/" + name + ".tsv", std::ios::binary);
            dump << "idx\tstamped\tfmtA\ttemplateA\tfmtB\ttemplateB\n";
        }

        for (std::size_t index{0}; index < lines.size(); ++index)
        {
            const std::string& line{lines[index]};
            ++total_lines;

            // The stamped partition comes from the SHIPPED acceptor, never a re-implementation:
            // JenkinsStrategy sets a timestamp exactly on a timestamper-prefixed line.
            bool is_stamped{false};
            if (const auto parsed{strategy->parse(line, probe_arena)}; parsed.has_value())
                is_stamped = parsed->timestamp.has_value();
            else
                is_stamped = line.starts_with("[2") && strategy->confidence(line) > 0.0;
            if (is_stamped)
            {
                ++log_stamped;
                ++stamped_lines;
                stamped_raw.insert(line);
            }

            if (arm_a[index].produced)
            {
                ++total_events_a;
                log_a.add(arm_a[index].template_str);
                all_a.add(arm_a[index].template_str);
                if (is_stamped)
                    stamped_a.add(arm_a[index].template_str);
            }
            else if (!line.empty())
                ++declined_a;

            if (arm_b[index].produced)
            {
                ++total_events_b;
                log_b.add(arm_b[index].template_str);
                all_b.add(arm_b[index].template_str);
                if (is_stamped)
                    stamped_b.add(arm_b[index].template_str);
            }
            else if (!line.empty())
                ++declined_b;

            const bool moved{arm_a[index].produced != arm_b[index].produced ||
                             arm_a[index].template_str != arm_b[index].template_str};
            if (moved)
            {
                ++moved_lines;
                if (!is_stamped)
                {
                    ++unstamped_moved;
                    if (unstamped_moved <= kSampleTemplatesPrinted)
                        std::cout << "  UNSTAMPED LINE MOVED (" << log_path << ":" << index
                                  << ")\n    A=\"" << arm_a[index].template_str << "\"\n    B=\""
                                  << arm_b[index].template_str << "\"\n";
                }
            }

            if (dump.is_open())
                dump << index << '\t' << (is_stamped ? 1 : 0) << '\t'
                     << insight::to_string(arm_a[index].format) << '\t'
                     << escape_field(arm_a[index].template_str) << '\t'
                     << insight::to_string(arm_b[index].format) << '\t'
                     << escape_field(arm_b[index].template_str) << '\n';
        }

        std::cout << std::format("{:>7} {:>7} {:>7} {:>7} {:>7} {:>7}  {}\n", lines.size(),
                                 log_stamped, log_a.templates.size(), log_b.templates.size(),
                                 log_a.ids.size(), log_b.ids.size(), log_path);
    }

    const std::size_t shared_ids{static_cast<std::size_t>(std::ranges::count_if(
        all_a.ids, [&](const TemplateId& id) { return all_b.ids.contains(id); }))};
    const std::size_t shared_stamped_ids{static_cast<std::size_t>(std::ranges::count_if(
        stamped_a.ids, [&](const TemplateId& id) { return stamped_b.ids.contains(id); }))};

    std::cout << "\n=== adr/0046 Part 2 clause 2 — THE FOUR NUMBERS (slice-wide) ===\n"
              << "logs                         : " << logs.size() << "\n"
              << "lines (raw, '\\n'-split)      : " << total_lines << "\n"
              << "stamped lines                : " << stamped_lines << "\n"
              << "events A (+strip)            : " << total_events_a << "  declined " << declined_a
              << "\n"
              << "events B (-strip)            : " << total_events_b << "  declined " << declined_b
              << "\n"
              << "distinct templates  +strip   : " << all_a.templates.size() << "\n"
              << "distinct templates  -strip   : " << all_b.templates.size() << "\n"
              << "distinct template_id +strip  : " << all_a.ids.size() << "\n"
              << "distinct template_id -strip  : " << all_b.ids.size() << "\n"
              << "  ids shared across arms     : " << shared_ids << "\n"
              << "stamped-subset templates  A  : " << stamped_a.templates.size() << "\n"
              << "stamped-subset templates  B  : " << stamped_b.templates.size() << "\n"
              << "  stamped ids shared         : " << shared_stamped_ids << "\n"
              << "no-collapse CEILING (distinct raw stamped lines): " << stamped_raw.size() << "\n"
              << "lines whose template moved   : " << moved_lines
              << "  (unstamped among them: " << unstamped_moved << ")\n";

    // Arm B measures the STRIP, not the package removal — the construction premise, asserted.
    EXPECT_EQ(unstamped_moved, 0U)
        << "an UNSTAMPED line templated differently with and without the Jenkins package: arm B is "
           "then measuring package removal, not the timestamper strip, and the four numbers below "
           "do not answer adr/0046 clause 2";

    // The counts must agree with their id counts — a divergence is a SHA-256 collision, reported.
    EXPECT_EQ(all_a.templates.size(), all_a.ids.size());
    EXPECT_EQ(all_b.templates.size(), all_b.ids.size());

    const bool stable{all_b.templates.size() == all_a.templates.size()};
    const bool explodes{all_b.templates.size() >=
                            static_cast<std::size_t>(kExplosionCountRatio *
                                                     static_cast<double>(all_a.templates.size())) ||
                        static_cast<double>(stamped_b.templates.size()) >=
                            kExplosionCeilingShare * static_cast<double>(stamped_raw.size())};
    std::cout << "VERDICT: "
              << (explodes ? "EXPLODES (adr/0046 branch 2 — precision-first regression, T5 BLOCKED)"
                           : (stable ? "COUNT STABLE (adr/0046 branch 1 — honesty-only + "
                                       "re-baseline, T5 proceeds)"
                                     : "NEITHER pre-registered branch — report the numbers as a "
                                       "third outcome"))
              << "\n\n";

    // This test REPORTS; it never asserts the branch. The branch is the finding.
    SUCCEED();
}
// NOLINTEND
