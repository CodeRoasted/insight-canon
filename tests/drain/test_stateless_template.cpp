// NOLINTBEGIN
// Unit tests + measure-first gate for the stateless template masker
// (stateless_template_id.md D-TID-1/2). The property tests are committed regression
// guards; CardinalityVsDrain_Corpus is the F13-sizing measurement (env-gated, skipped
// unless CORPUS_DIR points at a log corpus) — the reading that gates the cascade.

#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight::tokenization;

namespace
{
DrainConfig cfg()
{
    DrainConfig c;
    c.similarity_threshold = 0.5;
    c.max_clusters = 100000;
    return c;
}

// Copy the masked template out immediately (the arena is reused across calls).
std::string masked(std::string_view content, ArenaAllocator& arena)
{
    arena.reset();
    return std::string{stateless_template(content, arena, cfg()).template_str};
}

std::string drain_tmpl(Drain& drain, ArenaAllocator& arena, std::string_view content)
{
    arena.reset();
    return std::string{drain.match_into_arena(content, arena).template_str};
}
} // namespace

// ── The core property: identity is a pure function of the line's own content ────

TEST(StatelessTemplate, PureFunctionOfContentNotOrderOrStream)
{
    ArenaAllocator arena{256U * 1024U};
    const std::string_view line{"connect to host db-7 failed after 30 ms"};

    // Prime with unrelated lines, then ask for `line` — the result must not depend on
    // anything seen before (statelessness).
    masked("a totally different line here", arena);
    masked("yet another unrelated message 42", arena);
    const std::string after_priming{masked(line, arena)};
    const std::string fresh{masked(line, arena)};
    EXPECT_EQ(after_priming, fresh)
        << "stateless_template must depend ONLY on its own content\n"
        << "after_priming=" << after_priming << "\nfresh=" << fresh;
}

TEST(StatelessTemplate, LogicallyIdenticalLinesShareTemplate)
{
    ArenaAllocator arena{256U * 1024U};
    // Differ only in masked tokens (a number, an IPv4) → one template.
    EXPECT_EQ(masked("request from 10.0.0.1 took 12 ms", arena),
              masked("request from 192.168.1.250 took 9999 ms", arena));
}

// The phantom pair, demonstrated AND killed. Drain's absorb_into learns to wildcard a
// non-numeric token that varied across the lines it happened to see; a different
// surrounding stream learns differently — so the SAME logical line gets two templates
// (a false NewTemplate + VanishedTemplate on an outcome flip). The stateless masker
// gives it ONE template regardless of stream. (It also shows the accepted tradeoff,
// D-TID-8: `eu-west` stays literal — the over-split F13 + the cardinality monitor size.)
TEST(StatelessTemplate, KillsThePhantomPair)
{
    ArenaAllocator arena{256U * 1024U};
    const std::string_view shared{"deploy region eu-west complete"};

    // Stream A: a sibling line first makes Drain wildcard the region token.
    Drain drain_a{cfg()};
    drain_tmpl(drain_a, arena, "deploy region us-east complete");
    const std::string drain_a_tmpl{drain_tmpl(drain_a, arena, shared)};

    // Stream B: the shared line alone — Drain keeps the region literal.
    Drain drain_b{cfg()};
    const std::string drain_b_tmpl{drain_tmpl(drain_b, arena, shared)};

    ASSERT_NE(drain_a_tmpl, drain_b_tmpl)
        << "precondition — Drain IS order/stream-dependent (the phantom pair)\n"
        << "stream A: " << drain_a_tmpl << "\nstream B: " << drain_b_tmpl;

    // The fix: one logical line → one stateless template, in either stream.
    Drain unused{cfg()};
    (void)drain_tmpl(unused, arena, "deploy region ap-south complete"); // priming has no effect
    const std::string m1{masked(shared, arena)};
    const std::string m2{masked(shared, arena)};
    EXPECT_EQ(m1, m2) << "the stateless template must be stream-invariant: " << m1 << " vs " << m2;
}

TEST(StatelessTemplate, StatusValueKeptDistinct)
{
    ArenaAllocator arena{256U * 1024U};
    // The green→red flip must NOT collapse: exit code 0 and exit code 1 are distinct
    // templates (status-value KEEP), while a bare count stays masked.
    EXPECT_NE(masked("process exited with exit code 0", arena),
              masked("process exited with exit code 1", arena));
    EXPECT_EQ(masked("served 200 requests", arena), masked("served 4096 requests", arena));
}

TEST(StatelessTemplate, CompositesNormalized)
{
    ArenaAllocator arena{256U * 1024U};
    // Source location, versioned ref, bracket index — collapse the variable numeric
    // run while keeping the semantic literal (which file / which package / which word).
    EXPECT_EQ(masked("error at tokenizer.cpp:4500:30: bad token", arena),
              masked("error at tokenizer.cpp:12:5: bad token", arena));
    EXPECT_EQ(masked("building zlib/1.3", arena), masked("building zlib/1.2.11", arena));
    EXPECT_EQ(masked("make[2]: entering", arena), masked("make[15]: entering", arena));
    // A different file / package / word stays distinct (the semantic part is kept).
    EXPECT_NE(masked("error at tokenizer.cpp:1:1: bad token", arena),
              masked("error at parser.cpp:1:1: bad token", arena));
}

// ── Measure-first gate: post-stateless cardinality vs Drain on a real corpus ─────
// Sizes the F13 need (the cardinality monitor's first reading) BEFORE the cascade.
// Skipped unless CORPUS_DIR is set to a directory of *.log files (the CI revert
// corpus). Reports: lines, Drain clusters, stateless distinct templates, the
// over-split ratio, and the singleton fraction (the over-split tail F13 must shrink).
TEST(StatelessTemplate, CardinalityVsDrain_Corpus)
{
    const char* const corpus_dir{std::getenv("CORPUS_DIR")};
    if (corpus_dir == nullptr)
        GTEST_SKIP() << "set CORPUS_DIR to a directory of *.log files to size F13";

    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator{corpus_dir})
        if (entry.is_regular_file() && entry.path().extension() == ".log")
            files.push_back(entry.path());
    std::ranges::sort(files); // deterministic order
    ASSERT_FALSE(files.empty()) << "no *.log files under " << corpus_dir;

    constexpr std::size_t kMaxLines{300000};
    ArenaAllocator arena{8U * 1024U * 1024U};
    LogParser parser{arena};
    Drain drain{cfg()};
    std::unordered_map<std::string, std::uint64_t> stateless_templates;
    std::size_t lines{0};

    for (const auto& file : files)
    {
        if (lines >= kMaxLines)
            break;
        std::ifstream in{file};
        std::string raw;
        while (lines < kMaxLines && std::getline(in, raw))
        {
            if (raw.empty())
                continue;
            arena.reset();
            const auto parsed{parser.parse_line(raw)};
            if (!parsed)
                continue;
            const std::string_view content{parsed->content};
            drain.match_into_arena(content, arena); // Drain clusters (its own arena persists)
            ++stateless_templates[std::string{stateless_template(content, arena, cfg()).template_str}];
            ++lines;
        }
    }

    const std::size_t drain_clusters{drain.cluster_count()};
    const std::size_t stateless_distinct{stateless_templates.size()};
    const std::size_t singletons{static_cast<std::size_t>(
        std::ranges::count_if(stateless_templates, [](const auto& kv) { return kv.second == 1; }))};
    const double ratio{drain_clusters > 0 ? static_cast<double>(stateless_distinct) /
                                                static_cast<double>(drain_clusters)
                                          : 0.0};

    std::cout << "\n=== Stateless template_id cardinality (F13 sizing) ===\n"
              << "files            : " << files.size() << "\n"
              << "lines            : " << lines << "\n"
              << "Drain clusters   : " << drain_clusters << "\n"
              << "Stateless distinct: " << stateless_distinct << "\n"
              << "over-split ratio : " << ratio << "x\n"
              << "singletons       : " << singletons << " ("
              << (stateless_distinct > 0 ? (100.0 * static_cast<double>(singletons) /
                                            static_cast<double>(stateless_distinct))
                                         : 0.0)
              << "% of distinct)\n";

    // The 30 loudest stateless templates with a long alnum-with-digit token (F13
    // candidates — tokens that varied but no rule masked).
    std::vector<std::pair<std::string, std::uint64_t>> by_count{stateless_templates.begin(),
                                                                stateless_templates.end()};
    std::ranges::sort(by_count, [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });
    std::cout << "--- top 15 by count ---\n";
    for (std::size_t i{0}; i < std::min<std::size_t>(15, by_count.size()); ++i)
        std::cout << by_count[i].second << "  " << by_count[i].first.substr(0, 120) << "\n";
    std::cout << "--- 40 singleton samples (the F13 over-split tail) ---\n";
    std::size_t shown{0};
    for (auto it{by_count.rbegin()}; it != by_count.rend() && shown < 40; ++it)
        if (it->second == 1)
        {
            std::cout << it->first.substr(0, 140) << "\n";
            ++shown;
        }
    std::cout << std::flush;

    EXPECT_GT(lines, 0u);
}

// NOLINTEND
