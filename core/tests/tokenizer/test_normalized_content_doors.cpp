// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_normalized_content_doors.cpp — the DOOR CENSUS of the typed ingest precondition
// (ADR-21.D3's door census and ADR-21.D4's pinned-absence traits, plus Eqya's ruling-1 addition).
//
// Homed beside the walker tests because the walkers are what the doors guard (flagged for Kleio's
// confirmation — test homing is hers). Four legs:
//
//   1. THE ABSENT CONSTRUCTOR, pinned as negative type traits. §12.5.5 item 4: an absence is the
//      one thing no green test observes — expressed here as static_asserts on PUBLIC
//      constructibility, which run in every build of this TU (stronger than a build-failing
//      fixture: it cannot be skipped). `std::is_constructible` tests accessibility from a
//      non-friend context, which is exactly the boundary claim.
//   2. THE OUTBOUND ACCESSOR EXISTS — NormalizedContent → string_view must remain expressible
//      (the seam's two byte-readers have no other edit; §12.5.2's "do not harden it away").
//   3. THE DECLARED PEEL DOOR — TransportStack::peel(NormalizedLine) → PeeledLine carries a
//      NormalizedContent, observation time and the blank-drop, over the catalogue's shipped row.
//      (The raw door peel_raw is covered by the transport suite; it mints nothing.)
//   4. THE FRIEND CENSUS — Eqya's ruling 1: "a friend list is only an audit surface if something
//      FAILS when it grows." A source census over the two declaring files pins NormalizedContent's
//      friends at exactly {NormalizedLine, LogParserPasskey} and the passkey's at exactly
//      {LogParser}. Verbose on failure: prints every friend line it found.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

import insight.canon.test;

using insight::tokenization::normalize;
using insight::tokenization::NormalizedContent;
using insight::tokenization::NormalizedLine;

// ── Leg 1: the absent constructor (the whole mechanism, §12.2) ──────────────────────────────────
static_assert(!std::is_constructible_v<NormalizedContent, std::string_view>,
              "NormalizedContent must NOT be constructible from raw bytes — that single absence "
              "is the completeness theorem's premise");
static_assert(!std::is_constructible_v<NormalizedContent, const char*>);
static_assert(!std::is_constructible_v<NormalizedContent, std::string>);
static_assert(!std::is_default_constructible_v<NormalizedContent>);
static_assert(!std::is_constructible_v<NormalizedLine, std::string_view>,
              "NormalizedLine is produced ONLY by normalize()");
static_assert(!std::is_default_constructible_v<NormalizedLine>);
// The §12.2 shape guard is asserted at the declaration site (canon.api.cppm); re-asserted here so
// this census file fails closed even if the declaration-site asserts are ever moved.
static_assert(sizeof(NormalizedContent) == sizeof(std::string_view));
static_assert(std::is_trivially_copyable_v<NormalizedContent>);

// ── Leg 2: the outbound accessor (outbound weakens nothing; its absence breaks the seam) ────────
static_assert(
    std::is_same_v<decltype(std::declval<const NormalizedContent&>().bytes()), std::string_view>);
static_assert(
    std::is_same_v<decltype(std::declval<const NormalizedLine&>().bytes()), std::string_view>);
// And the inbound direction must NOT exist as a conversion either.
static_assert(!std::is_convertible_v<std::string_view, NormalizedContent>);
static_assert(!std::is_convertible_v<std::string_view, NormalizedLine>);

namespace
{

// ── Leg 3: the declared peel door ────────────────────────────────────────────────────────────────
TEST(NormalizedContentDoors, DeclaredPeelYieldsTypedContentAndObservationTime)
{
    const std::array stack_names{std::string_view{"api-rfc3339-line-prefix"}};
    const insight::transport::TransportStack stack{insight::transport::resolve_transport_stack(
        insight::transport::IngestDeclaration{.stack = stack_names})};

    std::string scratch;
    const NormalizedLine line{normalize("2026-04-15T22:20:38.2879579Z ##[error]boom", scratch)};
    const insight::transport::PeeledLine peeled{stack.peel(line)};
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(peeled.content)>, NormalizedContent>);
    EXPECT_EQ(peeled.content.bytes(), "##[error]boom")
        << "declared peel over normalized bytes must remove the stamp + separator, got \""
        << std::string{peeled.content.bytes()} << '"';
    EXPECT_TRUE(peeled.observation_time.has_value())
        << "the declared LinePrefixTimestamp must extract the observation time";

    // The blank-drop survives the typed door (ADR-23's bundled decline).
    const NormalizedLine bare{normalize("2026-04-15T22:20:38.2879579Z ", scratch)};
    EXPECT_TRUE(stack.peel(bare).is_blank());

    // And the two doors agree byte-for-byte — one algorithm, two proofs (a divergence would be
    // the two-implementations defect the whole contract is about).
    EXPECT_EQ(peeled.content.bytes(), stack.peel_raw(line.bytes()).content)
        << "peel and peel_raw must be the same algorithm over the same bytes";
}

// ── Leg 4: the friend census ─────────────────────────────────────────────────────────────────────
[[nodiscard]] std::string read_source(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// The friend-declaration lines inside the class body opening at `class <name>` (first definition
// occurrence). Brace-matched, comment-line-insensitive (a `friend` in a comment does not count —
// lines are trimmed and must START with `friend`).
[[nodiscard]] std::vector<std::string> friends_of(const std::string& source,
                                                  const std::string& class_name)
{
    const std::string needle{"class " + class_name + "\n"};
    std::size_t pos{source.find(needle)};
    if (pos == std::string::npos)
        return {"<class '" + class_name + "' not found>"};
    pos = source.find('{', pos);
    if (pos == std::string::npos)
        return {"<no class body found for '" + class_name + "'>"};
    int depth{0};
    std::size_t end{pos};
    for (; end < source.size(); ++end)
    {
        if (source[end] == '{')
            ++depth;
        else if (source[end] == '}' && --depth == 0)
            break;
    }
    std::vector<std::string> found;
    std::istringstream body{source.substr(pos, end - pos)};
    std::string line;
    while (std::getline(body, line))
    {
        const std::size_t first{line.find_first_not_of(" \t")};
        if (first == std::string::npos)
            continue;
        const std::string trimmed{line.substr(first)};
        if (trimmed.starts_with("friend "))
            found.push_back(trimmed.substr(0, trimmed.find(';')));
    }
    return found;
}

// __FILE__ anchors the census to the source tree, so it runs wherever the repo is checked out
// (desk + CI both build from source; there is no installed-only execution of this suite).
const std::filesystem::path kThisFile{__FILE__};
const std::filesystem::path kCoreRoot{kThisFile.parent_path().parent_path().parent_path()};

TEST(NormalizedContentDoors, NormalizedContentHasExactlyTheTwoAuditedFriends)
{
    const std::string api{read_source(kCoreRoot / "api" / "canon.api.cppm")};
    ASSERT_FALSE(api.empty()) << "could not read canon.api.cppm from " << kCoreRoot;
    const std::vector<std::string> found{friends_of(api, "NormalizedContent")};
    // The passkey friend is QUALIFIED in source: a qualified friend is a pure reference to the
    // exported global-module forward declaration, which is what keeps gcc-15 and clang-21 binding
    // it to the ONE sealed entity (see the note at the declaration).
    const std::vector<std::string> expected{
        "friend class NormalizedLine", "friend class insight::tokenization::LogParserPasskey"};
    EXPECT_EQ(found, expected)
        << "NormalizedContent's friend list moved — that list is the DOOR CENSUS "
           "(ADR-21.D3): every entry is a producer "
           "of the walkers' currency, and growth is a ruling, not an edit. Found:\n"
        << ::testing::PrintToString(found);
}

TEST(NormalizedContentDoors, TheMintKeyHasExactlyOneFriendAndItIsTheParser)
{
    const std::string parse{read_source(kCoreRoot / "src" / "parse" / "canon.detail.parse.cppm")};
    ASSERT_FALSE(parse.empty()) << "could not read canon.detail.parse.cppm from " << kCoreRoot;
    const std::vector<std::string> found{friends_of(parse, "LogParserPasskey")};
    const std::vector<std::string> expected{"friend class LogParser"};
    EXPECT_EQ(found, expected)
        << "the passkey's friend list is pinned at ONE (Eqya ruling 1 on DONE Daidalos-1: "
           "asserted, not watched). Growing it — e.g. to reach the mint from the conformance kit "
           "— deletes the mechanism (§12.5.2's named shortcut). Found:\n"
        << ::testing::PrintToString(found);
}

} // namespace
// NOLINTEND
