// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_github_generated_equivalence.cpp — THE EQUIVALENCE HARNESS between the hand-written GitHub
// Actions semantic package (`insight.semantic.github`) and the PROJECTION of `github.dialect.yaml`
// (`insight.semantic.github_gen`), which `dialect_package_codegen.py` writes into this build tree
// on every build. It is the instrument that decides whether the generated package may take the
// hand-written one's place (ADR-17 / DN-17.D9 step 3, protocol mechanics amended at DN-17.D21).
//
// ── WHY THIS FILE IS HERE AND NOWHERE ELSE ──────────────────────────────────────────────────────
// `insight.semantic.github_gen` lives in a PRIVATE CMake module file set: it textually includes a
// projection that exists only inside a build tree, so it is never installed and never exported, and
// `insight_semantic_github_tests` is the ONLY target in the workspace that can name it. There is
// also no seam to cross — both packages are static data in one binary and every property below is a
// pure function of that data — so this is a unit-suite property, not an integration one. Homed
// here, it regenerates from the declaration on every build (the target depends on
// `insight_semantic_github_dialect_gen`) and `gtest_discover_tests` REGISTERS each case with ctest,
// so it is not a gate someone must remember to invoke.
//
// ── WHAT EACH LEG'S ORACLE IS, AND WHERE ITS INDEPENDENCE COMES FROM ────────────────────────────
// Both packages derive from the same declaration in the same build, so "SUT == ORACLE" is the live
// hazard here and each leg names its answer to it rather than assuming one:
//
//   * LEG 2 (manifest field-for-field) — oracle: the HAND-WRITTEN manifest, which no generator
//     wrote. Independence is authorship: `src/github.cppm` is human-authored C++ and the projection
//     is Python output. The comparator is canon's `manifest_equivalence_report`, NOT a comparison
//     written here: a second copy of a predicate is exactly what makes a comparison agree with
//     itself. It binds all fourteen manifest members by structured binding, so a fifteenth member
//     is a compile error in canon rather than a silently uncompared field.
//   * LEG 3 (code tier verdict-for-verdict) — oracle: the hand-written hook. The two hooks are
//     DISTINCT SYMBOLS (asserted below): the projected `kManifest` takes the address of a hook
//     defined in `src/github_gen_provenance.cpp`, module-attached to the generated module, so this
//     leg proves the projection BOUND THE HOOK THE DECLARATION NAMES and that the bound hook
//     answers what the shipped one answers. It cannot prove the two definitions stay in sync beyond
//     the corpus it is driven over — see the corpus's own non-degeneracy guard.
//   * LEG 4 (semantic_identity) — oracle: the hand-written package's composed digest. This is an
//     AGREEMENT compare and it proves invariance, never a frozen value: if a future pass edits the
//     declaration and `github.cppm` consistently, the digest moves and this leg stays green, which
//     is correct for an equivalence instrument. What pins the shipped ruleset itself is
//     `test_github_manifest_shape.cpp` (version + per-member counts), in this same binary.
//   * LEG 5 (the gates the package passes, re-run on the projection) — oracle: canon's conformance
//     kit, which is package-agnostic, ships to external package authors, and knows nothing about
//     either declaration. That is the one oracle here that is independent of BOTH subjects.
//
// LEG 1 of the protocol — "the generated TU satisfies the package's own fences" — is deliberately
// NOT a case in this file. Its red is a BUILD FAILURE, and nothing that must build this binary can
// assert that this binary does not build; when it fires it reds every case below with it. Its
// falsifiable form is the generator's own `--selftest`, which refuses each malformed declaration BY
// NAME and which `CMakeLists.txt` runs as the FIRST command of the codegen custom command, on every
// build of this package, before any generation (DN-17.D21 §1).
//
// ── RED-CAPABILITY IS OBSERVED HERE, NOT ASSERTED ───────────────────────────────────────────────
// Three cases below are DISCONFIRMING CONTROLS that mutate a copy of the projected manifest (or
// substitute a wrong hook) and require the instrument to notice. They make green mean something
// permanently, in the same binary, on the same subject — instead of resting on a mutation someone
// ran once by hand and wrote down.
//
// Determinism: seedless, single-threaded, no wall clock, no float. Every input is constexpr static
// data or a string built from it in declared order; the composition walkers are pure byte
// functions. Verbose on failure: every check prints the member, the index, and both values.
#include <gtest/gtest.h>

import std;
import insight.canon;             // compose / ComposedSemantics
import insight.canon.conformance; // run / round_trip_report / manifest_equivalence_report
import insight.semantic.github;
import insight.semantic.github_gen;

namespace
{
using insight::semantic::SemanticPackageManifest;
using insight::semantic::conformance::Report;

const SemanticPackageManifest& hand() noexcept
{
    return insight::semantic::github::kManifest;
}
const SemanticPackageManifest& gen() noexcept
{
    return insight::semantic::github_gen::kManifest;
}

// ⚠ NEVER compose the two together (DN-17.D9): the declaration says `name: "github"` VERBATIM, so
// composing both would publish one dialect name twice. Each is composed ALONE, which is also the
// only composition whose digest is comparable to the other's.
[[nodiscard]] insight::semantic::ComposedSemantics compose_alone(const SemanticPackageManifest& one)
{
    const std::array<SemanticPackageManifest, 1> single{one};
    return insight::semantic::compose(single);
}

void expect_all_checks_passed(const Report& report, const char* what)
{
    for (const auto& check : report.checks)
        EXPECT_TRUE(check.passed) << what << " [" << check.name << "] " << check.detail;
    EXPECT_TRUE(report.all_passed()) << what << ": " << report.summary();
}

// Two reports agree when they carry the same checks, in the same order, with the same verdicts.
// This is what "the gate the current package passes, re-run with the generated package in its
// place, identical output" means at the report tier.
void expect_reports_agree(const Report& lhs, const Report& rhs, const char* what)
{
    ASSERT_EQ(lhs.checks.size(), rhs.checks.size())
        << what << ": the two runs produced different check COUNTS — hand-written "
        << lhs.checks.size() << " (" << lhs.summary() << "), generated " << rhs.checks.size()
        << " (" << rhs.summary() << ")";
    for (std::size_t i{0}; i < lhs.checks.size(); ++i)
    {
        EXPECT_EQ(lhs.checks[i].name, rhs.checks[i].name)
            << what << ": check " << i << " is named differently in the two runs";
        EXPECT_EQ(lhs.checks[i].passed, rhs.checks[i].passed)
            << what << ": check " << i << " [" << lhs.checks[i].name
            << "] disagrees — hand-written " << (lhs.checks[i].passed ? "PASS" : "FAIL") << " \""
            << lhs.checks[i].detail << "\", generated " << (rhs.checks[i].passed ? "PASS" : "FAIL")
            << " \"" << rhs.checks[i].detail << "\"";
    }
}

// ── The code-tier corpus ────────────────────────────────────────────────────────────────────────
// Part 1: the command-echo SGR grammar, carried verbatim from `test_github_echoed_source.cpp` —
// the fixture set DN-17.D21 §2 names for this leg. It is the predicate's own decision boundary:
// both accepted parameter orderings, all three accepted resets, the empty span, the trailing
// CR/LF, and five shapes that must be refused (a red `31` colour, a bare workflow-command marker,
// un-wrapped bytes after the span, a missing close, plain text) plus a still-stamped line, which is
// refused since the T4 rip of the hook's leading-stamp skip.
constexpr std::array<std::string_view, 14> kGrammarCorpus{
    "\x1b[36;1m    echo \"Download failed after 3 attempts\" >&2\x1b[0m",
    "\x1b[36;1mset -e\x1b[0m",
    "2026-06-09T09:40:20.4309100Z \x1b[36;1m    exit 1\x1b[0m",
    "\x1b[1;36mif [ $i -eq 3 ]; then\x1b[0m",
    "\x1b[36;1m\x1b[0m",
    "\x1b[36;1mexit 1\x1b[m",
    "\x1b[36;1mexit 1\x1b[39m",
    "\x1b[36;1m  fi\x1b[0m\r\n",
    "\x1b[31mERROR\x1b[0m: db connection refused",
    "##[error]Process completed with exit code 1",
    "\x1b[36;1mecho building\x1b[0m && make all",
    "\x1b[36;1mset -e",
    "    echo \"Download failed after 3 attempts\"",
    ""};

// Part 2: DERIVED FROM THE MANIFEST, so the corpus grows with the declaration instead of with
// someone's memory. Every declared key — structural role, intent marker, generation template,
// level lift, outcome token — is fed to the hooks in three forms: bare, command-echo-wrapped, and
// wrapped in a colour that is NOT the command echo. A row kind added to the declaration widens
// this leg on the next build with no edit here.
[[nodiscard]] std::vector<std::string> code_tier_corpus()
{
    std::vector<std::string> corpus;
    for (const std::string_view line : kGrammarCorpus)
        corpus.emplace_back(line);

    std::vector<std::string_view> keys;
    for (const auto& row : hand().roles)
        keys.push_back(row.prefix);
    for (const auto& row : hand().markers)
        keys.push_back(row.prefix);
    for (const auto& row : hand().emits)
        keys.push_back(row.prefix);
    for (const auto& row : hand().level_lifts)
        keys.push_back(row.prefix);
    for (const auto& row : hand().outcome_tokens)
        keys.push_back(row.token);

    for (const std::string_view key : keys)
    {
        corpus.emplace_back(key);
        corpus.emplace_back("\x1b[36;1m" + std::string{key} + "\x1b[0m");
        corpus.emplace_back("\x1b[31m" + std::string{key} + "\x1b[0m");
    }
    return corpus;
}

// A printable rendering of a raw line: ESC is invisible in a terminal and an assertion nobody can
// read is an assertion nobody can act on.
[[nodiscard]] std::string printable(std::string_view line)
{
    std::string out;
    for (const char chr : line)
    {
        const auto byte{static_cast<unsigned char>(chr)};
        if (byte == 0x1bU)
            out += "<ESC>";
        else if (byte == '\r')
            out += "<CR>";
        else if (byte == '\n')
            out += "<LF>";
        else if (byte < 0x20U)
            out += "<" + std::to_string(static_cast<unsigned>(byte)) + ">";
        else
            out += chr;
    }
    return out;
}

// A manifest that is the projection MINUS its last structural-role row. The span still views the
// projection's own static storage, so nothing here fabricates a row: this is the smallest possible
// real difference between two rulesets, and it is what the disconfirming controls mutate.
[[nodiscard]] SemanticPackageManifest one_role_shorter(const SemanticPackageManifest& source)
{
    SemanticPackageManifest mutant{source};
    mutant.roles = source.roles.first(source.roles.size() - 1U);
    return mutant;
}
} // namespace

// ════════════════════════════════════════════════════════════════════════════════════════════════
// LEG 2 — the manifest is equal FIELD FOR FIELD
// ════════════════════════════════════════════════════════════════════════════════════════════════

// The subject must be LIVE before any equivalence green means anything.
// `manifest_equivalence_report` is an EQUIVALENCE report and never a NON-VACUITY one — its own
// contract says two EMPTY manifests return fourteen green checks — and it hands the choice of
// subject to the caller. This case IS that choice, asserted rather than assumed: every member the
// hand-written package populates is populated in the projection too, and every member it declares
// EMPTY is empty in the projection too. The exact counts and the version are pinned separately, in
// `test_github_manifest_shape.cpp` in this same binary, so this file states no golden of its own
// and cannot decay against one.
TEST(GithubGeneratedEquivalence, TheComparedSubjectIsLiveInBothPackages)
{
    // The structured binding is the coverage instrument, exactly as it is in the comparator: a
    // FIFTEENTH manifest member is a compile error here, never a member this liveness claim
    // silently never looked at.
    const auto& [name, version, roles, markers, emits, level_lifts, locations, value_classes,
                 outcome_tokens, outcome_markers, channels, dialect_revisions, strategy,
                 echoed_source]{gen()};

    EXPECT_FALSE(name.empty()) << "the projected package declares no name";
    EXPECT_FALSE(version.empty()) << "the projected package declares no ruleset version";
    EXPECT_FALSE(roles.empty()) << "no structural-role rows in the projection";
    EXPECT_FALSE(markers.empty()) << "no intent-marker rows in the projection";
    EXPECT_FALSE(emits.empty()) << "no generation-template rows in the projection";
    EXPECT_FALSE(level_lifts.empty()) << "no level-lift rows in the projection";
    EXPECT_FALSE(outcome_tokens.empty()) << "no run-outcome token rows in the projection";
    EXPECT_FALSE(channels.empty()) << "no declared intent-channel vocabulary in the projection";
    EXPECT_FALSE(dialect_revisions.empty()) << "no declared revision vocabulary in the projection";

    // The declared ABSENCES, asserted positively: the projection must reproduce the argued
    // emptiness, not merely fail to disagree about it. An omission and an exclusion must not look
    // alike at the value tier.
    EXPECT_TRUE(locations.empty()) << "the projection ships " << locations.size()
                                   << " location rows; this dialect declares NONE";
    EXPECT_TRUE(value_classes.empty()) << "the projection ships " << value_classes.size()
                                       << " value-class rows; this dialect declares NONE";
    EXPECT_TRUE(outcome_markers.empty()) << "the projection ships " << outcome_markers.size()
                                         << " outcome-marker rows; this dialect declares NONE";

    EXPECT_EQ(strategy, nullptr) << "the projection ships a format strategy the shipped package "
                                    "does not have";
    EXPECT_NE(echoed_source, nullptr)
        << "the projection's code tier is UNBOUND — `kManifest.echoed_source` is nullptr, so the "
           "verdict leg below would be comparing nothing";
}

TEST(GithubGeneratedEquivalence, TheManifestIsEqualFieldForField)
{
    const Report report{insight::semantic::conformance::manifest_equivalence_report(hand(), gen())};
    ASSERT_EQ(report.checks.size(), 14U)
        << "the comparator returned " << report.checks.size()
        << " checks; SemanticPackageManifest has fourteen members and the comparator returns "
           "exactly one check per member — a different count means the manifest grew and this "
           "expectation is the record of what was compared";
    expect_all_checks_passed(report, "hand-written vs projected manifest");
}

// DISCONFIRMING CONTROL for leg 2. Delete one structural-role row from the projection and require
// the comparator to say so, by member name. Without this, "fourteen green checks" is a statement
// about a comparator nobody in this binary has ever seen fail.
TEST(GithubGeneratedEquivalence, ControlTheComparatorRedsOnARemovedRow)
{
    ASSERT_GT(gen().roles.size(), 1U) << "the control needs at least two role rows to remove one";
    const SemanticPackageManifest mutant{one_role_shorter(gen())};
    const Report report{
        insight::semantic::conformance::manifest_equivalence_report(hand(), mutant)};

    EXPECT_FALSE(report.all_passed())
        << "removing a structural-role row from the projection changed NOTHING the comparator "
           "reports — the equivalence leg cannot fail, so its green says nothing: "
        << report.summary();

    const auto failed{std::ranges::find_if(report.checks, [](const auto& check) noexcept
                                           { return !check.passed; })};
    ASSERT_NE(failed, report.checks.end()) << report.summary();
    EXPECT_EQ(failed->name, "equivalence.roles")
        << "the removed row is a ROLE row, so the failing check must be the roles one; got ["
        << failed->name << "] " << failed->detail;
}

// ════════════════════════════════════════════════════════════════════════════════════════════════
// LEG 3 — the CODE TIER agrees verdict for verdict
// ════════════════════════════════════════════════════════════════════════════════════════════════
// Legs 2 and 4 are both structurally blind to this. `compose.cpp` serialises the code tier as two
// PRESENCE BYTES ("code tier, nominal"), and the comparator names its two code-tier checks
// `strategy_presence_only` / `echoed_source_presence_only` for the same reason: comparing two
// different function symbols by pointer value is a guaranteed can't-PASS. So the digest legs prove
// the ROWS identical while never proving the code tier does the same thing. This leg is that
// proof, and it is the only one of the five that reaches executable dialect knowledge.

TEST(GithubGeneratedEquivalence, TheProjectionBindsItsOwnDistinctHook)
{
    ASSERT_NE(hand().echoed_source, nullptr) << "the shipped package's code tier is unbound";
    ASSERT_NE(gen().echoed_source, nullptr) << "the projection's code tier is unbound";

    // Independence, asserted rather than assumed: if the projection had bound the hand-written
    // symbol, the verdict leg below would be comparing one function with itself and could not fail.
    EXPECT_TRUE(hand().echoed_source != gen().echoed_source)
        << "the two manifests bind the SAME function symbol — the verdict comparison would be "
           "vacuous. The projected package's hook must be its own module-attached definition "
           "(src/github_gen_provenance.cpp).";

    // And it is the hook the declaration NAMES, reached through the manifest rather than through
    // the namespace: what ships is the pointer, so what is proven must be the pointer.
    EXPECT_TRUE(gen().echoed_source == &insight::semantic::github_gen::is_echoed_source)
        << "`kManifest.echoed_source` in the projection does not point at the projected package's "
           "own `is_echoed_source` — the generator bound some other symbol";
}

TEST(GithubGeneratedEquivalence, TheCodeTierAgreesVerdictForVerdict)
{
    const std::vector<std::string> corpus{code_tier_corpus()};
    ASSERT_FALSE(corpus.empty()) << "empty corpus — the leg would pass on nothing";

    const auto lhs_hook{hand().echoed_source};
    const auto rhs_hook{gen().echoed_source};
    ASSERT_NE(lhs_hook, nullptr);
    ASSERT_NE(rhs_hook, nullptr);

    std::size_t accepted{0};
    std::size_t refused{0};
    std::size_t disagreements{0};
    for (const std::string& line : corpus)
    {
        const bool lhs{lhs_hook(line)};
        const bool rhs{rhs_hook(line)};
        (lhs ? accepted : refused) += 1U;
        if (lhs != rhs)
        {
            ++disagreements;
            EXPECT_EQ(lhs, rhs) << "the two code tiers disagree on [" << printable(line)
                                << "] — hand-written " << (lhs ? "true" : "false") << ", projected "
                                << (rhs ? "true" : "false");
        }
    }

    // A green is a claim about a SUBJECT, so this leg states its subject on every run rather than
    // only when it fails: the corpus is derived from the declaration and grows with it, and a
    // reader who only ever sees "Passed" would otherwise have no way to know over how many lines.
    std::cout << "[ CORPUS   ] code tier: " << corpus.size() << " lines — " << accepted
              << " echoed-source, " << refused << " not, " << disagreements << " disagreements\n";

    // NON-DEGENERACY: a hook that answered a constant would agree with any other hook that answered
    // the same constant, and this leg would be green on a predicate that decides nothing. The
    // corpus must exercise BOTH verdicts or the agreement above is worthless.
    EXPECT_GT(accepted, 0U) << "no corpus line is recognized as echoed-source — the corpus does "
                               "not reach the predicate's accepting branch ("
                            << corpus.size() << " lines)";
    EXPECT_GT(refused, 0U) << "every corpus line is recognized as echoed-source — the corpus does "
                              "not reach the predicate's refusing branch ("
                           << corpus.size() << " lines)";
    EXPECT_EQ(disagreements, 0U) << disagreements << " of " << corpus.size()
                                 << " corpus lines got different verdicts from the two code tiers";
}

// DISCONFIRMING CONTROL for leg 3: the comparison loop above must be able to SEE a disagreement.
// A wrong hook — one that answers a constant — is driven over the same corpus and required to
// diverge from the shipped one. Without this, "0 disagreements" is a statement about a loop nobody
// has ever watched detect anything.
TEST(GithubGeneratedEquivalence, ControlTheVerdictComparisonRedsOnAWrongHook)
{
    const std::vector<std::string> corpus{code_tier_corpus()};
    const auto lhs_hook{hand().echoed_source};
    ASSERT_NE(lhs_hook, nullptr);

    const insight::semantic::ProvenanceHook always_false{[](std::string_view) noexcept -> bool
                                                         { return false; }};

    std::size_t disagreements{0};
    for (const std::string& line : corpus)
        if (lhs_hook(line) != always_false(line))
            ++disagreements;

    EXPECT_GT(disagreements, 0U)
        << "a constant-false hook agreed with the shipped code tier on ALL " << corpus.size()
        << " corpus lines — the verdict comparison cannot distinguish a wrong hook from a right "
           "one, so its agreement means nothing";
}

// ════════════════════════════════════════════════════════════════════════════════════════════════
// LEG 4 — `semantic_identity` digest equal
// ════════════════════════════════════════════════════════════════════════════════════════════════

TEST(GithubGeneratedEquivalence, TheComposedSemanticIdentityIsEqual)
{
    const insight::semantic::ComposedSemantics lhs{compose_alone(hand())};
    const insight::semantic::ComposedSemantics rhs{compose_alone(gen())};
    EXPECT_EQ(lhs.identity(), rhs.identity())
        << "the two packages compose to DIFFERENT semantic_identity digests:\n"
           "  hand-written: "
        << lhs.identity_hex() << "\n  projected:    " << rhs.identity_hex()
        << "\nThe digest covers every data-tier field the serializer writes, so the two rulesets "
           "differ somewhere; the field-for-field leg above says WHERE.";
}

// DISCONFIRMING CONTROL for leg 4. The digest is a 16-byte hash and can only ever say THAT two
// rulesets differ — so the one thing worth proving about it here is that it does say so. One
// removed role row must move it.
TEST(GithubGeneratedEquivalence, ControlTheDigestMovesOnARemovedRow)
{
    ASSERT_GT(gen().roles.size(), 1U) << "the control needs at least two role rows to remove one";
    const insight::semantic::ComposedSemantics lhs{compose_alone(hand())};
    const insight::semantic::ComposedSemantics mutant{compose_alone(one_role_shorter(gen()))};
    EXPECT_NE(lhs.identity(), mutant.identity())
        << "removing a structural-role row from the projection left semantic_identity UNMOVED ("
        << lhs.identity_hex()
        << ") — the digest leg cannot fail on a real ruleset difference, so its green says nothing";
}

// ════════════════════════════════════════════════════════════════════════════════════════════════
// LEG 5 (the in-package half) — the gates this package passes, re-run on the projection
// ════════════════════════════════════════════════════════════════════════════════════════════════
// SCOPE, STATED SO IT IS NOT OVER-READ. The protocol's fifth condition is the golden/corpus gates
// re-run with the generated package linked in the hand-written one's place, byte-identical output.
// That substitution is the SWAP itself and it cannot happen here: the projected module is a PRIVATE
// file set that no other target can name, and the corpus arm that runs on real third-party GitHub
// bytes lives in another repo and skips green without its slice. What IS available at this stage —
// and what these two cases are — is the same condition at the PACKAGE-SUITE tier: canon's
// conformance kit and its round-trip closure kit, the two gates the shipped package is held to,
// re-run against the projection with the same report expected check for check.
//
// The kit is the one oracle in this file that is independent of BOTH subjects: it is
// package-agnostic, canon-shipped, self-adapting over whatever rows it is handed, and it ships
// installed so an external package author runs the identical gate.

TEST(GithubGeneratedEquivalence, TheProjectionPassesTheCanonConformanceKit)
{
    const Report projected{insight::semantic::conformance::run(gen())};
    expect_all_checks_passed(projected, "the projected package under the conformance kit");

    const Report shipped{insight::semantic::conformance::run(hand())};
    expect_reports_agree(shipped, projected, "conformance kit");
}

TEST(GithubGeneratedEquivalence, TheProjectionClosesTheRoundTrip)
{
    const insight::semantic::ComposedSemantics gen_composed{compose_alone(gen())};
    const Report projected{insight::semantic::conformance::round_trip_report(gen(), gen_composed)};
    ASSERT_FALSE(projected.checks.empty())
        << "no round-trip checks ran on the projection — it declared no markers?";
    expect_all_checks_passed(projected, "the projected package under the round-trip kit");

    const insight::semantic::ComposedSemantics hand_composed{compose_alone(hand())};
    const Report shipped{insight::semantic::conformance::round_trip_report(hand(), hand_composed)};
    expect_reports_agree(shipped, projected, "round-trip kit");
}
// NOLINTEND
