// refs: DN-43.O5, ADR-7.D7, ADR-7.D8
// invariant: every line of the re-cut HealthApp_2k.log routes to HealthApp at the public door with
// an event time present, and HealthAppStrategy::parse yields one: 2 000 of 2 000 on every count.
// invariant: the input is lines 1-2 000 of HealthApp.log in Zenodo 8196385's HealthApp.tar.gz, cut
// by the `loghub` registry's slice rule and mounted out of tree at CORPUS_LOGHUB_2K_DIR.
// invariant: the file identity comes FIRST -- size, sha256, then the CR count -- so logpai's _2k
// file, an LF-normalized copy, or any other bytes fail as the wrong file before any routing count.
// invariant: lines split as the production splitter does: on LF, with one CR immediately before
// it dropped; every line of this file ends CRLF, so the drop runs on all 2 000.
// refs: F-SRC-insight-eidos:acquisition.cpp:complete_line
// invariant: corpus-labelled -- the bytes never enter this repository, so an unset variable FAILS
// and the label keeps the default run free of it.
// note: byte-only and single-threaded: committed file order, no RNG, no clock, no float.
#include <gtest/gtest.h>

#include <picosha2.h>

import insight.canon.test;

namespace
{

using insight::LogFormat;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::HealthAppStrategy;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

constexpr const char* kCorpusVar{"CORPUS_LOGHUB_2K_DIR"};
constexpr std::string_view kFile{"HealthApp_2k.log"};
constexpr std::size_t kArenaBytes{std::size_t{1} << 16U};

// invariant: the re-cut's identity, pinned in the `loghub` registry's pin file
// (insight-canon/core/data/corpora/loghub/README.md): CRLF line ends on every line.
constexpr std::uintmax_t kBytes{187458U};
constexpr std::size_t kCarriageReturns{2000U};
constexpr std::size_t kLines{2000U};
constexpr std::string_view kFileSha256{
    "d6fe07b1c5a0269576343fcbf3dd6fc839b7ffe1d153b0abf02c0f5791b37862"};

class HealthAppRoutingPinGate : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        const char* const raw{std::getenv(kCorpusVar)};
        if (raw == nullptr || *raw == '\0')
            FAIL() << kCorpusVar << " unset -- the re-cut LogHub HealthApp_2k.log is not mounted.";
        file_ = std::filesystem::path{raw} / kFile;
        ASSERT_TRUE(std::filesystem::is_regular_file(file_))
            << kCorpusVar << " is set but " << file_.string()
            << " is missing -- a wiring error, not an absent corpus.";
    }

    std::filesystem::path file_;
};

// refs: DN-43.O5
TEST_F(HealthAppRoutingPinGate, EveryLineRoutesToHealthAppWithAnEventTime)
{
    std::ifstream input{file_, std::ios::binary};
    const std::string bytes{std::istreambuf_iterator<char>{input},
                            std::istreambuf_iterator<char>{}};
    ASSERT_EQ(bytes.size(), kBytes) << file_.string() << " is not the pinned file.";
    ASSERT_EQ(picosha2::hash256_hex_string(bytes), kFileSha256)
        << file_.string() << " is not the pinned file.";
    ASSERT_EQ(static_cast<std::size_t>(std::ranges::count(bytes, '\r')), kCarriageReturns)
        << file_.string() << " is not the re-cut's CRLF bytes.";

    const insight::semantic::ComposedSemantics composed{
        insight::test_support::degenerate_composition()};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, composed, insight::tokenization::StreamContext{}};
    std::size_t lines{0};
    std::size_t routed{0};
    std::size_t routed_timed{0};
    std::size_t timed{0};
    std::size_t first_unrouted{0};
    LogFormat first_unrouted_format{LogFormat::Unknown};
    std::size_t first_routed_untimed{0};
    std::size_t first_untimed{0};
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
        if (event->format == LogFormat::HealthApp)
        {
            ++routed;
            if (event->timestamp.has_value())
                ++routed_timed;
            else if (first_routed_untimed == 0U)
                first_routed_untimed = lines;
        }
        else if (first_unrouted == 0U)
        {
            first_unrouted = lines;
            first_unrouted_format = event->format;
        }
        arena.reset();

        const auto parsed{HealthAppStrategy{}.parse(line, arena)};
        if (parsed.has_value() && parsed->timestamp.has_value())
            ++timed;
        else if (first_untimed == 0U)
            first_untimed = lines;
        arena.reset();
    }

    ASSERT_EQ(lines, kLines);
    EXPECT_EQ(routed, kLines) << routed << " of " << kLines
                              << " lines routed to HealthApp at the public door; the first that "
                                 "did not is line "
                              << first_unrouted << ", routed to "
                              << insight::to_string(first_unrouted_format) << ".";
    EXPECT_EQ(routed_timed, kLines)
        << routed_timed << " of " << kLines
        << " lines routed to HealthApp with an event time present at the public door; the first "
           "routed line without one is line "
        << first_routed_untimed << " (0: every routed line had one; the shortfall is unrouted).";
    EXPECT_EQ(timed, kLines) << timed << " of " << kLines
                             << " lines gave an event time through HealthAppStrategy; the first "
                                "that did not is line "
                             << first_untimed << ".";
}

} // namespace
