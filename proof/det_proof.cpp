// NOLINTBEGIN Test
// Canon public determinism proof — fixture.
//
// The externally-checkable half of the "same input → same output, bit-for-bit"
// claim (bibles/determinism_model.md §3).
// Drives ONLY canon's public Apache API — the tokenizer/stateless-masker template set,
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
#include <optional>
#include <span>
#include <spdlog/common.h> // spdlog::level — named for init_logging's sink/level choice below
#include <string>
#include <vector>
#if defined(_WIN32)
#include <fcntl.h> // _O_BINARY
#include <io.h>    // _setmode, _fileno
#endif

// 1.5.1 unwrap (Approach B): the textual public headers are gone — the canon module's
// public surface (Tokenizer, det::FixedReducer, failure_lexicon cues, to_string,
// CanonicalEvent / StructuralRole) is all reachable through the single facade import.
import insight.canon;
// ADR-17: the Tokenizer now takes a ComposedSemantics. The proof composes the SAME package set a
// product binary does — insight_semantic_github + insight_semantic_jenkins +
// insight_semantic_test_frameworks (the eidos composition TU's exact set and order) — so the
// composed pipeline (GHA + Jenkins dialects + test-file locations) is what the 5-leg byte-identity
// compare proves (G-SP-1; the grammar-2 composed identity = G-OUT-6, extending G-SP-4).
import insight.semantic.github;
import insight.semantic.gitlab;
import insight.semantic.jenkins;
import insight.semantic.test_frameworks;

namespace
{
// det::i128 → decimal string (no std::to_string overload exists). Works on BOTH det_math 128-bit
// representations (native __int128 on gcc/clang, the portable struct on MSVC) — det::i128/u128 are
// the aliases from det_int128.hpp, and the ops used here (sign via is_negative()-equivalent
// compare, magnitude(), %/÷ by ten, != 0) are provided on both. This output IS part of the
// canonical digest, so it must be byte-identical across compilers — hence it goes through the same
// portable shim.
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
    // Strip BOTH separators: the gate passes POSIX paths on Linux ('/') and Windows paths on MSVC
    // ('\\', e.g. D:\...\corpus\ci_build.log). Splitting on '/' only left the full drive path on
    // Windows, so the digest's `## file` header — and ONLY that header — diverged cross-OS while
    // every templates/events/det_math payload was already byte-identical. The digest must encode
    // the basename, never the platform-specific input path.
    const auto slash{path.find_last_of("/\\")};
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

    // Binary stdout: the digest is hashed byte-for-byte, so the `\n` separators MUST stay LF on
    // every platform. Windows std::cout defaults to TEXT mode, translating `\n`→`\r\n`, which would
    // make the Windows digest diverge from the LF Linux golden on EVERY line. POSIX has no such
    // translation, so this is a Windows-only no-op elsewhere. (The divergence this prevents is a
    // platform artifact, never an engine difference — keep it out of the canonical bytes.)
#if defined(_WIN32)
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif

    // Same invariant, second threat: NOTHING but the digest may reach this stdout. Canon's engine
    // loggers carry a wall-clock `[%Y-%m-%d %H:%M:%S.%e]` pattern, so a record interleaved into
    // the hashed bytes makes the digest a function of the operator instead of of the corpus: two
    // runs of ONE binary on ONE input differ. Measured 2026-08-18 on the `malf inventory` build,
    // back when an un-initialised canon resolved to spdlog's default STDOUT logger: 18 such lines
    // on stdout for a 6-line input. det_public_proof.sh's cells compiled the macros out
    // (`-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF`, scripts/det_public_proof.sh:138), which is why
    // the release legs never saw it, while the inventory build, scripts/samples_showcase.sh and a
    // desk run all got the corrupting default.
    //
    // canon's un-initialised state is stderr-only now (DN-53.D3), so this call is no longer what
    // stands between the digest and a log line. It stays because it buys the other two things
    // this fixture wants and the quiet fallback deliberately does not give: the module records
    // carry their `[insight.*]` tag, and the level is `info` rather than the fallback's `warn`.
    // Level stays `info` rather than `off`: the diagnostics were never the defect, their
    // DESTINATION was — silencing them would answer artifact purity by deleting observability.
    insight::logging::init_logging(spdlog::level::info);

    namespace tk = insight::tokenization;

    // ASCII-only banner: the digest is hashed byte-for-byte, so a non-ASCII byte (the old em-dash,
    // U+2014) made the prologue's bytes depend on the compiler's execution charset — MSVC emitted a
    // different sequence than gcc/clang's UTF-8, diverging the digest on that ONE line while every
    // other (pure-ASCII) section matched. A byte-hashed canonical output must contain no character
    // whose encoding varies by toolchain/locale. Plain '--'.
    // v2: the grammar-2 cut (ADR-17) — jenkins joins the composed set and every file section
    // gains a `### run_outcome` line (the console-tail scan surface the compare must cover).
    // v4: grammar-5 (ADR-17) — gitlab joins the composed set with its own declared ARM and its
    // own corpus file, so the numeric-field extractor and the prefix-verdict outcome walker are
    // both on the cross-OS compare surface. The composed `semantic_identity` moves with the grammar
    // token, which is expected and is exactly what the emitted digest line records.
    // v3: T4 (ADR-22) — the dialect and the transport peel are DECLARED, so every file is scored
    // once per declared ARM (below) instead of once. The compare covers only what the fixture
    // EMITS, and after T4 an undeclared stream sees no concretely-gated row at all: without the
    // declared arms this proof would still be green while covering none of the dialect walkers.
    std::cout << "# canon public determinism proof -- v4\n";

    // The composition is loop-invariant (the SAME package set tokenizes every file), so build it
    // ONCE here and thread it into each file's per-file arena/Tokenizer below.
    const std::array<insight::semantic::SemanticPackageManifest, 4> manifests{
        insight::semantic::github::kManifest, insight::semantic::gitlab::kManifest,
        insight::semantic::jenkins::kManifest, insight::semantic::test_frameworks::kManifest};
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose(manifests)};

    // G-SP-4 (ADR-17): the composed `semantic_identity` content hash must be bit-identical
    // across independent builds and every OS/ISA leg (no paths/timestamps/link-order in its input —
    // by construction, verified anyway). Emitting it into THIS canonical digest folds G-SP-4 into
    // the existing 5-leg byte-identity compare — a divergent identity byte diverges the digest and
    // fails the gate. This is a REAL, non-redundant surface: the behavioral rows below could match
    // while the hash serialization itself diverges cross-toolchain (endianness, string_view
    // ordering) — exactly what this line catches. The package list rides for legibility (the wire
    // block's §4.2 label).
    std::cout << "# semantic_identity " << composed.identity_hex() << '\n';
    std::cout << "# semantic_packages";
    for (const auto& pkg : composed.packages())
        std::cout << ' ' << pkg.name << '@' << pkg.version;
    std::cout << '\n';

    // ── The declared ARMS (ADR-23 / ADR-22). ──
    //
    // EVERY arm is applied to EVERY file, on purpose. Choosing an arm per file would be inference —
    // "this one looks like GHA" — which is precisely the per-line content dependence T4 deleted,
    // and a proof binary that re-introduced it to look tidy would be proving the wrong thing.
    // Applying all arms to all files also gives the compare the NEGATIVE cells for free: a Jenkins
    // fixture read under the `github` declaration must produce no GHA structure, byte-identically,
    // on every leg.
    //
    // The stamped arm declares `api-rfc3339-line-prefix` because GitHub's serving API stamps every
    // line it returns; that peel used to be DETECTED by a strategy in the github package and is now
    // the caller's declaration.
    struct Arm
    {
        std::string_view label; // what the `## file` header records; "-" is the undeclared stream
        std::string_view dialect;
        std::span<const std::string_view> stack;
    };
    static constexpr std::array<std::string_view, 1> kRfc3339Stack{{"api-rfc3339-line-prefix"}};
    const std::array<Arm, 4> arms{
        Arm{.label = "-", .dialect = {}, .stack = {}},
        Arm{.label = "github+api-rfc3339-line-prefix",
            .dialect = insight::semantic::github::kDialect,
            .stack = kRfc3339Stack},
        // GitLab declares NO transport stack: its 32-byte runner prefix is peeled by the dialect
        // strategy, not declared, because a faithful declaration is a two-element ordered stack
        // (timestamp then stream tag) and that is T2's work (ADR-23), not a package landing's.
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
            // The ONE call a caller makes at stream open (ADR-23): both semantic coordinates
            // verified and filtered into the view, and the transport stack resolved — all before
            // the first line, so nothing downstream can depend on content.
            const insight::semantic::ResolvedStream stream{insight::semantic::resolve_stream(
                composed, insight::transport::IngestDeclaration{
                              .stack = arm.stack, .dialect = arm.dialect, .channel = {}})};

            // The peel runs at the CALLER, outside the tokenizer, and only `content` crosses (§4 —
            // line identity is a pure function of peeled content). `peeled_lines` is materialized
            // because the run-outcome scan takes a whole-log span; it also keeps the peel applied
            // exactly once per line rather than once per consumer.
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

            constexpr std::size_t kArenaBytes{std::size_t{1} << 22};
            tk::ArenaAllocator arena{kArenaBytes};
            tk::Tokenizer tokenizer{arena, tk::MaskConfig{}, stream.semantics};

            std::map<std::string, std::uint64_t> templates; // ordered → deterministic iteration
            struct Row
            {
                std::string level;
                std::string role;
                std::string tmpl;
                bool failure;
                bool warning;
                bool observed; // the peel extracted an observation time for this line
            };
            std::vector<Row> rows;
            rows.reserve(lines.size());

            for (std::size_t idx{0}; idx < peeled_lines.size(); ++idx)
            {
                auto event{tokenizer.process_line(peeled_lines[idx])};
                // THE TIMESTAMP HANDOVER (ADR-23 / ADR-22): the extract is
                // the CALLER's to inject, because §4 forbids handing the stack to the Tokenizer.
                // ⚠ An OBSERVATION time, never an ordering key and never a replay input.
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
                reducer.add_weighted_log2(count, count); // Σ c·log2(c), the det_math entropy term
            }

            std::cout << "### events\n";
            for (const auto& row : rows)
            {
                std::cout << row.level << '\t' << (row.failure ? 'F' : '-')
                          << (row.warning ? 'W' : '-') << (row.observed ? 'T' : '-') << '\t'
                          << row.role << '\t' << row.tmpl << '\n';
            }

            std::cout << "### det_math total=" << total
                      << " sum_c_log2c_qk=" << i128_to_dec(reducer.raw()) << '\n';

            // G-OUT-6 behavioral arm: the console-tail run-outcome scan + the degenerate (no
            // side-input) resolution, per file and per declared arm. The composed identity line
            // above already pins the grammar-2 HASH; this pins the outcome-scan BYTES — the compare
            // covers only what the fixture emits, so the surface must be emitted to be proven.
            // Byte-exact ASCII walk + last-match-wins integer line index ⇒ deterministic by
            // construction; any cross-leg divergence here is an engine bug the gate must catch.
            const insight::RunOutcomeScan outcome_scan{
                insight::scan_run_outcome(peeled_lines, stream.semantics)};
            // `{}` is the empty SideInputVerdict — no token AND no vocabulary — which is exactly
            // what the bare `""` meant here before DN-32.D6 made a caller-declared verdict a PAIR:
            // this proof declares nothing and asserts rung 2, the console tail. Under DN-32.D7 an
            // empty pair is the third state, never a defaulted success.
            //
            // `stream.semantics` is passed TWICE deliberately, and it cannot move a byte. The two
            // parameters are different questions — rung 2 reads the STREAM view (a marker came out
            // of these bytes), rung 1 reads the DECLARER's vocabulary — but rung 1's vocabulary
            // argument is only ever evaluated when the pair NAMES one. With an empty pair the
            // resolver never touches it, so the fourth argument is unread on this path and the
            // proof's output is byte-identical to the pre-DN-32.D6 form.
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
