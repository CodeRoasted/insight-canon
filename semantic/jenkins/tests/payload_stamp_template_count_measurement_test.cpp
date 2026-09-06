// refs: ADR-23.D1, ADR-23.D6, SRC-D-MSK-5
// invariant: this is the pre-registered measurement owed by the ruling that a PAYLOAD stamp is
// dialect content and never a transport envelope.
// invariant: the templates change on that class and that is settled; what is measured here is
// whether the template COUNT changes with them.
// invariant: arm A is the shipped world — the declared catalogue peel strips the stamp, then the
// tokenizer templates the peeled content.
// invariant: arm B is the stamp-stays-in-content world — the tokenizer with NO semantic package,
// so a stamped line is claimed by nobody and falls to the RawText floor with the stamp in it.
// assert: both arms run ONE empty composition, so the only difference is the peel; that premise is
// asserted rather than assumed — a line the acceptor rejects must template identically in both.
// invariant: the decision rule was fixed in the commit that PRECEDES the numbers — the commit
// order is the audit trail — and anything strictly between its two branches is a third outcome.
// invariant: the clause-2 classifier is now the FROZEN RECORD of the pre-fix measurement it
// correctly scored, not the repair's fitness predicate; the exit predicate is the triangle below.
// assert: its ceiling leg is can't-PASS on these bytes by construction, and its stable leg's exact
// equality is foreclosed by any template whose raw population is both stamped and unstamped.
// invariant: a gate that cannot return the bad answer is not a gate, so the counting path is fed a
// token the masker keeps verbatim and must track the line count.
// invariant: every read is binary, lines split on newline ONLY and no carriage return is trimmed,
// so both arms see identical bytes and the comparison is fair by construction.
// note: a CR-folding read has already fabricated a gate score in this workspace
// assert: all FOUR cases in this suite are labelled `corpus`, the two needing no mount included, so
// none of them runs in a default build — only under the corpus ctest selection.
// assert: an unset manifest variable is a hard FAIL, never a skip: a skip exits 0 and ctest counts
// a skipping case as passed.
// note: determinism: pure byte functions, no RNG, no clock, no float in any counted quantity
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.jenkins;

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

// post: raw bytes to lines, split on newline only — a carriage return stays in the line because
// it is content and not a delimiter.
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

// invariant: one arm's outcome for one line: the template it produced, or nothing when the line was
// declined because it was empty or blank after the peel.
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

// invariant: stampedness comes from the raw-bytes acceptor — the shared full-datetime grammar
// plus the row's position logic — which is the exact predicate the declared peel applies.
// refs: F-SRC-insight-canon:canon.api.cppm:rfc3339_datetime_length
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
    return close + 1U;
}

[[nodiscard]] const insight::transport::TransportStack& bracket_stack()
{
    static const std::array<std::string_view, 1> names{"bracket-rfc3339-line-prefix"};
    static const insight::transport::TransportStack stack{
        insight::transport::resolve_transport_stack(
            insight::transport::IngestDeclaration{.stack = names})};
    return stack;
}

// invariant: the +strip arm peels through the DECLARED stack and tokenizes the peeled content, so
// the arm delta against the same composed view is the peel ALONE.
// invariant: a stamped line is templated CARRIER-FENCED, reconstructing the single-claim semantics
// the deleted strategy had: its stripped content templated at the RawText grade, never re-claimed.
// assert: feeding the peeled payload back through detection instead lets a logfmt-shaped payload be
// claimed by a second strategy — 45 of 6 416 stamped lines on the rework's first unfenced run.
// invariant: a fused carrier is an INSTRUMENT failure and is left declined here; the exit gate's
// own carrier-failure counter reports it loudly on the oracle side.
// invariant: a line that peels blank is DROPPED, which is the catalogue-side successor of the
// strategy's own blank branch.
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
            if (const auto event{tokenizer.process_line(line)}; event.has_value())
            {
                outcome.produced = true;
                outcome.template_str = std::string{event->template_str};
                outcome.format = event->format;
            }
        }
        else
        {
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
                }
            }
        }
        outcomes.push_back(std::move(outcome));
    }
    return outcomes;
}

// invariant: the template count and the template-id count are kept side by side so their AGREEMENT
// is itself observable — a divergence would be a SHA-256 collision and is reported, not assumed.
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

// invariant: the instrument's positive control — with a token the masker keeps verbatim the
// distinct count must track the line count, or a `count stable` verdict is a counter artifact.
// assert: the second half is the other direction: identical lines must collapse to ONE template, so
// the counter is not merely counting lines.
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

// refs: SRC-D-MSK-5
// invariant: this test was PRE-REGISTERED to go red on the day the masker claimed the token to a
// stable normal form; that day came, and this is the rewrite it demanded.
// assert: what it asserts now is the repair — the stamp class collapses to the `[<*>]` normal
// form, and the bracket has stopped being a difference.
// assert: the bracketed and unbracketed spellings of one token both collapse, through different
// rules, and the token sits mid-line in every probe so no line's routing can differ.
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

// invariant: this case reads the private payload-stamped slice through the manifest variable, and
// an unset variable is a hard FAIL rather than a skip.
TEST(JenkinsPayloadStampMeasurement, TemplateCountUnderTheStrip)
{
    const auto manifest_path{env_value("JENKINS_PAYLOAD_STAMP_MANIFEST")};
    if (!manifest_path.has_value())
        FAIL() << "JENKINS_PAYLOAD_STAMP_MANIFEST unset — the payload-stamped slice is private and "
                  "out-of-tree";
    const std::vector<std::string> logs{read_manifest(*manifest_path)};
    ASSERT_FALSE(logs.empty()) << "manifest " << *manifest_path << " listed no logs";

    // invariant: ONE composed view serves both arms — the arm delta is the DECLARED peel alone.
    // note: the rows touch no template, so the empty composition keeps this visibly strategy-free
    const insight::semantic::ComposedSemantics without_jenkins{
        insight::semantic::compose(std::span<const insight::semantic::SemanticPackageManifest>{})};

    DistinctCounter all_a;
    DistinctCounter all_b;
    DistinctCounter stamped_a;
    DistinctCounter stamped_b;
    std::set<std::string> stamped_raw;
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

            // invariant: the stamped partition comes from the SHIPPED acceptor, never from a second
            // grammar.
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

    // assert: a line the acceptor rejects is handled identically by both arms, so a difference
    // there means they diverge by something other than the peel and no number below answers.
    EXPECT_EQ(unstamped_moved, 0U)
        << "an UNSTAMPED line templated differently with and without the Jenkins package: arm B is "
           "then measuring package removal, not the timestamper strip, and the four numbers below "
           "answer no question about the strip";

    // assert: each arm's template count must equal its template-id count — a divergence is a
    // SHA-256 collision and is reported rather than assumed away.
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

    // invariant: this case REPORTS and never asserts the branch; the branch is the finding.
    SUCCEED();
}

// invariant: the exit predicate is an IDENTITY per stamped line, not a threshold, and it is derived
// from the normal form's structure plus the chain's enumerated bundled behaviours.
// assert: with `rest` everything after the closing bracket and `rest'` its whitespace-stripped
// form, template_B is the bracket normal form followed by the mask image of `rest`.
// assert: and template_A is the mask image of `rest'` — asserted against this harness's OWN
// frozen whitespace spelling, never by calling the peel, which would move with a peel regression.
// invariant: where `rest'` is empty arm A blank-declines while arm B still renders the bare bracket
// form, and that cell is enumerated rather than assumed to be one template.
// invariant: the mask image is reached through the REAL chain behind a neutral carrier token,
// because the mask module is a sealed private file set this package test cannot import.
// assert: the carrier is guarded per line — the carrier template must begin with the carrier
// token verbatim, or the line is counted as a carrier failure and no verdict is read from it.
// invariant: the carrier also fences the oracle from strategy contamination: a bare `rest` re-run
// as its own line could be claimed by a builtin strategy that strips its own prefix.
// invariant: the verdict partition is CLOSED: all legs hold is REPAIRED, a surviving literal-keep
// is NOT REPAIRED, a carrier failure is INSTRUMENT, and the rest escalates.
// invariant: the triangle is OVER-MASKING-BLIND by construction — a leaking rule appears on both
// sides of each identity and cancels — so a green must not be credited a precision it cannot see.
// refs: F-SRC-insight-canon:test_stateless_template.cpp
// invariant: the P2 legs assert CONFORMANCE to the enumerated bundled behaviours, never the
// enumeration's wisdom, which is held by the strategy's own unit arms and a parked review item.
TEST(JenkinsPayloadStampMeasurement, PrefixImageExitGate)
{
    const auto manifest_path{env_value("JENKINS_PAYLOAD_STAMP_MANIFEST")};
    if (!manifest_path.has_value())
        FAIL() << "JENKINS_PAYLOAD_STAMP_MANIFEST unset — the payload-stamped slice is private and "
                  "out-of-tree";
    const std::vector<std::string> logs{read_manifest(*manifest_path)};
    ASSERT_FALSE(logs.empty()) << "manifest " << *manifest_path << " listed no logs";

    // invariant: same construction as the counting case above — one composed view, the arm delta
    // is the declared peel alone, and stampedness comes from the row's own acceptor.
    const insight::semantic::ComposedSemantics without_jenkins{
        insight::semantic::compose(std::span<const insight::semantic::SemanticPackageManifest>{})};

    constexpr std::string_view kCarrier{"maskerprobe"};
    constexpr std::string_view kBareNormalForm{"[<*>]"};
    constexpr std::string_view kImagePrefix{"[<*>] "};

    // invariant: the population counters come from the raw bytes and the shared grammar, never from
    // either arm, so a partition error cannot hide inside the thing it is partitioning.
    std::size_t stamped_lines{0};
    std::size_t unstamped_lines{0};
    std::size_t a_declined_cell_lines{0};
    std::size_t glued_stamp_lines{0};
    std::size_t carrier_failures{0};
    std::vector<std::string> glued_samples;
    std::vector<std::string> carrier_samples;

    std::size_t unstamped_lines_moved{0};
    std::vector<std::string> moved_samples;

    std::size_t p2a_violations{0};
    std::size_t p2b_violations{0};
    std::vector<std::string> p2a_samples;
    std::vector<std::string> p2b_samples;

    std::set<std::string> arm_a_all;
    std::set<std::string> arm_b_all;
    std::map<std::string, std::set<std::string>> rest_images_by_stripped;
    std::set<std::string> cell_b_templates;
    std::set<std::string> m_stamped_stripped;
    std::set<std::string> arm_a_unstamped;

    for (const std::string& log_path : logs)
    {
        const std::vector<std::string> lines{read_raw_lines(log_path)};
        ASSERT_FALSE(lines.empty()) << "empty or unreadable log: " << log_path;
        const auto arm_a{run_declared_peel_arm(lines, without_jenkins)};
        const auto arm_b{run_arm(lines, without_jenkins)};
        ASSERT_EQ(arm_a.size(), arm_b.size());

        // invariant: one mask image of `rest` and one of `rest'` per stamped line, batched per log.
        struct StampFacts
        {
            std::size_t line_index;
            std::size_t m_rest_index{SIZE_MAX};
            std::size_t m_stripped_index{SIZE_MAX};
            bool a_declined_cell{false};
        };
        std::vector<StampFacts> stamps;
        std::vector<std::string> carrier_lines;

        for (std::size_t index{0}; index < lines.size(); ++index)
        {
            const std::string& line{lines[index]};

            // invariant: ONE lens post-cut — the row's own acceptor. There is no second
            // instrument left to disagree with it.
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

        const auto extract_m{
            [&](std::size_t carrier_index) -> std::optional<std::string>
            {
                const LineOutcome& outcome{carrier_outcomes[carrier_index]};
                if (!outcome.produced || !outcome.template_str.starts_with(kCarrier))
                    return std::nullopt;
                const std::string_view tail{
                    std::string_view{outcome.template_str}.substr(kCarrier.size())};
                if (tail.empty())
                    return std::string{};
                if (!tail.starts_with(' '))
                    return std::nullopt;
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
                ++carrier_failures;
                if (carrier_samples.size() < kSampleTemplatesPrinted)
                    carrier_samples.push_back(log_path + ":" + std::to_string(facts.line_index));
                continue;
            }
            m_stamped_stripped.insert(*m_stripped);
            rest_images_by_stripped[*m_stripped].insert(*m_rest);

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

    // invariant: extinction is measured on the spike's INDEPENDENT loose stamp class at token
    // grain, so the bracket normal form itself exits the family and is never counted as a survivor.
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
                continue;
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

    // assert: the identity that closes the account: the arm-B distinct count equals arm A's plus
    // the dual cell, plus the per-image refinements, plus the blank-decline cell.
    // note: a residue here is a partition error the per-line legs did not catch
    std::set<std::string> dual_a;
    for (const std::string& template_str : m_stamped_stripped)
        if (arm_a_unstamped.contains(template_str))
            dual_a.insert(template_str);
    std::size_t refine_a{0};
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
