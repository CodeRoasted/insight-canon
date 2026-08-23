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
// RED-FIRST, THE STREAM ARM — and it did not need a mutation, because HEAD WAS THE MUTATION. The
// arm was authored and measured against the un-repaired tree on 2026-08-22, before DN-53.D3
// existed: child exit 5, **700 bytes** of canon module records on stdout (7 records × 100 bytes,
// one per accessor, all through `spdlog::default_logger()`), in an 803-byte window whose only
// other content was the control line. Control LANDED, sinkless (none), inaudible (none), 0 bytes
// before the control line — so exactly ONE condition fired, and it was the named property. Once
// D3 lands, the mutations that must red it again are:
//   * point `logger_for`'s state-(A) return back at `spdlog::default_logger()`
//       → exit 5, the 700 bytes above. This is the whole defect class.
//   * give state (A) a logger with no sinks of its own
//       → exit 7 — stdout clean, diagnostics deleted.
//   * set that logger's level to `err` (or `off`)
//       → exit 8 — stdout clean, diagnostics filtered. 7 and 8 are the two ways to pass a
//         stdout-purity gate by making canon mute, and neither is the fix.
//   * construct the capture AFTER the first registry touch (an instrument mutation, not a product
//     one) → exit 6 on Windows, and STILL EXIT 0 on POSIX — which is the whole reason the ordering
//     is stated as load-bearing rather than left to look like hygiene.
//
// ── THE THIRD PROPERTY: WHERE STATE (A)'s RECORDS GO — WHICH NOTHING GUARDED ──────────────────
// The arms above are about the FACADE'S SELF-REPORT: whether `logger_for` complains, and in which
// state. Neither says a word about where a MODULE'S OWN records land, and that is a different
// object. In state (A) `logger_for` returns `spdlog::default_logger()`, whose sink is STDOUT at
// info — so an entry point that links canon and never calls `init_logging` puts every
// INSIGHT_LOG_* record of every canon module into its own standard output. `DN-53.D1` owns the
// measurement: of six entry points found writing diagnostics into a machine artifact, FOUR were
// this shape and passed through no parameter at all — insight-metalog's determinism fixture among
// them, which returned two sha256 for two runs of one binary on one input, the differing bytes
// being a wall clock inside a log line.
//
// The number of tests guarding stdout purity in this workspace before this arm was ZERO. The one
// that reads like a guard — insight-eidos/sift/tests/crawl/crawl_pair_marker_test.cpp — asserts
// that a marker reached CAPTURED STDERR in a given order: a delivery/attribution property. It
// would stay green if canon resumed writing to stdout, and its workaround DEPENDS on this default
// existing.
//
// So the arm below measures the STREAM, not the logger. Bytes on fd 1 are what a downstream parser
// and a sha256 actually see, and a stream property survives whatever object `logger_for` is later
// made to return — where an assertion on the returned logger's sink CLASS would be a mirror of the
// implementation and would have to be rewritten by the change it is supposed to gate.
//
// ⚠ THE CAPTURE OPENS BEFORE THE FIRST SPDLOG CALL IN THE PROCESS, AND THAT ORDER IS LOAD-BEARING.
// It is the same hazard that produced the 2026-08-17 MSVC red on sift-crawl
// (`MEM:msvc-port-hazards`; crawl_pair_marker_test.cpp's ⚠⚠ block carries the measurement).
// spdlog's stdout colour sink binds to its target by a DIFFERENT mechanism per platform: POSIX
// (`ansicolor_stdout_sink`) holds `FILE* stdout` — a binding BY NAME, so a later `dup2` re-points
// fd 1 underneath it and the sink follows; Windows (`wincolor_stdout_sink`) caches
// `::GetStdHandle(STD_OUTPUT_HANDLE)` at CONSTRUCTION — a binding BY VALUE, and `_dup2` closes the
// handle it cached, after which spdlog DISCARDS `WriteFile`'s failure and the record vanishes with
// no diagnostic on any stream. That failure points the WRONG WAY here: a Windows leg would read
// "stdout is clean" while the bytes went to the console. The sink in question is spdlog's DEFAULT
// logger's, constructed lazily on the first registry touch — so `CaptureStdout()` is the child's
// FIRST statement, ahead of the precondition loop that touches the registry. Opening the capture
// first fixes both platforms with no `#ifdef` and no OS code (`_dup2` also updates `SetStdHandle`,
// measured on MSVC 14.52 by the sift arm).
//
// ⚠ THE ANTI-VACUITY CONTROL IS A WRITE THROUGH THE EXACT WRITER UNDER TEST. A byte-empty tail is
// also what a detached capture, a compile-time-elided macro layer, or a default logger not bound
// to stdout on this platform would produce — `MEM:synthetic-gate-vacuity`: *a test whose subject
// is a MEDIUM witnesses each WRITER separately*. A `std::cout` canary would prove the fd is
// captured and prove NOTHING about a spdlog sink on Windows, which is the leg that fails. So the
// child first emits one record through `spdlog::default_logger()` — the very object state (A)
// resolves to — via INSIGHT_LOG_WARN, the product's own macro layer with its elision threshold,
// and REQUIRES those bytes in the capture; everything measured is the tail AFTER that line. The
// control keeps its meaning once DN-53.D3 lands: the host's default logger still writes to stdout,
// so the arm becomes exactly the discriminator the fix is about — the host's default logger is
// audible on stdout in this same process, and canon's accessors are not.
//
// ⚠ AND AN OVER-REACH CONTROL, because silence is the cheapest way to pass a stdout-purity gate.
// A `logger_for` returning a sinkless logger, or one whose level filtered WARN out, would write
// zero bytes to stdout and satisfy the property while making canon mute in every un-initialised
// process — `MEM:synthetic-gate-vacuity` item 17, the fail-safe-is-vacuous direction: ask what a
// do-nothing mechanism scores. So each returned logger must OWN at least one sink of its own and
// must ADMIT the WARN, heard by a capture sink attached for the observation and removed after it,
// with the logger's LEVEL deliberately left untouched — a level that filters is precisely what
// this must catch. Residual, stated rather than hidden: a sink pointed at some third stream would
// pass both controls. Nothing proposes that, and the stream this arm owns is stdout.
//
// Determinism: no RNG, no threads, no wall clock, no timestamps in any assertion — the capture
// sink stores raw payloads, never formatted lines. Record ORDER is never asserted (the two names
// are interleaved on purpose and either may land first); only per-name counts and payload content
// are. The capture sink is null-mutex because the suite is single-threaded by construction. The
// state-(A) children are re-execs of this same binary, so they are as deterministic as the parent.
// The stream arm asserts a COUNT OF BYTES (zero) and never their content: the formatted lines it
// would otherwise compare carry a wall-clock timestamp, and the only place captured bytes appear
// is a failure message.

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

// ── The state-(A) STREAM arm's own channel ────────────────────────────────────────────────────
// Codes are disjoint from the self-report arm's above, so a parent failure names ONE property
// rather than "one of the state-(A) tests".
constexpr int kChildStdoutClean{0};
constexpr int kChildWroteToStdout{5};
constexpr int kChildStdoutProbeDeaf{6};
constexpr int kChildLoggerSinkless{7};
constexpr int kChildRecordInaudible{8};

constexpr std::string_view kStateAStdoutCleanMarker{"STATE-A-STDOUT-CLEAN-OK"};

// The control write's token — emitted through `spdlog::default_logger()`, the exact writer state
// (A) falls back to. Its bytes must be in the capture or nothing measured below is a measurement.
constexpr std::string_view kHostDefaultToken{"kleio-p1-host-default-canary"};

// The measured writes' token. Carries the accessor's index, so a line that leaks onto stdout names
// which accessor produced it instead of only proving that one did.
constexpr std::string_view kModuleRecordToken{"kleio-p1-module-record"};

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

// Renders captured stream bytes readable inside a failure message. The stream may legitimately
// carry ANSI colour escapes and newlines, and pasting either raw into a diagnostic makes the
// failure unreadable at best and repaints the reader's terminal at worst — a test that is verbose
// on failure has to stay LEGIBLE on failure.
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

// Attaches a capture sink to ONE logger for ONE observation and restores its sink list exactly.
//
// Its job is the OVER-REACH control described at the top: it answers "did the record survive the
// logger at all", which is the question a stdout-purity assertion cannot ask and which a
// do-nothing implementation would otherwise pass. The logger's LEVEL is deliberately never
// touched — a level that filters the record out is one of the two do-nothing shapes this exists to
// catch, and raising it would hide exactly that.
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

    // Sinks the logger owned BEFORE this probe attached. Zero means the record reaches nothing but
    // the probe — audible to this test and lost in production.
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

// The body of the state-(A) STREAM observation. Runs in its OWN re-exec'd child — a second child,
// not a second assertion inside the first: this arm deliberately EMITS through the accessors,
// which at HEAD lands seven records on the default logger, and that is precisely what the
// self-report arm above requires to be empty. One child cannot hold both properties, and merging
// them would trade a clean attribution for one saved process.
void observe_state_a_stdout()
{
    // FIRST STATEMENT, ahead of anything that could touch spdlog. See the ⚠ ordering block at the
    // top of this file: the registry — and with it the default logger and its stdout sink — must
    // be constructed INSIDE this capture, or the Windows leg reads a clean stdout while the bytes
    // go to the console.
    testing::internal::CaptureStdout();

    // Evaluated here, ACTED ON after the capture closes: a diagnostic written while fd 1 is
    // redirected would land in the measurand instead of in the parent's failure text.
    std::string registered_already;
    for (const auto name : kAllLoggers)
    {
        if (spdlog::get(std::string{name}) != nullptr)
        {
            registered_already = std::string{name};
            break;
        }
    }

    // THE CONTROL WRITE — first bytes in the window, through the exact writer under test.
    INSIGHT_LOG_WARN(spdlog::default_logger(), "{}", kHostDefaultToken);

    // THE MEASURED WRITES — one record per accessor, through the product's macro layer.
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

    // Bytes before the control LINE — not before the control token, which would count the control
    // record's own timestamp/level/source-loc header (74 bytes as formatted here) and read as
    // foreign output. Anything genuinely here belongs to whatever else this process wrote to
    // stdout; the accessors write strictly after the control, so the measurand is the tail. The
    // prefix is reported rather than asserted on — attributing a foreign byte to canon would be a
    // fabricated red — but it is reported in every message, pass or fail, so a green is never read
    // over a polluted window. Measured 0 here: gtest suppresses its own event forwarding in a
    // death-test child (`SuppressTestEventsIfInSubprocess`), so the window holds nothing but these
    // writes.
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

// ── State (A), the STREAM: a module's records never reach stdout ───────────────────────────────
// The property `DN-53.D6` calls P1, and the one this file did not hold: with init_logging() never
// called, no canon module logger writes a byte to the process's standard output. A second
// re-exec'd child, for the reason stated at observe_state_a_stdout().
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

// NOLINTEND
