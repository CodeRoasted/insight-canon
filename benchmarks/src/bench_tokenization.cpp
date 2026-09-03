// Tokenization throughput benchmark — two arms (ADR-17 / SRC-SP-5).
//
// Measures end-to-end Tokenizer::process_line() cost — stateless per-line template
// masking plus arena-backed CanonicalEvent emission — on a synthetic Zipf-ish corpus.
//
//   * BM_TokenizationThroughput            — the COMPOSED set (github + test_frameworks):
//     the gate metric. This is the shape every product binary runs.
//   * BM_TokenizationThroughputDegenerate  — compose({}): the format-partition control.
//     The corpus carries no CI-dialect content, so the composed-vs-degenerate delta on it is
//     what measures SRC-SP-5 — see canon.compose.cppm for the contract. Expected delta: noise.
//     A non-noise delta is a mechanism regression here, never a benchmark to re-baseline.
//
// Reported metrics:
//   * `items_per_second`  — log lines tokenized per wall second
//   * `ns_per_line`       — nanoseconds per line (lower is better)
//
// Architectural target (technical_docs/overview/architecture.md):
//   Steady-state per-line cost ≤ 1 µs at 32 templates.

#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// External module consumption (the det_proof shape): the measured object is the shipped
// library build, not a module-member rebuild.
import insight.canon;
import insight.semantic.github;
import insight.semantic.gitlab;
import insight.semantic.jenkins;
import insight.semantic.test_frameworks;

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

// The NON-OTEL NESTED-JSON workload (DN-29.D9's measure-first gate). The two arms above are
// plaintext KV lines: they never reach JsonStrategy, so they cannot see the cost of the OTLP
// document probe at all. This corpus is the population that DOES pay it — structured JSON whose
// nesting and escapes defeat the escape-free fast path, so every line lands on the simdjson slow
// path where the probe sits, while carrying no OTEL field whatsoever.
//
// Lines are deliberately LONG (a nested context object + an escaped message): the quantity under
// measurement is a whole-line scan versus a bounded prefix read, and on a short line the two are
// the same thing. This is the honest shape of an application's structured logging, not a
// worst case built to flatter the bound.
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

// Shared body: both arms run the identical corpus/loop; only the composition differs.
void run_throughput(benchmark::State& state, const insight::semantic::ComposedSemantics& composed)
{
    const auto n_templates{static_cast<std::size_t>(state.range(0))};
    constexpr std::size_t kLinesPerIter{1'000};

    const auto corpus{make_corpus(n_templates, kLinesPerIter, 42)};

    tok::ArenaAllocator arena{1U << 20U};
    tok::Tokenizer tokenizer{arena, tok::MaskConfig{}, composed};

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
        // the measurement. The masker is stateless — no template state to preserve.
        state.PauseTiming();
        arena.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * kLinesPerIter));
    state.counters["ns_per_line"] = benchmark::Counter(
        static_cast<double>(kLinesPerIter),
        benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert);
}

// The gate arm — the product composition (the det_proof set, built once, loop-invariant).
void BM_TokenizationThroughput(benchmark::State& state)
{
    static const std::array<insight::semantic::SemanticPackageManifest, 4> kManifests{
        insight::semantic::github::kManifest, insight::semantic::gitlab::kManifest,
        insight::semantic::jenkins::kManifest, insight::semantic::test_frameworks::kManifest};
    static const insight::semantic::ComposedSemantics composed{
        insight::semantic::compose(kManifests)};
    run_throughput(state, composed);
}

// The control arm — core-only. Delta vs the gate arm on this non-dialect corpus = the
// SRC-SP-5 composition overhead (expected: noise; dialect rows are format-partitioned).
void BM_TokenizationThroughputDegenerate(benchmark::State& state)
{
    static const insight::semantic::ComposedSemantics composed{insight::semantic::compose({})};
    run_throughput(state, composed);
}

// The non-OTEL nested-JSON arm. Not a gate metric and NOT part of the canon-#1 ordering
// assertion — it is the population the JSON slow-path probes are charged to, measured on its own
// so a probe's cost is attributable instead of diluted into a plaintext average.
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
