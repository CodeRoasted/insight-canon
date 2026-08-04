// f13_cardinality_measure — the standing F13 masker-cardinality RE-MEASURE instrument.
//
// WHY THIS IS A CLI TOOL AND NOT A TEST (DN-18.D1 § 3.3, Eqya ruling 2026-07-29).
// This measurement lived as `StatelessTemplate.CardinalityOnCorpus` inside the unit suite, env-gated
// on CORPUS_DIR. Its population is whatever directory the operator mounts — unnameable by
// construction, so the same invocation on two machines is not the same measurement, and clause 1 of
// the corpus-gate contract forbids gating on it. A test whose only assertion is `lines > 0` and
// whose meaning is a printed report is a measurement instrument wearing a test's clothes; homed
// here, the population is DECLARED (the report enumerates exactly which files and how many lines
// were consumed, and where the cap cut) rather than smuggled through an env variable in a
// deterministic tree.
//
// WHAT THE NUMBER IS FOR. Distinct-template count + singleton fraction size the F13 masking rules
// (ADR-16.D5): re-run after any F13 rule change. The one-time over-split ratio
// vs the now-ripped Drain was 4.12x → 1.79x at the § 8 gate; that comparison cannot re-run post-rip,
// and the standing production guard is the K_dim cardinality monitor (D-TID-7), not this tool.
// A number printed here is citable ONLY next to its population block — that is the whole contract.
//
// The pipeline is the production one: Tokenizer::process_line (parse → mask) over a degenerate,
// zero-package composition — generic corpus masking is semantic-unaware, and the tool must never
// link the semantic packages (SRC-SP-1 / R1 dependency arrow).

// Textual, not `import std`-provided: `stderr` is a macro whose expansion needs the FILE*
// declaration visible in this TU, which only the header brings.
#include <cstdio>

import std;
import insight.canon;

namespace
{
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

// Default line budget — the same sizing the test-era instrument used, kept so historical readings
// stay comparable order-of-magnitude. Overridable via argv[2]; the report states where it cut.
constexpr std::size_t kDefaultMaxLines{300000};
constexpr std::size_t kArenaBytes{8U * 1024U * 1024U};
constexpr std::size_t kTopTemplatesShown{15};
constexpr std::size_t kSingletonSamplesShown{40};
constexpr std::size_t kTemplatePreviewChars{140};

constexpr int kExitOk{0};
constexpr int kExitEmptyPopulation{1};
constexpr int kExitUsage{2};

struct FileConsumption
{
    std::filesystem::path path;
    std::size_t lines_consumed{0};
    bool truncated_by_cap{false};
};

void print_usage(std::string_view program_name)
{
    std::println(stderr, "usage: {} <corpus-dir> [max-lines]", program_name);
    std::println(stderr, "  <corpus-dir>  directory of *.log files (the population; walked sorted)");
    std::println(stderr, "  [max-lines]   line budget (default {})", kDefaultMaxLines);
}
} // namespace

int main(int argc, char** argv)
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
        const auto [parse_end, parse_err]{std::from_chars(
            budget_arg.data(), budget_arg.data() + budget_arg.size(), max_lines)};
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
    for (const auto& entry : fs::directory_iterator{corpus_dir})
        if (entry.is_regular_file() && entry.path().extension() == ".log")
            files.push_back(entry.path());
    std::ranges::sort(files); // deterministic order for a fixed tree
    if (files.empty())
    {
        std::println(stderr, "no *.log files under {}", corpus_dir.string());
        return kExitEmptyPopulation;
    }

    // Generic corpus masking is semantic-unaware — a degenerate (zero-package) composition.
    // `composed` precedes `tokenizer` so it outlives the const-ref the Tokenizer holds.
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
            // The production path: parse → mask, one event per content-bearing line. Unparsed
            // lines are skipped exactly as the test-era instrument skipped them. The template is
            // copied out before the per-line arena reset.
            if (const auto event{tokenizer.process_line(raw)}; event.has_value())
                ++template_counts[std::string{event->template_str}];
            arena.reset();
            ++record.lines_consumed;
            ++total_lines;
        }
        consumed.push_back(std::move(record));
    }

    // ── The population block: what was measured, declared, so the number is citable ──
    std::println("=== Stateless template_id cardinality (F13 re-measure) ===");
    std::println("population       : {} of {} *.log files under {} (sorted walk)", consumed.size(),
                 files.size(), corpus_dir.string());
    std::println("line budget      : {}{}", max_lines,
                 total_lines >= max_lines ? " (EXHAUSTED — the population below is cap-shaped, "
                                            "not the whole tree)"
                                          : "");
    for (const auto& record : consumed)
        std::println("  {:>9} lines  {}{}", record.lines_consumed, record.path.filename().string(),
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
    std::ranges::sort(by_count, [](const auto& lhs, const auto& rhs) {
        return lhs.second != rhs.second ? lhs.second > rhs.second : lhs.first < rhs.first;
    });
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
