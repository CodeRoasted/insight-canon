// NOLINTBEGIN
#include <gtest/gtest.h>

import insight.canon.test;

namespace fs = std::filesystem;

using insight::LogLevel;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::CanonicalEvent;
using insight::tokenization::Tokenizer;

namespace
{

constexpr std::size_t kArenaSize{32u << 20};
constexpr std::size_t kMaxDiagnosticSamples{8};
constexpr std::size_t kExpectedDatasetLineCount{2000};

struct DatasetExpectation
{
    std::string_view file_name;
    std::size_t min_ok_lines;
};

constexpr std::array<DatasetExpectation, 16> kDatasetExpectations{{
    {"Android_2k.log", 1900},
    {"Apache_2k.log", 1900},
    {"BGL_2k.log", 1770},
    {"HDFS_2k.log", 1900},
    {"HPC_2k.log", 1883},
    {"Hadoop_2k.log", 1900},
    {"HealthApp_2k.log", 1750},
    {"Linux_2k.log", 1892},
    {"Mac_2k.log", 1774},
    {"OpenSSH_2k.log", 1990},
    {"OpenStack_2k.log", 1900},
    {"Proxifier_2k.log", 1900},
    {"Spark_2k.log", 1900},
    {"Thunderbird_2k.log", 1900},
    {"Windows_2k.log", 1900},
    {"Zookeeper_2k.log", 1900},
}};

[[nodiscard]] bool env_is_enabled(const char* name)
{
    const char* value = std::getenv(name);
    if (value == nullptr)
    {
        return false;
    }

    const std::string_view text{value};
    return !text.empty() && text != "0" && text != "false" && text != "FALSE" && text != "off" &&
           text != "OFF";
}

[[nodiscard]] std::optional<double> env_min_success_rate()
{
    const char* value = std::getenv("INSIGHT_TOKENIZER_REGRESSION_MIN_SUCCESS_RATE");
    if (value == nullptr)
    {
        return std::nullopt;
    }

    try
    {
        const double parsed{std::stod(value)};
        if (parsed >= 0.0 && parsed <= 1.0)
        {
            return parsed;
        }
    }
    catch (...)
    {
    }

    return std::nullopt;
}

[[nodiscard]] DatasetExpectation expectation_for(const fs::path& file_path)
{
    const std::string file_name{file_path.filename().string()};
    const auto it{std::find_if(kDatasetExpectations.begin(), kDatasetExpectations.end(),
                               [&file_name](const DatasetExpectation& expectation)
                               { return expectation.file_name == file_name; })};
    if (it != kDatasetExpectations.end())
    {
        return *it;
    }

    return {"", 0};
}

[[nodiscard]] std::string quote(std::string_view value)
{
    std::ostringstream out;
    out << '"';
    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << ch;
            break;
        }
    }
    out << '"';
    return out.str();
}

[[nodiscard]] std::string format_event(const CanonicalEvent& event)
{
    std::ostringstream out;
    out << "ok" << " id=" << event.id << " template_id=" << event.template_id
        << " level=" << to_string(event.level) << " ts_ms="
        << std::chrono::duration_cast<std::chrono::milliseconds>(event.timestamp.time_since_epoch())
               .count()
        << " component=" << quote(event.component) << " template=" << quote(event.template_str)
        << " params=[";

    for (std::size_t index{0}; index < event.params.size(); ++index)
    {
        if (index != 0)
        {
            out << ", ";
        }
        out << quote(event.params[index]);
    }

    out << ']';
    return out.str();
}

[[nodiscard]] std::string format_result(const std::expected<CanonicalEvent, std::string>& result)
{
    if (result.has_value())
    {
        return format_event(result.value());
    }

    return "err error=" + quote(result.error());
}

// Locate the committed LogHub smoke slice by probing a few CWD-relative candidates.
// The slice is committed at "<repo>/data/corpora/loghub/slice" (ADR 0016 §5 — the
// LogHub 2k sample tier, CC-BY-4.0; the full corpus stays fetch-on-demand and is
// NOT read here). It is always in git, so the suite always runs (never silently
// skips on a clean checkout). The test may run with CWD set to the build directory
// (CTest WORKING_DIRECTORY), the package root, or — on Windows/Mac CI — yet another
// convention. We return the *validated* directory directly rather than deriving a
// "repo root" and re-appending the suffix, so the path that is checked and the path
// that is iterated can never disagree.
[[nodiscard]] fs::path find_dataset_dir()
{
    // Primary: the absolute slice path injected by CMake (LOGHUB_SLICE_DIR) — CWD-independent,
    // so the committed slice is always found regardless of where CTest launches the binary.
#ifdef LOGHUB_SLICE_DIR
    {
        std::error_code ec;
        const fs::path dir{LOGHUB_SLICE_DIR};
        if (fs::is_directory(dir, ec) && !ec)
        {
            return dir;
        }
    }
#endif
    // Fallback: probe a few CWD-relative candidates (e.g. a build without the define).
    static constexpr std::array<std::string_view, 4> kCandidates{{
        "data/corpora/loghub/slice",          // CWD == package root
        "../data/corpora/loghub/slice",       // CWD == an in-package subdir
        "../../data/corpora/loghub/slice",    // CWD == a nested build dir
        "../../../data/corpora/loghub/slice", // deeper build-tree nesting
    }};

    for (const std::string_view candidate : kCandidates)
    {
        std::error_code ec;
        const fs::path dir{fs::absolute(fs::path{candidate})};
        if (fs::is_directory(dir, ec) && !ec)
        {
            return dir;
        }
    }

    return {}; // Fixtures not found - tests will be skipped
}

[[nodiscard]] std::vector<std::string>
collect_dataset_path_strings(); // fwd (defined after collect_dataset_paths)

[[nodiscard]] std::vector<fs::path> collect_dataset_paths()
{
    const fs::path dataset_dir{find_dataset_dir()};
    if (dataset_dir.empty())
    {
        return {};
    }

    // Guarded iteration: an unguarded fs::directory_iterator on a missing/raced
    // directory throws, and an uncaught throw here aborts the whole suite
    // (std::terminate, exit 134) instead of the intended skip. The error_code
    // overloads make both construction and increment non-throwing, so a fixture
    // directory that disappears mid-walk degrades to "no datasets" (skip), never
    // an abort.
    std::error_code ec;
    fs::directory_iterator it{dataset_dir, ec};
    if (ec)
    {
        return {};
    }

    std::vector<fs::path> files;
    const fs::directory_iterator end{};
    for (; it != end; it.increment(ec))
    {
        if (ec)
        {
            return {};
        }

        std::error_code stat_ec;
        if (it->is_regular_file(stat_ec) && !stat_ec && it->path().extension() == ".log")
        {
            files.push_back(it->path());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

[[nodiscard]] std::vector<std::string> collect_dataset_path_strings()
{
    std::vector<std::string> out;
    for (const auto& path : collect_dataset_paths())
        out.push_back(path.string());
    return out;
}

[[nodiscard]] std::string dataset_name(const testing::TestParamInfo<std::string>& info)
{
    std::string name{fs::path{info.param}.stem().string()};
    std::replace_if(
        name.begin(), name.end(), [](unsigned char ch) { return !std::isalnum(ch); }, '_');
    return name;
}

struct DatasetRunSummary
{
    std::size_t total_lines{0};
    std::size_t ok_lines{0};
    std::size_t err_lines{0};
    std::vector<std::string> sample_outputs;
    std::vector<std::string> sample_errors;

    [[nodiscard]] double success_rate() const noexcept
    {
        if (total_lines == 0)
        {
            return 0.0;
        }
        return static_cast<double>(ok_lines) / static_cast<double>(total_lines);
    }
};

[[nodiscard]] DatasetRunSummary process_dataset_file(const fs::path& file_path, bool verbose_output)
{
    std::ifstream input(file_path, std::ios::binary);
    EXPECT_TRUE(input.is_open()) << "Failed to open dataset: " << file_path.string();

    DatasetRunSummary summary;
    if (!input.is_open())
    {
        return summary;
    }

    ArenaAllocator arena{kArenaSize};
    Tokenizer tokenizer{arena};

    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        ++summary.total_lines;
        const auto result{tokenizer.process_line(line)};
        const std::string formatted{format_result(result)};

        if (result.has_value())
        {
            ++summary.ok_lines;
            if (summary.sample_outputs.size() < kMaxDiagnosticSamples)
            {
                summary.sample_outputs.push_back("line=" + std::to_string(summary.total_lines) +
                                                 " " + formatted);
            }
        }
        else
        {
            ++summary.err_lines;
            if (summary.sample_errors.size() < kMaxDiagnosticSamples)
            {
                summary.sample_errors.push_back("line=" + std::to_string(summary.total_lines) +
                                                " raw=" + quote(line) + " " + formatted);
            }
        }

        if (verbose_output)
        {
            std::cout << file_path.filename().string() << ':' << summary.total_lines << ' '
                      << formatted << '\n';
        }
    }

    EXPECT_EQ(tokenizer.events_produced(), summary.ok_lines)
        << "events_produced mismatch for dataset " << file_path.string();

    return summary;
}

[[nodiscard]] std::string build_failure_report(const fs::path& file_path,
                                               const DatasetRunSummary& summary,
                                               std::size_t required_ok_lines,
                                               double required_success_rate)
{
    std::ostringstream out;
    out << "dataset=" << file_path.filename().string() << '\n'
        << "total_lines=" << summary.total_lines << '\n'
        << "ok_lines=" << summary.ok_lines << '\n'
        << "err_lines=" << summary.err_lines << '\n'
        << "success_rate=" << summary.success_rate() << '\n'
        << "required_ok_lines=" << required_ok_lines << '\n'
        << "required_success_rate=" << required_success_rate << '\n';

    if (!summary.sample_outputs.empty())
    {
        out << "sample_ok:" << '\n';
        for (const std::string& sample : summary.sample_outputs)
        {
            out << sample << '\n';
        }
    }

    if (!summary.sample_errors.empty())
    {
        out << "sample_err:" << '\n';
        for (const std::string& sample : summary.sample_errors)
        {
            out << sample << '\n';
        }
    }

    return out.str();
}

class TokenizerLoghubRegressionTest : public ::testing::TestWithParam<std::string>
{
};

// The committed slice is always in git, so this suite is normally instantiated.
// The guard remains only for a pathological CWD where the slice can't be located.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TokenizerLoghubRegressionTest);

TEST_P(TokenizerLoghubRegressionTest, ProcessesRealLoghubDataset)
{
    const fs::path dataset{
        GetParam()}; // param is std::string (gtest can't print fs::path under import std)
    const bool verbose_output{env_is_enabled("INSIGHT_TOKENIZER_REGRESSION_VERBOSE")};
    const std::optional<double> env_success_rate{env_min_success_rate()};
    const DatasetExpectation expected{expectation_for(dataset)};

    ASSERT_TRUE(fs::exists(dataset)) << "Dataset path does not exist: " << dataset.string();
    ASSERT_FALSE(expected.file_name.empty())
        << "Missing expectation for dataset: " << dataset.string();

    const DatasetRunSummary summary{process_dataset_file(dataset, verbose_output)};
    const double baseline_success_rate{static_cast<double>(expected.min_ok_lines) /
                                       static_cast<double>(kExpectedDatasetLineCount)};
    const double required_success_rate{env_success_rate.has_value()
                                           ? std::max(*env_success_rate, baseline_success_rate)
                                           : baseline_success_rate};
    const std::size_t required_ok_lines{
        std::max(expected.min_ok_lines,
                 static_cast<std::size_t>(required_success_rate *
                                          static_cast<double>(kExpectedDatasetLineCount)))};

    ASSERT_EQ(summary.total_lines, kExpectedDatasetLineCount)
        << "Unexpected dataset line count: " << dataset.string();
    EXPECT_GE(summary.ok_lines, required_ok_lines)
        << build_failure_report(dataset, summary, required_ok_lines, required_success_rate);
    EXPECT_GE(summary.success_rate(), required_success_rate)
        << build_failure_report(dataset, summary, required_ok_lines, required_success_rate);
}

INSTANTIATE_TEST_SUITE_P(TokenizationRegression, TokenizerLoghubRegressionTest,
                         ::testing::ValuesIn(collect_dataset_path_strings()), dataset_name);

TEST(TokenizerLoghubRegressionDiscoveryTest, FindsExpectedDatasets)
{
    const auto datasets{collect_dataset_paths()};
    if (datasets.empty())
    {
        GTEST_SKIP() << "Committed LogHub slice not located from this CWD "
                        "(expected at data/corpora/loghub/slice).";
    }
    ASSERT_EQ(datasets.size(), 16u);
}

} // namespace

// NOLINTEND
