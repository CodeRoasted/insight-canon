// NOLINTBEGIN — corpus gate: literals and printed diagnostics are intended.
// test_bracket_peel_equivalence_gate.cpp — G-T5-PEEL: the bracket row's peel-equivalence gate
// (jenkins_writer_envelope_t5.md §6; the G1-PEEL shape one row over; ADR-23 Part 1's owed
// obligation for Timestamper, discharged).
//
// ═══ THE ORACLE IS A FROZEN COPY (ADR-8) ═══
// Taken VERBATIM from `semantic/jenkins/src/jenkins_strategy.cpp` at insight-canon commit
// `f5e4838` — the last commit before T5 5.2 deleted that detection from production. Its
// provenance lives HERE, in the gate, and not only in the `git log` of a deleted file. It is
// FROZEN: an "improvement" to it is a DEFECT, not maintenance. Only the decision function
// `line ↦ (claimed?, content, timestamp)` travelled — the position logic (`[` at 0, datetime at
// 1, `]` adjacent, one past), the greedy `[ \t]+` strip, the blank decline, and the
// `parse_iso8601(interior)` extract. The gate NEVER calls the strategy it replaced (it no longer
// exists to call — the P2b discipline: implementation drift from the frozen spelling is red here,
// never a silent oracle shift).
//
// ═══ WHAT SHARING `rfc3339_datetime_length` MEANS (the can't-PASS ledger, §6) ═══
// The CHARACTER grammar is deliberately the one shared owner (bibles/jenkins_dialect.md §4
// item 3) — oracle and SUT both call it, so this gate cannot catch a defect INSIDE that grammar.
// What it scores is exactly the residual independent surface: the position logic, the strip, the
// blank decline, and the extract — the surfaces that were re-spelled in the catalogue peel.
// CLAIM WORD: REFACTOR-EQUIVALENCE, never external validity (ADR-23 verbatim: no sentence
// may cite this green as evidence the transport model is right about the world).
//
// ═══ POPULATION (clauses 1/3/4/5) ═══
// The 12 whole-stream traces of jenkins-markers/v2, EVERY `\n`-split line — selected by the
// committed sidecar's frozen `stamp_class` column (the studies/010 §6.2 classifier, generated
// corpora-side; never by inspection here), bytes verified against the attested sha256 digests.
// The per-line partition closes: stamped-equal + blank-dropped + unclaimed-identity + mismatch
// == total lines.
//
// ═══ FALSIFIABILITY — OBSERVED 2026-07-30, then recorded (clause 7); every mutation reverted ═══
//   P-A  the peel over-eats by one byte (`close + 1U` → `close + 2U` in transport.cpp): the
//        CORPUS ARM STAYED FULLY GREEN — 125 774/125 774 stamped lines still equal, 0 mismatches
//        — because every real whole-stream line carries a separator the strip eats anyway, AND
//        the frozen oracle strips the same way, so the blindness is BY CONSTRUCTION, not by
//        corpus luck. RED landed in the synthetic glued-form law alone ("payload" → "ayload").
//        ⚠ MEASURED BLINDNESS: the corpus arm cannot see an over-peel of separator width; the
//        synthetic strip laws are the carrying leg for that defect class — do not delete them
//        believing the 127 514-line arm covers it.
//   P-B  the acceptor widened past the `]` requirement (the close-byte check dropped): RED on
//        exactly the wrong-close decline arm ("[2026-01-02T03:04:05Z more] tail" peeled to
//        "more] tail" + a timestamp extracted); corpus green — no such shape exists in the 12
//        traces, which is why the arm is synthetic.
//   P-C  the extract dropped (observation_time never set for the bracket kind): RED —
//        127 502/127 514 lines mismatch, every one naming "observation_time differs" (the 12
//        unclaimed-identity lines stay green); the content cells all held: the #5 extract-equal
//        leg partitions from the byte legs exactly as designed.
//
// Determinism: byte-only — committed-order population, integer counts, no RNG, no clock, no
// float, no threads. sha256 is FIPS 180-4 over bytes; pure integer.
#include <gtest/gtest.h>

import std;
import insight.canon; // TransportStack / resolve_transport_stack / parse_iso8601 / rfc3339_…

namespace
{

constexpr const char* kCorpusVar{"CORPUS_JENKINS_MARKERS_DIR"};
constexpr std::string_view kTraceSidecar{"RETRO-v2.trace-sidecar.tsv"};
constexpr std::string_view kBytesRoot{"data/v2"};
constexpr std::string_view kBracketRow{"bracket-rfc3339-line-prefix"};
constexpr std::size_t kMaxReportedLines{10};

// ── The pins ──────────────────────────────────────────────────────────────────────────────────
// CORROBORATED — the stamp-class partition's whole-stream cell (studies/010 §6.2, the frozen
// sidecar column): 12 traces.
constexpr std::size_t kWholeStreamTraces{12};
// CHARACTERIZATION — measured by this gate's first run (2026-07-30), pinned so the population
// cannot drift silently (clause 3: the SIZE selects the pins; an unrecognized population FAILS).
constexpr std::size_t kTotalLines{127'514};
constexpr std::size_t kStampedEqual{125'774};
constexpr std::size_t kBlankDropped{1'728};
constexpr std::size_t kUnclaimedIdentity{12};
// The CLAIM: zero mismatches, all four compare surfaces (bundled #2–#4 byte-exact content, #5
// extract-equal), per line.
constexpr std::size_t kMismatches{0};

// ═══ THE FROZEN ORACLE (ADR-8) — do not tidy, do not modernize ═══
[[nodiscard]] constexpr bool oracle_is_space(char chr) noexcept
{
    return chr == ' ' || chr == '\t';
}

[[nodiscard]] std::size_t oracle_timestamper_prefix_end(std::string_view line) noexcept
{
    if (line.empty() || line.front() != '[')
        return 0U;
    // The character grammar is DELEGATED to the one shared owner — the frozen strategy already
    // delegated it (canon.api:1320), so freezing a second spelling here would create exactly the
    // divergence the one-owner rule killed. See the can't-PASS ledger above.
    const std::size_t datetime_len{insight::utils::rfc3339_datetime_length(line, 1U)};
    if (datetime_len == 0U)
        return 0U;
    const std::size_t close{1U + datetime_len};
    if (close >= line.size() || line[close] != ']')
        return 0U;
    return close + 1U;
}

struct OracleOutcome
{
    bool claimed{false};
    bool blank_declined{false};
    std::string_view content;
    std::optional<insight::Timestamp> timestamp;
};

[[nodiscard]] OracleOutcome oracle_strip(std::string_view line)
{
    const std::size_t prefix_end{oracle_timestamper_prefix_end(line)};
    if (prefix_end == 0U)
        return {.claimed = false, .blank_declined = false, .content = line,
                .timestamp = std::nullopt};
    OracleOutcome outcome;
    outcome.claimed = true;
    outcome.timestamp = insight::utils::parse_iso8601(line.substr(1U, prefix_end - 2U));
    std::string_view content{line.substr(prefix_end)};
    while (!content.empty() && oracle_is_space(content.front()))
        content.remove_prefix(1U);
    if (content.empty())
    {
        outcome.blank_declined = true; // bundled #4: the strategy declined; the peel DROPs
        return outcome;
    }
    outcome.content = content;
    return outcome;
}

// ── SHA-256 (FIPS 180-4) — frozen, pure integer, single-shot (the retrofit-gate block) ────────
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
                static_cast<std::uint32_t>(static_cast<unsigned char>(padded[block + 4U * word + 3U]));
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

class BracketPeelEquivalenceGate : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Clause 2 — UNSET vs SET-BUT-BROKEN must not share a verdict.
        const char* const raw{std::getenv(kCorpusVar)};
        if (raw == nullptr || *raw == '\0')
            GTEST_SKIP() << kCorpusVar
                         << " unset — the §2a-private Jenkins marker corpus is not present.";
        root_ = std::filesystem::path{raw};
        ASSERT_TRUE(std::filesystem::is_regular_file(root_ / kTraceSidecar))
            << kCorpusVar << " is set but " << kTraceSidecar
            << " is missing under it — a wiring error, not an absent corpus.";
        ASSERT_TRUE(std::filesystem::is_directory(root_ / kBytesRoot))
            << kCorpusVar << ": no " << kBytesRoot << "/ under it.";
    }

    static std::filesystem::path root_;
};
std::filesystem::path BracketPeelEquivalenceGate::root_{};

TEST_F(BracketPeelEquivalenceGate, DeclaredPeelIsByteIdenticalToTheFrozenStrategyStrip)
{
    // Population: the sidecar's whole-stream rows (frozen labels, clause 1), bytes verified.
    bool read_ok{true};
    const auto read_file{[&read_ok](const std::filesystem::path& path) {
        std::ifstream input{path, std::ios::binary};
        if (!input)
        {
            read_ok = false;
            return std::string{};
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return std::move(buffer).str();
    }};

    struct WholeStreamTrace
    {
        std::string path;
        std::string sha256;
        std::uint64_t size{0};
    };
    std::vector<WholeStreamTrace> traces;
    {
        const std::string sidecar{read_file(root_ / kTraceSidecar)};
        ASSERT_TRUE(read_ok);
        std::size_t line_no{0};
        for (std::size_t begin{0}; begin < sidecar.size();)
        {
            std::size_t end{sidecar.find('\n', begin)};
            if (end == std::string::npos)
                end = sidecar.size();
            const std::string_view line{sidecar.data() + begin, end - begin};
            begin = end + 1U;
            if (line.empty())
                continue;
            ++line_no;
            if (line_no == 1U)
                continue; // the header is frozen and positional — the retrofit gate pins it
            const auto fields{split_tabs(line)};
            ASSERT_EQ(fields.size(), 14U) << "unparseable sidecar row";
            if (fields[13] != "whole-stream")
                continue;
            std::uint64_t size{0};
            const auto [ptr, err]{
                std::from_chars(fields[2].data(), fields[2].data() + fields[2].size(), size)};
            ASSERT_TRUE(err == std::errc{} && ptr == fields[2].data() + fields[2].size());
            traces.push_back({.path = std::string{fields[0]}, .sha256 = std::string{fields[1]},
                              .size = size});
        }
    }
    // Clause 3 — the population SIZE selects the pins; an unrecognized population FAILS.
    ASSERT_EQ(traces.size(), kWholeStreamTraces)
        << "the sidecar's whole-stream cell yielded " << traces.size() << " traces, not "
        << kWholeStreamTraces
        << " — a re-classified corpus must force a DELIBERATE re-pin, never a silent redefine.";

    const std::array<std::string_view, 1> declared_names{kBracketRow};
    const insight::transport::TransportStack stack{insight::transport::resolve_transport_stack(
        insight::transport::IngestDeclaration{.stack = declared_names})};

    std::size_t total_lines{0};
    std::size_t stamped_equal{0};
    std::size_t blank_dropped{0};
    std::size_t unclaimed_identity{0};
    std::size_t mismatches{0};
    std::vector<std::string> mismatch_rows;

    for (const WholeStreamTrace& trace : traces)
    {
        read_ok = true;
        const std::string bytes{read_file(root_ / kBytesRoot / trace.path)};
        ASSERT_TRUE(read_ok) << trace.path;
        // Clause 4 — bytes verified, never assumed.
        ASSERT_EQ(bytes.size(), trace.size) << trace.path << ": size drifted from the attestation";
        ASSERT_EQ(sha256_hex(bytes), trace.sha256)
            << trace.path << ": sha256 differs from the attested digest — wrong bytes under a "
            << "right count is the fabricated-pass shape.";

        for (std::size_t begin{0}; begin < bytes.size();)
        {
            std::size_t end{bytes.find('\n', begin)};
            if (end == std::string::npos)
                end = bytes.size();
            const std::string_view raw_line{bytes.data() + begin, end - begin};
            begin = end + 1U;
            ++total_lines;

            const OracleOutcome oracle{oracle_strip(raw_line)};
            const insight::transport::RawPeeledLine declared{stack.peel_raw(raw_line)};

            bool line_ok{true};
            std::string detail;
            if (oracle.claimed)
            {
                // #5 extract-equal, then #2–#4 byte-exact content.
                if (declared.observation_time != oracle.timestamp)
                {
                    line_ok = false;
                    detail += " observation_time differs";
                }
                if (oracle.blank_declined)
                {
                    if (!declared.content.empty())
                    {
                        line_ok = false;
                        detail += " oracle blank-declined but the peel kept content";
                    }
                    else if (line_ok)
                        ++blank_dropped;
                }
                else if (declared.content != oracle.content)
                {
                    line_ok = false;
                    detail += " content bytes differ";
                }
                else if (line_ok)
                    ++stamped_equal;
            }
            else
            {
                // Totality is application, not effect: the declared rule's effect here is the
                // IDENTITY, and the extract must not fire.
                if (declared.content != raw_line || declared.observation_time.has_value())
                {
                    line_ok = false;
                    detail += " unclaimed line was not left identical";
                }
                else
                    ++unclaimed_identity;
            }
            if (!line_ok)
            {
                ++mismatches;
                if (mismatch_rows.size() < kMaxReportedLines)
                    mismatch_rows.push_back(trace.path + ":" + std::to_string(total_lines) + " —" +
                                            detail + "  raw=\"" + std::string{raw_line} + "\"");
            }
        }
    }

    const std::string report{"\n  whole-stream traces : " + std::to_string(traces.size()) +
                             "\n  total lines         : " + std::to_string(total_lines) +
                             "\n  stamped, equal      : " + std::to_string(stamped_equal) +
                             "\n  blank, dropped      : " + std::to_string(blank_dropped) +
                             "\n  unclaimed, identity : " + std::to_string(unclaimed_identity) +
                             "\n  MISMATCHES          : " + std::to_string(mismatches)};

    // ═══ THE CLAIM — refactor-equivalence, per line, zero mismatches ═══
    EXPECT_EQ(mismatches, kMismatches) << report;
    for (const std::string& row : mismatch_rows)
        ADD_FAILURE() << "  " << row;

    // Clause 3/5 — the pinned cells and the closed partition.
    EXPECT_EQ(total_lines, kTotalLines) << report;
    EXPECT_EQ(stamped_equal, kStampedEqual) << report;
    EXPECT_EQ(blank_dropped, kBlankDropped) << report;
    EXPECT_EQ(unclaimed_identity, kUnclaimedIdentity) << report;
    EXPECT_EQ(stamped_equal + blank_dropped + unclaimed_identity + mismatches, total_lines)
        << "the per-line partition does not close — a line vanished into a bucket nobody counted."
        << report;

    GTEST_LOG_(INFO) << "G-T5-PEEL green" << report;
}

// ═══ The synthetic arms — no corpus, run everywhere (the §6 decline + strip laws) ═══════════════

TEST(BracketPeelSyntheticArms, DeclineArmsAreIdentityAndExtractNothing)
{
    const std::array<std::string_view, 1> declared_names{kBracketRow};
    const insight::transport::TransportStack stack{insight::transport::resolve_transport_stack(
        insight::transport::IngestDeclaration{.stack = declared_names})};

    // The shipped strictness carve-outs, plus the malformed-optional-part law: every one fails
    // the SHARED grammar by construction and must peel to IDENTITY with no extract.
    constexpr std::array<std::string_view, 8> kDeclines{
        "[10.20.30.40] connection open",            // Proxifier
        "[Mon Oct 03 12:00:00 2026] [error] child", // ApacheError
        "[12:34:56] step output",                   // bare time
        "[Pipeline] sh",                            // the marker prefix itself
        "[v1.2.3] released",                        // version interior
        "[2026-01-02T03:04:05+9] truncated zone",   // malformed OPTIONAL part = hard 0
        "2026-01-02T03:04:05Z not bracketed",       // no bracket — the OTHER row's shape
        "[2026-01-02T03:04:05Z more] tail",         // valid datetime, WRONG close byte — the arm
                                                    // an acceptor widened past `]` peels
    };
    for (const std::string_view line : kDeclines)
    {
        const insight::transport::RawPeeledLine peeled{stack.peel_raw(line)};
        EXPECT_EQ(peeled.content, line)
            << "decline arm was not left byte-identical: \"" << line << "\"";
        EXPECT_FALSE(peeled.observation_time.has_value())
            << "decline arm extracted a timestamp: \"" << line << "\"";
    }
}

TEST(BracketPeelSyntheticArms, StripAndBlankLawsMatchTheFrozenSpelling)
{
    const std::array<std::string_view, 1> declared_names{kBracketRow};
    const insight::transport::TransportStack stack{insight::transport::resolve_transport_stack(
        insight::transport::IngestDeclaration{.stack = declared_names})};

    // The greedy `[ \t]+` strip (bundled #3) and the blank DROP (bundled #4), plus the glued
    // form (acceptor at 0, EMPTY separator run — a peel case, not a decline: the frozen strategy
    // claimed regardless of what follows the `]`).
    constexpr std::string_view kStamped{"[2026-06-23T15:11:09.020Z]  \t hello"};
    const auto stripped{stack.peel_raw(kStamped)};
    EXPECT_EQ(stripped.content, "hello");
    ASSERT_TRUE(stripped.observation_time.has_value());
    EXPECT_EQ(*stripped.observation_time,
              *insight::utils::parse_iso8601("2026-06-23T15:11:09.020Z"));

    const auto blank{stack.peel_raw("[2026-06-23T15:11:09.020Z] \t ")};
    EXPECT_TRUE(blank.content.empty()) << "a stamp-only line must peel to blank (DROP)";
    EXPECT_TRUE(blank.observation_time.has_value());

    const auto glued{stack.peel_raw("[2026-06-23T15:11:09.020Z]payload")};
    EXPECT_EQ(glued.content, "payload") << "the glued form is a peel case, not a decline";

    // Second-precision zone form (the shared grammar accepts offsets; the strip is form-blind).
    const auto offset_form{stack.peel_raw("[2026-06-23T15:11:09+02:00] tail")};
    EXPECT_EQ(offset_form.content, "tail");
}
} // namespace
// NOLINTEND
