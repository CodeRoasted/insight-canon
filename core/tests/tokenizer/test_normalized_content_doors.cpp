// invariant: the DOOR CENSUS of the typed ingest precondition — the doors, the pinned-absence
// traits, and the friend census.
// invariant: HOMED beside the walker tests because the walkers are what the doors GUARD.
// invariant: LEG 1 is THE ABSENT CONSTRUCTOR, pinned as NEGATIVE type traits — an absence is the
// one thing no green test observes.
// invariant: expressed as static assertions on PUBLIC constructibility, which run in every build of
// this unit, and that is STRONGER than a build-failing fixture because it cannot be skipped.
// invariant: the constructibility trait tests accessibility from a NON-FRIEND context, which is
// exactly the boundary claim.
// invariant: LEG 2 is THE OUTBOUND ACCESSOR — the typed content must remain convertible to a byte
// view, because the seam's two byte-readers have no other edit and it must not be hardened away.
// invariant: LEG 3 is THE DECLARED PEEL DOOR — the typed peel carries typed content, an
// observation time and the blank-drop, over the catalogue's shipped row.
// invariant: the raw door is covered by the transport suite, since it MINTS nothing.
// invariant: LEG 4 is THE FRIEND CENSUS, on the ruling that a friend list is only an audit surface
// if something FAILS when it grows.
// invariant: a SOURCE census over the two declaring files pins each type's friends exactly, and it
// is verbose on failure — it prints every friend line it found.
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

static_assert(!std::is_constructible_v<NormalizedContent, std::string_view>,
              "NormalizedContent must NOT be constructible from raw bytes — that single absence "
              "is the completeness theorem's premise");
static_assert(!std::is_constructible_v<NormalizedContent, const char*>);
static_assert(!std::is_constructible_v<NormalizedContent, std::string>);
static_assert(!std::is_default_constructible_v<NormalizedContent>);
static_assert(!std::is_constructible_v<NormalizedLine, std::string_view>,
              "NormalizedLine is produced ONLY by normalize()");
static_assert(!std::is_default_constructible_v<NormalizedLine>);
// invariant: the SHAPE guard — view-sized and trivially copyable — is asserted at the
// declaration site and re-asserted here.
// invariant: so this census file fails closed even if the declaration-site assertions are ever
// moved.
static_assert(sizeof(NormalizedContent) == sizeof(std::string_view));
static_assert(std::is_trivially_copyable_v<NormalizedContent>);

static_assert(
    std::is_same_v<decltype(std::declval<const NormalizedContent&>().bytes()), std::string_view>);
static_assert(
    std::is_same_v<decltype(std::declval<const NormalizedLine&>().bytes()), std::string_view>);
// invariant: the INBOUND direction must not exist as a CONVERSION either, not only as a
// constructor.
static_assert(!std::is_convertible_v<std::string_view, NormalizedContent>);
static_assert(!std::is_convertible_v<std::string_view, NormalizedLine>);

namespace
{

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

    // invariant: the blank-drop survives the typed door, which is the bundled decline behaviour.
    const NormalizedLine bare{normalize("2026-04-15T22:20:38.2879579Z ", scratch)};
    EXPECT_TRUE(stack.peel(bare).is_blank());

    // invariant: the two doors agree BYTE-FOR-BYTE — one algorithm, two proofs.
    // invariant: a divergence would be the two-implementations defect the whole contract is about.
    EXPECT_EQ(peeled.content.bytes(), stack.peel_raw(line.bytes()).content)
        << "peel and peel_raw must be the same algorithm over the same bytes";
}

[[nodiscard]] std::string read_source(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// invariant: the friend lines are taken from inside the class body at its FIRST definition
// occurrence, brace-matched.
// invariant: comment-insensitive — a `friend` inside a comment does not count, because lines are
// trimmed and must START with the keyword.
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

// invariant: the census is anchored to the SOURCE TREE by the compiler's own file macro, so it runs
// wherever the repo is checked out.
// invariant: the desk and CI both build from source, and there is no installed-only execution of
// this suite.
const std::filesystem::path kThisFile{__FILE__};
const std::filesystem::path kCoreRoot{kThisFile.parent_path().parent_path().parent_path()};

TEST(NormalizedContentDoors, NormalizedContentHasExactlyTheTwoAuditedFriends)
{
    const std::string api{read_source(kCoreRoot / "api" / "canon.api.cppm")};
    ASSERT_FALSE(api.empty()) << "could not read canon.api.cppm from " << kCoreRoot;
    const std::vector<std::string> found{friends_of(api, "NormalizedContent")};
    // invariant: the passkey friend is QUALIFIED in source, and a qualified friend is a pure
    // reference to the exported global-module forward declaration.
    // invariant: that is what binds it to the ONE sealed entity, so the qualification is kept for a
    // by-construction reason and NOT because a named compiler still needs it.
    // invariant: unqualifying it was RE-MEASURED green on both shipped compilers on 2026-09-03.
    // invariant: what that same experiment found STILL LIVE is the export on the forward
    // declaration itself.
    // invariant: this census pins the SPELLING and the declaration's own note pins the export.
    const std::vector<std::string> expected{"friend class NormalizedLine",
                                            "friend class insight::tokenization::LogParserPasskey"};
    EXPECT_EQ(found, expected)
        << "NormalizedContent's friend list moved — that list is the DOOR CENSUS: every entry is "
           "a producer of the walkers' currency, so growth is a ruling, not an edit. Found:\n"
        << ::testing::PrintToString(found);
}

TEST(NormalizedContentDoors, TheMintKeyHasExactlyOneFriendAndItIsTheParser)
{
    const std::string parse{read_source(kCoreRoot / "src" / "parse" / "canon.detail.parse.cppm")};
    ASSERT_FALSE(parse.empty()) << "could not read canon.detail.parse.cppm from " << kCoreRoot;
    const std::vector<std::string> found{friends_of(parse, "LogParserPasskey")};
    const std::vector<std::string> expected{"friend class LogParser"};
    EXPECT_EQ(found, expected)
        << "the passkey's friend list is pinned at ONE, and that one is the parser — asserted "
           "here, not merely watched. Growing it — e.g. to reach the mint from the conformance "
           "kit — deletes the mechanism: the passkey exists so that LogParser is the only minter "
           "of normalized content. Found:\n"
        << ::testing::PrintToString(found);
}

} // namespace
