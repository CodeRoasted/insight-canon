
// invariant: every declared module logger must resolve to a REGISTERED logger carrying its OWN
// name.
// invariant: the defect this stops was a hand-enumerated copy of the name list that had silently
// lost one name, so that accessor fell through to the default logger.
// invariant: every warning from that module then shipped untagged and unroutable by name — a
// lossiness warning that was itself silently lost.
// refs: ADR-9.D3
// invariant: this enumerates the ACCESSORS and not the name set, because iterating the set would
// make the test SELF-REFERENTIAL — a deleted name would vanish from both loops at once.
// invariant: that is the subject-equals-oracle degeneracy, and it is precisely the shape of the
// original bug: a list checked against a copy of itself.
// invariant: a null check is NOT the assertion — the fallback returns a perfectly valid default
// logger, so a null check would have passed throughout the period the bug was live.
// invariant: the assertion is on the logger's NAME.
#include <gtest/gtest.h>

import insight.canon.test;

namespace
{
using namespace insight::logging;

// invariant: the accessor list is written out by hand ON PURPOSE, for the reason in the header.
// invariant: the return type is deduced rather than spelled, because canon keeps its third-party
// logging surface private to its own build and importers get reachability without visibility.
// invariant: member access on the reachable type is all this needs, so nothing here names a
// third-party type.
[[nodiscard]] auto bound_loggers()
{
    return std::vector{
        std::pair{kArenaLogger, arena_logger()},
        std::pair{kMaskLogger, mask_logger()},
        std::pair{kPipelineLogger, pipeline_logger()},
        std::pair{kDetectorLogger, detector_logger()},
        std::pair{kParserLogger, parser_logger()},
        std::pair{kStrategyLogger, strategy_logger()},
        std::pair{kTokenizerLogger, tokenizer_logger()},
    };
}
} // namespace

TEST(LoggerRegistration, EveryDeclaredLoggerResolvesToItsOwnNameNotTheDefault)
{
    init_logging();

    for (const auto& [declared, logger] : bound_loggers())
    {
        ASSERT_NE(logger, nullptr) << "accessor for '" << declared << "' returned null";
        EXPECT_EQ(logger->name(), std::string{declared})
            << "logger for '" << declared << "' is actually named '" << logger->name()
            << "' — an UNREGISTERED name falls back to spdlog's default logger, so every message "
               "on this channel ships untagged and cannot be routed by name";
    }
}

// invariant: the anti-drift arm — a new name added without an accessor arm above would leave this
// file testing one fewer and still passing.
// invariant: pinning the COUNT forces the author of the next logger to visit this test.
TEST(LoggerRegistration, EveryNameInTheRegistrationSetHasAnAccessorArmHere)
{
    EXPECT_EQ(bound_loggers().size(), kAllLoggers.size())
        << "kAllLoggers has " << kAllLoggers.size() << " names but this test binds "
        << bound_loggers().size()
        << " accessors — add the missing accessor arm to bound_loggers() above, or this gate "
           "silently stops covering the new logger";
}
