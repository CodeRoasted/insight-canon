// payload_stamp_template_count_measurement_test.cpp — the pre-registered measurement owed by the
// ruling that a PAYLOAD stamp is dialect content, never a transport envelope (and the evidence
// that settles what the masker then owes).
//
// THE QUESTION. The Jenkins `payload-stamped` class is NOT declarable as transport
// (the stamp is a payload-determined subset, not a stream property), so on that class the
// timestamper stamps stay in CONTENT and those lines lose their Jenkins claim → they re-route to
// RawText and template WITH the stamp. The templates change; that is settled. What is NOT settled
// is whether the template COUNT changes with them, and that difference is the difference between a
// cosmetic re-baseline and a precision-first regression.
//
// THE ARMS, and why arm B is constructed the way it is.
//   A (+strip, the shipped world) — POST-CUT: the DECLARED catalogue peel
//       (`bracket-rfc3339-line-prefix`) strips the stamp, then the tokenizer templates the
//       peeled content. (Pre-cut this arm was JenkinsStrategy claiming and stripping; the
//       strategy died at the identity cut and G-T5-PEEL certifies the peel equals its strip.)
//   B (−strip, the stamp-stays-in-content world) — Tokenizer composed with NO semantic package. A
//       stamped line is claimed by nobody and falls to the RawText floor with the stamp in content.
// B is the right vehicle for that world ONLY IF removing the whole package changes nothing for
// the lines that are not supposed to move. That is not assumed: `TemplateCountUnderTheStrip`
// asserts the moving set is EXACTLY the stamped set (an unstamped line must template identically
// in both arms — the "byte-identical RawText fallback" the strategy comment claims, verified on
// real bytes rather than trusted). If an unstamped line ever moved, arm B would be measuring the
// package removal instead of the strip, and the assertion says so loudly.
//
// PRE-REGISTERED DECISION RULE (fixed here, in the commit that precedes the numbers — the commit
// order is the audit trail). Let `distinct_A` / `distinct_B` be the
// distinct template counts over the whole payload-stamped slice, and `ceiling` the distinct RAW
// stamped-line count (what "no collapse at all" would score):
//   * STABLE   ⇒ distinct_B == distinct_A. Branch 1 (honesty-only + re-baseline; the move rides
//                the cut as specified) — the id SETS still move, which is why the id counts are
//                reported alongside.
//   * EXPLODES ⇒ distinct_B >= 2 × distinct_A, or the stamped-subset arm-B distinct count reaches
//                >= 0.5 × ceiling (the token defeating collapse toward per-line uniqueness).
//                Branch 2 (a precision-first regression against the degradation contract; the
//                cut is blocked until the masker claims the token to a stable normal form).
//   * Anything strictly between is a THIRD outcome and is reported as such — it is never rounded
//     into a branch.
//
// STATUS OF THAT CLASSIFIER AFTER THE SRC-D-MSK-5 BRACKETED-STAMP REPAIR (2fe2e85): it is the
// FROZEN RECORD of the pre-fix measurement it correctly scored (EXPLODES at
// 95.9% of ceiling), and it is NOT the repair's fitness predicate — its ceiling leg is can't-PASS
// on these bytes from the pre-fix record alone (arm A alone at 3 138/6 055 = 51.8% ≥ the 0.5 bar,
// and B ≥ A is a theorem of the bracket-keeping normal form plus the strip), and its STABLE leg's
// exact equality is foreclosed by any template whose raw population is both stamped and
// unstamped. It keeps REPORTING its counts below — the post-fix counts enter the record there, as
// scored evidence — while the PREFIX-IMAGE triangle (PrefixImageExitGate, this file) carries
// the exit assertions: an identity, not a threshold, ratified before each application (Eqya,
// 2026-07-30).
//
// THE VACUITY GUARD. A gate that cannot return the bad answer is not a gate.
// `CounterCanReportAnExplosion` feeds the SAME counting path a block of lines carrying a token the
// masker keeps verbatim and asserts the count tracks the line count — so a green "count stable" on
// the corpus is a fact about the corpus, not a saturated instrument.
//
// CORPUS-GATED. The Jenkins marker corpus is private and out-of-tree, so this SKIPS cleanly
// when the manifest env is unset/missing — green in CI and on every clone.
//   JENKINS_PAYLOAD_STAMP_MANIFEST  one absolute log path per line (the 19 payload-stamped logs of
//                        jenkins-markers/v2; the class definition is the frozen triage in
//                        coderoast-corpora .../scripts/t0_transport.py — one classifier, one
//                        owner, never re-invented here)
//   JENKINS_PAYLOAD_STAMP_OUT       (optional) directory for a per-line TSV dump, for the offline
//   cross-check
//
// BYTE FIDELITY. Every read is binary; lines are split on '\n' ONLY and no '\r' is trimmed
// (a CR-folding read has already fabricated a gate score in this workspace). Both arms see the
// identical bytes, so the comparison is fair by construction.
//
// Determinism: pure byte functions, no RNG, no clock, no float in any counted quantity.
#include <gtest/gtest.h>

import std;
import insight.canon; // Tokenizer / ArenaAllocator / MaskConfig / compose / template_id_of
import insight.semantic.jenkins; // kManifest (the code tier is empty since T5 5.2)

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

// ── POST-CUT: the +strip arm and the stampedness lens, re-owned ──
// JenkinsStrategy died at the identity cut; the strip is now the DECLARED catalogue row
// (`bracket-rfc3339-line-prefix`, peel-equivalence certified by G-T5-PEEL against the strip
// frozen into that gate). This harness's +strip arm therefore peels through the declared stack and
// tokenizes the peeled content — the arm delta against `run_arm` on the SAME composed view is the
// peel ALONE, which is a strictly cleaner construction than the pre-cut package-±-composition
// arms (the old premise check "an unstamped line must not move" is now structural rather than
// assumed). Stampedness comes from the raw-bytes acceptor (one owner: the shared
// rfc3339_datetime_length grammar + the row's position logic), the same predicate the catalogue
// peel applies.
[[nodiscard]] std::size_t bracket_prefix_end(std::string_view line)
{
    if (line.empty() || line.front() != '[')
        return 0U;
    const std::size_t datetime_len{insight::utils::rfc3339_datetime_length(line, 1U)};
    if (datetime_len == 0U)
        return 0U;
    const std::size_t close{1U + datetime_len};
    if (close >= line.size() || line[close] != ']')
        return 0U;
    return close + 1U; // one past the ']'
}

[[nodiscard]] const insight::transport::TransportStack& bracket_stack()
{
    static const std::array<std::string_view, 1> names{"bracket-rfc3339-line-prefix"};
    static const insight::transport::TransportStack stack{
        insight::transport::resolve_transport_stack(
            insight::transport::IngestDeclaration{.stack = names})};
    return stack;
}

[[nodiscard]] std::vector<LineOutcome>
run_declared_peel_arm(const std::vector<std::string>& lines,
                      const insight::semantic::ComposedSemantics& composed)
{
    ArenaAllocator arena{kArenaBlockBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, composed};
    constexpr std::string_view kPeelCarrier{"maskerprobe"};
    std::vector<LineOutcome> outcomes;
    outcomes.reserve(lines.size());
    for (const std::string& line : lines)
    {
        LineOutcome outcome;
        const std::size_t prefix_end{bracket_prefix_end(line)};
        if (prefix_end == 0U)
        {
            // Unstamped: the peel is the identity, and the pre-cut arm A templated the line
            // through the ORDINARY chain (any structured claim included) — identical to arm B,
            // which is P0's premise.
            if (const auto event{tokenizer.process_line(line)}; event.has_value())
            {
                outcome.produced = true;
                outcome.template_str = std::string{event->template_str};
                outcome.format = event->format;
            }
        }
        else
        {
            // Stamped: peel via the declared stack, then template CARRIER-FENCED — the pre-cut
            // +strip world's SINGLE-CLAIM semantics reconstructed: JenkinsStrategy claimed the
            // WHOLE stamped line, so its stripped content templated at the RawText grade and was
            // never re-claimed by a second strategy. Feeding the peeled payload back through
            // detection instead would let a logfmt-shaped payload be claimed by KeyValue — a
            // world neither the pre-cut chain nor any production path produces (the
            // payload-stamped class is NOT declarable as transport). MEASURED when this rework
            // first ran without the fence (2026-07-30): 45/6 416 stamped lines diverged exactly
            // that way — the corruption the M-oracle's carrier comment below pre-named.
            const insight::transport::RawPeeledLine peeled{bracket_stack().peel_raw(line)};
            if (!peeled.content.empty())
            {
                const std::string carried{std::string{kPeelCarrier} + " " +
                                          std::string{peeled.content}};
                if (const auto event{tokenizer.process_line(carried)}; event.has_value())
                {
                    const std::string_view templ{event->template_str};
                    if (templ.starts_with(kPeelCarrier) && templ.size() > kPeelCarrier.size() &&
                        templ[kPeelCarrier.size()] == ' ')
                    {
                        outcome.produced = true;
                        outcome.template_str = std::string{templ.substr(kPeelCarrier.size() + 1U)};
                        outcome.format = event->format;
                    }
                    // A fused carrier is an instrument failure; left as DECLINED here, and the
                    // exit gate's own carrier-failure counter (same fence, same bytes) reports it
                    // loudly on the M side.
                }
            }
            // blank-after-peel: DROP (bundled #4, catalogue-side) — declined, like the
            // strategy's blank branch.
        }
        outcomes.push_back(std::move(outcome));
    }
    return outcomes;
}

// Distinct-set accumulator: the two counts the decision rule asks for, kept side by side so their
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

// What the real masker does to the real token, run through the real chain — MEASURED, both ways.
//
// HISTORY, because this test is the record of an intended failure. As written at `c848c52` it was
// the CHARACTERIZATION of the bracketed-stamp literal-keep defect: the whole-token RFC3339 stamp
// fell through every rule to LITERAL KEEP (rule #1's `:digit` trigger fired but its letter-leading
// ANCHOR gate did not; `bracket_index` declined at the `-`; the digit-leading whole-token mask
// never saw the `[`-leading byte), so three same-shape stamped lines were THREE templates, each
// equal to its raw line, and the header declared: "the day the masker claims `[<RFC3339>]` to a
// stable normal form, this test goes RED and must be rewritten to the new normal form. That is the
// intended failure, not a regression." That day came with SRC-D-MSK-5 `bracket_timestamp`
// (kCanonicalizationVersion -8): the RED fired exactly as designed
// (observed 2026-07-30, all three per-line verbatim EXPECTs), and this is the rewrite it demanded.
//
// What it asserts NOW: the stamp class collapses to the `[<*>]` normal form — the unit-mechanism
// arm behind the prefix-image exit predicate (the corpus arms are PrefixImageExitGate below;
// the frozen clause-2 classifier keeps reporting in TemplateCountUnderTheStrip, unchanged). The
// bracket remains the ENTIRE difference, now in the fixed direction: the bracketed and unbracketed
// forms of the same token BOTH collapse, through different rules (bracket_timestamp → `[<*>]`,
// digit-leading mask → `<*>`).
TEST(JenkinsPayloadStampMeasurement, TheMaskerClaimsTheTimestamperTokenToTheBracketNormalForm)
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
        EXPECT_EQ(outcomes[index].template_str, "[<*>] + git fetch --tags")
            << "the bracketed RFC3339 token must mask to the bracket normal form `[<*>]` — the "
               "SRC-D-MSK-5 claim, the bracketed branch of the stamp rule discharged";
    }
    EXPECT_EQ(outcomes[0].template_str, outcomes[1].template_str)
        << "two lines differing ONLY in the stamp's milliseconds must now share ONE template — "
           "the measured collapse loss is the thing this rule repairs";
    EXPECT_EQ(outcomes[0].template_str, outcomes[2].template_str)
        << "a different day must not fork the template";

    // The mechanism, pinned AT THE MASK LAYER, both sides of the bracket. The token sits mid-line
    // in all four probes so no line's strategy routing can differ — the only difference reaching
    // the masker is the pair of brackets, and BOTH forms now collapse.
    const std::array<std::string, 2> unbracketed{"fetched at 2026-06-23T15:11:09.020Z ok",
                                                 "fetched at 2026-06-24T09:02:44.001Z ok"};
    std::vector<std::string> bare{unbracketed.begin(), unbracketed.end()};
    const auto bare_outcomes{run_arm(bare, none)};
    ASSERT_EQ(bare_outcomes.size(), unbracketed.size());
    ASSERT_TRUE(bare_outcomes[0].produced);
    ASSERT_TRUE(bare_outcomes[1].produced);
    std::cout << "  unbracketed[0] = \"" << bare_outcomes[0].template_str << "\"\n";
    EXPECT_EQ(bare_outcomes[0].template_str, bare_outcomes[1].template_str)
        << "the unbracketed token is digit-leading → masked → collapses (unchanged by SRC-D-MSK-5)";
    const std::array<std::string, 2> bracketed{"fetched at [2026-06-23T15:11:09.020Z] ok",
                                               "fetched at [2026-06-24T09:02:44.001Z] ok"};
    std::vector<std::string> kept{bracketed.begin(), bracketed.end()};
    const auto kept_outcomes{run_arm(kept, none)};
    ASSERT_TRUE(kept_outcomes[0].produced);
    ASSERT_TRUE(kept_outcomes[1].produced);
    std::cout << "  bracketed[0]   = \"" << kept_outcomes[0].template_str << "\"\n";
    EXPECT_EQ(kept_outcomes[0].template_str, kept_outcomes[1].template_str)
        << "re-bracketing the SAME token, same position, same routing, must no longer lose "
           "collapse — the bracket stopped being a difference";
    EXPECT_EQ(kept_outcomes[0].template_str, "fetched at [<*>] ok")
        << "and the bracket class marker survives in the template";
}

// The measurement. Corpus-gated: skips cleanly without the private corpus.
TEST(JenkinsPayloadStampMeasurement, TemplateCountUnderTheStrip)
{
    const auto manifest_path{env_value("JENKINS_PAYLOAD_STAMP_MANIFEST")};
    if (!manifest_path.has_value())
        FAIL() << "JENKINS_PAYLOAD_STAMP_MANIFEST unset — the payload-stamped slice is private and "
                  "out-of-tree";
    const std::vector<std::string> logs{read_manifest(*manifest_path)};
    ASSERT_FALSE(logs.empty()) << "manifest " << *manifest_path << " listed no logs";

    // POST-CUT: one composed view for both arms — the arm delta is the DECLARED peel alone (see
    // run_declared_peel_arm). The package's rows are dialect-gated recognition data and do not
    // touch templates, so composing it here would change nothing; the empty composition keeps the
    // construction visibly strategy-free.
    const insight::semantic::ComposedSemantics without_jenkins{
        insight::semantic::compose(std::span<const insight::semantic::SemanticPackageManifest>{})};

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

    const auto out_dir{env_value("JENKINS_PAYLOAD_STAMP_OUT")};

    std::cout << "\n=== payload-stamp template measurement — per-log ===\n"
              << std::format("{:>7} {:>7} {:>7} {:>7} {:>7} {:>7}  {}\n", "lines", "stamped",
                             "tmplA", "tmplB", "idA", "idB", "log");

    for (const std::string& log_path : logs)
    {
        const std::vector<std::string> lines{read_raw_lines(log_path)};
        ASSERT_FALSE(lines.empty()) << "empty or unreadable log: " << log_path;

        const auto arm_a{run_declared_peel_arm(lines, without_jenkins)};
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

            // The stamped partition comes from the SHIPPED acceptor — post-cut that is the
            // catalogue row's own grammar (bracket_prefix_end: the shared rfc3339_datetime_length
            // + the row's position logic), the exact predicate the declared peel applies.
            const bool is_stamped{bracket_prefix_end(line) != 0U};
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

    std::cout << "\n=== payload-stamp template measurement — THE FOUR NUMBERS (slice-wide) ===\n"
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
           "answer no question about the strip";

    // The counts must agree with their id counts — a divergence is a SHA-256 collision, reported.
    EXPECT_EQ(all_a.templates.size(), all_a.ids.size());
    EXPECT_EQ(all_b.templates.size(), all_b.ids.size());

    const bool stable{all_b.templates.size() == all_a.templates.size()};
    const bool explodes{all_b.templates.size() >=
                            static_cast<std::size_t>(kExplosionCountRatio *
                                                     static_cast<double>(all_a.templates.size())) ||
                        static_cast<double>(stamped_b.templates.size()) >=
                            kExplosionCeilingShare * static_cast<double>(stamped_raw.size())};
    std::cout << "FROZEN clause-2 CLASSIFIER reads: "
              << (explodes ? "EXPLODES" : (stable ? "COUNT STABLE" : "NEITHER branch"))
              << "  — the frozen record's reading, NOT the exit verdict: post-SRC-D-MSK-5 the exit "
                 "predicate is the prefix-image triangle (PrefixImageExitGate, below), and this "
                 "classifier's ceiling leg is can't-PASS on these bytes by construction\n\n";

    // This test REPORTS; it never asserts the branch. The branch is the finding.
    SUCCEED();
}

// ═══ The PREFIX-IMAGE exit gate — the TRIANGLE (repair landed at 2fe2e85) ═══
//
// Derived a priori from the normal form's structure AND the shipped chain's ENUMERATED behaviors
// (the chain's enumerated bundled list: #2 the stamp strip, #3 the greedy `[ \t]+` space strip,
// #4 the blank-line decline). Per stamped raw line `[STAMP]<sep>rest-tail`, with
// `rest := everything after the ']'` and `rest′ := strip_ws(rest)` computed by THIS HARNESS'S OWN
// frozen `[ \t]+` spelling — never by calling the strategy (an oracle that called the strategy
// would move WITH a strip regression):
//
//   (P2a)  template_B(ℓ) == "[<*>]" ⧺ M(rest)     — the masker composes over the claimed prefix
//   (P2b)  template_A(ℓ) == M(rest′)              — bundled #2 + #3, asserted against the frozen
//                                                   spelling, not trusted
//
// with the A-DECLINED CELL at the same strip boundary (rest′ empty: arm A blank-declines per
// bundled #4, arm B renders "[<*>]" ⧺ M(rest) — enumerated by P2a, never assumed one template),
// and the GLUED-STAMP cell boundary-defined (acceptor at 0, EMPTY `[ \t]+` separator run).
//
// HOW M IS REACHED, stated because the mask module is sealed (PRIVATE file set — this package
// test cannot import insight.canon.detail.mask): M(x) is obtained through the REAL chain behind a
// neutral RawText CARRIER token — template("maskerprobe " ⧺ x) minus the carrier — resting on the
// same prev-neutrality the derivation verifies at source (the single prev-consulting rule
// tests is_status_keyword, and "maskerprobe" is not one), and guarded per line: the carrier
// template must begin with the carrier token verbatim, or the line is counted as a CARRIER
// failure (INSTRUMENT arm, never silently absorbed). The carrier also fences M from strategy
// contamination: a bare `rest` re-run as its own line could be claimed by a builtin strategy that
// strips its own prefix, which would corrupt the oracle exactly on the lines that matter.
//
// Verdict partition (closed): all legs hold → REPAIRED; P1(a) > 0 → NOT REPAIRED; P0 or the
// carrier fails → INSTRUMENT; P2a/P2b/P3 fail with P1(a)=0 (incl. glued cell > 0) →
// NEW PHENOMENON, escalated, never rounded into a branch.
//
// Declared limitation: the triangle is OVER-MASKING-BLIND by construction — a leaking rule
// appears on both sides of each identity and cancels. Whoever reads a green must not credit it a
// precision it cannot see. Named holders: the decline-list unit arms (canon core) and the D11
// collateral leg. And P2 asserts CONFORMANCE to the bundled enumeration above, not the
// enumeration's wisdom — the merit of bundled #3 (a stack frame's leading tab is content by any
// reasonable reading) is held by the strategy's own unit arms and by a parked review item
// (Eqya·9), measurement-gated, never a second change in this pass.
TEST(JenkinsPayloadStampMeasurement, PrefixImageExitGate)
{
    const auto manifest_path{env_value("JENKINS_PAYLOAD_STAMP_MANIFEST")};
    if (!manifest_path.has_value())
        FAIL() << "JENKINS_PAYLOAD_STAMP_MANIFEST unset — the payload-stamped slice is private and "
                  "out-of-tree";
    const std::vector<std::string> logs{read_manifest(*manifest_path)};
    ASSERT_FALSE(logs.empty()) << "manifest " << *manifest_path << " listed no logs";

    // POST-CUT: same construction as TemplateCountUnderTheStrip — one composed view, the arm
    // delta is the DECLARED peel alone, stampedness from the row's own acceptor.
    const insight::semantic::ComposedSemantics without_jenkins{
        insight::semantic::compose(std::span<const insight::semantic::SemanticPackageManifest>{})};

    constexpr std::string_view kCarrier{"maskerprobe"};
    constexpr std::string_view kBareNormalForm{"[<*>]"};
    constexpr std::string_view kImagePrefix{"[<*>] "};

    // ── population counters (raw bytes + the shared grammar; never from either arm) ──
    std::size_t stamped_lines{0};
    std::size_t unstamped_lines{0};
    std::size_t a_declined_cell_lines{0}; // rest′ == "" at the frozen [ \t]+ boundary
    std::size_t glued_stamp_lines{0};     // acceptor at 0, EMPTY separator run, tail non-empty
    std::size_t carrier_failures{0};      // carrier template did not begin with the carrier token
    std::vector<std::string> glued_samples;
    std::vector<std::string> carrier_samples;

    // ── P0 ──
    std::size_t unstamped_lines_moved{0};
    std::vector<std::string> moved_samples;

    // ── P2 per-line violation counters ──
    std::size_t p2a_violations{0}; // template_B != "[<*>]" ⧺ M(rest)
    std::size_t p2b_violations{0}; // template_A != M(rest′), or produced/declined mismatch
    std::vector<std::string> p2a_samples;
    std::vector<std::string> p2b_samples;

    // ── P3 term accumulators (raw + masker derived) ──
    std::set<std::string> arm_a_all;
    std::set<std::string> arm_b_all;
    std::map<std::string, std::set<std::string>> rest_images_by_stripped; // M(rest′) → {M(rest)}
    std::set<std::string> cell_b_templates;   // expected B templates over the A-declined cell
    std::set<std::string> m_stamped_stripped; // M(rest′) over stamped non-declined lines
    std::set<std::string> arm_a_unstamped;    // A-templates of unstamped lines (P0 ⇒ == B's)

    for (const std::string& log_path : logs)
    {
        const std::vector<std::string> lines{read_raw_lines(log_path)};
        ASSERT_FALSE(lines.empty()) << "empty or unreadable log: " << log_path;
        const auto arm_a{run_declared_peel_arm(lines, without_jenkins)};
        const auto arm_b{run_arm(lines, without_jenkins)};
        ASSERT_EQ(arm_a.size(), arm_b.size());

        // Batch the carrier lines for this log: one M(rest) and one M(rest′) per stamped line.
        struct StampFacts
        {
            std::size_t line_index;
            std::size_t m_rest_index{SIZE_MAX};     // into carrier_outcomes
            std::size_t m_stripped_index{SIZE_MAX}; // into carrier_outcomes
            bool a_declined_cell{false};
        };
        std::vector<StampFacts> stamps;
        std::vector<std::string> carrier_lines;

        for (std::size_t index{0}; index < lines.size(); ++index)
        {
            const std::string& line{lines[index]};

            // ONE lens post-cut: the row's own acceptor (the pre-cut strategy-vs-acceptor
            // premise cell died with the strategy — there is no second instrument to disagree).
            const std::size_t prefix_end{bracket_prefix_end(line)};
            const bool is_stamped{prefix_end != 0U};

            if (!is_stamped)
            {
                if (!line.empty())
                    ++unstamped_lines;
                if (arm_a[index].produced)
                {
                    arm_a_all.insert(arm_a[index].template_str);
                    arm_a_unstamped.insert(arm_a[index].template_str);
                }
                if (arm_b[index].produced)
                    arm_b_all.insert(arm_b[index].template_str);
                const bool moved{arm_a[index].produced != arm_b[index].produced ||
                                 arm_a[index].template_str != arm_b[index].template_str};
                if (moved)
                {
                    ++unstamped_lines_moved;
                    if (moved_samples.size() < kSampleTemplatesPrinted)
                        moved_samples.push_back(log_path + ":" + std::to_string(index));
                }
                continue;
            }

            ++stamped_lines;
            if (arm_a[index].produced)
                arm_a_all.insert(arm_a[index].template_str);
            if (arm_b[index].produced)
                arm_b_all.insert(arm_b[index].template_str);

            // rest / sep / rest′ at the frozen [ \t]+ boundary — the harness's OWN spelling.
            const std::string_view rest{prefix_end != 0
                                            ? std::string_view{lines[index]}.substr(prefix_end)
                                            : std::string_view{}};
            std::size_t sep_len{0};
            while (sep_len < rest.size() && (rest[sep_len] == ' ' || rest[sep_len] == '\t'))
                ++sep_len;
            const std::string_view rest_prime{rest.substr(sep_len)};
            if (!rest.empty() && sep_len == 0)
            {
                ++glued_stamp_lines;
                if (glued_samples.size() < kSampleTemplatesPrinted)
                    glued_samples.push_back(log_path + ":" + std::to_string(index));
            }

            StampFacts facts{.line_index = index};
            facts.a_declined_cell = rest_prime.empty();
            facts.m_rest_index = carrier_lines.size();
            carrier_lines.push_back(std::string{kCarrier} + " " + std::string{rest});
            if (!facts.a_declined_cell)
            {
                facts.m_stripped_index = carrier_lines.size();
                carrier_lines.push_back(std::string{kCarrier} + " " + std::string{rest_prime});
            }
            stamps.push_back(facts);
        }

        const auto carrier_outcomes{run_arm(carrier_lines, without_jenkins)};
        ASSERT_EQ(carrier_outcomes.size(), carrier_lines.size());

        // Extract M(x) from a carrier outcome; nullopt = carrier failure (INSTRUMENT arm).
        const auto extract_m{
            [&](std::size_t carrier_index) -> std::optional<std::string>
            {
                const LineOutcome& outcome{carrier_outcomes[carrier_index]};
                if (!outcome.produced || !outcome.template_str.starts_with(kCarrier))
                    return std::nullopt;
                const std::string_view tail{
                    std::string_view{outcome.template_str}.substr(kCarrier.size())};
                if (tail.empty())
                    return std::string{}; // M(x) is empty (x was empty/space-only)
                if (!tail.starts_with(' '))
                    return std::nullopt; // carrier token fused — carrier failure
                return std::string{tail.substr(1U)};
            }};

        for (const StampFacts& facts : stamps)
        {
            const auto m_rest{extract_m(facts.m_rest_index)};
            if (!m_rest.has_value())
            {
                ++carrier_failures;
                if (carrier_samples.size() < kSampleTemplatesPrinted)
                    carrier_samples.push_back(log_path + ":" + std::to_string(facts.line_index));
                continue;
            }
            const std::string expected_b{m_rest->empty() ? std::string{kBareNormalForm}
                                                         : std::string{kImagePrefix} + *m_rest};

            // (P2a) — per line, the masker composes over the claimed prefix.
            const LineOutcome& b_outcome{arm_b[facts.line_index]};
            if (!b_outcome.produced || b_outcome.template_str != expected_b)
            {
                ++p2a_violations;
                if (p2a_samples.size() < kSampleTemplatesPrinted)
                    p2a_samples.push_back(
                        log_path + ":" + std::to_string(facts.line_index) + " B=\"" +
                        (b_outcome.produced ? b_outcome.template_str : "<declined>") +
                        "\" expected=\"" + expected_b + "\"");
            }

            if (facts.a_declined_cell)
            {
                ++a_declined_cell_lines;
                cell_b_templates.insert(expected_b);
                // (P2b, declined form) — bundled #4: arm A must have declined the line.
                if (arm_a[facts.line_index].produced)
                {
                    ++p2b_violations;
                    if (p2b_samples.size() < kSampleTemplatesPrinted)
                        p2b_samples.push_back(log_path + ":" + std::to_string(facts.line_index) +
                                              " A produced on an A-declined-cell line: \"" +
                                              arm_a[facts.line_index].template_str + "\"");
                }
                continue;
            }

            const auto m_stripped{extract_m(facts.m_stripped_index)};
            if (!m_stripped.has_value() || m_stripped->empty())
            {
                ++carrier_failures; // rest′ is non-empty here, so M(rest′) must be non-empty
                if (carrier_samples.size() < kSampleTemplatesPrinted)
                    carrier_samples.push_back(log_path + ":" + std::to_string(facts.line_index));
                continue;
            }
            m_stamped_stripped.insert(*m_stripped);
            rest_images_by_stripped[*m_stripped].insert(*m_rest);

            // (P2b) — bundled #2 + #3 (now the declared peel's), asserted against the frozen
            // spelling.
            const LineOutcome& a_outcome{arm_a[facts.line_index]};
            if (!a_outcome.produced || a_outcome.template_str != *m_stripped)
            {
                ++p2b_violations;
                if (p2b_samples.size() < kSampleTemplatesPrinted)
                    p2b_samples.push_back(
                        log_path + ":" + std::to_string(facts.line_index) + " A=\"" +
                        (a_outcome.produced ? a_outcome.template_str : "<declined>") +
                        "\" M(rest')=\"" + *m_stripped + "\"");
            }
        }
    }

    // ── P1: extinction under the spike's INDEPENDENT loose stamp class, token grain ──
    constexpr std::string_view kLooseInteriorClass{"0123456789T:.Z+-"};
    std::size_t cell_a_full_datetime_survivors{0};
    std::size_t cell_b_declined_variants{0};
    std::vector<std::string> p1a_samples;
    std::vector<std::string> p1b_samples;
    for (const std::string& template_str : arm_b_all)
    {
        std::size_t cursor{0};
        while (cursor < template_str.size())
        {
            while (cursor < template_str.size() && template_str[cursor] == ' ')
                ++cursor;
            const std::size_t start{cursor};
            while (cursor < template_str.size() && template_str[cursor] != ' ')
                ++cursor;
            const std::string_view token{template_str.data() + start, cursor - start};
            if (token.size() < 3U || token.front() != '[' || token.back() != ']')
                continue;
            const std::string_view interior{token.substr(1U, token.size() - 2U)};
            if (interior.find_first_not_of(kLooseInteriorClass) != std::string_view::npos)
                continue; // not the loose family (`[<*>]` exits here: '<' is not in class)
            if (insight::utils::rfc3339_datetime_length(token, 1U) == interior.size())
            {
                ++cell_a_full_datetime_survivors;
                if (p1a_samples.size() < kSampleTemplatesPrinted)
                    p1a_samples.push_back(std::string{token});
            }
            else
            {
                ++cell_b_declined_variants;
                if (p1b_samples.size() < kSampleTemplatesPrinted)
                    p1b_samples.push_back(std::string{token});
            }
        }
    }

    // ── P3: distinct_B == distinct_A + |dual(A)| + |refine(A)| + |cell_B| ──
    std::set<std::string> dual_a; // raw+masker stamped side ∩ A's unstamped population
    for (const std::string& template_str : m_stamped_stripped)
        if (arm_a_unstamped.contains(template_str))
            dual_a.insert(template_str);
    std::size_t refine_a{0}; // per stripped image, distinct rest-images beyond the first
    std::vector<std::string> refine_samples;
    for (const auto& [stripped, images] : rest_images_by_stripped)
    {
        refine_a += images.size() - 1U;
        if (images.size() > 1U && refine_samples.size() < kSampleTemplatesPrinted)
            refine_samples.push_back("\"" + stripped + "\" ← " + std::to_string(images.size()) +
                                     " rest-images");
    }
    const std::size_t predicted_b{arm_a_all.size() + dual_a.size() + refine_a +
                                  cell_b_templates.size()};

    // ── the report — every number with its denominator, before any assertion fires ──
    std::cout << "\n=== PREFIX-IMAGE TRIANGLE exit gate ===\n"
              << "logs                                   : " << logs.size() << "\n"
              << "stamped / unstamped lines              : " << stamped_lines << " / "
              << unstamped_lines << "\n"
              << "A-declined (ts-only) cell lines        : " << a_declined_cell_lines << "\n"
              << "glued-stamp cell (expect 0)            : " << glued_stamp_lines << "\n"
              << "carrier failures (expect 0)            : " << carrier_failures << "\n"
              << "P0 unstamped moved (expect 0)          : " << unstamped_lines_moved << "\n"
              << "P1(a) full-datetime survivors (MUST 0) : " << cell_a_full_datetime_survivors
              << "\n"
              << "P1(b) rule-declined variants (declared): " << cell_b_declined_variants << "\n"
              << "P2a per-line violations (of " << stamped_lines
              << ")          : " << p2a_violations << "\n"
              << "P2b per-line violations (of " << stamped_lines
              << ")          : " << p2b_violations << "\n"
              << "P3 |dual(A)| / |refine(A)| / |cell_B|  : " << dual_a.size() << " / " << refine_a
              << " / " << cell_b_templates.size() << "\n"
              << "P3 distinct_A + terms = predicted_B    : " << arm_a_all.size() << " + "
              << dual_a.size() << " + " << refine_a << " + " << cell_b_templates.size() << " = "
              << predicted_b << "\n"
              << "P3 distinct_B                          : " << arm_b_all.size() << "\n";
    for (const std::string& sample : p1b_samples)
        std::cout << "  P1(b) sample: " << sample << "\n";
    for (const std::string& sample : dual_a)
        std::cout << "  dual(A): \"" << sample << "\"\n";
    for (const std::string& sample : refine_samples)
        std::cout << "  refine(A): " << sample << "\n";
    for (const std::string& sample : cell_b_templates)
        std::cout << "  cell_B: \"" << sample << "\"\n";

    const bool instrument_ok{unstamped_lines_moved == 0 && carrier_failures == 0};
    const bool p1a_zero{cell_a_full_datetime_survivors == 0};
    const bool p2_holds{p2a_violations == 0 && p2b_violations == 0 && glued_stamp_lines == 0};
    const bool p3_holds{arm_b_all.size() == predicted_b};
    const char* verdict{!instrument_ok
                            ? "INSTRUMENT — the arm construction or the carrier broke; no verdict "
                              "about the masker may be read"
                        : !p1a_zero ? "NOT REPAIRED — the literal-KEEP class survives"
                        : (!p2_holds || !p3_holds)
                            ? "NEW PHENOMENON — the masker is not prefix-compositional on these "
                              "bytes, or the strategy drifted from its enumerated bundled "
                              "behaviors; escalate, never round into a branch"
                            : "REPAIRED — the masker claims the token to a stable normal form; "
                              "the T5 content move unblocks"};
    std::cout << "VERDICT: " << verdict << "\n\n";

    EXPECT_EQ(unstamped_lines_moved, 0U)
        << "P0 (INSTRUMENT arm): an unstamped line moved between arms.";
    for (const std::string& sample : moved_samples)
        ADD_FAILURE() << "  P0 moved: " << sample;
    EXPECT_EQ(carrier_failures, 0U)
        << "INSTRUMENT arm: the neutral carrier was claimed or altered — the M oracle is invalid "
           "on those lines; nothing below may be read.";
    for (const std::string& sample : carrier_samples)
        ADD_FAILURE() << "  carrier failure at " << sample;

    EXPECT_EQ(cell_a_full_datetime_survivors, 0U)
        << "P1(a) (NOT-REPAIRED arm): a bracketed FULL-DATETIME token survived into an arm-B "
           "template.";
    for (const std::string& sample : p1a_samples)
        ADD_FAILURE() << "  P1(a) survivor: " << sample;

    EXPECT_EQ(p2a_violations, 0U)
        << "P2a (NEW-PHENOMENON arm): template_B != \"[<*>]\" ⧺ M(rest) — the masker does not "
           "compose over the claimed prefix.";
    for (const std::string& sample : p2a_samples)
        ADD_FAILURE() << "  P2a: " << sample;
    EXPECT_EQ(p2b_violations, 0U)
        << "P2b (NEW-PHENOMENON arm): template_A != M(strip_ws(rest)) — the DECLARED peel drifted "
           "from the 0046 enumeration's frozen [ \\t]+ spelling.";
    for (const std::string& sample : p2b_samples)
        ADD_FAILURE() << "  P2b: " << sample;
    EXPECT_EQ(glued_stamp_lines, 0U)
        << "P2 glued-stamp cell (NEW-PHENOMENON arm): acceptor at 0 with an EMPTY separator run.";
    for (const std::string& sample : glued_samples)
        ADD_FAILURE() << "  glued stamp at " << sample;

    EXPECT_EQ(arm_b_all.size(), predicted_b)
        << "P3 (NEW-PHENOMENON arm): distinct_B != distinct_A + |dual(A)| + |refine(A)| + "
           "|cell_B| — a partition error P0/P2 did not catch.";
}
