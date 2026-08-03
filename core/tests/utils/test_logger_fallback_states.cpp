// NOLINTBEGIN — unit test: short identifiers and test-specific patterns are fine.
//
// test_logger_fallback_states.cpp — the accessor fallback is TWO states, and the whole point of
// the change under test is that they stopped being the same state.
//
//   (A) init_logging() never ran. Nothing is registered, the default logger is all there is, and
//       falling back is CORRECT. This is the unit-test path of every suite in the workspace, and
//       it MUST stay silent.
//   (B) init_logging() ran and the NAME is still unregistered. A programming error — the shape
//       that hid `kPipelineLogger`'s absence for weeks, because the old fallback degraded to the
//       default logger identically in both states and nothing at runtime could tell them apart.
//
// The sibling suite (test_logger_registration.cpp) pins that each declared name RESOLVES to a
// logger of its own name. That is the presence check. This suite pins what happens when presence
// FAILS — the two behaviours the fix introduced — and neither suite can see the other's property:
// registration is about the normal path, this is about the two degraded ones.
//
// WHY NO TEST SEAM. State (B) is reachable through the public spdlog registry alone:
// `spdlog::drop(name)` after a successful `init_logging()` leaves the facade initialised with that
// one name missing, which is state (B) exactly. Production code therefore grows no hook, no
// injectable flag, no `#ifdef TESTING` — the test drives the product through the same registry the
// product uses.
//
// THE ANTI-VACUITY DEVICE — read this before trusting any green here. Two of the three arms assert
// ZERO records, and a zero is exactly what a sink that never attached, a level that filters the
// record out, or a WARN elided at compile time would also produce. Each such arm therefore ends by
// emitting a CANARY through `INSIGHT_LOG_WARN` — the same macro, the same
// `detail::log_message`, the same level, the same logger object the facade's own report would
// take — and requires it to land. A silent arm can only pass on a probe that has just demonstrated
// it can hear. (This is why <utils/log_macros.hpp> is included in a test that logs nothing of its
// own: the canary must ride the product's macro layer, elision threshold included, not spdlog
// directly.)
//
// ── THE PROCESS-GLOBAL ORDERING HAZARD AND HOW IT IS RESOLVED ────────────────────────────────
// Three pieces of state in logger.cpp are process-global and ONE-WAY: the `std::call_once` flag,
// the `initialised()` atomic, and the memo of already-reported names. Consequences, and the
// answer to each:
//
// 1. STATE (A) IS UNREACHABLE ONCE ANY TEST HAS INITIALISED. `call_once` cannot be rewound, so a
//    plain TEST asserting silence would be asserting it about whatever state the tests that
//    happened to run earlier left behind — green by accident under ctest (gtest_discover_tests
//    gives one process per case) and meaningless under a direct `./insight_canon_tests` run. It is
//    therefore run in a CHILD PROCESS, and the death-test style is set to "threadsafe" as a
//    LOAD-BEARING choice, not hygiene: the default "fast" style forks, and a fork inherits the
//    parent's already-true `initialised()` and its already-populated registry, so a forked child
//    is never in state (A). "threadsafe" re-EXECs the binary with a filter naming only this test,
//    which is the only way to get a genuinely virgin process out of this binary. The child asserts
//    its own precondition (no module name registered) and reports through its exit code and
//    stderr, both of which gtest surfaces on failure.
//
// 2. THE DROP IS FULLY REVERSED, AND WITH THE SAME OBJECT. `RegistrationHold` saves the
//    `shared_ptr` before dropping and re-registers THAT pointer, so the restored logger is the
//    identical object — same sinks, same level, same pattern. spdlog's `register_logger` only
//    inserts into the map (it is `initialize_logger`, which this does not call, that would re-clone
//    the registry formatter), so nothing about the logger is reconstructed approximately. State (B)
//    thus leaves no trace another suite can observe, and this file is order-independent with
//    respect to every other suite in the binary.
//
// 3. THE MEMO IS THE ONE RESIDUE, AND IT IS THE PROPERTY ITSELF. `report_unregistered_once`
//    remembers a reported name for the life of the process, so a name can be used by exactly ONE
//    state-(B) observation per process. That is why both names live in a single TEST rather than
//    one test each: split across two TESTs they would be order-dependent through the memo, and a
//    correct implementation could red. The same residue makes this file incompatible with
//    `--gtest_repeat` on the raw binary (the second repetition of the state-(B) test sees a primed
//    memo and observes zero warnings). That is the design being pinned, not a flake: ctest re-runs
//    the PROCESS, so `ctest --repeat` is unaffected.
//
// RED-FIRST RECIPE (for the run that carries this suite) — in
// insight-canon/core/src/utils/logger.cpp:
//   * delete the `report_unregistered_once(name)` call in `logger_for`
//       → StateB arm reds: "expected EXACTLY 1 … got 0" for BOTH names.
//   * drop the `if (initialised())` guard so the report fires unconditionally
//       → StateA arm reds: child exits 2 with SEVEN records, one per name (the memo still caps the
//         flood — this mutation removes the state split, not the once-ness). That is the
//         regression that would make every unit test in the workspace noisy.
//   * point the WARN at a named logger instead of `spdlog::default_logger()`
//       → StateB arm reds at 0 records for both names: the only logger guaranteed to exist in a
//         degraded facade is the default one, and a report the probe cannot hear is a report an
//         operator cannot either.
//   * replace the per-name memo with a single `static std::once_flag` (report once GLOBALLY)
//       → StateB arm reds on the SECOND name only: 1 record for insight.pipeline, 0 for
//         insight.mask — the "once per name" half, which a single-name test cannot see.
//   * remove the memo entirely (warn on every call)
//       → StateB arm reds at 8 records per name, naming the flood the design refuses.
//   * store `initialised()` FIRST in the call_once lambda instead of last
//       → no arm reds here (the window is intra-lambda); noted so the run is not read as covering
//       it.
//   * make `logger_for` return the named logger without consulting the registry (or delete the
//     WARN's `kAllLoggers` remediation clause)
//       → StateB content arms red on the missing substring, with the payload printed.
//
// Determinism: no RNG, no threads, no wall clock, no timestamps in any assertion — the capture
// sink stores raw payloads, never formatted lines. Record ORDER is never asserted (the two names
// are interleaved on purpose and either may land first); only per-name counts and payload content
// are. The capture sink is null-mutex because the suite is single-threaded by construction. The
// state-(A) child is a re-exec of this same binary, so it is as deterministic as the parent.

#include <gtest/gtest.h> // textual std-pulling include FIRST (precedes imports)
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h> // get / drop / register_logger / default_logger — the PUBLIC registry
#include <utils/log_macros.hpp> // the canary rides the product's macro layer, elision included

import insight.canon.test;

namespace
{
using namespace insight::logging;

// How many times each accessor is called inside one observation. Any value above one distinguishes
// "once per offending name" from "once per call"; eight makes a per-call flood unmistakable in the
// failure message.
constexpr int kCallsPerAccessor{8};

// The child's verdict channel. Exit codes are named because the parent's failure text quotes the
// number and a bare integer there is unreadable.
constexpr int kChildSilent{0};
constexpr int kChildSpoke{2};
constexpr int kChildPreconditionViolated{3};
constexpr int kChildProbeDeaf{4};

// Matched against the child's stderr by the death-test matcher (a regex — kept alphanumeric and
// hyphenated so it carries no metacharacters).
constexpr std::string_view kStateASilentMarker{"STATE-A-FACADE-SILENT-OK"};

// The canary's token. Unique enough that it can never be confused with a facade record.
constexpr std::string_view kCanaryToken{"kleio-probe-canary"};

// Substrings the state-(B) warning must carry. `kAllLoggers` is an IDENTIFIER, not prose: the
// remediation clause can be reworded freely, but the symbol it points at cannot change without the
// symbol itself being renamed — the same reason the n-gram cap suite anchors on `max_ngram_keys`.
// "NOT REGISTERED" is the state claim an operator greps for; it is the one phrase that must survive
// a rewrite, because a warning that does not say WHICH of the two fallback states fired is the
// very ambiguity this change removed.
constexpr std::string_view kStateClaim{"NOT REGISTERED"};
constexpr std::string_view kRemediationAnchor{"kAllLoggers"};

// The accessors, hand-enumerated exactly as the sibling registration suite enumerates them and for
// the same reason: iterating `kAllLoggers` to reach them is impossible (there is no name→accessor
// map in the product), and the independent list is what keeps a dropped name visible. The
// static_assert is the compile-time half of that suite's runtime anti-drift arm — an eighth logger
// cannot be added without this file failing to compile.
constexpr std::array kAccessors{&arena_logger,    &mask_logger,   &pipeline_logger,
                                &detector_logger, &parser_logger, &strategy_logger,
                                &tokenizer_logger};
static_assert(kAccessors.size() == kAllLoggers.size(),
              "kAllLoggers gained or lost a name: add or remove the matching accessor above, or "
              "this suite silently stops covering it");

// Records what landed, unformatted — the pattern is a presentation choice and pinning it would
// hold a property this suite has no business holding (and would drag a timestamp into a
// deterministic assertion).
class CapturingSink final : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
{
  public:
    struct Record
    {
        std::string logger_name;
        spdlog::level::level_enum level{spdlog::level::off};
        std::string payload;
    };

    [[nodiscard]] const std::vector<Record>& records() const noexcept
    {
        return records_;
    }

  protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        records_.push_back(
            Record{.logger_name = std::string{msg.logger_name.data(), msg.logger_name.size()},
                   .level = msg.level,
                   .payload = std::string{msg.payload.data(), msg.payload.size()}});
    }
    void flush_() override {}

  private:
    std::vector<Record> records_;
};

[[nodiscard]] std::string dump(const std::vector<CapturingSink::Record>& records)
{
    std::string out{"captured " + std::to_string(records.size()) +
                    " record(s) on the default logger:"};
    for (const CapturingSink::Record& record : records)
        out += "\n  [" + record.logger_name + "] [" +
               std::string{spdlog::level::to_string_view(record.level).data(),
                           spdlog::level::to_string_view(record.level).size()} +
               "] " + record.payload;
    if (records.empty())
        out += " (none)";
    return out;
}

// Listens on `spdlog::default_logger()` — the object BOTH degraded states resolve to, and the one
// `report_unregistered_once` writes its warning on. Never on a named logger: in state (A) no named
// logger exists at all, and in state (B) the name in question has just been dropped, so a probe
// attached by name would hear nothing in either case and every arm here would be vacuous.
//
// Constructed inside each test AFTER that test's `init_logging()` call, never in a fixture SetUp:
// `init_logging` ends with `spdlog::set_level`, which walks the registry — the default logger
// included — and would silently undo a level set beforehand. Whether it actually runs (first call
// in the process) or no-ops (a sibling suite got there first) would then decide the probe's level,
// making the observation depend on test order. Constructing after removes that entirely.
class DefaultLoggerProbe
{
  public:
    DefaultLoggerProbe()
        : logger_{spdlog::default_logger()}, sink_{std::make_shared<CapturingSink>()},
          saved_sinks_{logger_->sinks()}, saved_level_{logger_->level()}
    {
        logger_->sinks().push_back(sink_);
        logger_->set_level(spdlog::level::trace);
    }

    // The default logger is process-wide state shared with every other suite in this binary:
    // restored exactly, not merely detached.
    ~DefaultLoggerProbe()
    {
        logger_->sinks() = saved_sinks_;
        logger_->set_level(saved_level_);
    }

    DefaultLoggerProbe(const DefaultLoggerProbe&) = delete;
    DefaultLoggerProbe& operator=(const DefaultLoggerProbe&) = delete;
    DefaultLoggerProbe(DefaultLoggerProbe&&) = delete;
    DefaultLoggerProbe& operator=(DefaultLoggerProbe&&) = delete;

    [[nodiscard]] const std::vector<CapturingSink::Record>& records() const noexcept
    {
        return sink_->records();
    }

    // Records whose payload names `name`. The module names are mutually non-containing, so a
    // record can belong to at most one of them.
    [[nodiscard]] std::vector<CapturingSink::Record> records_naming(std::string_view name) const
    {
        std::vector<CapturingSink::Record> matched;
        for (const CapturingSink::Record& record : records())
            if (record.payload.find(name) != std::string::npos)
                matched.push_back(record);
        return matched;
    }

    // Emits, through the product's own macro layer, a record that MUST be heard. See the
    // anti-vacuity note at the top: this is what separates "the facade said nothing" from "the
    // probe could not have heard anything anyway".
    void emit_canary()
    {
        INSIGHT_LOG_WARN(logger_, "{}", kCanaryToken);
    }

    [[nodiscard]] bool canary_landed() const
    {
        return !records_naming(kCanaryToken).empty();
    }

  private:
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<CapturingSink> sink_;
    std::vector<spdlog::sink_ptr> saved_sinks_;
    spdlog::level::level_enum saved_level_{spdlog::level::off};
};

// Unregisters a name and puts the SAME logger object back. See hazard note 2 at the top — the
// saved `shared_ptr` keeps the logger alive across the drop, so restoration reconstructs nothing.
class RegistrationHold
{
  public:
    explicit RegistrationHold(std::string_view name) : saved_{spdlog::get(std::string{name})}
    {
        if (saved_)
            spdlog::drop(std::string{name});
    }

    ~RegistrationHold()
    {
        if (saved_)
            spdlog::register_logger(saved_);
    }

    RegistrationHold(const RegistrationHold&) = delete;
    RegistrationHold& operator=(const RegistrationHold&) = delete;
    RegistrationHold(RegistrationHold&&) = delete;
    RegistrationHold& operator=(RegistrationHold&&) = delete;

    [[nodiscard]] bool holds_a_logger() const noexcept
    {
        return saved_ != nullptr;
    }

  private:
    std::shared_ptr<spdlog::logger> saved_;
};

// Sets the death-test style for the duration of one test and restores it. Other suites in this
// binary (compose, transport) run EXPECT_DEATH under the default style; flipping it globally would
// silently change how THEY spawn.
class DeathTestStyleHold
{
  public:
    explicit DeathTestStyleHold(const char* style) : saved_{GTEST_FLAG_GET(death_test_style)}
    {
        GTEST_FLAG_SET(death_test_style, style);
    }

    ~DeathTestStyleHold()
    {
        GTEST_FLAG_SET(death_test_style, saved_);
    }

    DeathTestStyleHold(const DeathTestStyleHold&) = delete;
    DeathTestStyleHold& operator=(const DeathTestStyleHold&) = delete;
    DeathTestStyleHold(DeathTestStyleHold&&) = delete;
    DeathTestStyleHold& operator=(DeathTestStyleHold&&) = delete;

  private:
    std::string saved_;
};

void call_every_accessor()
{
    for (int call{0}; call < kCallsPerAccessor; ++call)
        for (const auto accessor : kAccessors)
            (void)accessor();
}

[[noreturn]] void abandon_child(int code, const std::string& reason)
{
    // stderr, not stdout: the death-test child's stderr is the pipe the parent reads, and gtest
    // prints it verbatim under "Actual msg" when the exit code or the marker does not match.
    std::cerr << "\n[state-A child] " << reason << "\n";
    std::cerr.flush();
    // _Exit, not exit: atexit handlers here belong to whatever sanitizer or runtime the build
    // carries, and one of them turning a correct child into a non-zero exit would read as a
    // product failure. The child's only job is to report this verdict.
    std::_Exit(code);
}

// The body of the state-(A) observation. Runs in a re-exec'd child (see hazard note 1) where
// `init_logging()` has never been called by anyone. Deliberately NOT marked [[noreturn]] even
// though every path exits: the death-test macro emits a "the statement did not exit" abort right
// after the statement, and telling the compiler that code is unreachable only invites a warning
// about gtest's own machinery.
void observe_state_a()
{
    for (const auto name : kAllLoggers)
    {
        if (spdlog::get(std::string{name}) != nullptr)
            abandon_child(kChildPreconditionViolated,
                          "precondition violated: '" + std::string{name} +
                              "' is already registered, so this process is NOT in state (A) and "
                              "the silence below would be about a different state. Something ran "
                              "init_logging() before this test — check that the death-test style "
                              "is 'threadsafe' (a forked child inherits the parent's registry).");
    }

    DefaultLoggerProbe probe;
    call_every_accessor();
    const std::vector<CapturingSink::Record> observed{probe.records()};

    probe.emit_canary();
    if (!probe.canary_landed())
        abandon_child(kChildProbeDeaf,
                      "the probe never heard its own canary, so the emptiness it reports proves "
                      "nothing: the capture sink did not attach to spdlog::default_logger(), the "
                      "level filtered the record out, or INSIGHT_LOG_WARN is elided at compile "
                      "time in this build. Fix the probe before reading any silence here.");

    if (!observed.empty())
        abandon_child(kChildSpoke,
                      "an accessor SPOKE with init_logging() never called. This is the state where "
                      "falling back to the default logger is correct and silence is mandatory: "
                      "every unit test in the workspace logs before init, and a warning here makes "
                      "all of them noisy. Expected 0 record(s) from " +
                          std::to_string(kAccessors.size()) + " accessor(s) called " +
                          std::to_string(kCallsPerAccessor) + " time(s) each; " + dump(observed));

    std::cerr << "\n[state-A child] " << kStateASilentMarker << " — " << kAccessors.size()
              << " accessor(s) × " << kCallsPerAccessor << " call(s) produced 0 record(s), on a "
              << "probe that heard its own canary.\n";
    std::cerr.flush();
    std::_Exit(kChildSilent);
}

} // namespace

// ── State (B): the name is missing and the facade says so, ONCE, per NAME ─────────────────────
// The historically offending name is one of the two on purpose: `kPipelineLogger` is the one the
// impl unit's private copy of the list had lost. The second name is what makes "once per NAME"
// falsifiable — a global once-flag passes a single-name test and still leaves the second module's
// absence silent forever.
TEST(LoggerFallbackStateB, AnUnregisteredNameAfterInitWarnsExactlyOncePerName)
{
    init_logging(); // call_once — a no-op if a sibling suite got here first

    ASSERT_NE(spdlog::get(std::string{kPipelineLogger}), nullptr)
        << "'" << kPipelineLogger
        << "' must be registered before this test unregisters it — init_logging() did not create "
           "it, so this process is not in the state this test starts from";
    ASSERT_NE(spdlog::get(std::string{kMaskLogger}), nullptr)
        << "'" << kMaskLogger << "' must be registered before this test unregisters it";

    DefaultLoggerProbe probe;
    {
        const RegistrationHold pipeline_hold{kPipelineLogger};
        const RegistrationHold mask_hold{kMaskLogger};
        ASSERT_TRUE(pipeline_hold.holds_a_logger() && mask_hold.holds_a_logger())
            << "the holds must own the loggers they unregistered, or the registry cannot be put "
               "back and every later suite in this binary inherits a broken facade";
        ASSERT_EQ(spdlog::get(std::string{kPipelineLogger}), nullptr)
            << "the drop did not unregister '" << kPipelineLogger
            << "' — the facade is not in state (B) and this test would be measuring normal "
               "operation";
        ASSERT_EQ(spdlog::get(std::string{kMaskLogger}), nullptr)
            << "the drop did not unregister '" << kMaskLogger << "'";

        // Interleaved and repeated: the accessors are called from every log site of their module,
        // which is exactly the flood the once-per-name memo exists to prevent.
        for (int call{0}; call < kCallsPerAccessor; ++call)
        {
            (void)pipeline_logger();
            (void)mask_logger();
        }
    } // registration restored HERE — before any assertion can leave the function

    ASSERT_NE(spdlog::get(std::string{kPipelineLogger}), nullptr)
        << "the hold failed to re-register '" << kPipelineLogger
        << "'; every later suite in this binary would see a degraded facade";
    ASSERT_NE(spdlog::get(std::string{kMaskLogger}), nullptr)
        << "the hold failed to re-register '" << kMaskLogger << "'";

    const std::vector<CapturingSink::Record> pipeline{probe.records_naming(kPipelineLogger)};
    const std::vector<CapturingSink::Record> mask{probe.records_naming(kMaskLogger)};

    // THE LOAD-BEARING COUNT. One per offending name, whatever the call count.
    const std::string pipeline_reading{
        pipeline.empty() ? "the missing registration is SILENT again, which is the defect that hid "
                           "this name for weeks"
                         : "a per-call warning answers a silent degradation with a flood"};
    EXPECT_EQ(pipeline.size(), 1U)
        << "expected EXACTLY 1 warning for the unregistered '" << kPipelineLogger << "' after "
        << kCallsPerAccessor << " accessor call(s); got " << pipeline.size() << " — "
        << pipeline_reading << "\n"
        << dump(probe.records());

    const std::string mask_reading{
        mask.empty() && pipeline.size() == 1U
            ? "one name reported and the other not: the memo is GLOBAL rather than per-name, so "
              "the SECOND module to go missing stays silent forever"
            : "a per-call warning answers a silent degradation with a flood"};
    EXPECT_EQ(mask.size(), 1U) << "expected EXACTLY 1 warning for the unregistered '" << kMaskLogger
                               << "' after " << kCallsPerAccessor << " accessor call(s); got "
                               << mask.size() << " — " << mask_reading << "\n"
                               << dump(probe.records());

    // Nothing else was said. A generic extra line would be the flood in another costume.
    EXPECT_EQ(probe.records().size(), 2U)
        << "two unregistered names must produce exactly two records and nothing more\n"
        << dump(probe.records());

    if (pipeline.size() == 1U)
    {
        const CapturingSink::Record& warning{pipeline.front()};
        EXPECT_EQ(warning.level, spdlog::level::warn)
            << "an unregistered module logger is a WARN: it is a defect in the build, not a note\n"
            << dump(probe.records());
        EXPECT_NE(warning.payload.find(kStateClaim), std::string::npos)
            << "the warning must say WHICH fallback state fired — '" << kStateClaim
            << "' is what separates (B) from the legitimate silent (A); payload was:\n  "
            << warning.payload;
        EXPECT_NE(warning.payload.find(kRemediationAnchor), std::string::npos)
            << "the warning must name '" << kRemediationAnchor
            << "', the list to add the name to — a diagnostic that does not say what to do leaves "
               "the reader where the silence did; payload was:\n  "
            << warning.payload;
    }
}

// ── Normal operation: the facade never reports on itself when every name is present ───────────
// Not vacuous on its own: the canary requirement below means this arm can only pass on a probe
// that has just demonstrated it can hear a record of the very level and path the facade would use.
TEST(LoggerFallbackNormalOperation, NoNameIsReportedWhenEveryNameIsRegistered)
{
    init_logging();

    for (const auto name : kAllLoggers)
        ASSERT_NE(spdlog::get(std::string{name}), nullptr)
            << "'" << name
            << "' is not registered, so this process is in state (B) and this test cannot speak "
               "about normal operation — if the state-(B) suite ran first, its restoration failed";

    DefaultLoggerProbe probe;
    call_every_accessor();
    const std::vector<CapturingSink::Record> observed{probe.records()};

    probe.emit_canary();
    ASSERT_TRUE(probe.canary_landed())
        << "the probe never heard its own canary — the silence asserted below would prove nothing";

    EXPECT_TRUE(observed.empty())
        << "the facade must be silent about itself when every name resolves: " << kAccessors.size()
        << " accessor(s) called " << kCallsPerAccessor
        << " time(s) each produced a report on the default logger\n"
        << dump(observed);
}

// ── State (A): init_logging() never ran, and the accessors say nothing at all ─────────────────
// Runs in a re-exec'd child; the suite name ends in "DeathTest" both by gtest convention and to
// tell a reader at a glance that this one does not observe the parent process. See hazard note 1
// for why "threadsafe" is the mechanism and not a preference.
TEST(LoggerFallbackStateADeathTest, AccessorsStaySilentWhenInitLoggingNeverRan)
{
    const DeathTestStyleHold style{"threadsafe"};

    EXPECT_EXIT(observe_state_a(), ::testing::ExitedWithCode(kChildSilent),
                std::string{kStateASilentMarker})
        << "the child observes the facade with init_logging() never called. Exit " << kChildSpoke
        << " = an accessor spoke (every unit test in the workspace becomes noisy); exit "
        << kChildPreconditionViolated << " = the child was not in state (A) at all; exit "
        << kChildProbeDeaf
        << " = the probe could not hear its own canary, so its silence was vacuous. The child's "
           "own diagnostic is printed above under \"Actual msg\".";
}

// NOLINTEND
