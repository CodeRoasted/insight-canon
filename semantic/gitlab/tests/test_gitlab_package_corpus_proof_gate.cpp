// refs: ADR-8, ADR-17, STU-12
// invariant: the MARKER axis claims FIDELITY OF TRANSCRIPTION, never independent validation: the
// rows were written by reading the frozen instrument, so oracle and SUT share an intent.
// invariant: the OUTCOME axis claims RECOVERY of GitLab's own REST `job_status` from console bytes
// alone — a producer this codebase never authored; stronger, still not external validity.
// invariant: it does NOT re-earn, widen or re-open the depth claim, and says nothing about
// prevalence beyond the corpus's declared residuals.
// assert: three axes, never a bare "modern leg": the BANNER leg is READ from the committed sidecar
// and never re-derived here.
// invariant: the STAMPED axis is derived IN-BAND by the shipped strategy and cross-checked against
// the sidecar's own column; ran-job is exogenous and counted, never scored.
// invariant: no reported figure may span two legs, and every one carries its leg in the assertion's
// own name.
// assert: the SUT is the shipped symbols — `make_strategy`, `recognize`, `scan_run_outcome`,
// `resolve_run_outcome`, `map_outcome_token` — with stage 1 the public `normalize`.
// assert: red-capability was OBSERVED 2026-07-29 and both mutations reverted — disabling the
// transport peel zeroes every stamped-trace marker cell (482 per-trace disagreements).
// assert: reverting the outcome scan to a `\n`-only anchor collapses the banner-old verdicts to the
// RECORDED pre-anchor engine, so the gate reproduces the historical defect exactly.
// note: determinism: committed-order population, integer counts, no RNG, clock, float, thread
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.gitlab;

using insight::map_outcome_token;
using insight::resolve_run_outcome;
using insight::RunOutcome;
using insight::RunOutcomeScan;
using insight::scan_run_outcome;
using insight::semantic::ComposedSemantics;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

namespace
{

// refs: ADR-8
// invariant: the sidecar is the COMMITTED projection of the manifest's 627 trace rows and the delta
// file is the standing harness's per-trace emission; this binary cannot see the JSON.
// assert: that the sidecar equals projection(manifest) is the CORPORA repo's governance, and the
// delta file's eidos pre/post columns belong to the two-path gate in `insight_sift_tests`.
// note: a `const char*`, not a string_view: it is handed to getenv, which needs the terminator
constexpr const char* kCorpusVar{"CORPUS_GITLAB_MARKERS_DIR"};
constexpr std::string_view kSidecarFile{"PROBE-v1.trace-sidecar.tsv"};
constexpr std::string_view kOracleFile{"PROBE-v1.recognition-delta.tsv"};
constexpr std::string_view kBytesRoot{"data/v1"};

// invariant: the identifiers name the BANNER axis and the strings are the sidecar's own, verbatim
// from the frozen classifier through the generator.
constexpr std::string_view kLegBannerModern{"modern"};
constexpr std::string_view kLegBannerOld{"old"};
constexpr std::string_view kLegVoid{"void"};
constexpr std::string_view kLegUnclassified{"unclassified"};

constexpr std::size_t kMaxReportedRows{10};

// invariant: CORROBORATED = a recorded closed measurement reproduced independently;
// CHARACTERIZATION = measured HERE first and pinned so it cannot move silently.
// note: a characterization guards regression; it confirms no prediction
constexpr std::size_t kTraceRows{627};
constexpr std::size_t kBannerModernTraces{445};
constexpr std::size_t kBannerOldTraces{174};
constexpr std::size_t kVoidTraces{8};
constexpr std::size_t kUnclassifiedTraces{0};
constexpr std::size_t kApiSuccess{458};
constexpr std::size_t kApiFailed{144};
constexpr std::size_t kApiCanceled{25};
// assert: stamped and banner are TWO axes and only their crossing is a cell — 482 = 445
// banner-modern + 37 banner-old contaminants, scored and never averaged in.
// note: the mixed rate is never asserted: 7.7 % of stamped traces carry 17.7 % of the loss
constexpr std::size_t kStampedTraces{482};
constexpr std::size_t kStampedBannerModern{445};
constexpr std::size_t kStampedBannerOld{37};
constexpr std::size_t kMarkersBannerModern{2963};
constexpr std::size_t kMarkersStampedBannerOld{230};
constexpr std::size_t kMarkersUnstamped{294};
constexpr std::size_t kMarkersStampedAxisTotal{3193};
constexpr std::size_t kStudyStartsBannerModern{3001};
constexpr std::size_t kStudyStartsStampedAxis{3231};
constexpr std::size_t kStudyStartsUnstamped{1054};
struct OutcomeLegPins
{
    std::size_t success;
    std::size_t failure;
    std::size_t aborted;
    std::size_t none;
};
constexpr OutcomeLegPins kOutcomeBannerModern{
    .success = 329, .failure = 93, .aborted = 11, .none = 12};
constexpr OutcomeLegPins kOutcomeBannerOld{.success = 111, .failure = 52, .aborted = 6, .none = 5};
constexpr std::size_t kAbortedConsoleCeiling{17};
constexpr std::size_t kRecoveredTraces{602};
constexpr std::size_t kAgreements{599};
constexpr std::size_t kDisagreements{3};
constexpr std::size_t kDisagreementsConsoleSuccess{2};
constexpr std::size_t kDisagreementsConsoleFailure{1};
// invariant: pinned ZERO because all 60 malformed `section_start:` occurrences, across 21 traces,
// ride the runner's SGR-wrapped command echo and are rejected by POSITION first.
// note: a nonzero value is a new producer shape at a line start: establish which before a re-pin
constexpr std::size_t kSectionShapedDeclined{0};

constexpr std::array<std::uint32_t, 64> kSha256RoundConstants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

[[nodiscard]] constexpr std::uint32_t rotate_right(std::uint32_t value,
                                                   std::uint32_t count) noexcept
{
    return (value >> count) | (value << (32U - count));
}

[[nodiscard]] std::string sha256_hex(std::string_view bytes)
{
    std::array<std::uint32_t, 8> digest{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    const std::uint64_t bit_length{static_cast<std::uint64_t>(bytes.size()) * 8U};
    std::string padded{bytes};
    padded.push_back(static_cast<char>(0x80));
    while (padded.size() % 64U != 56U)
        padded.push_back('\0');
    for (int shift{56}; shift >= 0; shift -= 8)
        padded.push_back(static_cast<char>((bit_length >> static_cast<unsigned>(shift)) & 0xFFU));

    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t block{0}; block < padded.size(); block += 64U)
    {
        for (std::size_t word{0}; word < 16U; ++word)
            schedule[word] =
                (static_cast<std::uint32_t>(static_cast<unsigned char>(padded[block + 4U * word]))
                 << 24U) |
                (static_cast<std::uint32_t>(
                     static_cast<unsigned char>(padded[block + 4U * word + 1U]))
                 << 16U) |
                (static_cast<std::uint32_t>(
                     static_cast<unsigned char>(padded[block + 4U * word + 2U]))
                 << 8U) |
                static_cast<std::uint32_t>(
                    static_cast<unsigned char>(padded[block + 4U * word + 3U]));
        for (std::size_t word{16}; word < 64U; ++word)
        {
            const std::uint32_t sigma0{rotate_right(schedule[word - 15U], 7U) ^
                                       rotate_right(schedule[word - 15U], 18U) ^
                                       (schedule[word - 15U] >> 3U)};
            const std::uint32_t sigma1{rotate_right(schedule[word - 2U], 17U) ^
                                       rotate_right(schedule[word - 2U], 19U) ^
                                       (schedule[word - 2U] >> 10U)};
            schedule[word] = schedule[word - 16U] + sigma0 + schedule[word - 7U] + sigma1;
        }
        auto [wa, wb, wc, wd, we, wf, wg, wh] = digest;
        for (std::size_t round{0}; round < 64U; ++round)
        {
            const std::uint32_t big_sigma1{rotate_right(we, 6U) ^ rotate_right(we, 11U) ^
                                           rotate_right(we, 25U)};
            const std::uint32_t choose{(we & wf) ^ (~we & wg)};
            const std::uint32_t temp1{wh + big_sigma1 + choose + kSha256RoundConstants[round] +
                                      schedule[round]};
            const std::uint32_t big_sigma0{rotate_right(wa, 2U) ^ rotate_right(wa, 13U) ^
                                           rotate_right(wa, 22U)};
            const std::uint32_t majority{(wa & wb) ^ (wa & wc) ^ (wb & wc)};
            const std::uint32_t temp2{big_sigma0 + majority};
            wh = wg;
            wg = wf;
            wf = we;
            we = wd + temp1;
            wd = wc;
            wc = wb;
            wb = wa;
            wa = temp1 + temp2;
        }
        digest[0] += wa;
        digest[1] += wb;
        digest[2] += wc;
        digest[3] += wd;
        digest[4] += we;
        digest[5] += wf;
        digest[6] += wg;
        digest[7] += wh;
    }

    constexpr std::string_view kHexDigits{"0123456789abcdef"};
    std::string out;
    out.reserve(64U);
    for (const std::uint32_t word : digest)
        for (int shift{28}; shift >= 0; shift -= 4)
            out.push_back(kHexDigits[(word >> static_cast<unsigned>(shift)) & 0xFU]);
    return out;
}

// invariant: the TSV readers index POSITIONALLY against a frozen header, so any column drift is an
// integrity error rather than a guess.
struct SidecarRow
{
    std::string path;
    std::string sha256;
    std::uint64_t bytes{0};
    std::uint64_t job_id{0};
    std::string job_status;
    std::string leg;
};

struct OracleRow
{
    std::string leg;
    bool stamped{false};
    std::uint64_t study_starts{0};
    std::uint64_t markers_canon{0};
};

[[nodiscard]] std::vector<std::string_view> split_tabs(std::string_view line)
{
    std::vector<std::string_view> fields;
    std::size_t begin{0};
    while (true)
    {
        const std::size_t tab{line.find('\t', begin)};
        fields.push_back(line.substr(begin, tab - begin));
        if (tab == std::string_view::npos)
            return fields;
        begin = tab + 1U;
    }
}

[[nodiscard]] std::uint64_t parse_count(std::string_view field, bool& parse_ok)
{
    std::uint64_t value{0};
    const auto [ptr, err]{std::from_chars(field.data(), field.data() + field.size(), value)};
    if (err != std::errc{} || ptr != field.data() + field.size())
        parse_ok = false;
    return value;
}

struct TraceResult
{
    bool stamped{false};
    std::size_t markers{0};
    std::size_t section_declined{0};
    bool outcome_recovered{false};
    RunOutcome outcome{RunOutcome::Unknown};
};

[[nodiscard]] const char* outcome_name(RunOutcome outcome)
{
    switch (outcome)
    {
    case RunOutcome::Success:
        return "Success";
    case RunOutcome::Failure:
        return "Failure";
    case RunOutcome::Aborted:
        return "Aborted";
    case RunOutcome::Unstable:
        return "Unstable";
    case RunOutcome::Unknown:
        return "Unknown";
    }
    return "?";
}

struct OutcomeLegCells
{
    std::size_t success{0};
    std::size_t failure{0};
    std::size_t aborted{0};
    std::size_t none{0};
};

struct CorpusScore
{
    std::size_t rows{0};
    std::size_t banner_modern{0};
    std::size_t banner_old{0};
    std::size_t void_rows{0};
    std::size_t unclassified{0};
    std::size_t api_success{0};
    std::size_t api_failed{0};
    std::size_t api_canceled{0};
    std::vector<std::string> integrity_errors;

    std::size_t stamped_traces{0};
    std::size_t stamped_banner_modern{0};
    std::size_t stamped_banner_old{0};
    std::size_t unstamped_banner_modern{0};
    std::size_t markers_banner_modern{0};
    std::size_t markers_stamped_banner_old{0};
    std::size_t markers_unstamped{0};
    std::size_t markers_stamped_axis{0};
    std::size_t section_shaped_declined{0};
    std::size_t oracle_study_banner_modern{0};
    std::size_t oracle_study_stamped_axis{0};
    std::size_t oracle_study_unstamped{0};
    std::size_t per_trace_marker_mismatches{0};
    std::size_t per_trace_stamped_mismatches{0};
    std::vector<std::string> reported;

    OutcomeLegCells outcome_banner_modern;
    OutcomeLegCells outcome_banner_old;
    std::size_t recovered{0};
    std::size_t agreements{0};
    std::size_t disagreements{0};
    std::size_t disagreements_console_success{0};
    std::size_t disagreements_console_failure{0};
    std::size_t disagreements_api_canceled{0};
    std::vector<std::string> disagreement_rows;
    std::vector<std::string> vocabulary_errors;
};

[[nodiscard]] ComposedSemantics gitlab_only()
{
    const std::array manifests{insight::semantic::gitlab::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::gitlab::kDialect,
                                                            {});
}

// invariant: per `\n`-line — stage 1 `normalize`, then the SHIPPED strategy's `parse`, then the
// SHIPPED `recognize()`; a declined line scores verbatim, the RawText fall-through.
[[nodiscard]] TraceResult score_trace(const std::string& bytes, const ComposedSemantics& composed,
                                      insight::tokenization::IFormatStrategy& strategy,
                                      ArenaAllocator& arena)
{
    TraceResult result;
    std::vector<std::string> outcome_lines;
    std::string stage1_scratch;
    std::string refixpoint_scratch;

    for (std::size_t begin{0}; begin < bytes.size();)
    {
        std::size_t end{bytes.find('\n', begin)};
        if (end == std::string::npos)
            end = bytes.size();
        const std::string_view raw_line{bytes.data() + begin, end - begin};
        begin = end + 1U;
        if (raw_line.empty())
            continue;

        // assert: the outcome scan takes the RAW `\n`-split line — it drives its own LogParser
        // and its own `\r` anchoring, so handing it stripped content would DOUBLE-NORMALIZE.
        // note: that consumer defect would then be committed inside the gate built to catch it
        outcome_lines.emplace_back(raw_line);

        const auto normalized{insight::tokenization::normalize(raw_line, stage1_scratch)};
        if (normalized.bytes().empty())
            continue;
        auto content{normalized.undeclared_suffix(0)};
        if (const auto parsed{strategy.parse(normalized.bytes(), arena)}; parsed.has_value())
        {
            // invariant: the strategy arena-stores VERBATIM bytes of an already normalized input,
            // so stage 1 is a FIXED POINT on them and re-normalizing is the honest re-attestation.
            content = insight::tokenization::normalize(parsed->content, refixpoint_scratch)
                          .undeclared_suffix(0);
            if (parsed->timestamp.has_value())
                result.stamped = true;
        }
        const auto marker{recognize(content, composed)};
        if (marker.kind != IntentMarkerKind::None)
            ++result.markers;
        else if (content.bytes().starts_with("section_start:"))
            ++result.section_declined;
        arena.reset();
    }

    const RunOutcomeScan scan{scan_run_outcome(outcome_lines, composed)};
    if (scan.marker_present)
    {
        result.outcome_recovered = true;
        result.outcome = resolve_run_outcome({}, scan, composed, composed).outcome;
    }
    return result;
}

class GitLabPackageCorpusProofGate : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // assert: an UNSET variable and a SET-BUT-BROKEN mount BOTH FAIL since the Founder's ruling
        // of 2026-09-04; what differs is the diagnostic, not the verdict.
        // note: empty counts as unset — an undefined `vars.X` expands to "" on a runner
        const char* const raw{std::getenv(kCorpusVar)};
        if (raw == nullptr || *raw == '\0')
            FAIL() << kCorpusVar
                   << " unset — the private GitLab marker corpus is not present. Point it "
                      "at the tree holding "
                   << kSidecarFile << ", " << kOracleFile
                   << " and data/v1/ (i.e. .../coderoast-corpora/gitlab_corpora/marker_corpus).";

        root_ = std::filesystem::path{raw};
        // invariant: set-but-broken is a WIRING failure, not an absent corpus: the operator
        // declared the corpus present, so a quiet pass here is how a mis-wired gate greens forever.
        ASSERT_TRUE(std::filesystem::is_regular_file(root_ / kSidecarFile))
            << kCorpusVar << " is set to '" << raw << "' but there is no " << kSidecarFile
            << " under it. The corpus is declared present, so this is a wiring error — unset the "
               "variable if this machine has no local copy of the private corpus.";
        ASSERT_TRUE(std::filesystem::is_regular_file(root_ / kOracleFile))
            << kCorpusVar << " is set to '" << raw << "' but there is no " << kOracleFile
            << " under it.";
        ASSERT_TRUE(std::filesystem::is_directory(root_ / kBytesRoot))
            << kCorpusVar << " is set to '" << raw << "' but there is no " << kBytesRoot
            << "/ under it — the trace bytes are materialized out-of-tree and must be mounted.";
    }

    // invariant: the score is computed ONCE and the three tests assert different legs of the same
    // deterministic pass.
    [[nodiscard]] static const CorpusScore& score()
    {
        static const CorpusScore computed{compute_score()};
        return computed;
    }

    [[nodiscard]] static std::string read_file(const std::filesystem::path& path, bool& read_ok)
    {
        std::ifstream input{path, std::ios::binary};
        if (!input)
        {
            read_ok = false;
            return {};
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return std::move(buffer).str();
    }

    [[nodiscard]] static CorpusScore compute_score()
    {
        CorpusScore corpus;

        bool sidecar_ok{true};
        const std::string sidecar_text{read_file(root_ / kSidecarFile, sidecar_ok)};
        if (!sidecar_ok)
        {
            corpus.integrity_errors.push_back("could not read the committed sidecar");
            return corpus;
        }
        std::vector<SidecarRow> sidecar;
        std::unordered_map<std::string, OracleRow> oracle;
        {
            constexpr std::string_view kSidecarHeader{
                "path\tsha256\tbytes\tjob_id\tjob_status\tleg"};
            std::size_t line_no{0};
            for (std::size_t begin{0}; begin < sidecar_text.size();)
            {
                std::size_t end{sidecar_text.find('\n', begin)};
                if (end == std::string::npos)
                    end = sidecar_text.size();
                const std::string_view line{sidecar_text.data() + begin, end - begin};
                begin = end + 1U;
                if (line.empty())
                    continue;
                ++line_no;
                if (line_no == 1U)
                {
                    if (line != kSidecarHeader)
                        corpus.integrity_errors.push_back(
                            "sidecar header drifted from the frozen column order — the reader "
                            "indexes positionally and must be revisited, not guessed at");
                    continue;
                }
                const auto fields{split_tabs(line)};
                bool row_ok{fields.size() == 6U};
                if (row_ok)
                {
                    SidecarRow row{.path = std::string{fields[0]},
                                   .sha256 = std::string{fields[1]},
                                   .job_status = std::string{fields[4]},
                                   .leg = std::string{fields[5]}};
                    row.bytes = parse_count(fields[2], row_ok);
                    row.job_id = parse_count(fields[3], row_ok);
                    if (row_ok)
                        sidecar.push_back(std::move(row));
                }
                if (!row_ok)
                    corpus.integrity_errors.push_back("unparseable sidecar row " +
                                                      std::to_string(line_no));
            }
        }
        {
            bool oracle_ok{true};
            const std::string oracle_text{read_file(root_ / kOracleFile, oracle_ok)};
            if (!oracle_ok)
            {
                corpus.integrity_errors.push_back(
                    "could not read the committed recognition oracle");
                return corpus;
            }
            constexpr std::string_view kOracleHeader{
                "path\tleg\tstamped\tstudy_starts\tmarkers_canon\tmarkers_eidos_pre"
                "\tmarkers_eidos_post\tlost_pre\tgained_pre\tlost_post\tgained_post"};
            std::size_t line_no{0};
            for (std::size_t begin{0}; begin < oracle_text.size();)
            {
                std::size_t end{oracle_text.find('\n', begin)};
                if (end == std::string::npos)
                    end = oracle_text.size();
                const std::string_view line{oracle_text.data() + begin, end - begin};
                begin = end + 1U;
                if (line.empty())
                    continue;
                ++line_no;
                if (line_no == 1U)
                {
                    if (line != kOracleHeader)
                        corpus.integrity_errors.push_back(
                            "recognition-oracle header drifted from the frozen column order");
                    continue;
                }
                const auto fields{split_tabs(line)};
                bool row_ok{fields.size() == 11U};
                if (row_ok)
                {
                    OracleRow row{.leg = std::string{fields[1]}};
                    row.stamped = fields[2] == "1";
                    row_ok = row.stamped || fields[2] == "0";
                    row.study_starts = parse_count(fields[3], row_ok);
                    row.markers_canon = parse_count(fields[4], row_ok);
                    if (row_ok)
                        oracle.emplace(std::string{fields[0]}, std::move(row));
                }
                if (!row_ok)
                    corpus.integrity_errors.push_back("unparseable recognition-oracle row " +
                                                      std::to_string(line_no));
            }
        }

        const ComposedSemantics composed{gitlab_only()};
        const std::unique_ptr<insight::tokenization::IFormatStrategy> strategy{
            insight::semantic::gitlab::make_strategy()};
        constexpr std::size_t kArenaBlockBytes{64U * 1024U};
        ArenaAllocator arena{kArenaBlockBytes};

        for (const SidecarRow& row : sidecar)
        {
            ++corpus.rows;
            if (row.leg == kLegBannerModern)
                ++corpus.banner_modern;
            else if (row.leg == kLegBannerOld)
                ++corpus.banner_old;
            else if (row.leg == kLegVoid)
                ++corpus.void_rows;
            else if (row.leg == kLegUnclassified)
                ++corpus.unclassified;
            else
                corpus.integrity_errors.push_back("sidecar row '" + row.path +
                                                  "' carries an unknown leg '" + row.leg + "'");
            if (row.job_status == "success")
                ++corpus.api_success;
            else if (row.job_status == "failed")
                ++corpus.api_failed;
            else if (row.job_status == "canceled")
                ++corpus.api_canceled;
            else
                corpus.vocabulary_errors.push_back("trace '" + row.path + "': job_status '" +
                                                   row.job_status +
                                                   "' is outside the recorded vocabulary");

            // assert: the bytes are VERIFIED, sha256 and size, against the sidecar's attestation: a
            // right count over wrong bytes is the fabricated-pass shape counts cannot see.
            bool read_ok{true};
            const std::string bytes{read_file(root_ / kBytesRoot / row.path, read_ok)};
            if (!read_ok)
            {
                corpus.integrity_errors.push_back("trace '" + row.path +
                                                  "' is attested by the sidecar but not readable");
                continue;
            }
            if (bytes.size() != row.bytes)
                corpus.integrity_errors.push_back("trace '" + row.path + "' size " +
                                                  std::to_string(bytes.size()) + " != attested " +
                                                  std::to_string(row.bytes));
            if (sha256_hex(bytes) != row.sha256)
                corpus.integrity_errors.push_back("trace '" + row.path +
                                                  "' sha256 differs from the attested digest — "
                                                  "wrong bytes under a right count is the "
                                                  "fabricated-pass shape");

            const auto oracle_it{oracle.find(row.path)};
            if (oracle_it == oracle.end())
            {
                corpus.integrity_errors.push_back("trace '" + row.path +
                                                  "' has no recognition-oracle row");
                continue;
            }
            const OracleRow& expect{oracle_it->second};
            if (expect.leg != row.leg)
                corpus.integrity_errors.push_back(
                    "trace '" + row.path +
                    "': the two committed files disagree on the banner leg ('" + row.leg +
                    "' vs '" + expect.leg +
                    "') — one classifier, one owner, so this can "
                    "only be generator drift");

            // invariant: the void leg is a COUNTED cell, never a filter and never a parse failure,
            // and it must BE the zero-byte cell.
            // note: leg and emptiness disagreeing is an integrity error
            if (row.leg == kLegVoid || bytes.empty())
            {
                if ((row.leg == kLegVoid) != bytes.empty())
                    corpus.integrity_errors.push_back(
                        "trace '" + row.path + "': void-leg and zero-byte disagree (leg '" +
                        row.leg + "', " + std::to_string(bytes.size()) + " bytes)");
                continue;
            }

            const TraceResult result{score_trace(bytes, composed, *strategy, arena)};

            corpus.section_shaped_declined += result.section_declined;
            if (result.stamped)
            {
                ++corpus.stamped_traces;
                corpus.markers_stamped_axis += result.markers;
                if (row.leg == kLegBannerModern)
                {
                    ++corpus.stamped_banner_modern;
                    corpus.markers_banner_modern += result.markers;
                }
                else
                {
                    ++corpus.stamped_banner_old;
                    corpus.markers_stamped_banner_old += result.markers;
                }
            }
            else
            {
                if (row.leg == kLegBannerModern)
                    ++corpus.unstamped_banner_modern;
                corpus.markers_unstamped += result.markers;
            }
            if (row.leg == kLegBannerModern)
                corpus.oracle_study_banner_modern += expect.study_starts;
            if (expect.stamped)
                corpus.oracle_study_stamped_axis += expect.study_starts;
            else
                corpus.oracle_study_unstamped += expect.study_starts;

            // assert: per-TRACE agreement is the anti-compensation leg — an aggregate is
            // satisfiable by per-trace errors that cancel, and this is not.
            if (result.markers != expect.markers_canon)
            {
                ++corpus.per_trace_marker_mismatches;
                if (corpus.reported.size() < kMaxReportedRows)
                    corpus.reported.push_back(
                        "PER-TRACE MARKER DISAGREEMENT " + row.path + " (banner leg '" + row.leg +
                        "', stamped " + (result.stamped ? "yes" : "no") + "): shipped engine " +
                        std::to_string(result.markers) + " vs committed oracle " +
                        std::to_string(expect.markers_canon));
            }
            if (result.stamped != expect.stamped)
            {
                ++corpus.per_trace_stamped_mismatches;
                if (corpus.reported.size() < kMaxReportedRows)
                    corpus.reported.push_back(
                        "STAMPED-AXIS DISAGREEMENT " + row.path + ": shipped strategy says " +
                        (result.stamped ? "stamped" : "unstamped") + ", committed oracle says " +
                        (expect.stamped ? "stamped" : "unstamped"));
            }

            // invariant: the outcome cells are cut by BANNER leg BEFORE anything is asserted, so no
            // cell ever spans two legs.
            OutcomeLegCells& cells{row.leg == kLegBannerModern ? corpus.outcome_banner_modern
                                                               : corpus.outcome_banner_old};
            if (!result.outcome_recovered)
            {
                ++cells.none;
            }
            else
            {
                ++corpus.recovered;
                switch (result.outcome)
                {
                case RunOutcome::Success:
                    ++cells.success;
                    break;
                case RunOutcome::Failure:
                    ++cells.failure;
                    break;
                case RunOutcome::Aborted:
                    ++cells.aborted;
                    break;
                default:
                    corpus.vocabulary_errors.push_back(
                        "trace '" + row.path + "': the console scan resolved to " +
                        outcome_name(result.outcome) +
                        ", outside this package's three PrefixIsVerdict verdicts");
                    break;
                }
                const auto api_mapped{map_outcome_token(row.job_status, composed)};
                if (!api_mapped.has_value())
                {
                    corpus.vocabulary_errors.push_back("trace '" + row.path + "': job_status '" +
                                                       row.job_status +
                                                       "' does not map in the composed vocabulary");
                }
                else if (*api_mapped == result.outcome)
                {
                    ++corpus.agreements;
                }
                else
                {
                    ++corpus.disagreements;
                    if (result.outcome == RunOutcome::Success)
                        ++corpus.disagreements_console_success;
                    if (result.outcome == RunOutcome::Failure)
                        ++corpus.disagreements_console_failure;
                    if (row.job_status == "canceled")
                        ++corpus.disagreements_api_canceled;
                    corpus.disagreement_rows.push_back(
                        "console-vs-API divergence: " + row.path + " (job " +
                        std::to_string(row.job_id) + ", banner leg '" + row.leg + "'): API '" +
                        row.job_status + "', console " + outcome_name(result.outcome) +
                        " — the console's declared subordination to the API result "
                        "(SRC-D-OUT-RUN-1), a counted cell, never a row defect");
                }
            }
        }
        return corpus;
    }

    static std::filesystem::path root_;
};

std::filesystem::path GitLabPackageCorpusProofGate::root_{};

// invariant: this block prints on ANY failure so a red is diagnosable without re-running under a
// debugger — actual-vs-expected, with the trace named.
[[nodiscard]] std::string report(const CorpusScore& corpus)
{
    std::ostringstream out;
    out << "\n  population rows            : " << corpus.rows << " (pinned " << kTraceRows << ")"
        << "\n  banner-modern / old / void : " << corpus.banner_modern << " / " << corpus.banner_old
        << " / " << corpus.void_rows << " (pinned " << kBannerModernTraces << " / "
        << kBannerOldTraces << " / " << kVoidTraces << ")"
        << "\n  banner-unclassified        : " << corpus.unclassified << " (pinned "
        << kUnclassifiedTraces << ")"
        << "\n  api success/failed/canceled: " << corpus.api_success << " / " << corpus.api_failed
        << " / " << corpus.api_canceled << " (pinned " << kApiSuccess << " / " << kApiFailed
        << " / " << kApiCanceled << ")"
        << "\n  integrity errors           : " << corpus.integrity_errors.size() << " (pinned 0)"
        << "\n  stamped traces             : " << corpus.stamped_traces << " (pinned "
        << kStampedTraces << " = " << kStampedBannerModern << " banner-modern + "
        << kStampedBannerOld << " banner-old)"
        << "\n  markers, banner-modern     : " << corpus.markers_banner_modern << " (pinned "
        << kMarkersBannerModern << ")"
        << "\n  markers, stamped∧banner-old: " << corpus.markers_stamped_banner_old << " (pinned "
        << kMarkersStampedBannerOld << ")"
        << "\n  markers, unstamped         : " << corpus.markers_unstamped << " (pinned "
        << kMarkersUnstamped << ")"
        << "\n  markers, STAMPED axis      : " << corpus.markers_stamped_axis << " (pinned "
        << kMarkersStampedAxisTotal << " — the recall claim's numerator)"
        << "\n  per-trace marker mismatches: " << corpus.per_trace_marker_mismatches
        << " (pinned 0)   <<< THE TRANSCRIPTION CLAIM"
        << "\n  outcome banner-modern      : " << corpus.outcome_banner_modern.success << " S / "
        << corpus.outcome_banner_modern.failure << " F / " << corpus.outcome_banner_modern.aborted
        << " A / " << corpus.outcome_banner_modern.none << " none (pinned "
        << kOutcomeBannerModern.success << "/" << kOutcomeBannerModern.failure << "/"
        << kOutcomeBannerModern.aborted << "/" << kOutcomeBannerModern.none << ")"
        << "\n  outcome banner-old         : " << corpus.outcome_banner_old.success << " S / "
        << corpus.outcome_banner_old.failure << " F / " << corpus.outcome_banner_old.aborted
        << " A / " << corpus.outcome_banner_old.none << " none (pinned "
        << kOutcomeBannerOld.success << "/" << kOutcomeBannerOld.failure << "/"
        << kOutcomeBannerOld.aborted << "/" << kOutcomeBannerOld.none << ")"
        << "\n  recovered / agree / diverge: " << corpus.recovered << " / " << corpus.agreements
        << " / " << corpus.disagreements << " (pinned " << kRecoveredTraces << " / " << kAgreements
        << " / " << kDisagreements << ")";
    for (const std::string& row : corpus.integrity_errors)
        out << "\n  INTEGRITY: " << row;
    for (const std::string& row : corpus.vocabulary_errors)
        out << "\n  VOCABULARY: " << row;
    for (const std::string& row : corpus.disagreement_rows)
        out << "\n  " << row;
    for (const std::string& row : corpus.reported)
        out << "\n  " << row;
    if (corpus.reported.size() == kMaxReportedRows)
        out << "\n  … per-trace reporting truncated at " << kMaxReportedRows << " rows";
    return std::move(out).str();
}

TEST_F(GitLabPackageCorpusProofGate, ThePopulationIsTheCommittedSidecarVerifiedAgainstTheBytes)
{
    const CorpusScore& corpus{score()};

    ASSERT_EQ(corpus.rows, kTraceRows)
        << "the committed sidecar yielded " << corpus.rows << " trace rows, not " << kTraceRows
        << ". A re-sliced or re-versioned corpus must force a DELIBERATE re-pin of every cell in "
           "this gate, not silently redefine what it measures."
        << report(corpus);

    EXPECT_TRUE(corpus.integrity_errors.empty())
        << corpus.integrity_errors.size()
        << " integrity error(s) — attested digests, sizes, or the sidecar↔oracle join failed."
        << report(corpus);

    EXPECT_EQ(corpus.banner_modern, kBannerModernTraces) << report(corpus);
    EXPECT_EQ(corpus.banner_old, kBannerOldTraces) << report(corpus);
    EXPECT_EQ(corpus.void_rows, kVoidTraces)
        << "void (zero-byte, served HTTP 200) is a COUNTED cell, never a filter and never a parse "
           "failure attributed to the engine."
        << report(corpus);
    EXPECT_EQ(corpus.unclassified, kUnclassifiedTraces)
        << "a banner the frozen classifier could not read is a named cell — the prevalence study "
           "measured 0, so a nonzero count is a corpus finding, not a formatting detail."
        << report(corpus);
    EXPECT_EQ(corpus.banner_modern + corpus.banner_old + corpus.void_rows + corpus.unclassified,
              corpus.rows)
        << "the banner-leg cells do not sum to the population — records vanished into a bucket "
           "nobody counted."
        << report(corpus);

    EXPECT_EQ(corpus.api_success, kApiSuccess) << report(corpus);
    EXPECT_EQ(corpus.api_failed, kApiFailed) << report(corpus);
    EXPECT_EQ(corpus.api_canceled, kApiCanceled) << report(corpus);
}

TEST_F(GitLabPackageCorpusProofGate, TheMarkerLegCarriesTheRecordedDepthPerTraceAndPerAxis)
{
    const CorpusScore& corpus{score()};
    ASSERT_EQ(corpus.rows, kTraceRows) << report(corpus);

    EXPECT_EQ(corpus.per_trace_marker_mismatches, 0U)
        << "the shipped package and the committed oracle disagree on at least one trace's marker "
           "count. Aggregates may still balance — that is exactly why this cell exists."
        << report(corpus);
    EXPECT_EQ(corpus.per_trace_stamped_mismatches, 0U)
        << "the shipped strategy and the committed oracle disagree on the STAMPED axis for at "
           "least one trace — the in-band derivation and the model have diverged."
        << report(corpus);

    EXPECT_EQ(corpus.stamped_traces, kStampedTraces) << report(corpus);
    EXPECT_EQ(corpus.stamped_banner_modern, kStampedBannerModern)
        << "every banner-modern trace is stamped — a drift here re-opens the leg-vocabulary "
           "collision (stamped and banner-modern are two axes, and only their crossing is a cell)."
        << report(corpus);
    EXPECT_EQ(corpus.stamped_banner_old, kStampedBannerOld)
        << "the 37 stamped∧banner-old contaminants are a named, scored, never-averaged cell."
        << report(corpus);
    EXPECT_EQ(corpus.unstamped_banner_modern, 0U) << report(corpus);

    EXPECT_EQ(corpus.markers_banner_modern, kMarkersBannerModern)
        << "banner-modern (runner banner >= 18.9; all stamped) — the assertion leg's recognition, "
           "through stage 1 + the shipped strategy peel + the shipped recognize()."
        << report(corpus);
    EXPECT_EQ(corpus.markers_stamped_banner_old, kMarkersStampedBannerOld)
        << "stamped∧banner-old — scored and reported, never gated as depth (the old banner leg's "
           "depth claim was withdrawn); pinned so the contaminant cell cannot drift silently."
        << report(corpus);
    EXPECT_EQ(corpus.markers_unstamped, kMarkersUnstamped)
        << "unstamped (all banner-old) — the old-leg numerator, on the banner axis the prevalence "
           "study actually cut."
        << report(corpus);
    EXPECT_EQ(corpus.markers_stamped_axis, kMarkersStampedAxisTotal)
        << "the STAMPED-axis total — 3193, the measured figure the package header's recall claim "
           "rests on. This cell names the stamped axis: it is NOT a banner-leg number."
        << report(corpus);
    EXPECT_EQ(corpus.markers_banner_modern + corpus.markers_stamped_banner_old +
                  corpus.markers_unstamped,
              corpus.markers_stamped_axis + corpus.markers_unstamped)
        << "the marker cells do not close over the crossed axes." << report(corpus);

    EXPECT_EQ(corpus.oracle_study_banner_modern, kStudyStartsBannerModern) << report(corpus);
    EXPECT_EQ(corpus.oracle_study_stamped_axis, kStudyStartsStampedAxis) << report(corpus);
    EXPECT_EQ(corpus.oracle_study_unstamped, kStudyStartsUnstamped) << report(corpus);

    EXPECT_EQ(corpus.section_shaped_declined, kSectionShapedDeclined)
        << "`section_start:`-shaped content at offset 0 was DECLINED by the row grammar — on this "
           "corpus that cell is pinned ZERO (the malformed `%s` class never reaches a line start; "
           "it rides the `$ printf` command echo and is rejected by position). A nonzero count is "
           "a producer shape at a genuine line start the rows do not carry — a corpus finding or "
           "a shape-rule regression, and which one must be established before any re-pin."
        << report(corpus);
}

TEST_F(GitLabPackageCorpusProofGate, TheOutcomeLegRecoversThePlatformVerdictFromConsoleBytesAlone)
{
    const CorpusScore& corpus{score()};
    ASSERT_EQ(corpus.rows, kTraceRows) << report(corpus);

    EXPECT_TRUE(corpus.vocabulary_errors.empty())
        << "a verdict left the recorded vocabulary — every cell below is suspect."
        << report(corpus);

    EXPECT_EQ(corpus.outcome_banner_modern.success, kOutcomeBannerModern.success) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_modern.failure, kOutcomeBannerModern.failure) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_modern.aborted, kOutcomeBannerModern.aborted) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_modern.none, kOutcomeBannerModern.none)
        << "banner-modern `none` is 12 traces with NO terminal outcome line anywhere in the "
           "console — the engine is CORRECT to report none; there is no evidence to recover."
        << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_old.success, kOutcomeBannerOld.success) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_old.failure, kOutcomeBannerOld.failure) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_old.aborted, kOutcomeBannerOld.aborted) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_old.none, kOutcomeBannerOld.none) << report(corpus);

    EXPECT_EQ(corpus.outcome_banner_modern.success + corpus.outcome_banner_modern.failure +
                  corpus.outcome_banner_modern.aborted + corpus.outcome_banner_modern.none,
              kBannerModernTraces)
        << "the banner-modern outcome partition does not close." << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_old.success + corpus.outcome_banner_old.failure +
                  corpus.outcome_banner_old.aborted + corpus.outcome_banner_old.none,
              kBannerOldTraces)
        << "the banner-old outcome partition does not close." << report(corpus);

    EXPECT_EQ(corpus.outcome_banner_modern.aborted + corpus.outcome_banner_old.aborted,
              kAbortedConsoleCeiling)
        << "Aborted must equal the console CEILING: GitLab put `ERROR: Job failed: canceled` on "
           "17 of the 25 cancelled jobs, and the `\\r`-aware anchor (insight-canon c6a1e84) is "
           "what makes all 17 reachable (11 banner-modern `\\n`-delimited + 6 banner-old "
           "`\\r`-delimited). 25 is NOT a recall target — the other 8 carry no cancel line at "
           "all, a property of GitLab's producer, not of our rows."
        << report(corpus);

    EXPECT_EQ(corpus.recovered, kRecoveredTraces) << report(corpus);
    EXPECT_EQ(corpus.agreements, kAgreements) << report(corpus);
    EXPECT_EQ(corpus.disagreements, kDisagreements)
        << "the disagreement cell is 3 console-vs-API divergences, ALL DECLARED — GitLab's console "
           "and GitLab's API disagreeing with each other, our rows faithfully reporting the "
           "console. A gate pinning zero here would be pinning an artefact of not seeing — the "
           "pre-anchor zero was exactly that."
        << report(corpus);
    EXPECT_EQ(corpus.disagreements_api_canceled, kDisagreements)
        << "every recorded divergence has API verdict `canceled` — a divergence on any other API "
           "verdict is a NEW class, not a bigger cell."
        << report(corpus);
    EXPECT_EQ(corpus.disagreements_console_success, kDisagreementsConsoleSuccess)
        << "two cancelled jobs end on `Job succeeded` (the package source declares them)."
        << report(corpus);
    EXPECT_EQ(corpus.disagreements_console_failure, kDisagreementsConsoleFailure)
        << "one cancelled job ends on `ERROR: Job failed: exit code 128`." << report(corpus);

    GTEST_LOG_(INFO) << "GitLab package proof green" << report(corpus);
}

} // namespace
