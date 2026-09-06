// refs: ADR-17
#include <benchmark/benchmark.h>
#include <spdlog/common.h>

import insight.canon;

int main(int argc, char** argv)
{
    // assert: this may execv the process image, so no work may precede it.
    benchmark::MaybeReenterWithoutASLR(argc, argv);

    // assert: no other call to init_logging runs in this binary, so this level takes.
    // refs: DN-53.D7
    insight::logging::init_logging(spdlog::level::off);

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
    {
        return 1;
    }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
