// refs: DN-18.D1, ADR-16.D5, SRC-SP-1
#include <cstdio>

import std;
import insight.canon;

namespace
{
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

constexpr std::size_t kDefaultMaxLines{300000};
// note: the product is formed AT std::size_t; an unsigned-int one wraps past 4 GiB.
constexpr std::size_t kArenaBytes{std::size_t{8} * 1024 * 1024};
constexpr std::size_t kTopTemplatesShown{15};
constexpr std::size_t kSingletonSamplesShown{40};
constexpr std::size_t kTemplatePreviewChars{140};

constexpr int kExitOk{0};
constexpr int kExitEmptyPopulation{1};
constexpr int kExitUsage{2};
constexpr int kExitFatal{4};

struct FileConsumption
{
    std::filesystem::path path;
    std::size_t lines_consumed{0};
    bool truncated_by_cap{false};
};

void print_usage(std::string_view program_name)
{
    std::println(stderr, "usage: {} <corpus-dir> [max-lines]", program_name);
    std::println(
        stderr,
        "  <corpus-dir>  root of a *.log tree (the population; walked RECURSIVELY, sorted)");
    std::println(stderr, "  [max-lines]   line budget (default {})", kDefaultMaxLines);
}
} // namespace

// post: prints a population block; a number from this report is citable only beside it.
int main(int argc, char** argv)
try
{
    const std::span<char*> arguments{argv, static_cast<std::size_t>(argc)};
    if (arguments.size() < 2 || arguments.size() > 3)
    {
        print_usage(arguments.empty() ? "f13_cardinality_measure" : arguments[0]);
        return kExitUsage;
    }

    namespace fs = std::filesystem;
    const fs::path corpus_dir{arguments[1]};
    std::size_t max_lines{kDefaultMaxLines};
    if (arguments.size() == 3)
    {
        const std::string_view budget_arg{arguments[2]};
        const auto [parse_end, parse_err]{
            std::from_chars(budget_arg.data(), budget_arg.data() + budget_arg.size(), max_lines)};
        if (parse_err != std::errc{} || parse_end != budget_arg.data() + budget_arg.size() ||
            max_lines == 0)
        {
            std::println(stderr, "max-lines must be a positive integer, got '{}'", budget_arg);
            return kExitUsage;
        }
    }

    std::error_code dir_error;
    if (!fs::is_directory(corpus_dir, dir_error))
    {
        std::println(stderr, "not a directory: {}", corpus_dir.string());
        return kExitUsage;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator{corpus_dir})
        if (entry.is_regular_file() && entry.path().extension() == ".log")
            files.push_back(entry.path());
    std::ranges::sort(files);
    if (files.empty())
    {
        std::println(stderr, "no *.log files anywhere under {} (walked recursively)",
                     corpus_dir.string());
        return kExitEmptyPopulation;
    }

    // note: `composed` precedes `tokenizer` so it outlives the const-ref the Tokenizer holds.
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose({})};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, composed};

    std::unordered_map<std::string, std::uint64_t> template_counts;
    std::vector<FileConsumption> consumed;
    consumed.reserve(files.size());
    std::size_t total_lines{0};

    for (const auto& file : files)
    {
        if (total_lines >= max_lines)
            break;
        FileConsumption record{.path = file};
        std::ifstream input{file};
        std::string raw;
        while (std::getline(input, raw))
        {
            if (total_lines >= max_lines)
            {
                record.truncated_by_cap = true;
                break;
            }
            if (raw.empty())
                continue;
            // assert: `event->template_str` views arena bytes that the reset below frees.
            if (const auto event{tokenizer.process_line(raw)}; event.has_value())
                ++template_counts[std::string{event->template_str}];
            arena.reset();
            ++record.lines_consumed;
            ++total_lines;
        }
        consumed.push_back(std::move(record));
    }

    std::println("=== Stateless template_id cardinality (F13 re-measure) ===");
    std::println("population       : {} of {} *.log files under {} (recursive, sorted walk)",
                 consumed.size(), files.size(), corpus_dir.string());
    std::println("line budget      : {}{}", max_lines,
                 total_lines >= max_lines ? " (EXHAUSTED — the population below is cap-shaped, "
                                            "not the whole tree)"
                                          : "");
    for (const auto& record : consumed)
        std::println("  {:>9} lines  {}{}", record.lines_consumed,
                     record.path.lexically_relative(corpus_dir).generic_string(),
                     record.truncated_by_cap ? "  [TRUNCATED by the line budget]" : "");
    if (consumed.size() < files.size())
        std::println("  {} file(s) NOT REACHED (budget exhausted before them)",
                     files.size() - consumed.size());

    const std::size_t distinct{template_counts.size()};
    const std::size_t singletons{static_cast<std::size_t>(std::ranges::count_if(
        template_counts, [](const auto& entry) { return entry.second == 1; }))};

    std::println("lines            : {}", total_lines);
    std::println("distinct         : {}", distinct);
    std::println("singletons       : {} ({:.2f}% of distinct)", singletons,
                 distinct > 0
                     ? 100.0 * static_cast<double>(singletons) / static_cast<double>(distinct)
                     : 0.0);

    std::vector<std::pair<std::string, std::uint64_t>> by_count{template_counts.begin(),
                                                                template_counts.end()};
    std::ranges::sort(
        by_count, [](const auto& lhs, const auto& rhs)
        { return lhs.second != rhs.second ? lhs.second > rhs.second : lhs.first < rhs.first; });
    std::println("--- top {} by count ---", kTopTemplatesShown);
    for (std::size_t index{0}; index < std::min(kTopTemplatesShown, by_count.size()); ++index)
        std::println("{}  {}", by_count[index].second,
                     std::string_view{by_count[index].first}.substr(0, kTemplatePreviewChars));
    std::println("--- {} singleton samples (the F13 over-split tail) ---", kSingletonSamplesShown);
    std::size_t shown{0};
    for (auto iter{by_count.rbegin()}; iter != by_count.rend() && shown < kSingletonSamplesShown;
         ++iter)
        if (iter->second == 1)
        {
            std::println("{}", std::string_view{iter->first}.substr(0, kTemplatePreviewChars));
            ++shown;
        }

    return kExitOk;
}
catch (const std::exception& error)
{
    std::fputs("fatal: ", stderr);
    std::fputs(error.what(), stderr);
    std::fputs("\nfatal: the report above is PARTIAL and may not be cited\n", stderr);
    return kExitFatal;
}
catch (...)
{
    std::fputs("fatal: unknown exception\n", stderr);
    std::fputs("fatal: the report above is PARTIAL and may not be cited\n", stderr);
    return kExitFatal;
}
