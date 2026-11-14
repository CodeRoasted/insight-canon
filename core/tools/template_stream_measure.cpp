// refs: ADR-8.D5, ADR-16.D5, ADR-2.D9
// invariant: the per-line template stream of a population: one output line per input line, in
// input order, holding that line's template_str, or nothing when the tokenizer yields no event.
// invariant: it exists to compare two builds of the masker line by line, so the alignment is the
// instrument and no line is ever skipped or reordered.
// invariant: the tokenizer is configured as the FLAW-13 tool configures it - no composed
// vocabulary, the default MaskConfig, one tokenizer across the files in the order named.
// invariant: a CLI tool and not a test: its population is operator-mounted and third-party, so the
// stream goes where the operator points it and nothing is asserted here.
#include <cstdio>

import std;
import insight.canon;

namespace
{
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

// note: the product is formed AT std::size_t; an unsigned-int one wraps past 4 GiB.
constexpr std::size_t kArenaBytes{std::size_t{8} * 1024 * 1024};

constexpr int kExitOk{0};
constexpr int kExitUsage{2};
constexpr int kExitUnreadable{3};
constexpr int kExitFatal{4};

void print_usage(std::string_view program_name)
{
    std::println(stderr, "usage: {} <file> [<file> ...]", program_name);
    std::println(stderr, "  writes one line per input line to stdout: its template_str, empty when "
                         "the line yields no event");
    std::println(stderr, "  the files are read in the order named, through one tokenizer");
    std::println(stderr, "  stderr closes with `lines <N>`, the count of input lines streamed");
}

// post: true when every byte of `text` reached stdout.
bool write_out(std::string_view text)
{
    return std::fwrite(text.data(), 1, text.size(), stdout) == text.size();
}
} // namespace

// post: exit 0 only when every named file was read whole and every line's record was written.
int main(int argc, char** argv)
try
{
    const std::span<char*> arguments{argv, static_cast<std::size_t>(argc)};
    if (arguments.size() < 2)
    {
        print_usage(arguments.empty() ? "template_stream_measure" : arguments[0]);
        return kExitUsage;
    }

    // note: `composed` precedes `tokenizer` so it outlives the const-ref the Tokenizer holds.
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose({})};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, composed, insight::tokenization::StreamContext{}};

    std::uint64_t streamed{0};
    std::string raw;
    for (const char* const name : arguments.subspan(1))
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
