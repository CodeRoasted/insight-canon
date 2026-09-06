
// invariant: the accessor fallback is TWO states, and the change under test is that they stopped
// being the same state.
// invariant: state A is that init_logging never ran — nothing is registered, falling back is
// CORRECT, and it is the unit-test path of every suite in the workspace, so it MUST stay silent.
// invariant: state B is that init_logging ran and the NAME is unregistered — a programming error,
// and the shape that hid one name for weeks because both states degraded identically.
// invariant: the sibling registration suite pins that each declared name RESOLVES; this pins what
// happens when presence FAILS, and neither suite can see the other's property.
// invariant: NO test seam is needed — dropping a name from the public registry after a successful
// init IS state B, so production code grows no hook, no flag and no test-only branch.
// invariant: two of the three arms assert ZERO records, which is also what a detached sink, a
// filtering level or an elided macro would produce.
// invariant: so each such arm ends by emitting a CANARY through the product's own macro layer, the
// same level and the same logger object, and requires it to land.
// invariant: a silent arm can only pass on a probe that has just demonstrated it can hear.
// refs: MEM:synthetic-gate-vacuity-vs-judgment
// invariant: three pieces of state in the implementation are process-global and ONE-WAY — the
// call-once flag, the initialised atomic, and the memo of already-reported names.
// invariant: each of the three is answered at the entity that answers it, below.
// refs: DN-53.D1, DN-53.D3, DN-53.D6
#include <gtest/gtest.h>
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>
#include <utils/log_macros.hpp>

import insight.canon.test;

namespace
{
using namespace insight::logging;

// invariant: any value above one distinguishes once-per-offending-NAME from once-per-CALL, and
// eight makes a per-call flood unmistakable in the failure message.
constexpr int kCallsPerAccessor{8};

// invariant: the child's verdict channel — the codes are NAMED because the parent's failure text
// quotes the number, and a bare integer there is unreadable.
constexpr int kChildSilent{0};
constexpr int kChildSpoke{2};
constexpr int kChildPreconditionViolated{3};
constexpr int kChildProbeDeaf{4};

constexpr std::string_view kStateASilentMarker{"STATE-A-FACADE-SILENT-OK"};

// invariant: the stream arm's codes are DISJOINT from the self-report arm's, so a parent failure
// names ONE property rather than one-of-the-state-A-tests.
constexpr int kChildStdoutClean{0};
constexpr int kChildWroteToStdout{5};
constexpr int kChildStdoutProbeDeaf{6};
constexpr int kChildLoggerSinkless{7};
constexpr int kChildRecordInaudible{8};

constexpr std::string_view kStateAStdoutCleanMarker{"STATE-A-STDOUT-CLEAN-OK"};

// invariant: the control write goes through the exact writer state A falls back to, and its bytes
// must be in the capture or nothing measured below is a measurement.
constexpr std::string_view kHostDefaultToken{"kleio-p1-host-default-canary"};

// invariant: the measured token carries the accessor's INDEX, so a line that leaks onto the stream
// names which accessor produced it instead of only proving that one did.
constexpr std::string_view kModuleRecordToken{"kleio-p1-module-record"};

constexpr std::string_view kCanaryToken{"kleio-probe-canary"};

// invariant: the remediation anchor is an IDENTIFIER and not prose — the clause can be reworded
// freely, but the symbol cannot change without the symbol itself being renamed.
// invariant: the state claim is the phrase an operator greps for, and it must survive a rewrite
// because a warning that does not say WHICH fallback state fired is the ambiguity this removed.
constexpr std::string_view kStateClaim{"NOT REGISTERED"};
constexpr std::string_view kRemediationAnchor{"kAllLoggers"};

// invariant: the accessors are hand-enumerated for the same reason the sibling suite enumerates
// them — there is no name-to-accessor map, so an independent list is what keeps a drop visible.
// invariant: the static assertion is the compile-time half of that anti-drift arm: an eighth logger
// cannot be added without this file failing to compile.
constexpr std::array kAccessors{&arena_logger,    &mask_logger,   &pipeline_logger,
                                &detector_logger, &parser_logger, &strategy_logger,
                                &tokenizer_logger};
static_assert(kAccessors.size() == kAllLoggers.size(),
              "kAllLoggers gained or lost a name: add or remove the matching accessor above, or "
              "this suite silently stops covering it");

// invariant: records are kept UNFORMATTED — the pattern is a presentation choice, and pinning it
// would hold a property this suite has no business holding and drag a clock into an assertion.
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

// invariant: the stream may legitimately carry escapes and newlines, and pasting either raw into a
// diagnostic makes the failure unreadable at best and repaints the reader's terminal at worst.
// invariant: a test that is verbose on failure has to stay LEGIBLE on failure.
[[nodiscard]] std::string escape_bytes(std::string_view bytes)
{
    constexpr std::string_view kHexDigits{"0123456789abcdef"};
    constexpr unsigned char kFirstPrintable{0x20};
    constexpr unsigned char kDelete{0x7F};
    constexpr unsigned int kNibbleBits{4U};
    constexpr unsigned int kNibbleMask{0x0FU};

    std::string out;
    out.reserve(bytes.size());
    for (const char chr : bytes)
    {
        const auto raw{static_cast<unsigned char>(chr)};
        if (chr == '\n')
            out += "\\n";
        else if (chr == '\r')
            out += "\\r";
        else if (raw < kFirstPrintable || raw == kDelete)
        {
            out += "\\x";
            out += kHexDigits[(raw >> kNibbleBits) & kNibbleMask];
            out += kHexDigits[raw & kNibbleMask];
        }
        else
            out += chr;
    }
    return out;
}

// post: a capture sink attached to ONE logger for ONE observation, with its sink list restored
// exactly.
// invariant: it is the OVER-REACH control — it answers whether the record survived the logger at
// all, which a stream-purity assertion cannot ask and a do-nothing implementation would pass.
// invariant: the logger's LEVEL is deliberately never touched, because a level that filters the
// record out is one of the two do-nothing shapes this exists to catch.
// refs: MEM:synthetic-gate-vacuity-vs-judgment
class AttachedProbe
{
  public:
    explicit AttachedProbe(std::shared_ptr<spdlog::logger> logger)
        : logger_{std::move(logger)}, sink_{std::make_shared<CapturingSink>()},
          saved_sinks_{logger_->sinks()}
    {
        logger_->sinks().push_back(sink_);
    }

    ~AttachedProbe()
    {
        logger_->sinks() = saved_sinks_;
    }

    AttachedProbe(const AttachedProbe&) = delete;
    AttachedProbe& operator=(const AttachedProbe&) = delete;
    AttachedProbe(AttachedProbe&&) = delete;
    AttachedProbe& operator=(AttachedProbe&&) = delete;

    // invariant: ZERO own sinks means the record reaches nothing but the probe — audible to this
    // test and lost in production.
    [[nodiscard]] std::size_t own_sink_count() const noexcept
    {
        return saved_sinks_.size();
    }

    [[nodiscard]] std::size_t heard() const noexcept
    {
        return sink_->records().size();
    }

  private:
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<CapturingSink> sink_;
    std::vector<spdlog::sink_ptr> saved_sinks_;
};

[[nodiscard]] std::string indices(const std::vector<std::size_t>& values)
{
    if (values.empty())
        return "(none)";
    std::string out;
    for (const std::size_t value : values)
        out += (out.empty() ? "#" : ", #") + std::to_string(value);
    return out;
}

// invariant: the probe listens on the DEFAULT logger, the object BOTH degraded states resolve to
// and the one the report is written on.
// invariant: never on a named logger — in state A no named logger exists and in state B the name
// has just been dropped, so a probe attached by name would make every arm vacuous.
// invariant: constructed INSIDE each test after that test's init call and never in a fixture setup,
// because init ends by walking the registry and would silently undo a level set before.
// invariant: whether init actually runs or no-ops would then decide the probe's level, making the
// observation depend on test ORDER; constructing after removes that entirely.
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

    // invariant: the default logger is process-wide state shared with every other suite in this
    // binary, so it is RESTORED exactly rather than merely detached.
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

    // invariant: the module names are mutually non-containing, so a record can belong to at most
    // one of them.
    [[nodiscard]] std::vector<CapturingSink::Record> records_naming(std::string_view name) const
    {
        std::vector<CapturingSink::Record> matched;
        for (const CapturingSink::Record& record : records())
            if (record.payload.find(name) != std::string::npos)
                matched.push_back(record);
        return matched;
    }

    // invariant: the canary rides the PRODUCT's own macro layer, which is what separates the facade
    // said nothing from the probe could not have heard anything anyway.
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

// invariant: the drop is FULLY REVERSED and with the SAME object — the shared pointer is saved
// before dropping and re-registered, so the restored logger has the same sinks and level.
// invariant: registering only inserts into the map, so nothing about the logger is reconstructed
// approximately.
// invariant: state B therefore leaves NO trace another suite can observe, and this file is
// order-independent with respect to every other suite in the binary.
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

// invariant: the death-test style is set for the duration of ONE test and restored, because other
// suites in this binary run death tests under the default style.
// invariant: flipping it globally would silently change how THEY spawn.
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
    // invariant: stderr and not stdout — the child's stderr is the pipe the parent reads, and the
    // harness prints it verbatim when the exit code or the marker does not match.
    std::cerr << "\n[state-A child] " << reason << "\n";
    std::cerr.flush();
    // invariant: an immediate exit, not a normal one: the exit handlers here belong to whatever
    // sanitizer or runtime the build carries, and one of them could turn a correct child non-zero.
    // invariant: the child's only job is to report this verdict.
    std::_Exit(code);
}

// pre: runs in a re-exec'd child where init_logging has never been called by anyone.
// invariant: deliberately NOT marked as never-returning even though every path exits — the death
// macro emits its own abort right after, and marking it invites a warning about that.
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

// pre: runs in its OWN re-exec'd child — a second child, not a second assertion inside the first.
// invariant: this arm deliberately EMITS through the accessors, which is precisely what the
// self-report arm requires to be empty, so one child cannot hold both properties.
// invariant: merging them would trade a clean attribution for one saved process.
void observe_state_a_stdout()
{
    // pre: the capture MUST be the first statement, ahead of anything that could touch the logging
    // registry.
    // invariant: the default logger's stream sink binds to its target by a DIFFERENT mechanism per
    // platform — by NAME on one, by a handle cached at CONSTRUCTION on the other.
    // invariant: a redirect after construction is followed on the first and silently discarded on
    // the second, and that failure points the WRONG WAY: the leg would read a clean stream.
    // invariant: opening the capture first fixes both platforms with no conditional compilation and
    // no operating-system code.
    // refs: MEM:msvc-port-hazards
    testing::internal::CaptureStdout();

    // invariant: the precondition is EVALUATED here and acted on after the capture closes — a
    // diagnostic written while the stream is redirected would land in the measurand.
    std::string registered_already;
    for (const auto name : kAllLoggers)
    {
        if (spdlog::get(std::string{name}) != nullptr)
        {
            registered_already = std::string{name};
            break;
        }
    }

    // invariant: the control write is the FIRST bytes in the window, through the exact writer under
    // test.
    INSIGHT_LOG_WARN(spdlog::default_logger(), "{}", kHostDefaultToken);

    std::vector<std::size_t> sinkless;
    std::vector<std::size_t> inaudible;
    for (std::size_t index{0}; index < kAccessors.size(); ++index)
    {
        std::shared_ptr<spdlog::logger> logger{kAccessors[index]()};
        const AttachedProbe probe{logger};
        INSIGHT_LOG_WARN(logger, "{} #{}", kModuleRecordToken, index);
        if (probe.own_sink_count() == 0U)
            sinkless.push_back(index);
        if (probe.heard() != 1U)
            inaudible.push_back(index);
    }

    const std::string captured{testing::internal::GetCapturedStdout()};

    const std::size_t control_at{captured.find(kHostDefaultToken)};
    const bool control_landed{control_at != std::string::npos};
    const std::size_t control_eol{control_landed ? captured.find('\n', control_at)
                                                 : std::string::npos};
    const bool control_complete{control_landed && control_eol != std::string::npos};

    // invariant: the measurand is the tail after the control LINE, not after the control TOKEN —
    // the token would count the control record's own header and read as foreign output.
    // invariant: the prefix is REPORTED rather than asserted on, because attributing a foreign byte
    // to canon would be a fabricated red.
    // invariant: it is reported in every message, pass or fail, so a green is never read over a
    // polluted window.
    const std::size_t control_line_start{control_landed ? captured.rfind('\n', control_at)
                                                        : std::string::npos};
    const std::string prefix{(control_landed && control_line_start != std::string::npos)
                                 ? captured.substr(0, control_line_start + 1)
                                 : std::string{}};
    const std::string tail{control_complete ? captured.substr(control_eol + 1) : std::string{}};

    const std::string summary{
        "\n  stdout capture: " + std::to_string(captured.size()) +
        " byte(s) total; control write " + (control_landed ? "LANDED" : "MISSING") + "; " +
        std::to_string(prefix.size()) + " byte(s) preceded its LINE; " +
        std::to_string(tail.size()) + " byte(s) followed it — THE MEASURAND.\n  accessors: " +
        std::to_string(kAccessors.size()) + ", sinkless " + indices(sinkless) + ", inaudible " +
        indices(inaudible) + "\n  captured bytes: \"" + escape_bytes(captured) + "\""};

    if (!registered_already.empty())
        abandon_child(kChildPreconditionViolated,
                      "stdout-stream arm — precondition violated: '" + registered_already +
                          "' is already registered, so this process is NOT in state (A) and the "
                          "stream below belongs to a different state. Something ran init_logging() "
                          "before this test — check that the death-test style is 'threadsafe' (a "
                          "forked child inherits the parent's registry)." +
                          summary);

    if (!control_complete)
        abandon_child(
            kChildStdoutProbeDeaf,
            "stdout-stream arm — the control write never arrived intact, so the emptiness "
            "below proves nothing. One of: the capture never attached to fd 1; "
            "INSIGHT_LOG_WARN is elided at compile time in this build; "
            "spdlog::default_logger() is not bound to stdout on this platform; or the "
            "sink cached a handle the capture then closed (the Windows binding — see the "
            "⚠ ordering block, and fix the INSTRUMENT, not the product)." +
                summary);

    if (!tail.empty())
        abandon_child(
            kChildWroteToStdout,
            "stdout-stream arm — A CANON MODULE LOGGER WROTE TO STDOUT with init_logging() never "
            "called. Every entry point that links canon and calls nothing therefore emits its "
            "diagnostics into its own standard output: a machine artifact downstream parses or "
            "hashes stops being a function of the input and becomes a function of the operator's "
            "log level (DN-53.D1 (a) — 4 of the 6 measured arms were exactly this shape, "
            "insight-metalog's determinism fixture among them). Expected 0 byte(s) after the "
            "control line from " +
                std::to_string(kAccessors.size()) + " accessor(s)." + summary);

    if (!sinkless.empty())
        abandon_child(kChildLoggerSinkless,
                      "stdout-stream arm — stdout is clean because the record goes NOWHERE: the "
                      "logger returned by the accessor(s) listed owns no sink of its own, so the "
                      "only thing that heard the record was this test's probe. That satisfies a "
                      "stdout-purity assertion by deleting the diagnostics — canon would be mute "
                      "in every un-initialised process." +
                          summary);

    if (!inaudible.empty())
        abandon_child(kChildRecordInaudible,
                      "stdout-stream arm — stdout is clean because the record was FILTERED OUT: "
                      "the logger returned by the accessor(s) listed did not admit a WARN (exactly "
                      "1 was expected per accessor). Same trade as above in the other costume — "
                      "the level, not the sink." +
                          summary);

    std::cerr << "\n[state-A stdout child] " << kStateAStdoutCleanMarker << " — "
              << kAccessors.size()
              << " accessor(s) emitted one WARN each and contributed 0 byte(s) to stdout, in a "
                 "window where a record through spdlog::default_logger() demonstrably did land "
                 "there, and where every accessor's logger owned a sink of its own and admitted "
                 "the WARN."
              << summary << "\n";
    std::cerr.flush();
    std::_Exit(kChildStdoutClean);
}

} // namespace

// invariant: the historically offending name is one of the two on purpose — it is the one the
// implementation's private copy of the list had lost.
// invariant: the SECOND name is what makes once-per-NAME falsifiable: a global once-flag passes a
// single-name test and still leaves the second module's absence silent forever.
// invariant: the MEMO is the one residue that survives a test, so a name can be used by exactly ONE
// state-B observation per process.
// invariant: that is why both names live in a SINGLE test — split across two they would be
// order-dependent through the memo, and a CORRECT implementation could red.
// invariant: the same residue makes this file incompatible with repeating the raw binary, since the
// second repetition sees a primed memo; the harness re-runs the PROCESS, so it is unaffected.
TEST(LoggerFallbackStateB, AnUnregisteredNameAfterInitWarnsExactlyOncePerName)
{
    init_logging();

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

        // invariant: interleaved and repeated, because the accessors are called from every log site
        // of their module — which is exactly the flood the once-per-name memo exists to prevent.
        for (int call{0}; call < kCallsPerAccessor; ++call)
        {
            (void)pipeline_logger();
            (void)mask_logger();
        }
    }

    ASSERT_NE(spdlog::get(std::string{kPipelineLogger}), nullptr)
        << "the hold failed to re-register '" << kPipelineLogger
        << "'; every later suite in this binary would see a degraded facade";
    ASSERT_NE(spdlog::get(std::string{kMaskLogger}), nullptr)
        << "the hold failed to re-register '" << kMaskLogger << "'";

    const std::vector<CapturingSink::Record> pipeline{probe.records_naming(kPipelineLogger)};
    const std::vector<CapturingSink::Record> mask{probe.records_naming(kMaskLogger)};

    // invariant: THE LOAD-BEARING COUNT — one record per offending name, whatever the call count.
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

    // invariant: nothing else was said; a generic extra line would be the flood in another costume.
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

// invariant: not vacuous on its own — the canary requirement means this arm can only pass on a
// probe that has just heard a record of the very level and path the facade would use.
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

// invariant: state A is UNREACHABLE once any test has initialised, because the call-once flag
// cannot be rewound — so this runs in a CHILD PROCESS.
// invariant: a plain test asserting silence would be asserting it about whatever state the tests
// that happened to run earlier left behind: green by accident, and meaningless run directly.
// invariant: the threadsafe death-test style is LOAD-BEARING and not hygiene — the default style
// FORKS, and a fork inherits an already-true initialised flag and a populated registry.
// invariant: threadsafe re-EXECs the binary filtered to this one test, which is the only way to get
// a genuinely virgin process out of this binary.
// invariant: the child asserts its own precondition and reports through its exit code and its
// standard error, both of which the harness surfaces on failure.
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

// invariant: with init never called, no module logger writes a byte to the process's standard
// output — the property this file did not previously hold.
// invariant: a SECOND re-exec'd child, for the reason stated at its observation body.
// refs: DN-53.D6
TEST(LoggerFallbackStateADeathTest, NoModuleRecordReachesStdoutWhenInitLoggingNeverRan)
{
    const DeathTestStyleHold style{"threadsafe"};

    EXPECT_EXIT(observe_state_a_stdout(), ::testing::ExitedWithCode(kChildStdoutClean),
                std::string{kStateAStdoutCleanMarker})
        << "the child emits one record through every canon accessor with init_logging() never "
           "called, and requires the process's STDOUT to carry none of them. Exit "
        << kChildWroteToStdout
        << " = THE PROPERTY FAILED — canon module records are on stdout, so any entry point that "
           "links canon and calls nothing corrupts its own machine artifact. Exit "
        << kChildStdoutProbeDeaf
        << " = the control write never reached the capture, so the silence measured nothing: that "
           "is a defect in this INSTRUMENT, not in the product. Exit "
        << kChildLoggerSinkless << " / " << kChildRecordInaudible
        << " = the property was bought with silence — the accessor's logger owns no sink of its "
           "own, or filters a WARN out, which would make canon mute in every un-initialised "
           "process instead of moving it off stdout. Exit "
        << kChildPreconditionViolated
        << " = the child was not in state (A) at all. The child's own diagnostic, with the "
           "captured bytes, is printed above under \"Actual msg\".";
}
