// invariant: shared white-box test infrastructure — ONE test module per domain, mirroring the
// package's own module layout.
// invariant: every test unit imports THIS instead of spelling out the full import block.
// invariant: it re-exports the complete module surface, the public facade plus the sealed detail
// shards, so a test unit needs no further import beyond the harness.
export module insight.canon.test;
export import std;
export import insight.canon;
export import insight.canon.detail.scan;
export import insight.canon.detail.strategy;
export import insight.canon.detail.mask;
export import insight.canon.detail.parse;
export import insight.canon.detail.numa;
// invariant: the PROVIDER CONTRACT between core and the vocabulary packages — core tests build
// SYNTHETIC manifests and rows to exercise the algorithms VOCABULARY-FREE.
// invariant: the facade does not surface the provider interface, and a white-box core test
// legitimately does; package suites import it through their own package module instead.
export import insight.canon.spi;
// invariant: the conformance kit is a PUBLIC module unit rather than a sealed shard, because canon
// ships it INSTALLED.
// invariant: a core test exercising the kit's own algorithms still belongs to the same
// import-this-and-nothing-else contract as the rest of the surface.
export import insight.canon.conformance;

// invariant: a core test whose property is SEMANTIC-UNAWARE feeds the tokenizer a DEGENERATE,
// zero-package composition.
// invariant: core tests never link the semantic packages, which would invert the dependency arrow.
// invariant: the returned composition MUST OUTLIVE the tokenizer it feeds.
// invariant: bind it to a named local declared BEFORE the tokenizer and never pass the temporary
// inline, because the tokenizer holds a const-ref.
export namespace insight::test_support
{
[[nodiscard]] inline insight::semantic::ComposedSemantics degenerate_composition()
{
    return insight::semantic::compose({});
}

// invariant: the core package's source root reaches a test through the environment only, never a
// compile definition (ADR-2.D14).
// invariant: insight_canon_corpus stores this test binary, and a build path compiled into it makes
// the stored bytes depend on the home the create ran in.
// note: core/CMakeLists.txt's test registration sets it; a direct run sets it to core/.
inline constexpr const char* kCoreSourceRootVar{"INSIGHT_CANON_CORE_SOURCE_ROOT"};

// post: the directory kCoreSourceRootVar names in the environment; an empty path when the variable
// is unset or empty, which a caller reports as a failure (core_source_root_unset()), never a skip.
[[nodiscard]] inline std::filesystem::path core_source_root()
{
    const char* const raw{std::getenv(kCoreSourceRootVar)};
    return raw == nullptr ? std::filesystem::path{} : std::filesystem::path{raw};
}

// post: the failure text for an unset core source root, naming the variable and who sets it.
[[nodiscard]] inline std::string core_source_root_unset()
{
    return std::string{kCoreSourceRootVar} +
           " is unset or empty, so the package's source tree cannot be located: ctest's "
           "registration sets it, and a direct run of the binary sets it to the checkout's core/ "
           "directory.";
}

// note: clang-21.1.8 drops libc++'s jthread lock lambda below gtest (llvm-project#172241).
// post: one jthread runs the task; join() and the destructor (after request_stop) wait for it.
class TestThread
{
  public:
    explicit TestThread(std::function<void()> task);
    TestThread(const TestThread&) = delete;
    TestThread& operator=(const TestThread&) = delete;
    TestThread(TestThread&&) = delete;
    TestThread& operator=(TestThread&&) = delete;
    ~TestThread();

    void join();

  private:
    std::jthread thread_;
};

TestThread::TestThread(std::function<void()> task) : thread_{std::move(task)} {}

// note: hand-written, never `= default`: MSVC re-emits a defaulted one in each importer (LNK2005).
TestThread::~TestThread()
{
    if (thread_.joinable())
    {
        thread_.request_stop();
        thread_.join();
    }
}

void TestThread::join()
{
    thread_.join();
}
} // namespace insight::test_support
