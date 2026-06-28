// NOLINTBEGIN
// Unit tests + measurement for the stateless template masker
// (stateless_template_id.md D-TID-1/2). The property tests are committed regression
// guards — chiefly the phantom-pair kill (the whole point, §9.5). CardinalityOnCorpus
// is the masker-cardinality measurement (env-gated, skipped unless CORPUS_DIR points at
// a log corpus); the one-time over-split ratio vs the (now-ripped) Drain was 4.12x→1.79x,
// recorded at the re-measure gate (§8, commit 3829a88) — the standing guard is now the
// K_dim cardinality monitor, not a vs-Drain ratio.

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
    EXPECT_EQ(after_priming, fresh)
        << "stateless_template must depend ONLY on its own content\n"
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
// shows the accepted tradeoff, D-TID-8: `eu-west` stays literal — a letter-leading word,
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
    // The accepted tradeoff (D-TID-8): the region word is KEPT literal, not wildcarded.
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
    EXPECT_EQ(
        masked("[6226:0609/094020.430910:ERROR:dbus/bus.cc:408] Failed to connect to the bus", arena),
        masked("[6225:0528/144005.901629:ERROR:dbus/bus.cc:408] Failed to connect to the bus", arena))
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
        << "a /tmp SOURCE path (:line) is a diagnostic composite first — file kept, only :line masks";
}

// ── F13 strengthening (§8 / D-TID-11..13) — the re-measure rule set ──────────────

TEST(StatelessTemplate, AnsiEscapesStrippedBeforeTokenization)
{
    // D-TID-11: colour codes are presentation, stripped at ingest. Two coloured
    // variants of one line fold to the same colour-free content.
    std::string clean;
    strip_escape_sequences("\x1b[31mERROR\x1b[0m: pool down", clean);
    EXPECT_EQ(clean, "ERROR: pool down");
    strip_escape_sequences("plain text, no escapes", clean);
    EXPECT_EQ(clean, "plain text, no escapes");
    // An OSC sequence (ESC ] ... BEL) is dropped whole. (`\a` = BEL 0x07; avoid the
    // greedy `\x07b` hex-escape that would absorb the trailing 'b'.)
    strip_escape_sequences("a\x1b]0;title\ab", clean);
    EXPECT_EQ(clean, "ab");
}

TEST(StatelessTemplate, DigitLeadingTokensMask)
{
    ArenaAllocator arena{256U * 1024U};
    // One rule subsumes numbers-with-separators, decimals, number+unit, versions —
    // no unit lexicon. Each pair differs only in a digit-leading token → one template.
    EXPECT_EQ(masked("built in 6.2s", arena), masked("built in 11.9s", arena));        // duration
    EXPECT_EQ(masked("done 76.5%", arena), masked("done 100.0%", arena));              // percent
    EXPECT_EQ(masked("compiled 31,260 targets", arena), masked("compiled 9 targets", arena)); // grouped
    EXPECT_EQ(masked("installing pkg 0.25.5-3", arena), masked("installing pkg 1.2.11", arena)); // version
    EXPECT_EQ(masked("freed 512MB", arena), masked("freed 8GB", arena));               // number+unit
}

TEST(StatelessTemplate, LetterLeadingKeptUuidAndHashMasked)
{
    ArenaAllocator arena{256U * 1024U};
    // Letter-leading words are KEPT (the F13 boundary — D-TID-14): a word is not a number.
    EXPECT_NE(masked("decode utf8 stream", arena), masked("decode ascii stream", arena));
    EXPECT_EQ(masked("decode utf8 stream", arena), masked("decode utf8 stream", arena));
    EXPECT_NE(masked("hash sha256 ok", arena), masked("hash sha512 ok", arena)); // short, letter-leading → kept
    // UUID + long hash collapse (high-card identity).
    EXPECT_EQ(masked("temp f7f63412-b7a7-468d-bd31-1a6ae1ca2680 ready", arena),
              masked("temp 8b4537c3-1dd0-411a-a760-2aeb13934993 ready", arena));
    EXPECT_EQ(masked("commit 9fd7fb4c0de0abcd1234", arena),
              masked("commit deadbeefcafe0badf00d5678", arena));
}

TEST(StatelessTemplate, HashCounterAndWorkerBracketCollapse)
{
    ArenaAllocator arena{256U * 1024U};
    EXPECT_EQ(masked("step #26 done", arena), masked("step #7 done", arena));     // #-counter
    EXPECT_EQ(masked("[gw0] PASSED test_x", arena), masked("[gw3] PASSED test_x", arena)); // xdist worker
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
    // A value-WORD stays literal (the D-TID-14 boundary; the registry's job): user=alice
    // ≠ user=bob.
    EXPECT_NE(masked("login user=alice", arena), masked("login user=bob", arena));
    // Status-value KEEP (KV form): a green→red flip must NOT collapse.
    EXPECT_NE(masked("request status=200", arena), masked("request status=500", arena));
    EXPECT_NE(masked("proc code=0", arena), masked("proc code=1", arena));
    // …but a non-status numeric value beyond the status-digit gate masks.
    EXPECT_EQ(masked("listening port=8080", arena), masked("listening port=9090", arena));
}

// D-TID-22: a declared currency MARKER glued to a digit-led numeric core masks to <marker><*>
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
    // Boundary (D-TID-22 does NOT cross): `$`+letter has no digit core → kept literal, distinct.
    EXPECT_NE(masked("export $HOME", arena), masked("export $PATH", arena));
    EXPECT_NE(masked("cfg path=$HOME", arena), masked("cfg path=$ROOT", arena));
    // Letter-prefixed ids stay D-TID-18 registry-class (untouched, still distinct).
    EXPECT_NE(masked("ticket ORD-123", arena), masked("ticket ORD-456", arena));
    // `$42abc` is not a clean number (trailing alpha) → kept literal, like `#42abc`.
    EXPECT_NE(masked("ref $42abc", arena), masked("ref $99xyz", arena));
}

// ── Masker cardinality on a real corpus (the standing F13 re-measure instrument) ──
// Reports the masker's distinct-template count + singleton fraction on a corpus — the
// reading used to size F13 (the one-time over-split ratio vs the now-ripped Drain was
// 4.12x→1.79x at the §8 gate; that comparison cannot re-run post-rip, and the standing
// guard is the K_dim cardinality monitor). Re-run this after any F13 rule change.
// Skipped unless CORPUS_DIR is a directory of *.log files (the CI revert corpus).
TEST(StatelessTemplate, CardinalityOnCorpus)
{
    const char* const corpus_dir{std::getenv("CORPUS_DIR")};
    if (corpus_dir == nullptr)
        GTEST_SKIP() << "set CORPUS_DIR to a directory of *.log files to measure cardinality";

    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator{corpus_dir})
        if (entry.is_regular_file() && entry.path().extension() == ".log")
            files.push_back(entry.path());
    std::ranges::sort(files); // deterministic order
    ASSERT_FALSE(files.empty()) << "no *.log files under " << corpus_dir;

    constexpr std::size_t kMaxLines{300000};
    ArenaAllocator arena{8U * 1024U * 1024U};
    LogParser parser{arena};
    std::unordered_map<std::string, std::uint64_t> stateless_templates;
    std::size_t lines{0};

    for (const auto& file : files)
    {
        if (lines >= kMaxLines)
            break;
        std::ifstream in{file};
        std::string raw;
        while (lines < kMaxLines && std::getline(in, raw))
        {
            if (raw.empty())
                continue;
            arena.reset();
            // parse_line ANSI-strips at ingest (D-TID-11) and the masker runs on the
            // parsed content — exactly the production path.
            const auto parsed{parser.parse_line(raw)};
            if (!parsed)
                continue;
            ++stateless_templates[std::string{
                stateless_template(parsed->content, arena, cfg()).template_str}];
            ++lines;
        }
    }

    const std::size_t stateless_distinct{stateless_templates.size()};
    const std::size_t singletons{static_cast<std::size_t>(
        std::ranges::count_if(stateless_templates, [](const auto& kv) { return kv.second == 1; }))};

    std::cout << "\n=== Stateless template_id cardinality (F13 re-measure) ===\n"
              << "files            : " << files.size() << "\n"
              << "lines            : " << lines << "\n"
              << "Stateless distinct: " << stateless_distinct << "\n"
              << "singletons       : " << singletons << " ("
              << (stateless_distinct > 0 ? (100.0 * static_cast<double>(singletons) /
                                            static_cast<double>(stateless_distinct))
                                         : 0.0)
              << "% of distinct)\n";

    // The 30 loudest stateless templates with a long alnum-with-digit token (F13
    // candidates — tokens that varied but no rule masked).
    std::vector<std::pair<std::string, std::uint64_t>> by_count{stateless_templates.begin(),
                                                                stateless_templates.end()};
    std::ranges::sort(by_count, [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });
    std::cout << "--- top 15 by count ---\n";
    for (std::size_t i{0}; i < std::min<std::size_t>(15, by_count.size()); ++i)
        std::cout << by_count[i].second << "  " << by_count[i].first.substr(0, 120) << "\n";
    std::cout << "--- 40 singleton samples (the F13 over-split tail) ---\n";
    std::size_t shown{0};
    for (auto it{by_count.rbegin()}; it != by_count.rend() && shown < 40; ++it)
        if (it->second == 1)
        {
            std::cout << it->first.substr(0, 140) << "\n";
            ++shown;
        }
    std::cout << std::flush;

    EXPECT_GT(lines, 0u);
}

// NOLINTEND
