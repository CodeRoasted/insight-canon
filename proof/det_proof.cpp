// NOLINTBEGIN Test
// Canon public determinism proof — fixture.
//
// The externally-checkable half of the "same input → same output, bit-for-bit"
// claim (insight_determinism_model.md § "Public proof-gate (canon, Apache)").
// Drives ONLY canon's public Apache API — the tokenizer/Drain template set,
// `failure_lexicon` token-scan, and `det_math` — over a canon-local PUBLIC corpus,
// and emits a CANONICAL, INTEGER-DOMAIN text digest. No metalog, no eidos, no
// proprietary surface; nothing here reveals the moat.
//
// scripts/det_public_proof.sh builds this across the gcc x clang x -O{0,2,3} x
// -ffp-contract{off,fast} matrix; the digest MUST be byte-identical across every
// build and match the committed golden (proof/golden.sha256). Determinism is
// preserved by construction:
//   * std::map (ordered) for the template set — iteration order is by key, never
//     hash-order (the std::hash cross-stdlib hazard cannot appear);
//   * det_math entropy emitted as the RAW Qk __int128 (Σ c·log2(c) via the no-libm
//     det_log2_fixed) — integer-domain, so no float formatting can diverge;
//   * per-file fresh arena/Tokenizer — the template set is a pure function of that
//     file's content and line order.

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "insight/core/types.hpp"
#include "insight/math/det_math.hpp"
#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/canonical_event.hpp"
#include "insight/tokenization/tokenizer_engine.hpp"
#include "insight/utils/failure_lexicon.hpp"

namespace
{
// __int128 → decimal string (no std::to_string overload exists for it).
std::string i128_to_dec(__int128 value)
{
    if (value == 0)
        return "0";
    const bool negative{value < 0};
    unsigned __int128 magnitude{negative ? static_cast<unsigned __int128>(-(value + 1)) + 1U
                                         : static_cast<unsigned __int128>(value)};
    std::string out;
    while (magnitude != 0)
    {
        out.push_back(static_cast<char>('0' + static_cast<int>(magnitude % 10)));
        magnitude /= 10;
    }
    if (negative)
        out.push_back('-');
    std::ranges::reverse(out);
    return out;
}

std::string basename_of(const std::string& path)
{
    const auto slash{path.find_last_of('/')};
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
} // namespace

// Proof tool, not a hot path; basename_of's substr(slash+1) runs only when find_last_of
// returned a valid index. main is flagged by default; an escaping exception just aborts the tool —
// acceptable in a standalone proof binary's main.
// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: det_proof <corpus-file> [<corpus-file> ...]\n";
        return 2;
    }

    namespace tk = insight::tokenization;

    std::cout << "# canon public determinism proof — v1\n";

    for (int arg = 1; arg < argc; ++arg)
    {
        std::ifstream input{argv[arg], std::ios::binary};
        if (!input)
        {
            std::cerr << "cannot open " << argv[arg] << "\n";
            return 2;
        }
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(input, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(std::move(line));
            line.clear();
        }

        constexpr std::size_t kArenaBytes{std::size_t{1} << 22};
        tk::ArenaAllocator arena{kArenaBytes};
        tk::Tokenizer tokenizer{arena};

        std::map<std::string, std::uint64_t> templates; // ordered → deterministic iteration
        struct Row
        {
            std::string level;
            std::string role;
            std::string tmpl;
            bool failure;
            bool warning;
        };
        std::vector<Row> rows;
        rows.reserve(lines.size());

        for (const auto& raw : lines)
        {
            const auto event{tokenizer.process_line(raw)};
            std::string tmpl{event ? std::string{event->template_str}
                                   : std::string{"<<parse-error>>"}};
            std::string level{event ? std::string{insight::to_string(event->level)}
                                    : std::string{"Unknown"}};
            std::string role{event ? std::string{insight::to_string(event->structural_role)}
                                   : std::string{"None"}};
            const bool failure{insight::utils::contains_failure_cue(raw)};
            const bool warning{insight::utils::contains_warning_cue(raw)};
            ++templates[tmpl];
            rows.push_back({std::move(level), std::move(role), std::move(tmpl), failure, warning});
        }

        std::cout << "## file " << basename_of(argv[arg]) << "\n";
        std::cout << "### templates (" << templates.size() << ")\n";
        std::uint64_t total{0};
        insight::det::FixedReducer reducer;
        for (const auto& [tmpl, count] : templates)
        {
            std::cout << count << '\t' << tmpl << '\n';
            total += count;
            reducer.add_weighted_log2(count, count); // Σ c·log2(c), the det_math entropy term
        }

        std::cout << "### events\n";
        for (const auto& row : rows)
        {
            std::cout << row.level << '\t' << (row.failure ? 'F' : '-') << (row.warning ? 'W' : '-')
                      << '\t' << row.role << '\t' << row.tmpl << '\n';
        }

        std::cout << "### det_math total=" << total
                  << " sum_c_log2c_qk=" << i128_to_dec(reducer.raw()) << '\n';
    }

    return 0;
}
// NOLINTEND Test
