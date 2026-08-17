// NOLINTBEGIN
// Unit tests: allow short identifiers and test-specific patterns
//
// Every declared module logger must resolve to a REGISTERED logger carrying its own name.
//
// The defect this gate exists to stop: the impl unit kept a hand-enumerated copy of the logger
// name list, that copy had silently lost `kPipelineLogger`, and `pipeline_logger()` therefore
// fell through to `spdlog::default_logger()`. Every pipeline WARN — the §13 cube-collapse one and
// the `max_ngram_keys` truncation one landed under ADR-9.D3 — shipped untagged and unroutable by
// name, invisible to any sink attached to "insight.pipeline". A lossiness warning that was itself
// silently lost.
//
// WHY THIS ENUMERATES THE ACCESSORS AND NOT `kAllLoggers` — the load-bearing choice. Iterating
// `kAllLoggers` here would make the test SELF-REFERENTIAL: deleting a name from the set removes it
// from `init_logging`'s loop AND from the test's loop in the same stroke, and the suite stays
// green. That is the SUT==ORACLE degeneracy, and it is precisely the shape of the original bug —
// a list checked against a copy of itself. The seven accessors are an INDEPENDENT enumeration, so
// a name dropped from `kAllLoggers` leaves its accessor falling back to the default logger and
// this test reds on the name mismatch.
//
// `EXPECT_NE(logger, nullptr)` is NOT the assertion: the fallback returns a perfectly valid
// default logger, so a null-check would have passed throughout the entire period the bug was live.
// The assertion is on the logger's NAME.

#include <gtest/gtest.h>

import insight.canon.test;

namespace
{
using namespace insight::logging;

// (declared constant, what the accessor actually handed back) — the accessor list is written out
// by hand ON PURPOSE; see the header note.
//
// The return type is deduced rather than spelled: `spdlog::logger` is included in canon's global
// module fragment, so importers get reachability but not visibility, and canon deliberately keeps
// its spdlog surface private to its own build (canon.api.cppm's header note). Member access on the
// reachable type is all this test needs, so nothing here names a third-party type.
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
    init_logging(); // defaults to info; call_once — a no-op if another suite got here first

    for (const auto& [declared, logger] : bound_loggers())
    {
        ASSERT_NE(logger, nullptr) << "accessor for '" << declared << "' returned null";
        EXPECT_EQ(logger->name(), std::string{declared})
            << "logger for '" << declared << "' is actually named '" << logger->name()
            << "' — an UNREGISTERED name falls back to spdlog's default logger, so every message "
               "on this channel ships untagged and cannot be routed by name";
    }
}

// The anti-drift arm: a new logger constant added to `kAllLoggers` without an accessor arm above
// would leave this file testing six of seven and still passing. Pinning the count forces the
// author of the eighth logger to visit this test.
TEST(LoggerRegistration, EveryNameInTheRegistrationSetHasAnAccessorArmHere)
{
    EXPECT_EQ(bound_loggers().size(), kAllLoggers.size())
        << "kAllLoggers has " << kAllLoggers.size() << " names but this test binds "
        << bound_loggers().size()
        << " accessors — add the missing accessor arm to bound_loggers() above, or this gate "
           "silently stops covering the new logger";
}
// NOLINTEND
