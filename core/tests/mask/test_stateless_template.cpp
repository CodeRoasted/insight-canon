
// invariant: the stateless template masker's unit suite — the property tests are committed
// regression guards, chiefly the phantom-pair kill.
// invariant: the masker-cardinality RE-MEASURE used to live here as an env-gated test; it is a
// measurement over an operator-mounted population and not a regression property.
// invariant: so it moved out of the unit tree to a CLI instrument.
// refs: SRC-D-TID-1, SRC-D-TID-2
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight::tokenization;

namespace
{
MaskConfig cfg()
{
    return MaskConfig{};
}

// invariant: the masked template is copied out IMMEDIATELY, because the arena is reused across
// calls.
std::string masked(std::string_view content, ArenaAllocator& arena)
{
    arena.reset();
    return std::string{stateless_template(content, arena, cfg()).template_str};
}
} // namespace

TEST(StatelessTemplate, PureFunctionOfContentNotOrderOrStream)
{
    ArenaAllocator arena{256U * 1024U};
    const std::string_view line{"connect to host db-7 failed after 30 ms"};

    // invariant: primed with unrelated lines first, so the result must not depend on anything seen
    // before — that is statelessness.
    masked("a totally different line here", arena);
    masked("yet another unrelated message 42", arena);
    const std::string after_priming{masked(line, arena)};
    const std::string fresh{masked(line, arena)};
    EXPECT_EQ(after_priming, fresh) << "stateless_template must depend ONLY on its own content\n"
                                    << "after_priming=" << after_priming << "\nfresh=" << fresh;
}

TEST(StatelessTemplate, LogicallyIdenticalLinesShareTemplate)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: two lines differing only in masked tokens must reach ONE template.
    EXPECT_EQ(masked("request from 10.0.0.1 took 12 ms", arena),
              masked("request from 192.168.1.250 took 9999 ms", arena));
}

// invariant: THE PHANTOM PAIR, KILLED — the old stateful learner wildcarded a non-numeric token
// that varied across the lines it happened to see.
// invariant: so a different surrounding stream learned differently and the SAME logical line got
// two templates, a false new-template plus a vanished-template on an outcome flip.
// invariant: the stateless masker decides masking per token from the line's OWN content, so the
// shared line yields ONE template whatever surrounds it and the phantom cannot form.
// invariant: it also shows the accepted tradeoff — a letter-leading region word stays literal,
// because it is not a syntactic high-cardinality class.
TEST(StatelessTemplate, KillsThePhantomPair)
{
    ArenaAllocator arena{256U * 1024U};
    const std::string_view shared{"deploy region eu-west complete"};

    // invariant: the shared line is computed in three different surrounding streams, each primed
    // with a DIFFERENT sibling whose region token varies.
    // invariant: a stateful learner would wildcard the region word differently per stream; this
    // masker cannot, because it never looks at the siblings.
    masked("deploy region us-east complete", arena);
    const std::string in_stream_a{masked(shared, arena)};
    masked("deploy region ap-south complete", arena);
    const std::string in_stream_b{masked(shared, arena)};
    const std::string alone{masked(shared, arena)};

    EXPECT_EQ(in_stream_a, in_stream_b)
        << "the stateless template must be stream-invariant (the phantom pair is impossible):\n"
        << "stream A: " << in_stream_a << "\nstream B: " << in_stream_b;
    EXPECT_EQ(in_stream_a, alone) << "priming must have zero effect: " << in_stream_a << " vs "
                                  << alone;
    // invariant: the accepted tradeoff — the region word is KEPT literal rather than wildcarded.
    // refs: SRC-D-TID-14
    EXPECT_NE(in_stream_a.find("eu-west"), std::string::npos)
        << "a letter-leading word stays literal (F13 boundary): " << in_stream_a;
}

TEST(StatelessTemplate, StatusValueKeptDistinct)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: the green-to-red flip must NOT collapse — two different exit codes are distinct
    // templates, while a bare count stays masked.
    EXPECT_NE(masked("process exited with exit code 0", arena),
              masked("process exited with exit code 1", arena));
    EXPECT_EQ(masked("served 200 requests", arena), masked("served 4096 requests", arena));
}

TEST(StatelessTemplate, CompositesNormalized)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: a source location, a versioned reference and a bracket index each collapse the
    // variable numeric run while KEEPING the semantic literal.
    EXPECT_EQ(masked("error at tokenizer.cpp:4500:30: bad token", arena),
              masked("error at tokenizer.cpp:12:5: bad token", arena));
    EXPECT_EQ(masked("building zlib/1.3", arena), masked("building zlib/1.2.11", arena));
    EXPECT_EQ(masked("make[2]: entering", arena), masked("make[15]: entering", arena));
    // invariant: a different file, package or word stays distinct, because the semantic part is
    // kept.
    EXPECT_NE(masked("error at tokenizer.cpp:1:1: bad token", arena),
              masked("error at parser.cpp:1:1: bad token", arena));
}

// invariant: the whole-token bracketed stamp fell through every rule to literal KEEP, because the
// bracket is the entire difference — unbracketed the same token is digit-leading and masks.
// invariant: so on an undeclared timestamper stream every stamped line was its own template, at
// 95.9 % of the no-collapse ceiling.
// invariant: the rule claims the bracketed full-datetime class and NOTHING adjacent to it, which is
// precision-first, so the decline list stays byte-identical.
// invariant: these arms are one of the two NAMED holders of the over-masking blind spot — the A/B
// prefix-image comparison cancels a leak that hits both arms.
// invariant: so the decline list HERE, plus the corpus collateral leg, is what carries that hazard.
// refs: SRC-D-MSK-5
TEST(StatelessTemplate, BracketTimestampCollapsesTheStampClass)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: three same-shape lines differing only in the stamp must reach ONE template — the
    // measured shape inverted, since pre-fix these were three templates each equal to its raw line.
    const std::string first{masked("[2026-06-23T15:11:09.020Z] + git fetch --tags", arena)};
    const std::string second{masked("[2026-06-23T15:11:10.884Z] + git fetch --tags", arena)};
    const std::string third{masked("[2026-06-24T09:02:44.001Z] + git fetch --tags", arena)};
    EXPECT_EQ(first, second) << "same-shape stamped lines must now collapse";
    EXPECT_EQ(first, third) << "a different day must not fork the template";
    EXPECT_EQ(first, "[<*>] + git fetch --tags")
        << "normal form is the bracket convention: the bracket survives, the instance masks";
    // invariant: the whole-token trigger is position-independent within the line, and the shared
    // grammar accepts the zone forms exactly like the timestamper acceptor.
    EXPECT_EQ(masked("fetched at [2026-06-23T15:11:09.020Z] ok", arena),
              masked("fetched at [2026-06-24T09:02:44.001Z] ok", arena))
        << "re-bracketing the token mid-line must no longer defeat collapse";
    EXPECT_EQ(masked("[2026-06-23T15:11:09+02:00] x", arena), "[<*>] x");
    EXPECT_EQ(masked("[2026-06-23T15:11:09-0700] x", arena), "[<*>] x");
    EXPECT_EQ(masked("[2026-06-23T15:11:09] x", arena), "[<*>] x");
}

TEST(StatelessTemplate, BracketTimestampDeclinesEverythingAdjacentToTheClass)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: the decline list, byte-identical through the masker.
    // invariant: date-only, time-only, word, version and trailing-punctuation forms are NOT the
    // claimed class and stay literal KEEPs.
    EXPECT_EQ(masked("[2026-06-23] x", arena), "[2026-06-23] x") << "date-only interior declined";
    EXPECT_EQ(masked("[15:11:09] x", arena), "[15:11:09] x") << "time-only interior declined";
    EXPECT_EQ(masked("[INFO] x", arena), "[INFO] x") << "word interior declined";
    EXPECT_EQ(masked("[Pipeline] x", arena), "[Pipeline] x") << "word interior declined";
    EXPECT_EQ(masked("[EnvInject] x", arena), "[EnvInject] x") << "word interior declined";
    EXPECT_EQ(masked("[v1.2.3] x", arena), "[v1.2.3] x") << "version interior declined";
    EXPECT_EQ(masked("[2026-06-23T15:11:09.020Z], x", arena), "[2026-06-23T15:11:09.020Z], x")
        << "trailing punctuation breaks the whole-token trigger — declined, declared";
    EXPECT_EQ(masked("[2026-06-23T15:11] x", arena), "[2026-06-23T15:11] x")
        << "a truncated time is not a full datetime — declined";
    // invariant: the bare-integer interior stays the bracket-index rule's, via its OWN rule.
    // invariant: the output-class collision is named and accepted, and the CLAIM stays partitioned.
    EXPECT_EQ(masked("[42] x", arena), "[<*>] x") << "bracket_index's claim, unchanged";
}

// invariant: the diagnostic prefix is ONE whitespace-delimited token, and the old source-location
// normalizer masked only the trailing line number and kept the whole prefix as path-like.
// invariant: so the high-cardinality process, date and time segments survived, and a line
// byte-identical in baseline read as a NEW error pattern.
// invariant: the repair masks EVERY digit-leading sub-segment independently while keeping the
// letter-leading class anchors, so both sides collapse to one template and are dropped.
// refs: SRC-D-MSK-1
TEST(StatelessTemplate, DiagnosticCompositeCollapsesChromiumPrefix)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: the exact reported pair — only the process id, date and time differ, so ONE
    // template.
    EXPECT_EQ(masked("[6226:0609/094020.430910:ERROR:dbus/bus.cc:408] Failed to connect to the bus",
                     arena),
              masked("[6225:0528/144005.901629:ERROR:dbus/bus.cc:408] Failed to connect to the bus",
                     arena))
        << "Chromium PID/date/time segments mask; ERROR/dbus/bus.cc kept → baseline ≡ changed";
    // invariant: the letter-leading class anchor is KEPT, so a different file in the prefix stays
    // distinct.
    EXPECT_NE(masked("[6226:0609/094020.430910:ERROR:dbus/bus.cc:408] x", arena),
              masked("[6226:0609/094020.430910:ERROR:net/socket.cc:408] x", arena))
        << "letter-leading segments (the stable class) are kept — dbus/bus.cc ≠ net/socket.cc";
    // invariant: it subsumes the old source-location behaviour exactly, which is why that case is
    // kept as a regression guard.
    EXPECT_EQ(masked("error at tokenizer.cpp:4500:30: bad token", arena),
              masked("error at tokenizer.cpp:12:5: bad token", arena))
        << "source-location masking unchanged under the generalized rule";
}

// invariant: the status-value carve-out MUST survive PER-SEGMENT, and it is load-bearing.
// invariant: a digit segment that is a status value, immediately preceded WITHIN the composite by a
// status keyword, is KEPT exactly as the bare-token rule keeps it.
// invariant: so a varying id masks while the status flip stays split — never collapse a
// categorical status change.
TEST(StatelessTemplate, DiagnosticCompositeKeepsStatusValuePerSegment)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: ONE composite that masks the request id AND keeps the status value, so both paths
    // are proven by a single fixture.
    EXPECT_EQ(masked("[req:42/status:500] handled", arena),
              masked("[req:99/status:500] handled", arena))
        << "the varying request id masks; the same status:500 is kept → collapse on the id only";
    EXPECT_NE(masked("[req:42/status:500] handled", arena),
              masked("[req:42/status:200] handled", arena))
        << "the per-segment status carve-out: status:500 ≠ status:200 must NOT collapse";
    EXPECT_NE(masked("worker exit:0 done", arena), masked("worker exit:1 done", arena))
        << "exit:0 ≠ exit:1 — the colon-form of the exit-code carve-out, per-segment";
}

// invariant: randomized temp directories carry a random base-62 suffix that is letter-leading and
// neither hex nor a UUID, so the existing masks kept it literal and every run was a template.
// invariant: the suffix is UNDECIDABLE, but the ROOT is an enumerable byte-exact catalog.
// invariant: a child of an ephemeral root is a per-run instance BY CONSTRUCTION, so the post-root
// remainder masks, which is lossless for diffing.
// refs: SRC-D-MSK-2
TEST(StatelessTemplate, EphemeralRootPathMasksRemainder)
{
    ArenaAllocator arena{256U * 1024U};
    EXPECT_EQ(masked("opened /tmp/pw-electron-userdata-Kw9v4a ok", arena),
              masked("opened /tmp/pw-duplicate-collections-kvJMB5 ok", arena))
        << "/tmp/<random> → /tmp/<*> on both sides — the novelty fatigue is killed";
    // invariant: a STABLE temp file collapses with the random-suffix variants too, which is also
    // non-diffable and therefore lossless.
    EXPECT_EQ(masked("opened /tmp/transient ok", arena),
              masked("opened /tmp/pw-electron-userdata-Kw9v4a ok", arena))
        << "a stable child of /tmp is itself non-diffable — collapses to the same /tmp/<*>";
    EXPECT_EQ(masked("dir /var/folders/aB/cD ready", arena),
              masked("dir /var/folders/xY/zW ready", arena))
        << "/var/folders (macOS) is in the ephemeral-root catalog";
}

// invariant: THE GUARD — this is an ephemeral-ROOT catalog and NOT a general absolute-path
// masker.
// invariant: a path under a non-ephemeral root keeps its identity, and a temp-rooted SOURCE path
// keeps its file-and-line shape because the diagnostic composite is checked FIRST.
TEST(StatelessTemplate, NonEphemeralPathsAndSourcePathsUntouched)
{
    ArenaAllocator arena{256U * 1024U};
    EXPECT_NE(masked("reading /etc/hosts now", arena), masked("reading /etc/passwd now", arena))
        << "/etc is NOT ephemeral — /etc/hosts ≠ /etc/passwd (no over-masking)";
    EXPECT_NE(masked("exec /usr/bin/foo", arena), masked("exec /usr/bin/bar", arena))
        << "/usr/bin is NOT ephemeral — distinct binaries stay distinct";
    EXPECT_NE(masked("at /tmp/build/foo.cc:42 oops", arena),
              masked("at /tmp/build/bar.cc:42 oops", arena))
        << "a /tmp SOURCE path (:line) is a diagnostic composite first — file kept, only :line "
           "masks";
}

// invariant: the re-measure rule set, whose discriminator is the digit-leading rule the whole model
// rests on.
// refs: SRC-D-TID-12
TEST(StatelessTemplate, DigitLeadingTokensMask)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: ONE rule subsumes numbers with separators, decimals, number-plus-unit and
    // versions, with no unit lexicon.
    EXPECT_EQ(masked("built in 6.2s", arena), masked("built in 11.9s", arena));
    EXPECT_EQ(masked("done 76.5%", arena), masked("done 100.0%", arena));
    EXPECT_EQ(masked("compiled 31,260 targets", arena), masked("compiled 9 targets", arena));
    EXPECT_EQ(masked("installing pkg 0.25.5-3", arena), masked("installing pkg 1.2.11", arena));
    EXPECT_EQ(masked("freed 512MB", arena), masked("freed 8GB", arena));
}

TEST(StatelessTemplate, LetterLeadingKeptUuidAndHashMasked)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: letter-leading words are KEPT, because a word is not a number.
    // refs: SRC-D-TID-14
    EXPECT_NE(masked("decode utf8 stream", arena), masked("decode ascii stream", arena));
    EXPECT_EQ(masked("decode utf8 stream", arena), masked("decode utf8 stream", arena));
    EXPECT_NE(masked("hash sha256 ok", arena), masked("hash sha512 ok", arena));
    EXPECT_EQ(masked("temp f7f63412-b7a7-468d-bd31-1a6ae1ca2680 ready", arena),
              masked("temp 8b4537c3-1dd0-411a-a760-2aeb13934993 ready", arena));
    EXPECT_EQ(masked("commit 9fd7fb4c0de0abcd1234", arena),
              masked("commit deadbeefcafe0badf00d5678", arena));
}

TEST(StatelessTemplate, HashCounterAndWorkerBracketCollapse)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: the class MARKER is kept, so a counter and a worker bracket remain distinct
    // classes.
    EXPECT_EQ(masked("step #26 done", arena), masked("step #7 done", arena));
    EXPECT_EQ(masked("[gw0] PASSED test_x", arena), masked("[gw3] PASSED test_x", arena));
    EXPECT_NE(masked("#26", arena), masked("[gw26]", arena));
}

TEST(StatelessTemplate, KvNumericValueMaskedWordKept)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: a key with a digit-leading value masks the VALUE and keeps the KEY, so per-id
    // lines collapse to one template and no error singleton produces a false diff.
    // refs: SRC-D-TID-13
    EXPECT_EQ(masked("checkout completed order=100000", arena),
              masked("checkout completed order=999999", arena));
    EXPECT_EQ(masked("payment timeout txn=50000", arena),
              masked("payment timeout txn=70000", arena));
    EXPECT_EQ(masked("GC pause=512ms heap=87%", arena), masked("GC pause=9ms heap=3%", arena));
    // invariant: a value WORD stays literal, which is the boundary and the registry's job.
    // refs: SRC-D-TID-14
    EXPECT_NE(masked("login user=alice", arena), masked("login user=bob", arena));
    // invariant: the status-value KEEP in its key-value form — a green-to-red flip must NOT
    // collapse.
    EXPECT_NE(masked("request status=200", arena), masked("request status=500", arena));
    EXPECT_NE(masked("proc code=0", arena), masked("proc code=1", arena));
    // invariant: a non-status numeric value beyond the status-digit gate DOES mask.
    EXPECT_EQ(masked("listening port=8080", arena), masked("listening port=9090", arena));
}

// invariant: a declared currency MARKER glued to a digit-led numeric core masks to the keep-marker
// shape, which is the decidable-numeric refinement of the registry rule.
// invariant: it closes the over-split twin, so a per-amount total collapses to one stable template
// and its vanish can form.
// refs: SRC-D-TID-22
TEST(StatelessTemplate, CurrencyMarkerNumberMasked)
{
    ArenaAllocator arena{256U * 1024U};
    EXPECT_EQ(masked("order completed $463", arena), masked("order completed $18", arena));
    EXPECT_EQ(masked("charged $463.50", arena), masked("charged $9.99", arena));
    EXPECT_EQ(masked("order completed total=$463", arena),
              masked("order completed total=$18", arena));
    // invariant: the marker is KEPT — legible, and a distinct class from a bare number and from a
    // counter.
    EXPECT_NE(masked("$463", arena), masked("463", arena));
    EXPECT_NE(masked("$463", arena), masked("#463", arena));
    // invariant: the BOUNDARY the rule does not cross — a marker followed by letters has no digit
    // core and is kept literal and distinct.
    EXPECT_NE(masked("export $HOME", arena), masked("export $PATH", arena));
    EXPECT_NE(masked("cfg path=$HOME", arena), masked("cfg path=$ROOT", arena));
    // invariant: letter-prefixed ids stay in the registry class, untouched and still distinct.
    EXPECT_NE(masked("ticket ORD-123", arena), masked("ticket ORD-456", arena));
    EXPECT_NE(masked("ref $42abc", arena), masked("ref $99xyz", arena));
}

// invariant: WHY THE CONSTANT PINS EXIST WHEN THE SUITE ABOVE DOES NOT COVER THEM.
// invariant: every test above asserts a COLLAPSE, which stays green for ANY hash floor — either
// both sides mask or both stay literal, and either way they match.
// invariant: MUTATION TESTING CONFIRMED IT — halving the floor left all 43 committed expectations
// green, and the floor's stated rationale was asserted nowhere.
// invariant: pinning a threshold requires EXACT template strings on BOTH sides of the boundary —
// literal at floor minus one, masked at the floor.
// invariant: the floor is declared ONCE and read by two paths — the standalone whole-token check
// and the embedded-identity scanner.
// invariant: so the two pins guard two PATHS rather than two copies: a path that stopped consulting
// the constant, or applied it differently, moves only its own pin.
// invariant: this prose said TWO INDEPENDENT COPIES until 2026-09-07, and that was stale — the
// declaration was consolidated and the source now says so at the constant.
// invariant: these assert CURRENT SHIPPED BEHAVIOUR and do not argue the floor is correct.
// invariant: the value is not tunable by a threshold study — a red here means someone moved a
// load-bearing masking constant, which is an identity-affecting change requiring a version bump.
// refs: SRC-D-TID-16
namespace
{
// invariant: both fixtures are LETTER-leading on purpose, because a digit-leading hex run is masked
// by the digit-leading rule regardless of the floor, which would make the pin vacuous.
constexpr std::string_view kHexBelowFloor{"deadbeefcafe0ba"};
constexpr std::string_view kHexAtFloor{"deadbeefcafe0bad"};
constexpr std::size_t kFloorLen{16};
} // namespace

// invariant: the two assertions together admit EXACTLY ONE floor value — any lower floor reddens
// the first and any higher one reddens the second.
TEST(StatelessTemplate, HashFloorPinnedAtSixteenStandalone)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: the fixtures themselves are guarded, because a typo'd literal would silently move
    // the boundary under test and quietly re-vacuate the pin.
    ASSERT_EQ(kHexBelowFloor.size(), kFloorLen - 1U)
        << "fixture drift: the below-floor token must be exactly " << (kFloorLen - 1U)
        << " chars, got " << kHexBelowFloor.size() << " (" << kHexBelowFloor << ")";
    ASSERT_EQ(kHexAtFloor.size(), kFloorLen)
        << "fixture drift: the at-floor token must be exactly " << kFloorLen << " chars, got "
        << kHexAtFloor.size() << " (" << kHexAtFloor << ")";

    const std::string below{masked(std::string{kHexBelowFloor}, arena)};
    EXPECT_EQ(below, kHexBelowFloor)
        << "a " << kHexBelowFloor.size() << "-char hex run is BELOW the floor and must stay "
        << "literal — the floor moved DOWN (masking short hex-looking words)\n"
        << "  token    : " << kHexBelowFloor << "\n"
        << "  expected : " << kHexBelowFloor << " (kept)\n"
        << "  actual   : " << below;

    const std::string at_floor{masked(std::string{kHexAtFloor}, arena)};
    EXPECT_EQ(at_floor, "<*>") << "a " << kHexAtFloor.size()
                               << "-char hex run is AT the floor and must MASK — the "
                               << "floor moved UP (leaking high-card hashes into templates)\n"
                               << "  token    : " << kHexAtFloor << "\n"
                               << "  expected : <*>\n"
                               << "  actual   : " << at_floor;
}

// invariant: the same boundary on the EMBEDDED path — a delimiter-bounded run inside a larger
// token, which is a separate copy of the constant and therefore a separate pin.
TEST(StatelessTemplate, HashFloorPinnedAtSixteenEmbedded)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: a non-ephemeral, non-versioned path so every earlier rule declines and the token
    // actually REACHES the embedded-identity rule, without which the pin would be vacuous.
    const std::string below_line{std::string{"/var/cache/"} + std::string{kHexBelowFloor} + "/x"};
    const std::string at_floor_line{std::string{"/var/cache/"} + std::string{kHexAtFloor} + "/x"};

    const std::string below{masked(below_line, arena)};
    EXPECT_EQ(below, below_line) << "an embedded " << kHexBelowFloor.size()
                                 << "-char hex run is below the floor and must "
                                 << "stay literal (embedded floor moved DOWN)\n"
                                 << "  expected : " << below_line << " (kept)\n"
                                 << "  actual   : " << below;

    const std::string at_floor{masked(at_floor_line, arena)};
    EXPECT_EQ(at_floor, "/var/cache/<*>/x")
        << "an embedded " << kHexAtFloor.size() << "-char hex run must mask while the surrounding "
        << "path is KEPT (embedded floor moved UP, or the run boundary broke)\n"
        << "  input    : " << at_floor_line << "\n"
        << "  expected : /var/cache/<*>/x\n"
        << "  actual   : " << at_floor;
}

// invariant: the floor's STATED RATIONALE, asserted — these are real vocabulary, hex-alphabet
// words and identifiers that carry meaning and MUST NOT collapse into a wildcard.
// invariant: they also LADDER the sub-floor range, so a floor lowered to any smaller value reddens
// a NAMED token rather than an abstract length.
TEST(StatelessTemplate, HashFloorKeepsShortHexWordsLiteral)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: one of the words is not all-hex, and the rest are kept SOLELY by the floor.
    for (const std::string_view word :
         {std::string_view{"cafe"}, std::string_view{"facade"}, std::string_view{"ed25519"},
          std::string_view{"deadbeef"}, std::string_view{"LEB128"}})
    {
        const std::string got{masked(std::string{word}, arena)};
        EXPECT_EQ(got, word) << "a " << word.size() << "-char hex-looking WORD must stay literal — "
                             << "the floor's stated rationale (mask.cpp) is violated\n"
                             << "  word     : " << word << "\n"
                             << "  expected : " << word << " (kept)\n"
                             << "  actual   : " << got;
    }
}

// invariant: the hex classifier folds ASCII case, and no committed test exercised the fold.
// invariant: dropping it would leave every UPPERCASE hash unmasked — real corpus content such as
// uppercase git digests and checksum tables silently over-splitting.
// invariant: both the fold AND its bound are pinned, because folding must not turn non-hex letters
// into hex.
TEST(StatelessTemplate, HexClassifierFoldsAsciiCase)
{
    ArenaAllocator arena{256U * 1024U};
    constexpr std::string_view kUpperHex{"DEADBEEFCAFE0BAD"};
    constexpr std::string_view kMixedHex{"DeadBeefCafe0Bad"};
    constexpr std::string_view kUpperUuid{"F7F63412-B7A7-468D-BD31-1A6AE1CA2680"};
    constexpr std::string_view kUpperNonHex{"DEADBEEFCAFEXYZW"};

    const std::string upper{masked(std::string{kUpperHex}, arena)};
    EXPECT_EQ(upper, "<*>") << "UPPERCASE hex at the floor must mask — is_hex_char stopped folding "
                            << "case\n  token: " << kUpperHex
                            << "\n  expected: <*>\n  actual: " << upper;

    const std::string mixed{masked(std::string{kMixedHex}, arena)};
    EXPECT_EQ(mixed, "<*>") << "MIXED-case hex at the floor must mask\n  token: " << kMixedHex
                            << "\n  expected: <*>\n  actual: " << mixed;

    const std::string uuid{masked(std::string{kUpperUuid}, arena)};
    EXPECT_EQ(uuid, "<*>") << "an UPPERCASE UUID must mask (the 8-4-4-4-12 hex check folds case "
                           << "too)\n  token: " << kUpperUuid
                           << "\n  expected: <*>\n  actual: " << uuid;

    // invariant: the BOUND — same length, same case, but genuinely non-hex must be KEPT, which
    // guards a fix that masks long uppercase words wholesale instead of folding case.
    const std::string non_hex{masked(std::string{kUpperNonHex}, arena)};
    EXPECT_EQ(non_hex, kUpperNonHex)
        << "a same-length UPPERCASE non-hex word must stay literal — the case fold must not "
        << "widen the hex alphabet\n  token: " << kUpperNonHex << "\n  expected: " << kUpperNonHex
        << " (kept)\n  actual: " << non_hex;
}

// invariant: the ROOT is the decidable thing, not a hex or length heuristic, which a study
// falsified.
// invariant: a path component directly under a declared ephemeral root is a per-run instance and
// masks, while the location tail is PROTECTED.
// refs: SRC-D-MSK-4
TEST(EphemeralRootMask, G1_ReportedConanPairCollapses)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: the exact defect — two build-directory hashes for one logical diagnostic line.
    const std::string first{masked(
        "/home/runner/.conan2/p/b/insig247e3d1dffc33/p/include/insight/span_unpack.cpp:72:5:",
        arena)};
    const std::string second{masked(
        "/home/runner/.conan2/p/b/insigea56199c0f87b/p/include/insight/span_unpack.cpp:72:5:",
        arena)};
    EXPECT_EQ(first, second) << "the phantom pair must collapse\nfirst=" << first
                             << "\nsecond=" << second;
    EXPECT_EQ(first, "/home/runner/.conan2/p/b/<*>/p/include/insight/span_unpack.cpp:<*>:<*>:")
        << "instance masked, structure + tail kept\nfirst=" << first;
}

TEST(EphemeralRootMask, G2_TailSurvives)
{
    ArenaAllocator arena{256U * 1024U};
    const std::string tmpl{masked(
        "/home/runner/.conan2/p/b/insig247e3d1dffc33/p/include/insight/span_unpack.cpp:72:5:",
        arena)};
    EXPECT_NE(tmpl.find("span_unpack.cpp"), std::string::npos)
        << "the file:line tail is what makes a finding actionable — never mask it\ntmpl=" << tmpl;
}

TEST(EphemeralRootMask, G3_ContentClassStaysLiteral)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: none of these sits under a catalogued root, so the rule must not touch them, which
    // makes this the over-mask check.
    // invariant: a pinned action digest path keeps its class anchors and gains NO ephemeral
    // artifact, because its change is drift we WANT surfaced.
    const std::string sha{masked("_actions/actions/create-github-app-token/"
                                 "bcd2ba49abf26b56dd0dd2eb1c9dd5c77b096d4c/dist/main.cjs",
                                 arena)};
    EXPECT_NE(sha.find("_actions"), std::string::npos) << "sha=" << sha;
    EXPECT_NE(sha.find("main.cjs"), std::string::npos) << "sha=" << sha;
    EXPECT_EQ(sha.find(".conan2"), std::string::npos)
        << "no ephemeral artifact leaked in\nsha=" << sha;
    // invariant: a container image digest is colon-LETTER rather than colon-digit, so it is not a
    // composite.
    const std::string dig{
        masked("alpine@sha256:ff6bdca1a26e4cf3f60c76e9f6f8bb2adb1e5a5b6c7d8e9f0", arena)};
    EXPECT_NE(dig.find("alpine"), std::string::npos)
        << "the image-name class anchor survives\ndig=" << dig;
}

TEST(EphemeralRootMask, G4_TmpSubtreeRegressionByteIdentical)
{
    ArenaAllocator arena{256U * 1024U};
    EXPECT_EQ(masked("/tmp/pw-electron-userdata-Kw9v4a", arena), "/tmp/<*>");
}

TEST(EphemeralRootMask, G5_ClampKeepsTailUnderTmpDiagnostic)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: a temp-ROOTED diagnostic masks the instance AND keeps the tail, which is the
    // clamp.
    // invariant: this output CHANGES against the earlier rule, and that is an improvement — the
    // old ordering knowingly kept the phantom.
    const std::string tmpl{masked("/tmp/build-x/src/foo.cpp:42:5", arena)};
    EXPECT_EQ(tmpl, "/tmp/<*>/src/foo.cpp:<*>:<*>") << "tmpl=" << tmpl;
    EXPECT_NE(tmpl.find("foo.cpp"), std::string::npos) << "tail kept\ntmpl=" << tmpl;
}

TEST(EphemeralRootMask, G6_AnchorIsReal_MidPathTmpUntouched)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: the root token is anchored at TOKEN START, so a mid-path occurrence is NOT a root
    // and a user file under it is untouched.
    EXPECT_EQ(masked("/home/user/tmp/notes.txt", arena), "/home/user/tmp/notes.txt");
    // invariant: in a diagnostic, the child of a mid-path occurrence is likewise KEPT rather than
    // masked as an instance.
    const std::string diag{masked("/home/user/tmp/build.log:5:1", arena)};
    EXPECT_NE(diag.find("build.log"), std::string::npos)
        << "a mid-path tmp must not float and mask its child\ndiag=" << diag;
}

TEST(EphemeralRootMask, G7_NoCatalogedRootNoOverFire)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: the blast-radius FLOOR — a diagnostic under NO catalogued root masks exactly as
    // before, only its line and column, with every path component kept.
    // invariant: so a moved golden with no catalogued root is an over-fire.
    EXPECT_EQ(masked("/home/user/project/src/main.cpp:10:5", arena),
              "/home/user/project/src/main.cpp:<*>:<*>");
}

// invariant: WHICH OF THE TWO CONFIG KNOBS ACTUALLY GATES, AND ON WHICH TOKEN SHAPE.
// invariant: the two rules sit in ONE disjunction with digit-leading as a later disjunct, so a rule
// whose whole domain is digit-leading can never change an outcome.
// invariant: its knob is then inert and the predicate is dead at the output level, which is
// MEASURED true of the hex rule, whose domain is a STRICT SUBSET of digit-leading.
// invariant: IT IS NOT TRUE OF THE IP RULE, AND THE DIFFERENCE IS ONE CHARACTER — that grammar
// admits an optional leading bracket, and a token starting with one is NOT digit-leading.
// invariant: so the BRACKETED form reaches the IP rule and nothing else catches it, which makes its
// knob a live gate on exactly that shape.
// invariant: AND THAT DISTINCTION WAS MISSED BY A 14-TOKEN PROBE THAT CARRIED NO BRACKET — the
// sample said both knobs were inert and the population disagreed.
// invariant: recorded here rather than in a report, because the next person to shrink this test
// will be tempted by the same sample.
// invariant: the bracket is not one more case, it is the ONLY case that separates the two knobs.
// invariant: the bracketed address is NOT synthetic — it is an attested line, a catalogued row in
// the format docs, and the literal already ships inside a sibling gate.
// invariant: with the knob off it falls to literal KEEP, putting a raw client address into the
// template, the fingerprint and any rendered evidence.
// invariant: on a product whose claim is about what leaves the machine, that is a PRIVACY surface
// and not a config nicety.
// invariant: FALSIFIABILITY WAS OBSERVED, THEN REVERTED — disabling the IP rule wholesale was
// RED, and the whole-suite run says this arm is the SOLE guard, 1 failure out of 601.
// invariant: before this arm, deleting that rule outright moved no test in the repository.
// invariant: note WHICH leg caught it — the knob-ON leg; the knob-OFF leg stayed green,
// correctly, since staying literal is what a dead rule also produces.
// invariant: the two legs are not redundant, they fail in OPPOSITE directions, and only holding
// both distinguishes a gating knob from nothing masking this at all.
// invariant: disabling the hex rule wholesale left the entire suite GREEN at 596 of 596, which is
// the measurement the rip rests on.
// invariant: it is recorded here because after the rip there is no predicate left to mutate.
// refs: DN-027
namespace
{
// invariant: the masker under a CALLER-CHOSEN config, which is exactly why no committed test using
// the default helper could ever have observed a knob.
[[nodiscard]] std::string masked_with(std::string_view content, ArenaAllocator& arena,
                                      const MaskConfig& conf)
{
    arena.reset();
    return std::string{stateless_template(content, arena, conf).template_str};
}

[[nodiscard]] MaskConfig cfg_without_ip_masking()
{
    MaskConfig conf{};
    conf.mask_ip_addresses = false;
    return conf;
}
} // namespace

TEST(StatelessTemplate, Ipv4KnobGatesTheBracketedFormThatDigitLeadingCannotReach)
{
    ArenaAllocator arena{256U * 1024U};
    constexpr std::string_view kBracketed{"[10.20.30.40]"};
    constexpr std::string_view kBare{"10.20.30.40"};

    // invariant: THE DECISIVE LEG — the knob ON must mask and the knob OFF must KEEP.
    const std::string on{masked_with(kBracketed, arena, MaskConfig{})};
    EXPECT_EQ(on, "<*>") << "the bracketed IPv4 must mask with mask_ip_addresses ON — this is rule "
                            "4 doing the only work no other rule can do.\n  token: "
                         << kBracketed << "\n  actual: " << on;

    const std::string off{masked_with(kBracketed, arena, cfg_without_ip_masking())};
    EXPECT_EQ(off, kBracketed)
        << "the bracketed IPv4 must stay LITERAL with mask_ip_addresses OFF. If this masked "
           "anyway, rule 4 is inert exactly as rule 5 is — something upstream (a composite rule "
           "reached through the `maybe_composite` pre-gate, which runs BEFORE this disjunction) is "
           "claiming the token first, and the IP knob joins the rip.\n  token: "
        << kBracketed << "\n  expected: " << kBracketed << " (kept)\n  actual: " << off;

    // invariant: THE CONTRAST that names which shape the knob governs — the BARE form is
    // digit-leading, so it masks either way.
    // invariant: a reader who saw only the bare form would conclude the knob works when it is doing
    // nothing, so pinning both is what makes the decisive leg interpretable.
    EXPECT_EQ(masked_with(kBare, arena, MaskConfig{}), "<*>");
    EXPECT_EQ(masked_with(kBare, arena, cfg_without_ip_masking()), "<*>")
        << "the BARE IPv4 is digit-leading, so it masks regardless of the knob — rule 4 is not "
           "what catches it. If this ever KEEPS, digit-leading stopped covering the bare form and "
           "the knob's domain just widened silently.";
}

// invariant: THE WRAPPER SHELL — the IP grammar admitted ONE delimiter pair out of six, and the
// omission published a real third-party address.
// invariant: the test directly above pins the bracketed form and calls it the only case that
// separates the two knobs, which is true of the KNOB and was read as true of the GRAMMAR.
// invariant: the grammar admitted a leading square bracket and nothing else.
// invariant: a parenthesised address failed at byte 0, was not digit-leading, and carried no byte
// in the composite pre-gate's separator set.
// invariant: it therefore fell to literal KEEP.
// invariant: THIS IS NOT HYPOTHETICAL.
// invariant: the published showcase render carries SIX template rows whose masked text contains a
// bare routable third-party address, and the only reason is that one character.
// invariant: a templates section is precisely where a reader has been told the addresses are gone.
// invariant: AND THE ASYMMETRY WAS NEVER DECLARED — the rule already decided that a punctuation
// shell does not defeat the class, then implemented that over a hand-picked subset.
// invariant: NOTHING CHOSE ONE BRACKET OVER ANOTHER, so the repair is a declared pair catalog and
// the next delimiter is not a second incident.
// invariant: every leg below was OBSERVED RED on the pre-fix grammar, which is what makes this a
// regression guard and not a restatement of current behaviour.
// invariant: the knob-OFF leg is the NON-VACUITY arm — it proves the IP rule is what masks the
// wrapped form.
// invariant: if a composite rule ever claims these tokens first, that leg goes green-blind and this
// whole block stops testing the IP rule, which is exactly the trap the block above names.
TEST(StatelessTemplate, Ipv4MasksInsideEveryDeclaredWrapperPair)
{
    ArenaAllocator arena{256U * 1024U};

    // invariant: THE BALANCED SHELLS ARE DERIVED FROM THE DECLARED CATALOG, NOT TYPED OUT.
    // invariant: they were a hand-written list of six literals under a name claiming CATALOG
    // COMPLETENESS, so a seventh declared pair would have joined the grammar with this test green.
    // invariant: that is the same defect the repair exists for, one level up — the catalog's
    // whole point is that nothing CHOSE one bracket over another.
    // invariant: a witness list nobody derives makes exactly that choice again, so building the
    // tokens from the table means a new pair arrives with a row already asserting it.
    std::vector<std::string> shells;
    for (const auto& pair : kWrapperPairs)
        shells.push_back(std::string{pair.open} + "10.20.30.40" + std::string{pair.close});
    for (const std::string& tok : shells)
    {
        const std::string got{masked_with(tok, arena, MaskConfig{})};
        EXPECT_EQ(got, "<*>")
            << "a wrapped IPv4 must MASK for EVERY declared wrapper pair — this row was built "
               "from kWrapperPairs, so it covers the pair that exists rather than the pair "
               "someone remembered.\n  token:    "
            << tok << "\n  expected: <*>\n  actual:   " << got;
    }

    // invariant: the shapes that are NOT one-per-pair stay hand-written, because each is a distinct
    // claim about the GRAMMAR rather than about the catalog.
    for (const std::string_view tok : {
             // invariant: an opener with no closer still leaks, because the defect is the byte that
             // PRECEDES the address.
             // invariant: an unbalanced-open fragment is the same failure and not a lesser one.
             "(10.20.30.40",
             "{10.20.30.40",
             // invariant: the closer-only forms are a CONTROL and not a repair — they were never
             // broken, because byte 0 is a digit so the digit-leading rule already masked them.
             // invariant: that is exactly why the audit named the LEADING bracket.
             // invariant: they are pinned so a future change to the shell cannot quietly take them
             // away from digit-leading.
             "10.20.30.40)",
             "10.20.30.40}",
             "10.20.30.40>",
             "(10.20.30.40:8080)",
             "(10.20.30.40),",
             "\"10.20.30.40\".",
         })
    {
        const std::string got{masked_with(tok, arena, MaskConfig{})};
        EXPECT_EQ(got, "<*>")
            << "a wrapped IPv4 must MASK — the shell is punctuation, not part of the address, and "
               "the whole product claim about a `templates` section rests on this.\n  token:    "
            << tok << "\n  expected: <*>\n  actual:   " << got;
    }

    // invariant: NON-VACUITY — with the knob OFF every one of these must come back LITERAL.
    // invariant: if any masks anyway, something upstream of the rule's disjunction claimed the
    // token and the table above is no longer testing the IP grammar at all.
    // invariant: ONLY opener-led tokens belong in this leg, and the reason is MEASURED — a
    // trailing-closer form masks with the knob OFF, because byte 0 stays a digit.
    // invariant: putting a closer-only form here would assert a falsehood about which rule is under
    // test, so this leg must contain only shapes the digit-leading rule cannot reach.
    for (const std::string_view tok : {"(10.20.30.40)", "\"10.20.30.40\"", "{10.20.30.40"})
    {
        const std::string off{masked_with(tok, arena, cfg_without_ip_masking())};
        EXPECT_EQ(off, tok)
            << "with mask_ip_addresses OFF the wrapped IPv4 must stay literal — this leg is what "
               "proves rule 4 (and not a composite rule reached through the maybe_composite "
               "pre-gate) is what masks it.\n  token:    "
            << tok << "\n  expected: " << tok << " (kept)\n  actual:   " << off;
    }
}

// invariant: the six rows that ACTUALLY SHIPPED, verbatim from the published showcase artifact.
// invariant: a published surface is the strongest oracle available for whether we really fixed the
// thing we published.
TEST(StatelessTemplate, ThePublishedTemplateRowsThatLeakedARealAddressNowMask)
{
    ArenaAllocator arena{256U * 1024U};

    for (const std::string_view line :
         {"Authentication failed from <*> (163.27.187.39): Permission denied in replay cache code",
          "Authentication failed from <*> (163.27.187.39): Software caused connection abort",
          "mDNS_DeregisterInterface: Frequent transitions for interface en0 (10.105.162.32)",
          "mDNS_DeregisterInterface: Frequent transitions for interface en0 (10.142.110.44)",
          "DHCPREQUEST for <*> (10.100.0.250) from <*> via eth1",
          "DHCPREQUEST for <*> (10.100.0.250) from <*> via eth1: unknown lease <*>"})
    {
        const std::string got{masked(line, arena)};
        EXPECT_EQ(got.find('('), std::string::npos)
            << "no parenthesised token may survive into a template here — every one of these rows "
               "is an address.\n  line:   "
            << line << "\n  masked: " << got;
        for (const std::string_view leaked :
             {"163.27.187.39", "10.105.162.32", "10.142.110.44", "10.100.0.250"})
            EXPECT_EQ(got.find(leaked), std::string::npos)
                << "a real third-party address survived masking — this is the exact byte sequence "
                   "that reached the public hub.\n  leaked: "
                << leaked << "\n  masked: " << got;
    }
}

// invariant: THE BOUNDARY, STATED POSITIVELY — the shell must not become a licence to mask
// whatever sits inside a bracket.
// invariant: over-masking destroys distinguishing content permanently and invisibly, so the KEEP
// side is asserted as hard as the MASK side.
// invariant: the plain-word fixtures are attested NEIGHBOURS of the leaked rows in the same
// published artifact — they sat one token away and must not move.
// refs: ADR-16.D5
TEST(StatelessTemplate, TheWrapperShellDoesNotReachBeyondTheAddressClass)
{
    ArenaAllocator arena{256U * 1024U};

    for (const std::string_view tok : {
             "(anonymous)",
             "(reserved)",
             "(usable)",
             "(1.2.3)",
             "(1.2.3.4.5)",
             "(v1.2.3.4)",
             "(1.2.3.4x)",
         })
    {
        const std::string got{masked_with(tok, arena, MaskConfig{})};
        EXPECT_EQ(got, tok) << "this token is not an address and must survive verbatim — the shell "
                               "widens WHICH punctuation rule 4 tolerates, never WHAT it "
                               "matches.\n  token:    "
                            << tok << "\n  expected: " << tok << " (kept)\n  actual:   " << got;
    }
}

// invariant: the sibling with the same shape takes a DIFFERENT repair.
// invariant: a wrapped long hex run already normalizes when the wrapper is a square bracket — not
// because the hash rule tolerates a shell, since it requires the WHOLE token.
// invariant: it is because that bracket sits in the composite pre-gate's separator set, so the
// embedded-identity rule gets a look, and the other opener did not sit in that set.
// invariant: so the repair is in the PRE-GATE and not in the hash rule — it restores the normal
// form the bracketed shape already produces instead of inventing a second one.
// invariant: a wrapped UUID was always fine for the ACCIDENTAL reason that a UUID contains a
// hyphen, which is in the set, and that accident is what hid the hex case.
TEST(StatelessTemplate, AWrappedHashNormalizesLikeTheBracketedFormItAlreadyMatched)
{
    ArenaAllocator arena{256U * 1024U};
    struct Row
    {
        std::string_view token;
        std::string_view want;
    };
    for (const Row& row : {
             Row{.token = "[d41d8cd98f00b204e9800998ecf8427e]", .want = "[<*>]"},
             Row{.token = "(d41d8cd98f00b204e9800998ecf8427e)", .want = "(<*>)"},
             Row{.token = "{d41d8cd98f00b204e9800998ecf8427e}", .want = "{<*>}"},
             Row{.token = "\"d41d8cd98f00b204e9800998ecf8427e\"", .want = "\"<*>\""},
             // invariant: this row was already green before the repair, by the hyphen accident, and
             // is pinned so it STAYS green.
             Row{.token = "(3f2504e0-4f89-11d3-9a0c-0305e82c3301)", .want = "(<*>)"},
         })
    {
        const std::string_view tok{row.token};
        const std::string_view want{row.want};
        const std::string got{masked_with(tok, arena, MaskConfig{})};
        EXPECT_EQ(got, want)
            << "a wrapped high-cardinality identity must normalize to the SAME shape in every "
               "shell — two normal forms for one class means two templates for one logical "
               "line.\n  token:    "
            << tok << "\n  expected: " << want << "\n  actual:   " << got;
    }

    // invariant: the floor holds — a short hex-looking word is still a word, shell or no shell.
    for (const std::string_view tok : {"(deadbeef)", "(cafe)"})
        EXPECT_EQ(masked_with(tok, arena, MaskConfig{}), tok)
            << "the >=16 hash floor must not move — a wrapped short hex word is still content.";
}

// invariant: THE BOUNDARY THE HEX RIP MUST NOT CROSS, written BEFORE the rip while the knob still
// existed and both positions could be proven.
// invariant: the rip removed the knob, so the toggle is now unrepresentable and only the surviving
// half is kept.
// invariant: that half is the whole property — these tokens mask on digit-leading GROUND ALONE,
// which is why removing the rule moved nothing.
// invariant: deliberately NOT a test of the ripped rule: that rule could not be tested, which is
// why it is gone, and there is no predicate left to mutate.
// refs: DN-027
TEST(StatelessTemplate, HexTokensStillMaskViaDigitLeadingAfterTheRuleFiveRip)
{
    ArenaAllocator arena{256U * 1024U};

    for (const std::string_view tok : {"0xDEADBEEF", "0x1f,", "0xdeadbeef"})
    {
        const std::string masked{masked_with(tok, arena, MaskConfig{})};
        EXPECT_EQ(masked, "<*>")
            << "token '" << tok
            << "' must mask via digit-leading (it starts with '0'). With rule 5 gone this is "
               "the "
               "ONLY thing that can catch it. If this KEEPS, the rip was not behaviour-preserving "
               "and digit-leading is not the cover it was measured to be.\n  actual: "
            << masked;
    }
}

// invariant: the third alphabet arm of the embedded-identity rule — an extended date plus a
// colon-free time plus a mandatory zone letter, exactly 18 bytes, delimiter-bounded both sides.
// invariant: it closed 16 of 17 hand-read false alerts on a release bench, where a per-run instant
// entered template identity verbatim and made every run its own template.
// invariant: the three arms of that function are DISJOINT BY CONSTRUCTION, and the last test here
// is the detector for that.
namespace
{
// invariant: the containing directory is deliberately NOT a declared ephemeral root, because its
// direct children are a MIX of per-run instances and stable names, so declaring it over-masks.
// invariant: the fix rides on the CHILD's own grammar instead, which is why this is an alphabet arm
// rather than a catalog entry.
constexpr std::string_view kInstantWitness{"/home/runner/work/_temp/2026-06-09T185733Z.json"};
constexpr std::string_view kInstantWitnessMasked{"/home/runner/work/_temp/<*>.json"};
constexpr std::size_t kInstantLen{18};
} // namespace

// invariant: GATE 1 — the RED that existed before the arm, when this token came back
// byte-identical and carried the instant into the template and into the id hashed from it.
TEST(StatelessTemplate, CompactUtcInstantMasksInsideAPath)
{
    ArenaAllocator arena{256U * 1024U};
    ASSERT_EQ(std::string_view{"2026-06-09T185733Z"}.size(), kInstantLen)
        << "fixture drift: the grammar is a FIXED 18 bytes and the witness must carry exactly "
           "that many, else this test pins a different shape than the arm implements";

    const std::string got{masked(kInstantWitness, arena)};
    EXPECT_EQ(got, kInstantWitnessMasked)
        << "a compact UTC instant embedded in a path must mask while the surrounding path is "
           "KEPT — if it comes back verbatim the arm is gone and every run is its own "
           "template again\n  input    : "
        << kInstantWitness << "\n  expected : " << kInstantWitnessMasked
        << "\n  actual   : " << got;
}

// invariant: GATE 2, THE CAN'T-PASS CONTROL — a masking rule that cannot fail to mask is
// worthless, so each row is a NEAR MISS that must survive verbatim.
// invariant: every one is a live boundary of the grammar, and each is here because dropping it
// would enlarge the acceptance set silently.
// invariant: READ THIS BEFORE SIMPLIFYING THE COLON ROW — the colon-bearing form written into a
// REALISTIC path is claimed UPSTREAM by the diagnostic composite.
// invariant: its colon-digit trigger fires and the path supplies letter-leading anchors, so such a
// token never reaches the embedded-identity rule and asserting on it would prove nothing.
// invariant: the leading underscore and the absence of any letter-leading path segment are what
// make the composite decline, leaving the compact-instant arm the only rule that could claim it.
// invariant: that is the whole point of the row — it is the binary proof that the arm REFUSES the
// colon-bearing profile.
// invariant: that is what keeps the refusal to widen the shared datetime-length helper true in the
// code and not merely in prose.
TEST(StatelessTemplate, CompactUtcInstantGrammarDeclinesEveryNearMiss)
{
    ArenaAllocator arena{256U * 1024U};
    struct Row
    {
        std::string_view token;
        std::string_view why;
    };
    for (const Row& row : {
             Row{.token = "_2026-06-09T18:57:33Z.json",
                 .why = "the COLON-BEARING extended form is a different owner's grammar "
                        "(rfc3339_datetime_length) and this arm must never admit it"},
             Row{.token = "/home/runner/work/_temp/2026-06-09T185733.json",
                 .why = "no `Z` — the 17-byte zoneless form is a materially larger acceptance "
                        "set and has no right-hand literal anchor"},
             Row{.token = "/home/runner/work/_temp/2026-06-09T185733Zebra.json",
                 .why = "RIGHT delimiter gate: the byte after the 18 is alphanumeric, so these "
                        "are 18 bytes of a longer name, not a token"},
             Row{.token = "/home/runner/work/_temp/12026-06-09T185733Z.json",
                 .why = "LEFT delimiter gate: a valid instant sits at offset 1, and matching "
                        "there would slide the window over a longer name"},
         })
    {
        const std::string_view tok{row.token};
        const std::string got{masked(tok, arena)};
        EXPECT_EQ(got, tok) << "this token is a NEAR MISS and must survive VERBATIM — a grammar "
                               "that accepts it is not the ruled grammar.\n  reason   : "
                            << row.why << "\n  token    : " << tok << "\n  expected : " << tok
                            << " (kept)\n  actual   : " << got;
    }

    // invariant: the coordinate that keeps the row above from teaching a falsehood.
    // invariant: in a REALISTIC path the colon-bearing form IS masked, by the diagnostic composite,
    // one wildcard per segment.
    // invariant: pinned so a reader does not conclude that the colon form is never masked, which is
    // false, and so a claim about the boundary is written against what the binary does.
    constexpr std::string_view kColonInPath{"/home/runner/work/_temp/2026-06-09T18:57:33Z.json"};
    constexpr std::string_view kColonInPathMasked{"/home/runner/work/_temp/<*>:<*>:<*>"};
    const std::string colon_got{masked(kColonInPath, arena)};
    EXPECT_EQ(colon_got, kColonInPathMasked)
        << "the colon-bearing form in a real path is claimed by diagnostic_composite, NOT by "
           "the compact-instant arm — if this moved, the two rules now overlap and the "
           "boundary the LIM row states is no longer the boundary the code draws\n  input    : "
        << kColonInPath << "\n  expected : " << kColonInPathMasked
        << "\n  actual   : " << colon_got;
}

// invariant: GATE 3 — DISJOINTNESS AS AN INSTRUMENT, not as a comment.
// invariant: the rule runs three arms in ONE left-to-right scan, and their ORDER is a COST choice
// rather than a precedence, because no token can be claimed by two.
// invariant: three literal bytes forbid every pair.
// invariant: a UUID requires a hyphen where an instant requires a digit and a separator letter, and
// the hex arm requires a long consecutive hex run where an instant requires a hyphen.
// invariant: A DECLARED LIMITATION WITH NO DETECTOR IS A COMMENT; WITH AN ARM IT IS AN INSTRUMENT.
// invariant: so the three shapes are put in ONE token and the exact output is pinned, and if a
// future arm overlaps any of them the byte output moves and this fails loudly.
TEST(StatelessTemplate, EmbeddedIdentityArmsAreDisjoint)
{
    ArenaAllocator arena{256U * 1024U};
    // invariant: the containing directory is deliberately not a declared ephemeral root and the
    // token carries no colon-digit, so every earlier rule declines.
    // invariant: that makes the embedded-identity rule genuinely the rule under test, without which
    // this pin would be vacuous.
    constexpr std::string_view kAllThree{"/var/cache/f7f63412-b7a7-468d-bd31-1a6ae1ca2680/"
                                         "deadbeefcafe0badf00d/2026-06-09T185733Z/x"};
    constexpr std::string_view kAllThreeMasked{"/var/cache/<*>/<*>/<*>/x"};
    const std::string got{masked(kAllThree, arena)};
    EXPECT_EQ(got, kAllThreeMasked)
        << "each of the three embedded-identity shapes must be claimed by EXACTLY ONE arm, "
           "leaving exactly three wildcards and the surrounding path intact. A different "
           "string here means two arms overlap, or one arm now eats a neighbour's bytes.\n"
           "  input    : "
        << kAllThree << "\n  expected : " << kAllThreeMasked << "\n  actual   : " << got;

    // invariant: the NEGATIVE half, one per pair, asserted where an overlap would first show.
    // invariant: the instant must be claimed WHOLE rather than partially eaten by the hex-run scan,
    // which would leave residue, and neither other shape may be touched by the instant grammar.
    struct Row
    {
        std::string_view token;
        std::string_view want;
        std::string_view why;
    };
    for (const Row& row : {
             Row{.token = "/var/cache/2026-06-09T185733Z/x",
                 .want = "/var/cache/<*>/x",
                 .why = "the instant is claimed WHOLE — a partial claim leaves digits behind"},
             Row{.token = "/var/cache/f7f63412-b7a7-468d-bd31-1a6ae1ca2680/x",
                 .want = "/var/cache/<*>/x",
                 .why = "a UUID is still the UUID arm's; the instant grammar must not see it"},
             Row{.token = "/var/cache/deadbeefcafe0badf00d/x",
                 .want = "/var/cache/<*>/x",
                 .why = "a >=16 hex run is still the hex arm's; the instant grammar must not "
                        "see it"},
         })
    {
        const std::string one{masked(row.token, arena)};
        EXPECT_EQ(one, row.want) << "one shape, one arm, one wildcard.\n  reason   : " << row.why
                                 << "\n  input    : " << row.token << "\n  expected : " << row.want
                                 << "\n  actual   : " << one;
    }
}

// invariant: THE MASK IS AN IDENTITY INSTRUMENT, NOT A SCRUB, and the pair is asserted TOGETHER.
// invariant: a masked value is RELOCATED into the params, never deleted — the masker exists so
// two lines differing only in their instance share an identity, and params is where it goes.
// invariant: nobody had asserted the two halves together, so the shape read equally well as a
// scrub.
// invariant: a reader who takes it for a scrub will publish a document believing the values are
// gone.
// invariant: the fixture address is a RESERVED documentation range, routable nowhere, so it cannot
// become a real address by someone's later edit.
// refs: DN-86.D3
TEST(StatelessTemplate, MaskingRelocatesTheValueIntoParamsRatherThanDeletingIt)
{
    ArenaAllocator arena{256U * 1024U};
    constexpr std::string_view kDocAddress{"192.0.2.146"};
    const std::string line{std::string{"connection refused from "} + std::string{kDocAddress} +
                           " after 3 retries"};

    arena.reset();
    const StatelessTemplate result{stateless_template(line, arena, cfg())};

    // invariant: HALF 1 — the identity no longer carries the instance.
    EXPECT_EQ(result.template_str, "connection refused from <*> after <*> retries")
        << "the address must reach the template as a wildcard, or two hosts are two identities";
    EXPECT_EQ(result.template_str.find(kDocAddress), std::string_view::npos)
        << "the address is still in the template: " << result.template_str;

    // invariant: HALF 2 — the value is KEPT verbatim beside it, which is the half that makes
    // `mask` the wrong word for what happens to the value.
    EXPECT_NE(std::ranges::find(result.params, kDocAddress), result.params.end())
        << "the address left the template and is in NO param — that would be a scrub, and the "
           "egress ruling (DN-86.D5) rests on it NOT being one.\n  template : "
        << result.template_str << "\n  params   : " << result.params.size() << " entr(y|ies)";
}
