// NOLINTBEGIN
#include <benchmark/benchmark.h>

// Entry point for Google Benchmark.
// Individual benchmark functions are defined in bench_*.cpp files.
int main(int argc, char** argv)
{
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
// NOLINTEND
