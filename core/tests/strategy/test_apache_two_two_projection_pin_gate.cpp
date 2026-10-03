// refs: DN-43.D21, ADR-7.D7, ADR-7.D8
// invariant: R3 -- Apache 2.2 lines read byte-identically across the 2.4 reading: the projection
// (event time, level, component, content) of every line of the re-cut Apache_2k.log equals HEAD's.
// invariant: the input is lines 1-2 000 of Apache.log in Zenodo 8196385's Apache.tar.gz, cut by the
// `loghub` registry's slice rule and mounted out of tree at CORPUS_LOGHUB_2K_DIR.
// invariant: the file identity comes FIRST -- size, sha256, then the CR count -- so logpai's _2k
// file, a CRLF copy, or any other bytes fail as the wrong file before any projection number.
// invariant: lines split as the production splitter does: on LF, with one CR immediately before
// it dropped; the excerpt carries no CR, so the drop is never exercised on this input.
// refs: F-SRC-insight-eidos:acquisition.cpp:complete_line
// invariant: the partition pins are written from the bytes after that split, never from canon's
// output: 2 000 lines, 336 `[notice]` and 1 664 `[error]` seats, every clock fraction-free.
// invariant: the digest is a MEASUREMENT taken before the 2.4 change, at a canon commit whose
// source equals b96af4c's; a digest taken after that change proves nothing.
// invariant: corpus-labelled -- the bytes never enter this repository, so an unset variable FAILS
// and the label keeps the default run free of it.
// note: byte-only and single-threaded: committed file order, no RNG, no clock, no float.
#include <gtest/gtest.h>

#include <picosha2.h>

import insight.canon.test;

namespace
{

using insight::LogFormat;
using insight::LogLevel;
using insight::tokenization::ApacheErrorLogStrategy;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::ParsedLine;
using insight::tokenization::Tokenizer;

constexpr const char* kCorpusVar{"CORPUS_LOGHUB_2K_DIR"};
constexpr std::string_view kFile{"Apache_2k.log"};
constexpr std::size_t kArenaBytes{std::size_t{1} << 16U};

// invariant: the re-cut's identity, measured over the warehouse bytes on 2026-10-01 and pinned in
// the `loghub` registry's pin file: LF line ends on every line, the last one included.
constexpr std::uintmax_t kBytes{200328U};
constexpr std::size_t kCarriageReturns{0U};
constexpr std::size_t kLines{2000U};
constexpr std::string_view kFileSha256{
    "22c51ca1d49d0354cfbb7aa70408a7a436e4c220763b44b1bb45c8fc4f5d09e1"};

// invariant: the seat census, counted over the bytes; notice maps to Info by ADR-20.D16.
constexpr std::size_t kInfoLines{336U};
constexpr std::size_t kErrorLines{1664U};

// invariant: sha256 of the projection rendered by render_row over every line, measured before the
// 2.4 change at insight-canon 7f728a7, whose canon source equals b96af4c's.
constexpr std::string_view kProjectionSha256AtHead{
    "9dad52ad7ffaed6318e12b376f8a812b861b7ce8e18b90817d41d7d09a8cb18e"};

[[nodiscard]] std::string render_row(std::size_t line_number, const ParsedLine& parsed)
{
    const std::string when{
        parsed.timestamp.has_value()
            ? std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 parsed.timestamp->time_since_epoch())
                                 .count())
            : std::string{"-"}};
    return std::to_string(line_number) + '\t' + when + '\t' +
           std::string{to_string(parsed.level.value())} + '\t' +
           (parsed.level.is_declared() ? "declared" : "inferred") + '\t' +
           std::string{parsed.component} + '\t' + std::string{parsed.content} + '\n';
}

class ApacheTwoTwoProjectionPinGate : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        const char* const raw{std::getenv(kCorpusVar)};
        if (raw == nullptr || *raw == '\0')
            FAIL() << kCorpusVar << " unset -- the re-cut LogHub Apache_2k.log is not mounted.";
        file_ = std::filesystem::path{raw} / kFile;
        ASSERT_TRUE(std::filesystem::is_regular_file(file_))
            << kCorpusVar << " is set but " << file_.string()
            << " is missing -- a wiring error, not an absent corpus.";
    }

    std::filesystem::path file_;
};

// refs: DN-43.D21
TEST_F(ApacheTwoTwoProjectionPinGate, EveryTwoTwoLineProjectsAsAtHead)
{
    std::ifstream input{file_, std::ios::binary};
    const std::string bytes{std::istreambuf_iterator<char>{input},
                            std::istreambuf_iterator<char>{}};
    ASSERT_EQ(bytes.size(), kBytes) << file_.string() << " is not the pinned file.";
    ASSERT_EQ(picosha2::hash256_hex_string(bytes), kFileSha256)
        << file_.string() << " is not the pinned file.";
    ASSERT_EQ(static_cast<std::size_t>(std::ranges::count(bytes, '\r')), kCarriageReturns)
        << file_.string() << " is not the re-cut's LF bytes.";

    const insight::semantic::ComposedSemantics composed{
        insight::test_support::degenerate_composition()};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, composed, insight::tokenization::StreamContext{}};
    std::string projection;
    std::size_t lines{0};
    std::size_t claimed{0};
    std::size_t timed{0};
    std::size_t info{0};
    std::size_t error{0};
    std::size_t begin{0};
    while (begin < bytes.size())
    {
        const std::size_t end{std::min(bytes.find('\n', begin), bytes.size())};
        std::string_view line{std::string_view{bytes}.substr(begin, end - begin)};
        if (line.ends_with('\r'))
            line.remove_suffix(1);
        begin = end + 1;
        ++lines;

        const auto event{tokenizer.process_line(line)};
        ASSERT_TRUE(event.has_value()) << "line " << lines << ": " << event.error();
        if (event->format == LogFormat::ApacheError)
            ++claimed;
        arena.reset();

        const auto parsed{ApacheErrorLogStrategy{}.parse(line, arena)};
        ASSERT_TRUE(parsed.has_value()) << "line " << lines << ": " << parsed.error();
        timed += parsed->timestamp.has_value() ? 1U : 0U;
        info += parsed->level == LogLevel::Info ? 1U : 0U;
        error += parsed->level == LogLevel::Error ? 1U : 0U;
        projection += render_row(lines, *parsed);
        arena.reset();
    }

    ASSERT_EQ(lines, kLines);
    EXPECT_EQ(claimed, kLines) << "R3: " << claimed << " of " << kLines
                               << " 2.2 lines routed to ApacheError at the public door.";
    EXPECT_EQ(timed, kLines) << "R3: " << timed << " of " << kLines
                             << " fraction-free clocks gave an event time.";
    EXPECT_EQ(info, kInfoLines) << "R3: `[notice]` seats read Info on " << info << " lines.";
    EXPECT_EQ(error, kErrorLines) << "R3: `[error]` seats read Error on " << error << " lines.";

    const std::string measured{picosha2::hash256_hex_string(projection)};
    EXPECT_EQ(measured, kProjectionSha256AtHead)
        << "R3: the 2.2 projection digest is " << measured
        << ". Pin it ONLY from a run before the 2.4 change, at a canon commit whose source equals "
           "b96af4c's; after it a moved digest is a 2.2 line that stopped reading as before, never "
           "a re-pin.";
}

} // namespace
