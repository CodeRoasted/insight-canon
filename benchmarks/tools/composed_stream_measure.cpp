// refs: ADR-8.D5, ADR-16.D5, DN-163.O4
// invariant: the per-line template stream of a population read through the COMPOSED tokenizer: one
// output line per input line, in input order, its template_str, or nothing when it yields no event.
// invariant: it is template_stream_measure with the five shipped packages composed, a declared
// dialect and channel, and a declared stream context, so a declared value or an instance row shows.
// invariant: a CLI tool and not a test: its population is operator-mounted and third-party, so the
// stream goes where the operator points it and nothing is asserted here.
// invariant: the context is checked before any line is read, and a refusal is printed and exits.
#include <cstdio>

import std;
import insight.canon;
import insight.semantic.github;
import insight.semantic.gitlab;
import insight.semantic.jenkins;
import insight.semantic.package_managers;
import insight.semantic.test_frameworks;

namespace
{
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::StreamContext;
using insight::tokenization::Tokenizer;

// note: the product is formed AT std::size_t; an unsigned-int one wraps past 4 GiB.
constexpr std::size_t kArenaBytes{std::size_t{8} * 1024 * 1024};

constexpr int kExitOk{0};
constexpr int kExitUsage{2};
constexpr int kExitUnreadable{3};
constexpr int kExitFatal{4};

void print_usage()
{
    std::println(stderr, "usage: composed_stream_measure [--dialect D] [--channel C] "
                         "[--context KEY=VALUE]... <file> [<file> ...]");
    std::println(stderr, "  writes one line per input line to stdout: its template_str, empty when "
                         "the line yields no event");
    std::println(stderr, "  the files are read in the order named, through one tokenizer over the "
                         "five shipped packages, viewed for the declared dialect and channel");
    std::println(stderr, "  stderr closes with `lines <N>`, the count of input lines streamed");
}

// post: true when every byte of `text` reached stdout.
bool write_out(std::string_view text)
{
    return std::fwrite(text.data(), 1, text.size(), stdout) == text.size();
}

struct Options
{
    std::string dialect;
    std::string channel;
    StreamContext context;
    std::vector<std::string> files;
};

// post: the options, or nullopt on a flag with no value or a context with no `=`.
[[nodiscard]] std::optional<Options> parse(std::span<char*> arguments)
{
    Options options;
    for (std::size_t index{1}; index < arguments.size(); ++index)
    {
        const std::string_view arg{arguments[index]};
        const bool is_flag{arg == "--dialect" || arg == "--channel" || arg == "--context"};
        if (!is_flag)
        {
            options.files.emplace_back(arg);
            continue;
        }
        if (index + 1 >= arguments.size())
            return std::nullopt;
        const std::string_view value{arguments[++index]};
        if (arg == "--dialect")
            options.dialect = value;
        else if (arg == "--channel")
            options.channel = value;
        else
        {
            const std::size_t equals{value.find('=')};
            if (equals == std::string_view::npos || equals == 0)
                return std::nullopt;
            options.context.values.push_back({.key = std::string{value.substr(0, equals)},
                                              .value = std::string{value.substr(equals + 1)}});
        }
    }
    if (options.files.empty())
        return std::nullopt;
    return options;
}
} // namespace

// post: exit 0 only when every named file was read whole and every line's record was written.
int main(int argc, char** argv)
try
{
    const std::optional<Options> options{
        parse(std::span<char*>{argv, static_cast<std::size_t>(argc)})};
    if (!options.has_value())
    {
        print_usage();
        return kExitUsage;
    }
    static const std::array<insight::semantic::SemanticPackageManifest, 5> kManifests{
        insight::semantic::github::kManifest, insight::semantic::gitlab::kManifest,
        insight::semantic::jenkins::kManifest, insight::semantic::package_managers::kManifest,
        insight::semantic::test_frameworks::kManifest};
    // note: `composed` and `view` precede `tokenizer` so they outlive the const-ref it holds.
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose(kManifests)};
    const insight::semantic::ComposedSemantics view{
        composed.for_stream(options->dialect, options->channel)};
    if (const auto checked{insight::semantic::check_stream_context(options->context, view)};
        !checked)
    {
        std::println(stderr, "refused context: {}", checked.error());
        return kExitUsage;
    }
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, view, options->context};

    std::uint64_t streamed{0};
    std::string raw;
    for (const std::string& name : options->files)
    {
        std::ifstream input{name, std::ios::binary};
        if (!input)
        {
            std::println(stderr, "cannot open {}", name);
            return kExitUnreadable;
        }
        while (std::getline(input, raw))
        {
            // assert: `event->template_str` views arena bytes that the reset below frees, so the
            // record is written before the reset.
            if (const auto event{tokenizer.process_line(raw)}; event.has_value())
            {
                if (!write_out(event->template_str))
                    throw std::runtime_error{"stdout refused a record"};
            }
            if (!write_out("\n"))
                throw std::runtime_error{"stdout refused a record"};
            arena.reset();
            ++streamed;
        }
        if (input.bad())
        {
            std::println(stderr, "read error in {}", name);
            return kExitUnreadable;
        }
    }
    if (std::fflush(stdout) != 0)
        throw std::runtime_error{"stdout refused the final flush"};
    std::println(stderr, "lines {}", streamed);
    return kExitOk;
}
catch (const std::exception& error)
{
    std::fputs("fatal: ", stderr);
    std::fputs(error.what(), stderr);
    std::fputs("\nfatal: the stream above is PARTIAL and may not be read\n", stderr);
    return kExitFatal;
}
catch (...)
{
    std::fputs("fatal: unknown exception\n", stderr);
    std::fputs("fatal: the stream above is PARTIAL and may not be read\n", stderr);
    return kExitFatal;
}
