// NOLINTBEGIN : Unit tests may intentionally violate some style rules for clarity or simplicity.
// Unit tests: allow short identifiers and test-specific patterns
// tests/scan/test_fast_gates.cpp
//
// White-box unit tests for the insight.canon.detail.scan shard (the fast_gates
// predicate layer + the SSE2 sv_* scanning primitives). These gates run before
// every per-strategy RE2 probe, so a false NEGATIVE silently disables a format
// strategy and a false POSITIVE re-opens the RE2 cost the gate exists to kill —
// both invisible to the strategy tests (which feed only well-formed lines).
// Closes the tests/<domain> mirror gap flagged by the 1.5.2 cascade audit
// (scan was the one shard without a per-domain suite).

#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight::tokenization;

// ─────────────────────────────────────────────────────────────────────────────
// Character-class primitives
// ─────────────────────────────────────────────────────────────────────────────

TEST(FastGatesCharClass, DigitBoundaries)
{
    EXPECT_TRUE(is_digit('0'));
    EXPECT_TRUE(is_digit('9'));
    EXPECT_FALSE(is_digit('/')); // '0' - 1
    EXPECT_FALSE(is_digit(':')); // '9' + 1
    EXPECT_FALSE(is_digit(' '));
    // Signed-char trap: a high-bit byte must not wrap into the digit range —
    // the unsigned-subtraction trick is exactly what this guards.
    EXPECT_FALSE(is_digit('\xB0'));
}

TEST(FastGatesCharClass, AlphaBoundaries)
{
    EXPECT_TRUE(is_upper('A'));
    EXPECT_TRUE(is_upper('Z'));
    EXPECT_FALSE(is_upper('a'));
    EXPECT_FALSE(is_upper('@')); // 'A' - 1
    EXPECT_FALSE(is_upper('[')); // 'Z' + 1
    EXPECT_TRUE(is_lower('a'));
    EXPECT_TRUE(is_lower('z'));
    EXPECT_FALSE(is_lower('A'));
    EXPECT_FALSE(is_lower('`')); // 'a' - 1
    EXPECT_FALSE(is_lower('{')); // 'z' + 1
    EXPECT_FALSE(is_upper('\xC4'));
    EXPECT_FALSE(is_lower('\xE4'));
}

TEST(FastGatesCharClass, SpaceIsPosixSpaceTabOnly)
{
    EXPECT_TRUE(is_space(' '));
    EXPECT_TRUE(is_space('\t'));
    EXPECT_FALSE(is_space('\n')); // the gates see single LINES — '\n' is content
    EXPECT_FALSE(is_space('\r'));
    EXPECT_FALSE(is_space('x'));
}

TEST(FastGatesCharClass, SkipSpacesAndConsumeDigits)
{
    EXPECT_EQ(skip_spaces("a   b", 1), 4U);
    EXPECT_EQ(skip_spaces("ab", 1), 1U);  // nothing to skip
    EXPECT_EQ(skip_spaces("a  ", 1), 3U); // runs to end

    std::size_t pos{0};
    EXPECT_TRUE(consume_digits("123x", pos, 1U, 3U));
    EXPECT_EQ(pos, 3U) << "must advance past exactly the consumed digits";

    pos = 0;
    EXPECT_TRUE(consume_digits("12345", pos, 1U, 3U));
    EXPECT_EQ(pos, 3U) << "max_n caps consumption; the 4th digit is NOT an error";

    pos = 0;
    EXPECT_FALSE(consume_digits("1x", pos, 2U, 3U)) << "min_n=2 with one digit must fail";

    pos = 0;
    EXPECT_FALSE(consume_digits("xx", pos, 1U, 2U));
    EXPECT_EQ(pos, 0U) << "a failed consume from a non-digit must not advance";
}

// ─────────────────────────────────────────────────────────────────────────────
// Format prefix gates — one canonical accept + the discriminating rejects each.
// Accept lines are drawn from the real formats the strategies parse.
// ─────────────────────────────────────────────────────────────────────────────

TEST(FastGatesPrefix, BsdSyslog)
{
    EXPECT_TRUE(is_bsd_syslog_prefix("Jan 15 08:03:22 host sshd[42]: accepted"));
    EXPECT_TRUE(is_bsd_syslog_prefix("Jun  5 08:03:22 single-digit day"))
        << "double space + 1-digit day is valid BSD";
    EXPECT_FALSE(is_bsd_syslog_prefix("jan 15 08:03:22 lowercase month"));
    EXPECT_FALSE(is_bsd_syslog_prefix("Jan 15 08:03 no seconds"));
    EXPECT_FALSE(is_bsd_syslog_prefix("Jan 15 080322 no colons"));
    EXPECT_FALSE(is_bsd_syslog_prefix("Jan 15 08:03:2")); // below kBsdMinLen
}

TEST(FastGatesPrefix, IsoDateAndTimeAtOffset)
{
    EXPECT_TRUE(match_iso_date_at("xx2024-04-27", 2));
    EXPECT_FALSE(match_iso_date_at("xx2024/04/27", 2));
    EXPECT_FALSE(match_iso_date_at("2024-04-2", 0)) << "truncated day must not read out of bounds";
    EXPECT_TRUE(match_time_at("..10:15:00", 2));
    EXPECT_FALSE(match_time_at("..10-15-00", 2));
    EXPECT_FALSE(match_time_at("10:15:0", 0));
}

TEST(FastGatesPrefix, Rfc3339AndGithubActions)
{
    EXPECT_TRUE(is_rfc3339_prefix("2024-04-27T10:15:00Z payload"));
    EXPECT_FALSE(is_rfc3339_prefix("2024-04-27 10:15:00 space separator is not RFC 3339"));

    const std::string_view gha{"2024-04-27T10:15:00.1234567Z ##[group]Run actions/checkout"};
    EXPECT_TRUE(is_github_actions_prefix(gha));
    EXPECT_TRUE(is_github_actions_prefix("2024-04-27T10:15:00.1234567Z"))
        << "a blank GHA line is exactly the 28-char timestamp";
    EXPECT_FALSE(is_github_actions_prefix("2024-04-27T10:15:00.123456Z six fractional digits"))
        << "GHA is a STRICT subset: exactly 7 fractional digits (100-ns ticks)";
    EXPECT_FALSE(is_github_actions_prefix("2024-04-27T10:15:00.1234567+00:00 not Z"));
    EXPECT_FALSE(is_github_actions_prefix("2024-04-27T10:15:00.1234567Zx"))
        << "timestamp must be the whole line or followed by a space";
    EXPECT_TRUE(is_rfc3339_prefix(gha)) << "every GHA line is also RFC 3339 (the subset relation)";
}

TEST(FastGatesPrefix, IsoDatetimeSpace)
{
    EXPECT_TRUE(is_iso_datetime_space_prefix("2024-04-27 10:15:00 INFO ok", false));
    EXPECT_FALSE(is_iso_datetime_space_prefix("2024-04-27 10:15:00 INFO ok", true))
        << "require_fraction demands '.' or ',' after the seconds";
    EXPECT_TRUE(is_iso_datetime_space_prefix("2024-04-27 10:15:00.123 log4j", true));
    EXPECT_TRUE(is_iso_datetime_space_prefix("2024-04-27 10:15:00,123 log4j comma locale", true));
    EXPECT_FALSE(is_iso_datetime_space_prefix("2024-04-27T10:15:00 T separator", false));
}

TEST(FastGatesPrefix, NginxError)
{
    EXPECT_TRUE(is_nginx_error_prefix("2024/04/27 10:15:00 [error] 1#1: *5 connect() failed"));
    EXPECT_FALSE(is_nginx_error_prefix("2024-04-27 10:15:00 [error] dashes, not slashes"));
    EXPECT_FALSE(is_nginx_error_prefix(
        "2024/04/27 10:15:00                  [error] bracket past the scan window"))
        << "the '[' scan is bounded (kNginxScanTo) — a far bracket must not gate";
}

TEST(FastGatesPrefix, SparkAndHdfs)
{
    EXPECT_TRUE(is_spark_prefix("17/06/09 20:10:40 INFO executor.CoarseGrainedExecutorBackend"));
    EXPECT_FALSE(is_spark_prefix("2017/06/09 20:10:40 four-digit year is not spark"));
    EXPECT_TRUE(is_hdfs_prefix("081109 203615 148 INFO dfs.DataNode$PacketResponder"));
    EXPECT_FALSE(is_hdfs_prefix("081109 203615 INFO missing the block-id digit"));
}

TEST(FastGatesPrefix, Proxifier)
{
    EXPECT_TRUE(is_proxifier_prefix("[10.30 16:49:06] chrome.exe - proxy.example.com:443"));
    EXPECT_FALSE(is_proxifier_prefix("[10-30 16:49:06] dash date separator"));
    EXPECT_FALSE(is_proxifier_prefix("[10.30 16:49:06 missing closing bracket"));
}

TEST(FastGatesPrefix, Rfc5424)
{
    EXPECT_TRUE(is_rfc5424_prefix("<165>1 2024-04-27T10:15:00Z host app - - - msg"));
    EXPECT_TRUE(is_rfc5424_prefix("<7>1 2024-04-27 short PRI"));
    EXPECT_FALSE(is_rfc5424_prefix("<165> 2024-04-27T10:15:00Z no version digit"));
    EXPECT_FALSE(is_rfc5424_prefix("<165>1 Apr 27 not an ISO date"));
}

TEST(FastGatesPrefix, ApacheError)
{
    EXPECT_TRUE(is_apache_error_prefix("[Tue Apr 27 10:15:22 2024] [error] [client 1.2.3.4]"));
    EXPECT_FALSE(is_apache_error_prefix("[tue Apr 27 10:15:22 2024] lowercase day"));
    EXPECT_FALSE(is_apache_error_prefix("Tue Apr 27 10:15:22 2024 no bracket"));
}

TEST(FastGatesPrefix, BglHealthAppHpc)
{
    EXPECT_TRUE(is_bgl_prefix("- 1117838570 2005.06.03 R02-M1-N0-C:J12-U11 RAS KERNEL INFO"));
    EXPECT_FALSE(is_bgl_prefix("1117838570 2005.06.03 missing the leading dash"));
    EXPECT_FALSE(is_bgl_prefix("- 1117838570 2005-06-03 dashes, not dots"));

    EXPECT_TRUE(is_health_app_prefix("20171223-22:15:29:606|Step_LSC|30002312|onStandStepChanged"));
    EXPECT_TRUE(is_health_app_prefix("20171223-2:15:29:606|single-digit hour"));
    EXPECT_FALSE(is_health_app_prefix("20171223-22:15:29:66|two millis digits"));
    EXPECT_FALSE(is_health_app_prefix("20171223 22:15:29:606|space, not dash"));

    EXPECT_TRUE(is_hpc_prefix("227 node-246 unix.hw state_change.unavailable 1077804742 1 boot"));
    EXPECT_FALSE(is_hpc_prefix("227 node-246 unix.hw state_change.unavailable 107780 1 short ts"))
        << "the epoch field needs >= 10 digits";
    EXPECT_FALSE(is_hpc_prefix("node-246 starts with a token, not an id"));
}

TEST(FastGatesPrefix, ClfTimestampAnchor)
{
    EXPECT_TRUE(has_clf_timestamp(
        R"(10.0.0.1 - frank [27/Apr/2024:10:15:00 +0000] "GET / HTTP/1.1" 200 2326)"));
    EXPECT_FALSE(
        has_clf_timestamp(R"(10.0.0.1 - frank [27/apr/2024:10:15:00 +0000] lowercase month)"));
    EXPECT_FALSE(has_clf_timestamp(R"(no bracketed timestamp anywhere 27/Apr/2024:10:15:00)"));
}

TEST(FastGatesKv, CountKvPairSignatures)
{
    EXPECT_EQ(count_kv_pair_signatures(R"(level=info user_id=42 msg="hi")", 10U), 3U);
    EXPECT_EQ(count_kv_pair_signatures("level=info user_id=42 msg=x", 2U), 2U)
        << "the cap must short-circuit the scan";
    EXPECT_EQ(count_kv_pair_signatures("a == b and x = y", 10U), 0U)
        << "'==' and spaced '=' are not kv signatures";
    EXPECT_EQ(count_kv_pair_signatures("=x leading equals has no key char", 10U), 0U);
}

// ─────────────────────────────────────────────────────────────────────────────
// SSE2 sv_* scanning primitives. Inputs longer than one 16-byte SSE block
// exercise the vector loop; short inputs exercise the scalar tail.
// ─────────────────────────────────────────────────────────────────────────────

TEST(FastGatesScan, SvTakeTokenScalarAndSimdPaths)
{
    std::string_view sv{"alpha  beta\tgamma"};
    EXPECT_EQ(sv_take_token(sv), "alpha");
    EXPECT_EQ(sv_take_token(sv), "beta");
    EXPECT_EQ(sv_take_token(sv), "gamma");
    EXPECT_EQ(sv_take_token(sv), "") << "exhausted view yields empty tokens";
    EXPECT_TRUE(sv.empty());

    // > 16 bytes of leading whitespace + a > 16-byte token: both SIMD loops run.
    std::string_view simd{"                    a_token_longer_than_sixteen_bytes tail"};
    EXPECT_EQ(sv_take_token(simd), "a_token_longer_than_sixteen_bytes");
    EXPECT_EQ(simd, "tail") << "view must rest at the next token start";

    std::string_view all_ws{"        \t        "};
    EXPECT_EQ(sv_take_token(all_ws), "");
    EXPECT_TRUE(all_ws.empty()) << "all-whitespace input must drain the view";
}

TEST(FastGatesScan, SvSkipWs)
{
    std::string_view sv{"   x"};
    sv_skip_ws(sv);
    EXPECT_EQ(sv, "x");
    std::string_view untouched{"x  "};
    sv_skip_ws(untouched);
    EXPECT_EQ(untouched, "x  ") << "only LEADING whitespace is skipped";
}

TEST(FastGatesScan, SvTakeUntilTakeN)
{
    std::string_view sv{"key:value:rest"};
    EXPECT_EQ(sv_take_until(sv, ':'), "key");
    EXPECT_EQ(sv, "value:rest") << "delimiter itself must be consumed";
    EXPECT_EQ(sv_take_until(sv, '|'), "value:rest") << "missing delim returns the remainder";
    EXPECT_TRUE(sv.empty());

    std::string_view fixed{"abcdef"};
    EXPECT_EQ(sv_take_n(fixed, 4U), "abcd");
    EXPECT_EQ(fixed, "ef");
    EXPECT_EQ(sv_take_n(fixed, 99U), "ef") << "n past the end is capped, not UB";
    EXPECT_TRUE(fixed.empty());
}

TEST(FastGatesScan, SvTakeBracketedAndQuoted)
{
    std::string_view sv{"[27/Apr/2024:10:15:00] rest"};
    EXPECT_EQ(sv_take_bracketed(sv), "27/Apr/2024:10:15:00");
    EXPECT_EQ(sv, " rest");

    std::string_view not_bracketed{"x[y]"};
    EXPECT_EQ(sv_take_bracketed(not_bracketed), "") << "no leading '[' yields empty";
    EXPECT_EQ(not_bracketed, "x[y]") << "and must not consume anything";

    std::string_view quoted{R"("GET / HTTP/1.1" 200)"};
    EXPECT_EQ(sv_take_quoted(quoted), "GET / HTTP/1.1");
    EXPECT_EQ(quoted, " 200");
    std::string_view unquoted{"plain"};
    EXPECT_EQ(sv_take_quoted(unquoted), "");
}

// NOLINTEND
