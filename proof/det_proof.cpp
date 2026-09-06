
// note: bare and file-wide; measured to silence 4 checks, main's complexity among them.
// NOLINTBEGIN Test
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <spdlog/common.h>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

import insight.canon;
// invariant: the proof composes the SAME package set, in the same order, that the one product
// composition does — a divergence here proves a pipeline nothing ships.
// refs: ADR-17, F-SRC-insight-eidos:pipeline/composition.cpp
import insight.semantic.github;
import insight.semantic.gitlab;
import insight.semantic.jenkins;
import insight.semantic.test_frameworks;

namespace
{
// invariant: this decimal text is part of the canonical digest, so it goes through the portable
// 128-bit shim and is byte-identical on every compiler.
std::string i128_to_dec(insight::det::i128 value)
{
    using insight::det::i128;
    using insight::det::u128;
    const bool negative{!(value >= i128{0})};
    // assert: u128 is what REPRESENTS the most-negative value's magnitude; i128 cannot, and its own
    // operator set carries no remainder.
    const u128 magnitude{static_cast<u128>(negative ? -value : value)};
    std::string out;
    u128 rest{magnitude};
    while (rest != u128{0})
    {
        out.push_back(
            static_cast<char>('0' + static_cast<int>(static_cast<std::uint64_t>(rest % u128{10}))));
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
    // assert: both separators are stripped — a Windows drive path left the `## file` header
    // platform-dependent while every other section was already byte-identical.
    const auto slash{path.find_last_of("/\\")};
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
} // namespace

// post: a canonical, integer-domain text digest that must be byte-identical across every compiler,
// optimisation and stdlib leg the sweep builds.
// invariant: drives canon's public API only, over a public corpus — nothing here reveals the
// moat.
// refs: BIB:determinism_model
// note: an escaping exception aborts a standalone proof binary, which is acceptable here.
// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: det_proof <corpus-file> [<corpus-file> ...]\n";
        return 2;
    }

// assert: the digest is hashed byte for byte, so the separators must stay LF; Windows std::cout is
// text mode and would translate them, and POSIX makes this a no-op.
#if defined(_WIN32)
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif

    // invariant: nothing but the digest may reach this stdout — a log record interleaved into the
    // hashed bytes makes the digest a function of the operator, not of the corpus.
    // assert: this call no longer guards that; canon's un-initialised state is stderr-only. It
    // stays for the module tag and for level info rather than the fallback's warn.
    // refs: DN-53.D3
    // note: info rather than off — the destination was the defect, never the diagnostics.
    insight::logging::init_logging(spdlog::level::info);

    namespace tk = insight::tokenization;

    // assert: ASCII only — a non-ASCII byte makes the prologue depend on the compiler's execution
    // charset, and one line of the digest diverged on MSVC while the rest matched.
    std::cout << "# canon public determinism proof -- v4\n";

    // invariant: the composition is loop-invariant — the same package set tokenizes every file.
    const std::array<insight::semantic::SemanticPackageManifest, 4> manifests{
        insight::semantic::github::kManifest, insight::semantic::gitlab::kManifest,
        insight::semantic::jenkins::kManifest, insight::semantic::test_frameworks::kManifest};
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose(manifests)};

    // invariant: the composed identity hash carries no path, timestamp or link order, so it is
    // bit-identical across builds and legs.
    // note: the behavioural rows can match while the hash serialization itself diverges.
    std::cout << "# semantic_identity " << composed.identity_hex() << '\n';
    std::cout << "# semantic_packages";
    for (const auto& pkg : composed.packages())
        std::cout << ' ' << pkg.name << '@' << pkg.version;
    std::cout << '\n';

    // invariant: EVERY arm is applied to EVERY file — choosing an arm per file would be
    // inference, which is the per-line content dependence the declared-ingest cut deleted.
    // note: it also buys the negative cells: a Jenkins file read as github fires no gated row.
    // refs: ADR-22, ADR-23
    // note: GitHub's serving API stamps every line it returns, so the peel is declared here.
    struct Arm
    {
        // note: what the `## file` header records; "-" is the undeclared stream.
        std::string_view label;
        std::string_view dialect;
        std::span<const std::string_view> stack;
    };
    static constexpr std::array<std::string_view, 1> kRfc3339Stack{{"api-rfc3339-line-prefix"}};
    const std::array<Arm, 4> arms{
        Arm{.label = "-", .dialect = {}, .stack = {}},
        Arm{.label = "github+api-rfc3339-line-prefix",
            .dialect = insight::semantic::github::kDialect,
            .stack = kRfc3339Stack},
        // note: its runner prefix carries a line-delimitation field, so the dialect peels it.
        // refs: ADR-23.D1
        Arm{.label = "gitlab", .dialect = insight::semantic::gitlab::kDialect, .stack = {}},
        Arm{.label = "jenkins", .dialect = insight::semantic::jenkins::kDialect, .stack = {}},
    };

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

        for (const Arm& arm : arms)
        {
            // assert: one call at stream open resolves both semantic coordinates and the transport
            // stack, before the first line, so nothing downstream can depend on content.
            // refs: ADR-23
            const insight::semantic::ResolvedStream stream{insight::semantic::resolve_stream(
                composed, insight::transport::IngestDeclaration{
                              .stack = arm.stack, .dialect = arm.dialect, .channel = {}})};

            // assert: the peel runs at the CALLER and only `content` crosses; the vector applies it
            // exactly once per line.
            std::vector<std::string> peeled_lines;
            peeled_lines.reserve(lines.size());
            std::vector<std::optional<insight::Timestamp>> observation_times;
            observation_times.reserve(lines.size());
            for (const auto& raw : lines)
            {
                const insight::transport::RawPeeledLine peeled{stream.transport.peel_raw(raw)};
                peeled_lines.emplace_back(peeled.content);
                observation_times.push_back(peeled.observation_time);
            }

            // invariant: a fresh arena and Tokenizer per file and per arm, so the template set is a
            // pure function of that file's content and line order.
            constexpr std::size_t kArenaBytes{std::size_t{1} << 22};
            tk::ArenaAllocator arena{kArenaBytes};
            tk::Tokenizer tokenizer{arena, tk::MaskConfig{}, stream.semantics};

            // invariant: ordered, so iteration is by key and never by hash order — the
            // cross-stdlib std::hash hazard cannot appear.
            std::map<std::string, std::uint64_t> templates;
            struct Row
            {
                std::string level;
                std::string role;
                std::string tmpl;
                bool failure;
                bool warning;
                // note: true when the peel extracted an observation time for this line.
                bool observed;
            };
            std::vector<Row> rows;
            rows.reserve(lines.size());

            for (std::size_t idx{0}; idx < peeled_lines.size(); ++idx)
            {
                auto event{tokenizer.process_line(peeled_lines[idx])};
                // assert: an OBSERVATION time the caller injects, never an ordering key or a replay
                // input.
                // refs: ADR-22, ADR-23
                if (event && observation_times[idx])
                    event->timestamp = *observation_times[idx];
                std::string tmpl{event ? std::string{event->template_str}
                                       : std::string{"<<parse-error>>"}};
                std::string level{event ? std::string{insight::to_string(event->level)}
                                        : std::string{"Unknown"}};
                std::string role{event ? std::string{insight::to_string(event->structural_role)}
                                       : std::string{"None"}};
                const bool failure{insight::utils::contains_failure_cue(lines[idx])};
                const bool warning{insight::utils::contains_warning_cue(lines[idx])};
                ++templates[tmpl];
                rows.push_back({std::move(level), std::move(role), std::move(tmpl), failure,
                                warning, observation_times[idx].has_value()});
            }

            std::cout << "## file " << basename_of(argv[arg]) << " declared=" << arm.label << "\n";
            std::cout << "### templates (" << templates.size() << ")\n";
            std::uint64_t total{0};
            insight::det::FixedReducer reducer;
            for (const auto& [tmpl, count] : templates)
            {
                std::cout << count << '\t' << tmpl << '\n';
                total += count;
                // note: the det_math entropy term, a weighted log2 sum.
                reducer.add_weighted_log2(count, count);
            }

            std::cout << "### events\n";
            for (const auto& row : rows)
            {
                std::cout << row.level << '\t' << (row.failure ? 'F' : '-')
                          << (row.warning ? 'W' : '-') << (row.observed ? 'T' : '-') << '\t'
                          << row.role << '\t' << row.tmpl << '\n';
            }

            // assert: the entropy is emitted as the raw fixed-point integer, so no float formatting
            // can diverge between legs.
            std::cout << "### det_math total=" << total
                      << " sum_c_log2c_qk=" << i128_to_dec(reducer.raw()) << '\n';

            // assert: the compare covers only what this fixture EMITS, so these bytes are emitted
            // to be proven.
            const insight::RunOutcomeScan outcome_scan{
                insight::scan_run_outcome(peeled_lines, stream.semantics)};
            // assert: the empty side-input verdict is the third state, never a defaulted success
            // — this proof declares nothing and asserts rung 2.
            // refs: DN-32.D6, DN-32.D7
            // assert: `stream.semantics` is passed twice deliberately and cannot move a byte —
            // rung 1's vocabulary is read only when the pair names a token.
            const insight::RunOutcomeResolution outcome_resolution{
                insight::resolve_run_outcome({}, outcome_scan, stream.semantics, stream.semantics)};
            std::cout << "### run_outcome marker=" << (outcome_scan.marker_present ? '1' : '0')
                      << " token="
                      << (outcome_scan.marker_present ? std::string_view{outcome_scan.token}
                                                      : std::string_view{"-"})
                      << " resolved=" << insight::to_string(outcome_resolution.outcome) << '\n';
        }
    }

    return 0;
}
// NOLINTEND Test
