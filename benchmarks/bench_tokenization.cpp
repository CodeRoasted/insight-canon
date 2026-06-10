// NOLINTBEGIN
// Tokenization throughput benchmark.
//
// Measures end-to-end Tokenizer::process_line() cost — Drain template mining
// plus arena-backed CanonicalEvent emission — on a synthetic Zipf-ish corpus.
//
// Reported metrics:
//   * `items_per_second`  — log lines tokenized per wall second
//   * `ns_per_line`       — nanoseconds per line (lower is better)
//   * `templates_seen`    — distinct Drain clusters at end of run
//
// Architectural target (technical_docs/overview/architecture.md):
//   Steady-state per-line cost ≤ 1 µs at 32 templates / Drain depth 4.
//
// Modeled after insight-metalog/benchmarks/bench_metalog.cpp (Phase 3 anchor).

#include <benchmark/benchmark.h>

import insight.canon.bench;

namespace
{

namespace tok = insight::tokenization;

// Owns the synthetic line strings so string_views inside the tokenizer
// remain valid for the duration of the iteration.
struct SyntheticCorpus
{
    std::vector<std::string> lines;
};

// Generate a Zipf-ish workload: a small set of templates with random
// numeric values substituted in. Roughly mimics database/HTTP/system logs.
SyntheticCorpus make_corpus(std::size_t n_templates, std::size_t n_lines, std::uint32_t seed)
{
    static constexpr const char* kTemplates[] = {
        "Query completed table=users duration_ms={} rows={}",
        "Transaction rolled back xid={} error_code={}",
        "HTTP request method=GET path=/api/v1 status={} latency_ms={}",
        "Cache miss key={} ttl_s={} size_bytes={}",
        "Worker pool starvation active={} queued={}",
        "GC pause gen={} duration_ms={} freed_mb={}",
        "SSL handshake failed client=10.0.0.{} error={}",
        "Rate limit exceeded ip=10.0.0.{} requests={}",
    };
    constexpr std::size_t kTemplateCount{sizeof(kTemplates) / sizeof(kTemplates[0])};
    const std::size_t templates{n_templates < kTemplateCount ? n_templates : kTemplateCount};

    std::mt19937 rng{seed};
    std::uniform_int_distribution<std::size_t> tmpl_dist{0, templates - 1};
    std::uniform_int_distribution<std::uint32_t> val_dist{0, 999'999};

    SyntheticCorpus corpus;
    corpus.lines.reserve(n_lines);
    for (std::size_t i{0}; i < n_lines; ++i)
    {
        std::string base{kTemplates[tmpl_dist(rng)]};
        std::string out;
        out.reserve(base.size() + 32);
        for (std::size_t p{0}; p < base.size(); ++p)
        {
            if (p + 1 < base.size() && base[p] == '{' && base[p + 1] == '}')
            {
                out += std::to_string(val_dist(rng));
                ++p;
            }
            else
            {
                out += base[p];
            }
        }
        corpus.lines.push_back(std::move(out));
    }
    return corpus;
}

void BM_TokenizationThroughput(benchmark::State& state)
{
    const auto n_templates{static_cast<std::size_t>(state.range(0))};
    constexpr std::size_t kLinesPerIter{1'000};

    const auto corpus{make_corpus(n_templates, kLinesPerIter, 42)};

    tok::ArenaAllocator arena{1U << 20U};
    tok::Tokenizer tokenizer{arena, tok::DrainConfig{}};

    // Warm up so the steady-state path dominates.
    for (const auto& line : corpus.lines)
    {
        benchmark::DoNotOptimize(tokenizer.process_line(line));
    }
    arena.reset();

    for (auto _ : state)
    {
        for (const auto& line : corpus.lines)
        {
            benchmark::DoNotOptimize(tokenizer.process_line(line));
        }
        // Reset arena between iterations to avoid unbounded growth dominating
        // the measurement; Drain state is preserved (steady-state regime).
        state.PauseTiming();
        arena.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * kLinesPerIter));
    state.counters["ns_per_line"] = benchmark::Counter(
        static_cast<double>(kLinesPerIter),
        benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert);
    state.counters["templates_seen"] = static_cast<double>(tokenizer.cluster_count());
}

BENCHMARK(BM_TokenizationThroughput)->Arg(4)->Arg(8)->Unit(benchmark::kMicrosecond);

} // namespace
// NOLINTEND
