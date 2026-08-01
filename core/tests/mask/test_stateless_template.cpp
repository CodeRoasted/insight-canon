// NOLINTBEGIN
// Unit tests for the stateless template masker (stateless_template_id.md D-TID-1/D-TID-2).
// The property tests are committed regression guards — chiefly the phantom-pair kill
// (the whole point, §9.5). The F13 masker-cardinality RE-MEASURE lived here as an
// env-gated CardinalityOnCorpus test; it is a measurement over an operator-mounted
// population, not a regression property, so it moved out of the unit tree to the CLI
// instrument `core/tools/f13_cardinality_measure.cpp`.

#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight::tokenization;

namespace
{
MaskConfig cfg()
{
    return MaskConfig{}; // defaults: mask_ip_addresses / mask_hex_addresses on
}

// Copy the masked template out immediately (the arena is reused across calls).
std::string masked(std::string_view content, ArenaAllocator& arena)
{
    arena.reset();
    return std::string{stateless_template(content, arena, cfg()).template_str};
}
} // namespace

// ── The core property: identity is a pure function of the line's own content ────

TEST(StatelessTemplate, PureFunctionOfContentNotOrderOrStream)
{
    ArenaAllocator arena{256U * 1024U};
    const std::string_view line{"connect to host db-7 failed after 30 ms"};

    // Prime with unrelated lines, then ask for `line` — the result must not depend on
    // anything seen before (statelessness).
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
    // Differ only in masked tokens (a number, an IPv4) → one template.
    EXPECT_EQ(masked("request from 10.0.0.1 took 12 ms", arena),
              masked("request from 192.168.1.250 took 9999 ms", arena));
}

// The phantom pair, killed. The old stateful Drain learned (via absorb_into) to
// wildcard a non-numeric token that varied across the lines it happened to see, so a
// different surrounding stream learned differently — the SAME logical line got two
// templates (a false NewTemplate + VanishedTemplate on an outcome flip). The stateless
// masker decides masking per token from the line's OWN content, so the shared line
// yields ONE template no matter what surrounds it — the phantom cannot form. (It also
// shows the accepted tradeoff: `eu-west` stays literal — a letter-leading word,
// not a syntactic high-card class; the over-split that F13 + the cardinality monitor size.)
TEST(StatelessTemplate, KillsThePhantomPair)
{
    ArenaAllocator arena{256U * 1024U};
    const std::string_view shared{"deploy region eu-west complete"};

    // The shared line, computed in three different surrounding "streams" (each primed
    // with a DIFFERENT sibling whose region token varies). A stateful learner would
    // wildcard `eu-west` differently per stream; the stateless masker cannot — it never
    // looks at the siblings.
    masked("deploy region us-east complete", arena);
    const std::string in_stream_a{masked(shared, arena)};
    masked("deploy region ap-south complete", arena);
    const std::string in_stream_b{masked(shared, arena)};
    const std::string alone{masked(shared, arena)}; // no priming at all

    EXPECT_EQ(in_stream_a, in_stream_b)
        << "the stateless template must be stream-invariant (the phantom pair is impossible):\n"
        << "stream A: " << in_stream_a << "\nstream B: " << in_stream_b;
    EXPECT_EQ(in_stream_a, alone) << "priming must have zero effect: " << in_stream_a << " vs "
                                  << alone;
    // The accepted tradeoff (SRC-D-TID-14): the region word is KEPT literal, not wildcarded.
    EXPECT_NE(in_stream_a.find("eu-west"), std::string::npos)
        << "a letter-leading word stays literal (F13 boundary): " << in_stream_a;
}

TEST(StatelessTemplate, StatusValueKeptDistinct)
{
    ArenaAllocator arena{256U * 1024U};
    // The green→red flip must NOT collapse: exit code 0 and exit code 1 are distinct
    // templates (status-value KEEP), while a bare count stays masked.
    EXPECT_NE(masked("process exited with exit code 0", arena),
              masked("process exited with exit code 1", arena));
    EXPECT_EQ(masked("served 200 requests", arena), masked("served 4096 requests", arena));
}

TEST(StatelessTemplate, CompositesNormalized)
{
    ArenaAllocator arena{256U * 1024U};
    // Source location, versioned ref, bracket index — collapse the variable numeric
    // run while keeping the semantic literal (which file / which package / which word).
    EXPECT_EQ(masked("error at tokenizer.cpp:4500:30: bad token", arena),
              masked("error at tokenizer.cpp:12:5: bad token", arena));
    EXPECT_EQ(masked("building zlib/1.3", arena), masked("building zlib/1.2.11", arena));
    EXPECT_EQ(masked("make[2]: entering", arena), masked("make[15]: entering", arena));
    // A different file / package / word stays distinct (the semantic part is kept).
    EXPECT_NE(masked("error at tokenizer.cpp:1:1: bad token", arena),
              masked("error at parser.cpp:1:1: bad token", arena));
}

// ── D-MSK-5 (bibles/jenkins_dialect.md §4, ADR-23 erratum 2) — bracket_timestamp ──
// The whole-token bracketed RFC3339 stamp fell through every rule to literal KEEP ("the bracket
// is the entire difference"): unbracketed the same token is digit-leading and masks, so on an
// undeclared Jenkins timestamper stream every stamped line was its own template (95.9% of the
// no-collapse ceiling, ADR-23). These arms are the fix's UNIT gate: the stamp class
// collapses to `[<*>]`, and the §6.2 decline list stays byte-identical — the rule claims the
// bracketed full-datetime class and NOTHING adjacent to it (precision-first, ADR-9). These
// arms are also one of the two NAMED holders of the §6.5 P2 over-masking blind spot: the A/B
// prefix-image comparison cancels a leak that hits both arms, so the decline list HERE (plus the
// D11 collateral leg) is what carries that hazard.
TEST(StatelessTemplate, BracketTimestampCollapsesTheStampClass)
{
    ArenaAllocator arena{256U * 1024U};
    // Three same-shape lines differing only in the stamp → ONE template (the ADR-23 measured
    // shape, inverted: pre-fix these were three templates, each equal to its raw line).
    const std::string first{masked("[2026-06-23T15:11:09.020Z] + git fetch --tags", arena)};
    const std::string second{masked("[2026-06-23T15:11:10.884Z] + git fetch --tags", arena)};
    const std::string third{masked("[2026-06-24T09:02:44.001Z] + git fetch --tags", arena)};
    EXPECT_EQ(first, second) << "same-shape stamped lines must now collapse";
    EXPECT_EQ(first, third) << "a different day must not fork the template";
    EXPECT_EQ(first, "[<*>] + git fetch --tags")
        << "normal form is the bracket convention: the bracket survives, the instance masks";
    // Mid-line, and zone variants: the whole-token trigger is position-independent within the
    // line, and the shared grammar accepts Z / ±HH:MM / ±HHMM / zoneless exactly like the
    // Jenkins timestamper acceptor.
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
    // The §6.2 decline list, byte-identical through the masker: date-only, time-only, word,
    // version, and trailing-punctuation forms are NOT the claimed class and stay literal KEEPs.
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
    // The bare-integer interior stays bracket_index's: `[42]` still normalizes to `[<*>]` via its
    // OWN rule (the output-class collision is named and accepted; the CLAIM stays partitioned).
    EXPECT_EQ(masked("[42] x", arena), "[<*>] x") << "bracket_index's claim, unchanged";
}

// ── D-MSK-1 (§4.1) — generalized composite-numeric masking (Chromium/Electron prefix) ──
// The glog/Chromium diagnostic prefix `[PID:MMDD/HHMMSS.micros:ERROR:file.cc:line]` is ONE
// whitespace-delimited token. The old source-location normalizer masked only the trailing
// `:line` and kept the whole `/`-bearing prefix as "path-like", so the high-cardinality
// PID/date/time segments survived → a line byte-identical in baseline read as a NEW error
// pattern (P6 dbus, 12×/12×). D-MSK-1 masks EVERY digit-leading sub-segment independently,
// keeping the letter-leading class anchors (ERROR, dbus, bus.cc) → both sides collapse to
// one template → not-new → dropped.
TEST(StatelessTemplate, DiagnosticCompositeCollapsesChromiumPrefix)
{
    ArenaAllocator arena{256U * 1024U};
    // The exact P6 pair — only PID / date / time differ → ONE template.
    EXPECT_EQ(masked("[6226:0609/094020.430910:ERROR:dbus/bus.cc:408] Failed to connect to the bus",
                     arena),
              masked("[6225:0528/144005.901629:ERROR:dbus/bus.cc:408] Failed to connect to the bus",
                     arena))
        << "Chromium PID/date/time segments mask; ERROR/dbus/bus.cc kept → baseline ≡ changed";
    // The letter-leading class anchor is KEPT: a different file in the prefix stays distinct.
    EXPECT_NE(masked("[6226:0609/094020.430910:ERROR:dbus/bus.cc:408] x", arena),
              masked("[6226:0609/094020.430910:ERROR:net/socket.cc:408] x", arena))
        << "letter-leading segments (the stable class) are kept — dbus/bus.cc ≠ net/socket.cc";
    // It subsumes the old source-location behaviour exactly (regression guard).
    EXPECT_EQ(masked("error at tokenizer.cpp:4500:30: bad token", arena),
              masked("error at tokenizer.cpp:12:5: bad token", arena))
        << "source-location masking unchanged under the generalized rule";
}

// The status-value carve-out MUST survive PER-SEGMENT (the green→red split — load-bearing).
// A digit segment that is a status value (≤ max digits, immediately preceded WITHIN the
// composite by a status keyword: exit/code/signal/status) is KEPT, exactly as the bare-token
// rule keeps `exit code 0`→`exit code 1`. So a varying id masks while the status flip stays
// split — never collapse a categorical status change. [[diff-engine-significance-cut-invariant]]
TEST(StatelessTemplate, DiagnosticCompositeKeepsStatusValuePerSegment)
{
    ArenaAllocator arena{256U * 1024U};
    // ONE composite that masks the request id AND keeps the status value: proves both paths.
    EXPECT_EQ(masked("[req:42/status:500] handled", arena),
              masked("[req:99/status:500] handled", arena))
        << "the varying request id masks; the same status:500 is kept → collapse on the id only";
    EXPECT_NE(masked("[req:42/status:500] handled", arena),
              masked("[req:42/status:200] handled", arena))
        << "the per-segment status carve-out: status:500 ≠ status:200 must NOT collapse";
    EXPECT_NE(masked("worker exit:0 done", arena), masked("worker exit:1 done", arena))
        << "exit:0 ≠ exit:1 — the colon-form of the exit-code carve-out, per-segment";
}

// ── D-MSK-2 (§4.2) — ephemeral-root path masking (randomized temp dirs, P6) ─────
// Playwright temp dirs `/tmp/pw-electron-userdata-Kw9v4a` carry a random base-62 suffix —
// letter-leading, not hex/UUID, so the existing masks keep it literal → a new template per
// run → novelty fatigue. The suffix is undecidable, but the ROOT is an enumerable byte-exact
// catalog (/tmp, /var/tmp, /var/folders): a child of an ephemeral root is a per-run instance
// by construction, so the post-root remainder masks to `<root>/<*>` — lossless for diffing.
TEST(StatelessTemplate, EphemeralRootPathMasksRemainder)
{
    ArenaAllocator arena{256U * 1024U};
    // The random-suffix variants collapse to one template…
    EXPECT_EQ(masked("opened /tmp/pw-electron-userdata-Kw9v4a ok", arena),
              masked("opened /tmp/pw-duplicate-collections-kvJMB5 ok", arena))
        << "/tmp/<random> → /tmp/<*> on both sides — the novelty fatigue is killed";
    // …and a stable temp file collapses with them (also non-diffable — lossless).
    EXPECT_EQ(masked("opened /tmp/transient ok", arena),
              masked("opened /tmp/pw-electron-userdata-Kw9v4a ok", arena))
        << "a stable child of /tmp is itself non-diffable — collapses to the same /tmp/<*>";
    EXPECT_EQ(masked("dir /var/folders/aB/cD ready", arena),
              masked("dir /var/folders/xY/zW ready", arena))
        << "/var/folders (macOS) is in the ephemeral-root catalog";
}

// The guard: this is an ephemeral-ROOT catalog, NOT a general absolute-path masker. A path
// under a non-ephemeral root keeps its identity, and a /tmp SOURCE path (with :line) keeps
// its file:line shape (the diagnostic composite is checked FIRST).
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

// ── F13 strengthening (§8 / SRC-D-TID-11..13) — the re-measure rule set ──────────────

TEST(StatelessTemplate, DigitLeadingTokensMask)
{
    ArenaAllocator arena{256U * 1024U};
    // One rule subsumes numbers-with-separators, decimals, number+unit, versions —
    // no unit lexicon. Each pair differs only in a digit-leading token → one template.
    EXPECT_EQ(masked("built in 6.2s", arena), masked("built in 11.9s", arena)); // duration
    EXPECT_EQ(masked("done 76.5%", arena), masked("done 100.0%", arena));       // percent
    EXPECT_EQ(masked("compiled 31,260 targets", arena),
              masked("compiled 9 targets", arena)); // grouped
    EXPECT_EQ(masked("installing pkg 0.25.5-3", arena),
              masked("installing pkg 1.2.11", arena));                   // version
    EXPECT_EQ(masked("freed 512MB", arena), masked("freed 8GB", arena)); // number+unit
}

TEST(StatelessTemplate, LetterLeadingKeptUuidAndHashMasked)
{
    ArenaAllocator arena{256U * 1024U};
    // Letter-leading words are KEPT (the F13 boundary — SRC-D-TID-14): a word is not a number.
    EXPECT_NE(masked("decode utf8 stream", arena), masked("decode ascii stream", arena));
    EXPECT_EQ(masked("decode utf8 stream", arena), masked("decode utf8 stream", arena));
    EXPECT_NE(masked("hash sha256 ok", arena),
              masked("hash sha512 ok", arena)); // short, letter-leading → kept
    // UUID + long hash collapse (high-card identity).
    EXPECT_EQ(masked("temp f7f63412-b7a7-468d-bd31-1a6ae1ca2680 ready", arena),
              masked("temp 8b4537c3-1dd0-411a-a760-2aeb13934993 ready", arena));
    EXPECT_EQ(masked("commit 9fd7fb4c0de0abcd1234", arena),
              masked("commit deadbeefcafe0badf00d5678", arena));
}

TEST(StatelessTemplate, HashCounterAndWorkerBracketCollapse)
{
    ArenaAllocator arena{256U * 1024U};
    EXPECT_EQ(masked("step #26 done", arena), masked("step #7 done", arena)); // #-counter
    EXPECT_EQ(masked("[gw0] PASSED test_x", arena),
              masked("[gw3] PASSED test_x", arena)); // xdist worker
    // The class marker is kept (a counter ≠ a worker bracket).
    EXPECT_NE(masked("#26", arena), masked("[gw26]", arena));
}

TEST(StatelessTemplate, KvNumericValueMaskedWordKept)
{
    ArenaAllocator arena{256U * 1024U};
    // D-TID-13 extension: a key=<digit-leading-value> token masks the VALUE, keeps the
    // key — so per-id KV lines collapse to one template (no error-singleton false-diff).
    EXPECT_EQ(masked("checkout completed order=100000", arena),
              masked("checkout completed order=999999", arena));
    EXPECT_EQ(masked("payment timeout txn=50000", arena),
              masked("payment timeout txn=70000", arena));
    EXPECT_EQ(masked("GC pause=512ms heap=87%", arena), masked("GC pause=9ms heap=3%", arena));
    // A value-WORD stays literal (the SRC-D-TID-14 boundary; the registry's job): user=alice
    // ≠ user=bob.
    EXPECT_NE(masked("login user=alice", arena), masked("login user=bob", arena));
    // Status-value KEEP (KV form): a green→red flip must NOT collapse.
    EXPECT_NE(masked("request status=200", arena), masked("request status=500", arena));
    EXPECT_NE(masked("proc code=0", arena), masked("proc code=1", arena));
    // …but a non-status numeric value beyond the status-digit gate masks.
    EXPECT_EQ(masked("listening port=8080", arena), masked("listening port=9090", arena));
}

// SRC-D-TID-22: a declared currency MARKER glued to a digit-led numeric core masks to <marker><*>
// (keep-marker, the #42→#<*> shape) — the decidable-numeric refinement of D-TID-18. Closes the
// over-split twin: `order completed total=$N` collapses to one stable template so its vanish forms.
TEST(StatelessTemplate, CurrencyMarkerNumberMasked)
{
    ArenaAllocator arena{256U * 1024U};
    // Bare token (composite chain): per-amount lines collapse to one template.
    EXPECT_EQ(masked("order completed $463", arena), masked("order completed $18", arena));
    EXPECT_EQ(masked("charged $463.50", arena), masked("charged $9.99", arena)); // decimal core
    // KV value (normalize_kv_value marker-strip): total=$N → total=$<*>.
    EXPECT_EQ(masked("order completed total=$463", arena),
              masked("order completed total=$18", arena));
    // The marker is KEPT (legible + a distinct class): $463 ≠ a bare 463, and $ ≠ # counters.
    EXPECT_NE(masked("$463", arena), masked("463", arena));
    EXPECT_NE(masked("$463", arena), masked("#463", arena));
    // Boundary (SRC-D-TID-22 does NOT cross): `$`+letter has no digit core → kept literal,
    // distinct.
    EXPECT_NE(masked("export $HOME", arena), masked("export $PATH", arena));
    EXPECT_NE(masked("cfg path=$HOME", arena), masked("cfg path=$ROOT", arena));
    // Letter-prefixed ids stay D-TID-18 registry-class (untouched, still distinct).
    EXPECT_NE(masked("ticket ORD-123", arena), masked("ticket ORD-456", arena));
    // `$42abc` is not a clean number (trailing alpha) → kept literal, like `#42abc`.
    EXPECT_NE(masked("ref $42abc", arena), masked("ref $99xyz", arena));
}

// ── Constant-pinning guards ──────────────────────────────────────────────────────
// WHY THESE EXIST, and why the suite above does not already cover them: every test
// above asserts a COLLAPSE (`masked(a) == masked(b)`), which stays green for ANY hash
// floor — both sides mask, or both stay literal, either way they match. Mutation
// testing (studies/011 §7.1) confirmed it: shipping kMinHashLen 16 → 8 left all 43
// committed expectations green, and the floor's stated rationale ("keeps deadbeef,
// cafe literal", mask.cpp) was asserted nowhere. Pinning a threshold requires EXACT
// template strings on BOTH sides of the boundary — literal at floor-1, `<*>` at floor.
//
// kMinHashLen is declared TWICE (two independent function-local copies): in
// `is_uuid_or_long_hash` (the standalone whole-token path, dispatch step 3) and in
// `normalize_embedded_identity` (composite rule #7, a delimiter-bounded run inside a
// larger token). A guard on one leaves the other free to drift, so both are pinned.
//
// These assert CURRENT SHIPPED BEHAVIOUR; they do not argue the floor is correct.
// studies/011 rules the value is not tunable by a threshold study — a red here means
// someone moved a load-bearing masking constant, which is an identity-affecting change
// requiring a kCanonicalizationVersion bump (SRC-D-TID-16), not a test to retune.

namespace
{
// The floor boundary, as byte-exact literals. Both are LETTER-leading on purpose: a
// digit-leading hex run ("0123456789abcde") is masked by the digit-leading rule
// regardless of the floor, which would make the pin vacuous.
constexpr std::string_view kHexBelowFloor{"deadbeefcafe0ba"}; // 15 — floor - 1
constexpr std::string_view kHexAtFloor{"deadbeefcafe0bad"};   // 16 — the shipped floor
constexpr std::size_t kFloorLen{16};
} // namespace

// Pin the STANDALONE floor (is_uuid_or_long_hash) exactly at 16: 15 hex chars stay
// literal, 16 mask. Together these two assertions admit exactly one value — any floor
// ≤ 15 reddens the first, any floor ≥ 17 reddens the second.
TEST(StatelessTemplate, HashFloorPinnedAtSixteenStandalone)
{
    ArenaAllocator arena{256U * 1024U};
    // Guard the fixtures themselves: a typo'd literal would silently move the boundary
    // under test and quietly re-vacuate the pin.
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

// The same boundary on the EMBEDDED path (composite rule #7) — a delimiter-bounded hex
// run inside a larger path token. A separate kMinHashLen copy, so a separate pin.
TEST(StatelessTemplate, HashFloorPinnedAtSixteenEmbedded)
{
    ArenaAllocator arena{256U * 1024U};
    // A non-ephemeral, non-versioned path so rules #1-#6 all decline and the token
    // actually reaches embedded_identity (otherwise the pin would be vacuous).
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

// The floor's STATED RATIONALE, asserted (mask.cpp: "keeps short hex-looking words
// (deadbeef, cafe) literal"). These are real vocabulary — hex-alphabet words and
// identifiers that carry meaning and MUST NOT collapse into `<*>`. They also ladder the
// sub-floor range, so a floor lowered to 4/6/7/8 reddens a named token rather than an
// abstract length.
TEST(StatelessTemplate, HashFloorKeepsShortHexWordsLiteral)
{
    ArenaAllocator arena{256U * 1024U};
    // `LEB128` is not all-hex ('L'); the rest are, and are kept solely by the floor.
    for (const std::string_view word : {std::string_view{"cafe"},     // 4
                                        std::string_view{"facade"},   // 6
                                        std::string_view{"ed25519"},  // 7
                                        std::string_view{"deadbeef"}, // 8
                                        std::string_view{"LEB128"}})  // 6, not hex
    {
        const std::string got{masked(std::string{word}, arena)};
        EXPECT_EQ(got, word) << "a " << word.size() << "-char hex-looking WORD must stay literal — "
                             << "the floor's stated rationale (mask.cpp) is violated\n"
                             << "  word     : " << word << "\n"
                             << "  expected : " << word << " (kept)\n"
                             << "  actual   : " << got;
    }
}

// `is_hex_char` folds ASCII case (`chr | 32`). No committed test exercised the fold
// (studies/011 §7.2), so dropping it would leave every UPPERCASE hash unmasked — real
// corpus content (uppercase git SHAs, checksum tables) silently over-splitting. Pin
// both the fold AND its bound: folding must not turn non-hex letters into hex.
TEST(StatelessTemplate, HexClassifierFoldsAsciiCase)
{
    ArenaAllocator arena{256U * 1024U};
    constexpr std::string_view kUpperHex{"DEADBEEFCAFE0BAD"}; // 16, uppercase
    constexpr std::string_view kMixedHex{"DeadBeefCafe0Bad"}; // 16, mixed case
    constexpr std::string_view kUpperUuid{"F7F63412-B7A7-468D-BD31-1A6AE1CA2680"};
    // 16 chars, uppercase, but NOT all-hex (X/Y/Z/W) — the fold must not over-reach.
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

    // The bound: same length, same case, but genuinely non-hex → KEPT. Guards a "fix"
    // that masks long uppercase words wholesale instead of folding case.
    const std::string non_hex{masked(std::string{kUpperNonHex}, arena)};
    EXPECT_EQ(non_hex, kUpperNonHex)
        << "a same-length UPPERCASE non-hex word must stay literal — the case fold must not "
        << "widen the hex alphabet\n  token: " << kUpperNonHex << "\n  expected: " << kUpperNonHex
        << " (kept)\n  actual: " << non_hex;
}

// ── D-MSK-4 gates: ephemeral-root masking ──
// The ROOT — not a hex/length heuristic (study 011 falsified that) — is the decidable thing. A
// path component directly under a declared ephemeral root is a per-run instance and masks to <*>;
// the location tail is protected. G-MSK-1..7 are the builder's contract.

TEST(EphemeralRootMask, G1_ReportedConanPairCollapses)
{
    ArenaAllocator arena{256U * 1024U};
    // The exact defect: two conan build-dir hashes for one logical diagnostic line.
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
    // None of these sits under a catalogued root, so D-MSK-4 must not touch them (over-mask check).
    // A pinned action SHA path (its change is drift we WANT surfaced) keeps its class anchors and
    // gains NO ephemeral artifact.
    const std::string sha{masked("_actions/actions/create-github-app-token/"
                                 "bcd2ba49abf26b56dd0dd2eb1c9dd5c77b096d4c/dist/main.cjs",
                                 arena)};
    EXPECT_NE(sha.find("_actions"), std::string::npos) << "sha=" << sha;
    EXPECT_NE(sha.find("main.cjs"), std::string::npos) << "sha=" << sha;
    EXPECT_EQ(sha.find(".conan2"), std::string::npos)
        << "no ephemeral artifact leaked in\nsha=" << sha;
    // A container image digest — `sha256:` is colon-LETTER, not `:digit`, so not a composite.
    const std::string dig{
        masked("alpine@sha256:ff6bdca1a26e4cf3f60c76e9f6f8bb2adb1e5a5b6c7d8e9f0", arena)};
    EXPECT_NE(dig.find("alpine"), std::string::npos)
        << "the image-name class anchor survives\ndig=" << dig;
}

TEST(EphemeralRootMask, G4_TmpSubtreeRegressionByteIdentical)
{
    ArenaAllocator arena{256U * 1024U};
    // D-MSK-2 regression: a /tmp subtree collapses exactly as under -5.
    EXPECT_EQ(masked("/tmp/pw-electron-userdata-Kw9v4a", arena), "/tmp/<*>");
}

TEST(EphemeralRootMask, G5_ClampKeepsTailUnderTmpDiagnostic)
{
    ArenaAllocator arena{256U * 1024U};
    // A /tmp-ROOTED diagnostic: the instance masks AND the tail survives (the clamp, M4). This
    // output CHANGES vs -5 (an improvement: the old ordering knowingly kept the phantom).
    const std::string tmpl{masked("/tmp/build-x/src/foo.cpp:42:5", arena)};
    EXPECT_EQ(tmpl, "/tmp/<*>/src/foo.cpp:<*>:<*>") << "tmpl=" << tmpl;
    EXPECT_NE(tmpl.find("foo.cpp"), std::string::npos) << "tail kept\ntmpl=" << tmpl;
}

TEST(EphemeralRootMask, G6_AnchorIsReal_MidPathTmpUntouched)
{
    ArenaAllocator arena{256U * 1024U};
    // `tmp` is TokenStart, so a mid-path `tmp` is NOT a root: a user file under it is untouched…
    EXPECT_EQ(masked("/home/user/tmp/notes.txt", arena), "/home/user/tmp/notes.txt");
    // …and in a diagnostic, the child of a mid-path `tmp` is KEPT (not masked as an instance).
    const std::string diag{masked("/home/user/tmp/build.log:5:1", arena)};
    EXPECT_NE(diag.find("build.log"), std::string::npos)
        << "a mid-path tmp must not float and mask its child\ndiag=" << diag;
}

TEST(EphemeralRootMask, G7_NoCatalogedRootNoOverFire)
{
    ArenaAllocator arena{256U * 1024U};
    // Blast-radius floor: a diagnostic under NO catalogued root masks exactly as -5 — only the
    // :line:col, every path component kept. A moved golden with no catalogued root = over-fire.
    EXPECT_EQ(masked("/home/user/project/src/main.cpp:10:5", arena),
              "/home/user/project/src/main.cpp:<*>:<*>");
}

// NOLINTEND
