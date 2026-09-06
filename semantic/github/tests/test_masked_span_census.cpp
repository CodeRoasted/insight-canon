// refs: DN-38.D1, DN-38.D3
// invariant: this file measures HALF ONE — the rendered BYTES, a pure function of the NAMES. The
// row COUNT needs the engine and is measured outside this repo.
// note: half two is the `gd_gate1_over_merge` harness in `coderoast-corpora`, over the same pairs
// invariant: the population needs the GitHub-Actions marker vocabulary and the function is canon's,
// so both live in this repo: one build, no seam.
// assert: `recognize()` is driven rather than the banner grepped — a second rig over one corpus
// yields two honest numbers that read as a contradiction.
// note: the acquirer's DECLARED display names are a second surface and are NOT censused here
// invariant: no RNG, no thread, no wall clock, no float — the subject is a committed sorted TSV
// and every printed set is sorted, so the output is a pure function of the banked bytes.
// note: the whole suite is LABELLED `corpus` and is excluded from the default `malf test` run
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::canonicalize_intent;
using insight::discriminant_of;
using insight::semantic::ComposedSemantics;
using insight::semantic::ResolvedStream;
using insight::tokenization::IntentMarker;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;
using insight::transport::IngestDeclaration;
using insight::transport::PeeledLine;

namespace
{

// invariant: a RESTATEMENT of canon's trim set, never a fork — `TrimIsCanonInvariant` makes a
// divergence fail loudly instead of quietly renaming what the producer's name means.
[[nodiscard]] std::string_view trimmed(std::string_view name)
{
    const auto is_trim{[](char byte) { return byte == ' ' || byte == '\t' || byte == '\r'; }};
    while (!name.empty() && is_trim(name.front()))
        name.remove_prefix(1);
    while (!name.empty() && is_trim(name.back()))
        name.remove_suffix(1);
    return name;
}

// note: how many producer names already contain the separator is READABILITY, not correctness
constexpr std::string_view kPhaseSeparator{" ▸ "};

// invariant: a reporting cut, never a limit: it exists so the long tail is ENUMERATED, since one
// 300-byte outlier and two hundred of them are one maximum and a different decision.
constexpr std::size_t kLongName{100};

// invariant: each flag is a property of the RENDERED bytes, so a hazard the CLASS already carries
// is on the wire today and is reported beside it rather than charged to the ruling.
struct DisplayRisk
{
    bool non_ascii{false};
    bool interior_control{false};
    bool carries_separator{false};
    bool carries_quote{false};

    [[nodiscard]] bool any() const noexcept
    {
        return non_ascii || interior_control || carries_separator || carries_quote;
    }
};

[[nodiscard]] DisplayRisk risk_of(std::string_view rendered)
{
    DisplayRisk out;
    for (const char byte : rendered)
    {
        const auto raw{static_cast<unsigned char>(byte)};
        if (raw >= 0x80U)
            out.non_ascii = true;
        if (raw < 0x20U || raw == 0x7FU)
            out.interior_control = true;
        if (raw == static_cast<unsigned char>('"'))
            out.carries_quote = true;
    }
    out.carries_separator = rendered.find(kPhaseSeparator) != std::string_view::npos;
    return out;
}

[[nodiscard]] std::string show_risk(const DisplayRisk& risk)
{
    std::string out;
    const auto add{[&out](bool flag, std::string_view label)
                   {
                       if (flag)
                           out += (out.empty() ? "" : "+") + std::string{label};
                   }};
    add(risk.non_ascii, "non-ascii");
    add(risk.interior_control, "interior-control");
    add(risk.carries_separator, "separator");
    add(risk.carries_quote, "quote");
    return out.empty() ? "clean" : out;
}

// post: every byte outside printable ASCII is escaped, so a control byte in a payload cannot
// corrupt the census output it is reported in.
[[nodiscard]] std::string escaped(std::string_view text)
{
    static constexpr std::string_view kHex{"0123456789abcdef"};
    std::string out;
    for (const char byte : text)
    {
        const auto raw{static_cast<unsigned char>(byte)};
        if (raw >= 0x20U && raw < 0x7FU)
        {
            out.push_back(byte);
            continue;
        }
        out += "\\x";
        out.push_back(kHex[raw >> 4U]);
        out.push_back(kHex[raw & 0x0FU]);
    }
    return out;
}

// invariant: names are counted BOTH ways — DISTINCT is the population a rule applies to and
// OCCURRENCE is how often a reader meets it, and neither can be spent unlabelled.
struct Tally
{
    std::map<std::string, std::size_t> occurrences_by_name;
    std::set<std::string> names;
    std::size_t recognized{0};

    void add(const std::string& name)
    {
        ++occurrences_by_name[name];
        names.insert(name);
        ++recognized;
    }
};

struct Bucketed
{
    std::size_t names_moved{0};
    std::size_t names_unmoved{0};
    std::size_t occ_moved{0};
    std::size_t occ_unmoved{0};
    std::size_t longest_name{0};
    std::size_t longest_class{0};
    std::size_t widest_growth{0};
    std::size_t names_over_cut{0};
    std::size_t names_at_risk{0};
    // refs: MEM:prescriptive-instrument-false-positive-costs-truth
    // invariant: the number a ruling is taken on is `names_risk_introduced`, never `names_at_risk`:
    // charging a pre-existing hazard to the ruling makes the count say the opposite.
    std::size_t names_risk_introduced{0};
};

[[nodiscard]] bool risk_introduced(std::string_view name)
{
    return risk_of(trimmed(name)).any() && !risk_of(canonicalize_intent(name)).any();
}

[[nodiscard]] Bucketed bucket(const Tally& tally)
{
    Bucketed out;
    for (const std::string& name : tally.names)
    {
        const std::size_t occ{tally.occurrences_by_name.at(name)};
        const std::string_view rendered{trimmed(name)};
        const std::string clazz{canonicalize_intent(name)};
        if (rendered != clazz)
        {
            ++out.names_moved;
            out.occ_moved += occ;
        }
        else
        {
            ++out.names_unmoved;
            out.occ_unmoved += occ;
        }
        out.longest_name = std::max(out.longest_name, rendered.size());
        out.longest_class = std::max(out.longest_class, clazz.size());
        if (rendered.size() > clazz.size())
            out.widest_growth = std::max(out.widest_growth, rendered.size() - clazz.size());
        out.names_over_cut += static_cast<std::size_t>(rendered.size() > kLongName);
        out.names_at_risk += static_cast<std::size_t>(risk_of(rendered).any());
        out.names_risk_introduced += static_cast<std::size_t>(risk_introduced(name));
    }
    return out;
}

[[nodiscard]] std::string show(const Bucketed& b, const Tally& t)
{
    std::ostringstream out;
    out << "recognized=" << t.recognized << " distinct=" << t.names.size()
        << " | headline MOVES: " << b.names_moved << " names / " << b.occ_moved << " occ"
        << " | unchanged: " << b.names_unmoved << " / " << b.occ_unmoved
        << " | longest rendered name " << b.longest_name << "B (class today " << b.longest_class
        << "B, widest growth +" << b.widest_growth << "B)"
        << " | over the " << kLongName << "B cut: " << b.names_over_cut
        << " | at display risk: " << b.names_at_risk
        << " (of which INTRODUCED by clause 1: " << b.names_risk_introduced
        << "; the rest are hazards the class already carries and the wire already ships)";
    return out.str();
}

[[nodiscard]] std::optional<std::string> env(const char* key)
{
    if (const char* value = std::getenv(key); value != nullptr && *value != '\0')
        return std::string{value};
    return std::nullopt;
}

constexpr std::string_view kUnmounted{
    "CODEROAST_MARKER_COVERAGE_LOGS / CODEROAST_MARKER_COVERAGE_SUBJECT unset or missing — the "
    "banked GitHub run logs are "
    "third-party derivatives that stay outside every checkout, so this census runs only where the "
    "corpus is mounted."};

struct SubjectRow
{
    std::string pair;
    std::string side;
    std::string run_id;
    std::string repo;
    std::string workflow;
    std::string file;
};

// invariant: the TSV is read verbatim in file order and a short row is FATAL, because a subject
// that silently shrinks is the failure this whole file exists not to report.
[[nodiscard]] std::vector<SubjectRow> read_subject(const std::filesystem::path& path,
                                                   std::string& error)
{
    std::ifstream in{path};
    std::vector<SubjectRow> rows;
    if (!in)
    {
        error = "cannot open subject list " + path.string();
        return rows;
    }
    std::string line;
    std::size_t number{0};
    while (std::getline(in, line))
    {
        ++number;
        if (number == 1 && line.starts_with("pair\t"))
            continue;
        if (line.empty())
            continue;
        std::vector<std::string> cells;
        for (std::size_t at{0}; at <= line.size();)
        {
            const std::size_t tab{line.find('\t', at)};
            const std::size_t end{tab == std::string::npos ? line.size() : tab};
            cells.push_back(line.substr(at, end - at));
            at = end + 1;
            if (tab == std::string::npos)
                break;
        }
        if (cells.size() != 6)
        {
            error = "subject line " + std::to_string(number) + " has " +
                    std::to_string(cells.size()) + " columns, expected 6: \"" + line + "\"";
            return {};
        }
        rows.push_back(SubjectRow{.pair = cells[0],
                                  .side = cells[1],
                                  .run_id = cells[2],
                                  .repo = cells[3],
                                  .workflow = cells[4],
                                  .file = cells[5]});
    }
    return rows;
}

[[nodiscard]] std::string kind_name(IntentMarkerKind kind)
{
    switch (kind)
    {
    case IntentMarkerKind::Job:
        return "Job";
    case IntentMarkerKind::Step:
        return "Step";
    case IntentMarkerKind::None:
        return "None";
    }
    return "?";
}

} // namespace

// refs: MEM:synthetic-gate-vacuity-vs-judgment
// invariant: both detectors are exercised in BOTH directions here — a case that must fire and a
// case that must not — because a census whose detectors cannot fire reports meaningless zeros.
TEST(MaskedSpanCensus, TheRenderDeltaPredicateAndTheRiskDetectorBothFireAndBothStaySilent)
{
    struct DeltaCase
    {
        std::string_view name;
        bool moves;
        std::string_view why;
    };
    static constexpr std::array<DeltaCase, 6> kDeltaCases{{
        {"macos-14 (15.3)", true, "a runner-matrix cell: two masked spans, both rewritten"},
        {"ESLint v6", true, "one masked span, rewritten"},
        {"Lint", false, "no masked span at all — nothing to rewrite"},
        {"Build", false, "no masked span at all — nothing to rewrite"},
        {"Test (M)", false, "carries a masked span whose mask reproduces its own bytes"},
        {"  Lint\t", false, "differs from its class only by the trim, which clause 1 also applies"},
    }};
    for (const auto& [name, moves, why] : kDeltaCases)
    {
        const bool observed{trimmed(name) != canonicalize_intent(name)};
        EXPECT_EQ(observed, moves)
            << "the render-delta predicate answered " << (observed ? "MOVES" : "unchanged")
            << " for \"" << name << "\" and the case says " << (moves ? "MOVES" : "unchanged")
            << " — " << why << ".\n  trimmed name : \"" << escaped(trimmed(name))
            << "\"\n  class today  : \"" << escaped(canonicalize_intent(name)) << "\"";
    }
    EXPECT_FALSE(discriminant_of("Test (M)").empty())
        << "\"Test (M)\" no longer carries a masked span, so it is no longer the case that "
           "separates the render-delta predicate from a span count, and the comment above it is "
           "false.";

    EXPECT_FALSE(risk_of("Build and test (ubuntu-latest)").any())
        << "the risk detector fires on an ordinary ASCII job name, so every at-risk count below "
           "is noise.";
    EXPECT_TRUE(risk_of("Tests \xE2\x9C\x93").non_ascii) << "the non-ASCII flag does not fire";
    EXPECT_TRUE(risk_of("build\rtest").interior_control)
        << "the interior-control flag does not fire on a CR between two words — and an end-trimmed "
           "CR is NOT this case: canon's trim set removes those, this flag is for the ones it "
           "cannot reach.";
    EXPECT_FALSE(risk_of(trimmed("build \r")).interior_control)
        << "the interior-control flag fires on a CR the trim already removed, so it would charge "
           "`DN-38.D1` clause 1 for a byte clause 1 never renders.";
    EXPECT_TRUE(
        risk_of(std::string{"build"} + std::string{kPhaseSeparator} + "test").carries_separator)
        << "the separator flag does not fire";
    EXPECT_TRUE(risk_of("run \"the suite\"").carries_quote) << "the quote flag does not fire";

    EXPECT_FALSE(risk_introduced("run \"the suite\""))
        << "a double quote that survives masking is on the wire today — the class is \""
        << canonicalize_intent("run \"the suite\"")
        << "\" — so clause 1 introduces nothing and must not be charged for it.";
    EXPECT_TRUE(risk_introduced("deploy (\"eu-west-1\")"))
        << "a quote that the paren mask REMOVES from the class — class \""
        << canonicalize_intent("deploy (\"eu-west-1\")")
        << "\" — is a hazard clause 1 puts back in front of a reader, and the split must see it.";
}

TEST(MaskedSpanCensus, TrimIsCanonInvariant)
{
    static constexpr std::array<std::string_view, 4> kPadded{
        {"  macos-14 (15.3)", "macos-14 (15.3)\r", "\tESLint v6 ", "  Lint\t\r"}};
    for (const std::string_view padded : kPadded)
    {
        const std::string_view body{trimmed(padded)};
        EXPECT_EQ(canonicalize_intent(padded), canonicalize_intent(body))
            << "canon's trim set and this file's disagree on \"" << escaped(padded) << "\"";
        EXPECT_EQ(discriminant_of(padded), discriminant_of(body))
            << "canon's trim set and this file's disagree on \"" << escaped(padded) << "\"";
    }
}

TEST(MaskedSpanCensus, TheProducerNameRenderDeltaOnTheMarkerCoverageBank)
{
    const std::optional<std::string> logs{env("CODEROAST_MARKER_COVERAGE_LOGS")};
    const std::optional<std::string> subject_path{env("CODEROAST_MARKER_COVERAGE_SUBJECT")};
    if (!logs || !subject_path || !std::filesystem::exists(*logs) ||
        !std::filesystem::exists(*subject_path))
        FAIL() << kUnmounted;

    std::string subject_error;
    const std::vector<SubjectRow> subject{read_subject(*subject_path, subject_error)};
    ASSERT_TRUE(subject_error.empty()) << subject_error;
    ASSERT_FALSE(subject.empty()) << "the subject list is empty — the census measured nothing.";

    // invariant: the declaration is the one the 1 000-situation replay passes, spelled out rather
    // than deduced: deduction is content-sensitive and content is exactly what a pair varies.
    static constexpr std::array<std::string_view, 1> kStack{{"api-rfc3339-line-prefix"}};
    const ComposedSemantics composed{
        insight::semantic::compose(std::array{insight::semantic::github::kManifest})};
    const ResolvedStream stream{insight::semantic::resolve_stream(
        composed, IngestDeclaration{.stack = kStack,
                                    .dialect = insight::semantic::github::kDialect,
                                    .channel = insight::semantic::github::kChannelAnnotated})};

    std::map<std::string, std::map<std::string, Tally>> tallies;
    std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::string>>>>
        payloads;
    std::size_t files_read{0};
    std::size_t lines_total{0};
    std::size_t lines_blank_after_peel{0};
    std::size_t lines_recognized{0};
    std::size_t lines_other{0};
    std::vector<std::string> missing;
    std::string scratch;

    for (const SubjectRow& row : subject)
    {
        const std::filesystem::path path{std::filesystem::path{*logs} / row.file};
        std::ifstream in{path, std::ios::binary};
        if (!in)
        {
            missing.push_back(row.file);
            continue;
        }
        ++files_read;
        std::string line;
        while (std::getline(in, line))
        {
            ++lines_total;
            // invariant: the shipped order — canon's stage-1 normalize, then the DECLARED `peel`,
            // never the tokenizer's `peel_raw`, which is a different seam.
            const insight::tokenization::NormalizedLine normalized{
                insight::tokenization::normalize(line, scratch)};
            const PeeledLine peeled{stream.transport.peel(normalized)};
            if (peeled.is_blank())
            {
                ++lines_blank_after_peel;
                continue;
            }
            const IntentMarker marker{recognize(peeled.content, stream.semantics)};
            if (marker.kind == IntentMarkerKind::None)
            {
                ++lines_other;
                continue;
            }
            ++lines_recognized;
            const std::string payload{marker.name};
            tallies[row.side][kind_name(marker.kind)].add(payload);
            // note: a corpus-wide count is a ceiling; a report is ONE pair
            payloads[row.pair][row.side][kind_name(marker.kind)].insert(payload);
        }
    }

    ASSERT_TRUE(missing.empty()) << missing.size() << " subject member(s) absent under " << *logs
                                 << " — the census read a smaller corpus than it claims. First: "
                                 << (missing.empty() ? std::string{} : missing.front());

    // assert: line classification is TOTAL, so the denominator is the file's lines and never the
    // lines the reader happened to understand.
    ASSERT_EQ(lines_blank_after_peel + lines_recognized + lines_other, lines_total)
        << "line classification is not total: " << lines_blank_after_peel << " + "
        << lines_recognized << " + " << lines_other << " != " << lines_total;
    ASSERT_FALSE(tallies.empty()) << "no marker was recognized in " << files_read
                                  << " file(s) — the census walked the corpus and saw no name, so "
                                     "every zero below is about the reader, not about the bytes.";

    std::cout << "\n[DN-38 gate 2 / half one] subject " << subject.size() << " side-records, "
              << files_read << " read | lines " << lines_total << " (peeled-blank "
              << lines_blank_after_peel << ", marker " << lines_recognized << ", other "
              << lines_other << ")" << std::endl;

    std::map<std::string, std::map<std::string, std::string>> widest;
    for (const auto& [side, by_kind] : tallies)
        for (const auto& [kind, tally] : by_kind)
        {
            const Bucketed b{bucket(tally)};
            std::cout << "[DN-38 gate 2] side=" << side << " surface=marker/" << kind << "  "
                      << show(b, tally) << std::endl;
            // invariant: EVERY member is printed, never a head — an enumeration truncated for
            // readability is how a complete enumeration gets read as a complete disposition.
            for (const std::string& name : tally.names)
            {
                const std::string_view rendered{trimmed(name)};
                if (rendered.size() > widest[side][kind].size())
                    widest[side][kind] = std::string{rendered};
                const DisplayRisk risk{risk_of(rendered)};
                if (risk.any())
                    std::cout << "        RISK  "
                              << (risk_introduced(name) ? "INTRODUCED " : "pre-existing ")
                              << show_risk(risk) << "  \"" << escaped(rendered) << "\"  class=\""
                              << escaped(canonicalize_intent(name)) << "\"  x"
                              << tally.occurrences_by_name.at(name) << std::endl;
                if (rendered.size() > kLongName)
                    std::cout << "        LONG  " << rendered.size() << "B  \"" << escaped(rendered)
                              << "\"  (class " << canonicalize_intent(name).size() << "B)  x"
                              << tally.occurrences_by_name.at(name) << std::endl;
            }
        }

    // invariant: which job pairs with which step is the ENGINE's join, so the widest job beside the
    // widest step is a BOUND that no observed row need reach.
    for (const auto& [side, by_kind] : widest)
    {
        const auto job{by_kind.find("Job")};
        const auto step{by_kind.find("Step")};
        const std::size_t job_len{job == by_kind.end() ? 0 : job->second.size()};
        const std::size_t step_len{step == by_kind.end() ? 0 : step->second.size()};
        std::cout << "[DN-38 gate 2] side=" << side << " widest JOINED headline BOUND = " << job_len
                  << " + " << kPhaseSeparator.size() << " + " << step_len << " = "
                  << job_len + kPhaseSeparator.size() + step_len << "B"
                  << "\n        widest Job  \"" << escaped(job == by_kind.end() ? "" : job->second)
                  << "\"\n        widest Step \""
                  << escaped(step == by_kind.end() ? "" : step->second)
                  << "\"\n        ⚠ a BOUND: this job and this step need not sit in one row."
                  << std::endl;
    }

    // invariant: outside the MOVED set no row's `phase` can change a byte, so its COMPLEMENT is the
    // prediction a pinned re-run of the ruling's slice must meet.
    std::map<std::string, std::set<std::string>> moved;
    std::map<std::string, std::set<std::string>> at_risk;
    for (const auto& [pair, by_side] : payloads)
        for (const auto& [side, by_kind] : by_side)
            for (const auto& [kind, names] : by_kind)
                for (const std::string& name : names)
                {
                    const std::string_view rendered{trimmed(name)};
                    if (rendered != canonicalize_intent(name))
                        moved[pair].insert(kind);
                    if (risk_introduced(name))
                        at_risk[pair].insert(kind + ": \"" + escaped(rendered) + "\"");
                }

    const auto enumerate{
        [](std::string_view label, const std::map<std::string, std::set<std::string>>& set)
        {
            std::cout << "[DN-38 gate 2] " << label << ": " << set.size() << " pair(s)"
                      << std::endl;
            for (const auto& [pair, what] : set)
            {
                std::cout << "        pair " << pair << " —";
                for (const std::string& item : what)
                    std::cout << " [" << item << "]";
                std::cout << std::endl;
            }
        }};
    std::cout << "[DN-38 gate 2] MOVED (the pair carries >=1 payload whose rendered headline "
                 "changes under `DN-38.D1` clause 1; every OTHER report keeps every `phase` byte): "
              << moved.size()
              << " pair(s) — not enumerated, it is the majority set; its "
                 "COMPLEMENT is the prediction and it is "
              << (payloads.size() - moved.size()) << " pair(s)." << std::endl;
    enumerate("DISPLAY HAZARD INTRODUCED BY CLAUSE 1 (the rendered name carries a non-ASCII byte, "
              "an interior control byte, the phase separator or a double quote AND its class does "
              "not — the set the ruling is answerable for)",
              at_risk);
}
