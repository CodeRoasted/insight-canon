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

// 1.5.1 unwrap (Approach B): the textual public headers are gone — the canon module's
// public surface (Tokenizer, det::FixedReducer, failure_lexicon cues, to_string,
// CanonicalEvent / StructuralRole) is all reachable through the single facade import.
import insight.canon;

namespace
{
// det::i128 → decimal string (no std::to_string overload exists). Works on BOTH det_math 128-bit
// representations (native __int128 on gcc/clang, the portable struct on MSVC) — det::i128/u128 are
// the aliases from det_int128.hpp, and the ops used here (sign via is_negative()-equivalent compare,
// magnitude(), %/÷ by ten, != 0) are provided on both. This output IS part of the canonical digest,
// so it must be byte-identical across compilers — hence it goes through the same portable shim.
std::string i128_to_dec(insight::det::i128 value)
{
    using insight::det::i128;
    using insight::det::u128;
    const bool negative{!(value >= i128{0})};
    // magnitude in u128: -value for negatives (two's-complement -, exact for INT_MIN too).
    const u128 magnitude{static_cast<u128>(negative ? -value : value)};
    std::string out;
    u128 rest{magnitude};
    while (rest != u128{0})
    {
        out.push_back(static_cast<char>('0' + static_cast<int>(static_cast<std::uint64_t>(rest % u128{10}))));
        rest = rest / u128{10};
    }
    if (out.empty())
        out.push_back('0');
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
