#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <picosha2.h>
#include <span>
#include <spdlog/common.h>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
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

// invariant: a column's digest is the first 16 bytes of SHA-256 over the column, the width canon's
// own TemplateId truncates to.
constexpr std::size_t kColumnDigestBytes{16};
constexpr std::size_t kSha256Bytes{32};
constexpr std::string_view kDigestFlag{"--digest"};
// note: what a line canon refused renders in every column — whether a line is refused is output.
constexpr std::string_view kRefusedLine{"!"};
constexpr std::string_view kRowEnd{"\n"};

std::string column_digest_hex(picosha2::hash256_one_by_one& hasher)
{
    hasher.finish();
    std::array<unsigned char, kSha256Bytes> digest{};
    hasher.get_hash_bytes(digest.begin(), digest.end());
    constexpr std::string_view kHex{"0123456789abcdef"};
    constexpr unsigned kNibbleBits{4U};
    constexpr unsigned kNibbleMask{0xFU};
    std::string out;
    for (std::size_t idx{0}; idx < kColumnDigestBytes; ++idx)
    {
        out.push_back(kHex[(static_cast<unsigned>(digest[idx]) >> kNibbleBits) & kNibbleMask]);
        out.push_back(kHex[static_cast<unsigned>(digest[idx]) & kNibbleMask]);
    }
    return out;
}

// post: the package list an arm's output can depend on, `-` when it composes none.
std::string packages_label(const insight::semantic::ComposedSemantics& composition)
{
    std::string out;
    for (const auto& pkg : composition.packages())
    {
        if (!out.empty())
            out.push_back(',');
        out.append(pkg.name).push_back('@');
        out.append(pkg.version);
    }
    return out.empty() ? std::string{"-"} : out;
}
} // namespace

// post: a canonical, integer-domain text digest that must be byte-identical across every compiler,
// optimisation and stdlib leg the sweep builds.
// post: under --digest it prints, per file and arm, a digest of each member's rendered column in
// place of the rows — what the cut's generation gate banks and compares across two cuts.
// invariant: drives canon's public API only, over a public corpus — nothing here reveals the
// moat.
// refs: BIB:determinism_model, DN-108.D24
// note: an escaping exception aborts a standalone proof binary, and main is its one script.
// NOLINTNEXTLINE(bugprone-exception-escape,readability-function-cognitive-complexity)
int main(int argc, char** argv)
{
    const bool digest_mode{argc > 1 && std::string_view{argv[1]} == kDigestFlag};
    const int first_file{digest_mode ? 2 : 1};
    if (argc <= first_file)
    {
        std::cerr << "usage: det_proof [--digest] <corpus-file> [<corpus-file> ...]\n";
        return 2;
    }

// assert: the digest is hashed byte for byte, so the separators must stay LF; Windows std::cout is
// text mode and would translate them, and POSIX makes this a no-op.
#ifdef _WIN32
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif

    // invariant: nothing but the digest may reach this stdout — a log record interleaved into the
    // hashed bytes makes the digest a function of the operator, not of the corpus.
    // assert: this call no longer guards that; canon's un-initialised state is stderr-only. It
    // stays for the module tag and for level info rather than the fallback's warn.
    // refs: ADR-5.D1
    // note: info rather than off — the destination was the defect, never the diagnostics.
    insight::logging::init_logging(spdlog::level::info);

    namespace tk = insight::tokenization;

    // assert: ASCII only — a non-ASCII byte makes the prologue depend on the compiler's execution
    // charset, and one line of the digest diverged on MSVC while the rest matched.
    std::cout << (digest_mode ? "# canon generation digest -- v1\n"
                              : "# canon public determinism proof -- v6\n");

    // invariant: the composition is loop-invariant — the same package set tokenizes every file.
    const std::array<insight::semantic::SemanticPackageManifest, 4> manifests{
        insight::semantic::github::kManifest, insight::semantic::gitlab::kManifest,
        insight::semantic::jenkins::kManifest, insight::semantic::test_frameworks::kManifest};
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose(manifests)};
    // invariant: canon's rules ALONE — no package row and no package strategy fires, so a move on
    // this arm is canon's whatever any package did.
    // note: the undeclared arm is not that — it composes every package's undeclared rows.
    const insight::semantic::ComposedSemantics no_packages{insight::semantic::compose({})};

    // invariant: the composed identity hash carries no path, timestamp or link order, so it is
    // bit-identical across builds and legs.
    // note: the behavioural rows can match while the hash serialization itself diverges.
    std::cout << "# semantic_identity " << composed.identity_hex() << '\n';
    std::cout << "# semantic_packages";
    for (const auto& pkg : composed.packages())
        std::cout << ' ' << pkg.name << '@' << pkg.version;
    std::cout << '\n';
    if (digest_mode)
    {
        std::cout << "# token " << insight::kCanonicalizationVersion << '\n';
        std::cout << "# members";
        for (const std::string_view member : tk::kProjectionMembers)
            std::cout << ' ' << member;
        std::cout << '\n';
    }

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
        const insight::semantic::ComposedSemantics* composition;
    };
    static constexpr std::array<std::string_view, 1> kRfc3339Stack{{"api-rfc3339-line-prefix"}};
    const std::array<Arm, 5> arms{
        Arm{.label = "no-packages", .dialect = {}, .stack = {}, .composition = &no_packages},
        Arm{.label = "-", .dialect = {}, .stack = {}, .composition = &composed},
        Arm{.label = "github+api-rfc3339-line-prefix",
            .dialect = insight::semantic::github::kDialect,
            .stack = kRfc3339Stack,
            .composition = &composed},
        // note: its runner prefix carries a line-delimitation field, so the dialect peels it.
        // refs: ADR-23.D1
        Arm{.label = "gitlab",
            .dialect = insight::semantic::gitlab::kDialect,
            .stack = {},
            .composition = &composed},
        Arm{.label = "jenkins",
            .dialect = insight::semantic::jenkins::kDialect,
            .stack = {},
            .composition = &composed},
    };

    tk::ProjectionColumns columns;
    for (int arg = first_file; arg < argc; ++arg)
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

        // note: the digest names the file as it was handed over, the only name its caller can map.
        if (digest_mode)
            std::cout << "## file " << argv[arg] << '\n';

        for (const Arm& arm : arms)
        {
            // assert: one call at stream open resolves both semantic coordinates and the transport
            // stack, before the first line, so nothing downstream can depend on content.
            // refs: ADR-23
            const insight::semantic::ResolvedStream stream{insight::semantic::resolve_stream(
                *arm.composition, insight::transport::IngestDeclaration{
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
            std::vector<std::string> rows;
            std::array<picosha2::hash256_one_by_one, tk::kProjectionMembers.size()> hashers;
            std::uint64_t events{0};

            for (std::size_t idx{0}; idx < peeled_lines.size(); ++idx)
            {
                auto event{tokenizer.process_line(peeled_lines[idx])};
                const auto& observation{observation_times[idx]};
                // assert: an OBSERVATION time the caller injects, never an ordering key or a replay
                // input.
                // refs: ADR-22, ADR-23
                if (event && observation)
                    event->timestamp = *observation;
                if (event)
                    tk::render_projection(*event, columns);
                ++events;

                if (digest_mode)
                {
                    for (std::size_t member{0}; member < hashers.size(); ++member)
                    {
                        const std::string_view text{event ? std::string_view{columns[member]}
                                                          : kRefusedLine};
                        hashers[member].process(text.begin(), text.end());
                        hashers[member].process(kRowEnd.begin(), kRowEnd.end());
                    }
                    continue;
                }

                ++templates[event ? std::string{event->template_str}
                                  : std::string{"<<parse-error>>"}];
                std::string row;
                row.push_back(insight::utils::contains_failure_cue(lines[idx]) ? 'F' : '-');
                row.push_back(insight::utils::contains_warning_cue(lines[idx]) ? 'W' : '-');
                // note: T when the peel extracted an observation time for this line.
                row.push_back(observation.has_value() ? 'T' : '-');
                for (const std::string& text : columns)
                {
                    row.push_back('\t');
                    row.append(event ? std::string_view{text} : kRefusedLine);
                }
                rows.push_back(std::move(row));
            }

            if (digest_mode)
            {
                std::cout << "### arm " << arm.label << " packages "
                          << packages_label(*arm.composition) << " identity "
                          << stream.semantics.identity_hex() << " rows " << events << '\n';
                for (std::size_t member{0}; member < hashers.size(); ++member)
                    std::cout << tk::kProjectionMembers[member] << ' '
                              << column_digest_hex(hashers[member]) << '\n';
                continue;
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

            // note: a row is the failure, warning and observation cues, then canon's projection.
            std::cout << "### events\n";
            for (const std::string& row : rows)
                std::cout << row << '\n';

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
            // refs: ADR-22.D10
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
