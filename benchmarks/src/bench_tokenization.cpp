// refs: ADR-17
#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

import insight.canon;
import insight.semantic.github;
import insight.semantic.gitlab;
import insight.semantic.jenkins;
import insight.semantic.test_frameworks;

namespace
{

namespace tok = insight::tokenization;

// invariant: `lines` owns its strings, so a view into one is valid while it lives.
struct SyntheticCorpus
{
    std::vector<std::string> lines;
};

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

// refs: DN-29.D9
SyntheticCorpus make_nested_json_corpus(std::size_t n_lines, std::uint32_t seed)
{
    static constexpr const char* kMessages[] = {
        "connection reset by peer while reading \\\"upstream\\\" response",
        "retry budget exhausted for shard \\\"primary\\\"; falling back",
        "schema validation failed: expected \\\"integer\\\", saw \\\"string\\\"",
        "lease renewal deadline exceeded, releasing \\\"write\\\" lock",
    };
    constexpr std::size_t kMessageCount{sizeof(kMessages) / sizeof(kMessages[0])};

    std::mt19937 rng{seed};
    std::uniform_int_distribution<std::size_t> msg_dist{0, kMessageCount - 1};
    std::uniform_int_distribution<std::uint32_t> val_dist{0, 999'999};

    SyntheticCorpus corpus;
    corpus.lines.reserve(n_lines);
    for (std::size_t i{0}; i < n_lines; ++i)
    {
        std::string line;
        line.reserve(512);
        line += R"({"ts":"2026-01-15T10:22:)";
        line += std::to_string(i % 60 < 10 ? 0 : (i % 60) / 10);
        line += std::to_string((i % 60) % 10);
        line += R"(Z","level":"warn","component":"orders-api","message":")";
        line += kMessages[msg_dist(rng)];
        line += R"(","context":{"request":{"id":")";
        line += std::to_string(val_dist(rng));
        line += R"(","method":"POST","path":"/api/v1/orders"},"upstream":{"host":"10.0.0.)";
        line += std::to_string(val_dist(rng) % 256);
        line += R"(","attempt":)";
        line += std::to_string(val_dist(rng) % 5);
        line += R"(,"latency_ms":)";
        line += std::to_string(val_dist(rng) % 4000);
        line += R"(}},"tags":["orders","retry","degraded"]})";
        corpus.lines.push_back(std::move(line));
    }
    return corpus;
}

void run_throughput(benchmark::State& state, const insight::semantic::ComposedSemantics& composed)
{
    const auto n_templates{static_cast<std::size_t>(state.range(0))};
    constexpr std::size_t kLinesPerIter{1'000};

    const auto corpus{make_corpus(n_templates, kLinesPerIter, 42)};

    tok::ArenaAllocator arena{1U << 20U};
    tok::Tokenizer tokenizer{arena, tok::MaskConfig{}, composed};

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
        state.PauseTiming();
        arena.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * kLinesPerIter));
    state.counters["ns_per_line"] = benchmark::Counter(
        static_cast<double>(kLinesPerIter),
        benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert);
}

void BM_TokenizationThroughput(benchmark::State& state)
{
    static const std::array<insight::semantic::SemanticPackageManifest, 4> kManifests{
        insight::semantic::github::kManifest, insight::semantic::gitlab::kManifest,
        insight::semantic::jenkins::kManifest, insight::semantic::test_frameworks::kManifest};
    static const insight::semantic::ComposedSemantics composed{
        insight::semantic::compose(kManifests)};
    run_throughput(state, composed);
}

// refs: SRC-SP-5
void BM_TokenizationThroughputDegenerate(benchmark::State& state)
{
    static const insight::semantic::ComposedSemantics composed{insight::semantic::compose({})};
    run_throughput(state, composed);
}

// refs: DN-29.D9
void BM_TokenizationThroughputNestedJson(benchmark::State& state)
{
    static const std::array<insight::semantic::SemanticPackageManifest, 4> kManifests{
        insight::semantic::github::kManifest, insight::semantic::gitlab::kManifest,
        insight::semantic::jenkins::kManifest, insight::semantic::test_frameworks::kManifest};
    static const insight::semantic::ComposedSemantics composed{
        insight::semantic::compose(kManifests)};

    constexpr std::size_t kLinesPerIter{1'000};
    const auto corpus{make_nested_json_corpus(kLinesPerIter, 42)};

    tok::ArenaAllocator arena{1U << 22U};
    tok::Tokenizer tokenizer{arena, tok::MaskConfig{}, composed};

    for (const auto& line : corpus.lines)
        benchmark::DoNotOptimize(tokenizer.process_line(line));
    arena.reset();

    for (auto _ : state)
    {
        for (const auto& line : corpus.lines)
            benchmark::DoNotOptimize(tokenizer.process_line(line));
        state.PauseTiming();
        arena.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * kLinesPerIter));
    state.counters["ns_per_line"] = benchmark::Counter(
        static_cast<double>(kLinesPerIter),
        benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert);
}

BENCHMARK(BM_TokenizationThroughput)->Arg(4)->Arg(8)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_TokenizationThroughputDegenerate)->Arg(4)->Arg(8)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_TokenizationThroughputNestedJson)->Unit(benchmark::kMicrosecond);

} // namespace
