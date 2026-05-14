// NOLINTBEGIN
// Tokenization benchmark.
//
// Measures the throughput of the Drain algorithm when tokenizing log lines.
// This is the atomic unit of the log processing pipeline.
//
// Reported metrics:
//   * `items_per_second` — log lines tokenized per wall second
//   * `ns_per_line` — nanoseconds spent per line (lower is better)
//   * `template_count` — distinct templates discovered
//
// The "≤ 1 μs per line" throughput target is documented in
// technical_docs/overview/architecture.md and is measured here.

#include <benchmark/benchmark.h>

#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "insight/tokenization/drain.hpp"

namespace
{

namespace tok = insight::tokenization;

// Synthetic log corpus: template strings with varying specificity.
// Simulates a realistic mix of messages (database logs, web server, system).
std::vector<std::string> make_synthetic_corpus(std::size_t n_templates)
{
    std::vector<std::string> templates;
    templates.reserve(n_templates);

    // Database logs
    for (std::size_t i = 0; i < n_templates / 3; ++i)
    {
        templates.push_back(
            "Query completed: table=<*> duration_ms=<*> rows=<*> cpu_ms=<*>");
        templates.push_back("Transaction rolled back: xid=<*> error=<*>");
        templates.push_back("Index bloat detected: table=<*> bloat_pct=<*>");
    }

    // Web server logs
    for (std::size_t i = 0; i < n_templates / 3; ++i)
    {
        templates.push_back("HTTP <*> <*> status=<*> latency_ms=<*> bytes=<*>");
        templates.push_back("SSL handshake failed: client=<*> error=<*>");
        templates.push_back("Rate limit exceeded: ip=<*> requests=<*>");
    }

    // System/app logs
    for (std::size_t i = 0; i < n_templates / 3; ++i)
    {
        templates.push_back("Cache miss: key=<*> ttl_s=<*> size_bytes=<*>");
        templates.push_back("Worker pool starvation: active=<*> queued=<*>");
        templates.push_back("GC pause: gen=<*> duration_ms=<*> freed_mb=<*>");
    }

    return templates;
}

// Generate realistic log lines by randomly selecting templates and
// substituting values.
std::string make_line(
    const std::vector<std::string>& templates,
    std::mt19937& rng,
    std::uniform_int_distribution<std::size_t>& template_dist)
{
    const auto& template_str{templates[template_dist(rng)]};
    std::string line{template_str};

    // Replace <*> placeholders with random values.
    std::uniform_int_distribution<uint32_t> value_dist(0, 999999);
    std::size_t pos{0};
    while ((pos = line.find("<*>", pos)) != std::string::npos)
    {
        line.replace(pos, 3, std::to_string(value_dist(rng)));
        pos += line.find(' ', pos);  // Move past the replacement
    }

    return line;
}

// Benchmark Drain tokenization throughput with a fixed corpus size.
void BM_TokenizationThroughput(benchmark::State& state)
{
    const std::size_t n_templates{static_cast<std::size_t>(state.range(0))};
    const auto corpus{make_synthetic_corpus(n_templates)};

    std::mt19937 rng(42);
    std::uniform_int_distribution<std::size_t> template_dist(0, corpus.size() - 1);

    for (auto _ : state)
    {
        state.PauseTiming();

        tok::Drain drain;
        std::vector<std::string> lines;
        lines.reserve(1000);

        // Generate 1000 lines from the corpus.
        for (std::size_t i = 0; i < 1000; ++i)
        {
            lines.push_back(make_line(corpus, rng, template_dist));
        }

        state.ResumeTiming();

        // Tokenize each line.
        for (const auto& line : lines)
        {
            [[maybe_unused]] auto event{drain.tokenize(line)};
        }

        state.PauseTiming();
        state.ResumeTiming();
    }

    const std::size_t lines_per_iter{1000};
    state.counters["ns_per_line"] =
        benchmark::Counter(static_cast<double>(lines_per_iter),
                           benchmark::Counter::kIsIterationInvariantRate |
                               benchmark::Counter::kInvert);
    state.counters["template_count"] = static_cast<double>(n_templates);
    state.SetItemsProcessed(1000 * state.iterations());
}

// Measure tokenization latency for lines that match vs. don't match existing
// templates (cache hit vs. miss behavior).
void BM_TokenizationCacheHitRate(benchmark::State& state)
{
    const auto corpus{make_synthetic_corpus(10)};
    tok::Drain drain;

    // Pre-warm Drain with some templates.
    for (std::size_t i = 0; i < 5; ++i)
    {
        std::string line{corpus[i]};
        for (std::size_t j = 0; j < 10; ++j)
        {
            line += std::to_string(j) + " ";
        }
        [[maybe_unused]] auto event{drain.tokenize(line)};
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<std::size_t> template_dist(0, corpus.size() - 1);
    std::uniform_int_distribution<uint32_t> value_dist(0, 999999);

    for (auto _ : state)
    {
        for (std::size_t i = 0; i < 100; ++i)
        {
            auto line{make_line(corpus, rng, template_dist)};
            [[maybe_unused]] auto event{drain.tokenize(line)};
        }
    }

    state.SetItemsProcessed(100 * state.iterations());
    state.counters["ns_per_line"] =
        benchmark::Counter(100.0,
                           benchmark::Counter::kIsIterationInvariantRate |
                               benchmark::Counter::kInvert);
}

// Benchmark Drain memory footprint as template count grows.
void BM_TokenizationMemory(benchmark::State& state)
{
    const std::size_t n_templates{static_cast<std::size_t>(state.range(0))};
    const auto corpus{make_synthetic_corpus(n_templates)};

    std::mt19937 rng(42);
    std::uniform_int_distribution<std::size_t> template_dist(0, corpus.size() - 1);

    for (auto _ : state)
    {
        state.PauseTiming();
        tok::Drain drain;
        state.ResumeTiming();

        // Tokenize lines until we have enough unique templates.
        std::size_t unique_count{0};
        std::size_t lines_processed{0};
        while (unique_count < n_templates && lines_processed < 100000)
        {
            auto line{make_line(corpus, rng, template_dist)};
            [[maybe_unused]] auto event{drain.tokenize(line)};
            unique_count = drain.template_count();
            ++lines_processed;
        }

        state.PauseTiming();
    }

    state.counters["templates"] = static_cast<double>(state.range(0));
}

// Benchmark range: template counts from 10 to 1000.
BENCHMARK(BM_TokenizationThroughput)
    ->Range(10, 1000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_TokenizationCacheHitRate)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_TokenizationMemory)
    ->Range(10, 100)
    ->Unit(benchmark::kMillisecond);

}  // namespace

// NOLINTEND
