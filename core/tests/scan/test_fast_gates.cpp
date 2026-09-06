
// invariant: WHITE-BOX unit tests for the fast-gate predicate layer and the vectorized scanning
// primitives.
// invariant: these gates run BEFORE every per-strategy regex probe, so a false NEGATIVE silently
// disables a format strategy and a false POSITIVE re-opens the regex cost the gate exists to kill.
// invariant: both are INVISIBLE to the strategy tests, which feed only well-formed lines.
// invariant: it closes the per-domain mirror gap a cascade audit flagged — the scan shard was the
// one without its own suite.
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight::tokenization;

TEST(FastGatesCharClass, DigitBoundaries)
{
    EXPECT_TRUE(is_digit('0'));
    EXPECT_TRUE(is_digit('9'));
    EXPECT_FALSE(is_digit('/'));
    EXPECT_FALSE(is_digit(':'));
    EXPECT_FALSE(is_digit(' '));
    // invariant: the SIGNED-CHAR trap — a high-bit byte must not wrap into the digit range, which
    // is exactly what the unsigned-subtraction trick guards.
    EXPECT_FALSE(is_digit('\xB0'));
}

TEST(FastGatesCharClass, AlphaBoundaries)
{
    EXPECT_TRUE(is_upper('A'));
    EXPECT_TRUE(is_upper('Z'));
    EXPECT_FALSE(is_upper('a'));
    EXPECT_FALSE(is_upper('@'));
    EXPECT_FALSE(is_upper('['));
    EXPECT_TRUE(is_lower('a'));
    EXPECT_TRUE(is_lower('z'));
    EXPECT_FALSE(is_lower('A'));
    EXPECT_FALSE(is_lower('`'));
    EXPECT_FALSE(is_lower('{'));
    EXPECT_FALSE(is_upper('\xC4'));
    EXPECT_FALSE(is_lower('\xE4'));
}

TEST(FastGatesCharClass, SpaceIsPosixSpaceTabOnly)
{
    EXPECT_TRUE(is_space(' '));
    EXPECT_TRUE(is_space('\t'));
    // invariant: the gates see single LINES, so a newline is CONTENT and not whitespace.
    EXPECT_FALSE(is_space('\n'));
    EXPECT_FALSE(is_space('\r'));
    EXPECT_FALSE(is_space('x'));
}

TEST(FastGatesCharClass, SkipSpacesAndConsumeDigits)
{
    EXPECT_EQ(skip_spaces("a   b", 1), 4U);
    EXPECT_EQ(skip_spaces("ab", 1), 1U);
    EXPECT_EQ(skip_spaces("a  ", 1), 3U);

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

// invariant: one canonical ACCEPT per format plus the discriminating REJECTS, with accept lines
// drawn from the real formats the strategies parse.
TEST(FastGatesPrefix, BsdSyslog)
{
    EXPECT_TRUE(is_bsd_syslog_prefix("Jan 15 08:03:22 host sshd[42]: accepted"));
    EXPECT_TRUE(is_bsd_syslog_prefix("Jun  5 08:03:22 single-digit day"))
        << "double space + 1-digit day is valid BSD";
    EXPECT_FALSE(is_bsd_syslog_prefix("jan 15 08:03:22 lowercase month"));
    EXPECT_FALSE(is_bsd_syslog_prefix("Jan 15 08:03 no seconds"));
    EXPECT_FALSE(is_bsd_syslog_prefix("Jan 15 080322 no colons"));
    EXPECT_FALSE(is_bsd_syslog_prefix("Jan 15 08:03:2"));
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

// invariant: the core timestamp scan primitive stays HERE, and the dialect SUBSET — a 28-byte
// stamp with exactly seven fraction digits — is no longer a canon predicate at all.
// invariant: it became a DECLARED TRANSPORT ROW, so the grammar is owned by the catalogue and the
// peel-equivalence gate carries its own local oracle rather than a shared helper.
// invariant: this prose named a package-private helper and a covering confidence test until
// 2026-09-07, and NEITHER exists — the subset was retired, not relocated.
TEST(FastGatesPrefix, Rfc3339Prefix)
{
    EXPECT_TRUE(is_rfc3339_prefix("2024-04-27T10:15:00Z payload"));
    EXPECT_FALSE(is_rfc3339_prefix("2024-04-27 10:15:00 space separator is not RFC 3339"));
    // invariant: a high-resolution fractional timestamp is STILL a valid prefix — the subset
    // relation, asserted at the core-primitive level and independent of any dialect.
    EXPECT_TRUE(is_rfc3339_prefix("2024-04-27T10:15:00.1234567Z ##[group]Run actions/checkout"));
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
    EXPECT_TRUE(
        is_bgl_labelled_prefix("- 1117838570 2005.06.03 R02-M1-N0-C:J12-U11 RAS KERNEL INFO"));
    EXPECT_FALSE(is_bgl_labelled_prefix("1117838570 2005.06.03 missing the leading label"));
    EXPECT_FALSE(is_bgl_labelled_prefix("- 1117838570 2005-06-03 dashes, not dots"));

    // invariant: the alert-label column is a dash or a BOUNDED uppercase class name, and nothing
    // else.
    // refs: DN-43.D14
    EXPECT_TRUE(is_bgl_labelled_prefix(
        "KERNDTLB 1117838570 2005.06.03 R02-M1-N0-C:J12-U11 RAS KERNEL FATAL"));
    EXPECT_TRUE(
        is_bgl_labelled_prefix("APPSEV 1117838570 2005.06.03 R02-M1-N0 RAS APP FAILURE msg"));
    EXPECT_TRUE(is_bgl_labelled_prefix("R_ID9 1117838570 2005.06.03 R02-M1-N0 RAS APP INFO msg"));
    EXPECT_FALSE(is_bgl_labelled_prefix("kerndtlb 1117838570 2005.06.03 lowercase is not a label"));
    EXPECT_FALSE(is_bgl_labelled_prefix("Kerndtlb 1117838570 2005.06.03 mixed case is not one"));
    // invariant: one byte past the bound, so the label does not end at whitespace and the line is
    // NOT silently re-read as a shorter label plus a junk epoch.
    EXPECT_FALSE(is_bgl_labelled_prefix("ABCDEFGHIJKLMNOPQ 1117838570 2005.06.03 R02-M1-N0 RAS"));
    EXPECT_TRUE(is_bgl_labelled_prefix("ABCDEFGHIJKLMNOP 1117838570 2005.06.03 R02-M1-N0 RAS"));

    EXPECT_TRUE(is_health_app_prefix("20171223-22:15:29:606|Step_LSC|30002312|onStandStepChanged"));
    // invariant: every line below carries the record's THREE separators, so each arm fails for the
    // reason its text NAMES and not for its arity.
    // invariant: before that rule landed these two carried ONE separator each, which made the
    // second arm's green a statement about SEPARATOR COUNT rather than about field width.
    // refs: DN-43.D16, MEM:synthetic-gate-vacuity-vs-judgment
    EXPECT_TRUE(is_health_app_prefix("20171223-2:15:29:606|c|1|single-digit hour"));
    EXPECT_FALSE(is_health_app_prefix("20171223 22:15:29:606|c|1|space, not dash"));

    // invariant: EVERY clock field is variable-width, and this arm asserted the OPPOSITE until
    // 2026-09-03 — it PINNED a rejection that was wrong about the FORMAT, not about the code.
    // invariant: the reference corpus is not zero-padded anywhere.
    // invariant: the fixed-width requirements rejected 247 of its 2 000 lines, 12.35 %, which got
    // no event time and a whole-line raw template.
    // invariant: the flip is the RULING LANDING and not a regression.
    // invariant: the reason nine generations of green never showed it is that the generator's own
    // emitter zero-pads UNCONDITIONALLY, so every synthetic line of that format was padded.
    // refs: DN-43.O5
    EXPECT_TRUE(is_health_app_prefix("20171223-22:15:29:66|c|1|two millisecond digits"));
    EXPECT_TRUE(is_health_app_prefix("20171223-22:15:29:6|c|1|one millisecond digit"));
    EXPECT_TRUE(is_health_app_prefix("20171223-22:1:29:606|c|1|single-digit minute"));
    EXPECT_TRUE(is_health_app_prefix("20171223-2:1:9:6|c|1|every clock field one digit"));
    // invariant: still BOUNDED ABOVE — a field wider than the grammar allows is not a valid head.
    EXPECT_FALSE(is_health_app_prefix("20171223-22:151:29:606|c|1|three-digit minute"));
    EXPECT_FALSE(is_health_app_prefix("20171223-22:15:29:6066|c|1|four millisecond digits"));

    // invariant: ARITY IS GRAMMAR — the predicate proves all three separators, because the parse
    // consumes three UNCONDITIONALLY.
    // invariant: at one separator the second take swallowed the message body onto the component and
    // published an EMPTY content.
    // invariant: at two, the process-id skip consumed the body and it reached NO projection field
    // at all.
    // invariant: both now score zero and are demoted to raw text, which keeps every byte, instead
    // of being parsed into a lie.
    // refs: DN-43.D16
    EXPECT_FALSE(is_health_app_prefix("20171223-22:15:29:606|onStandStepChanged 3579"))
        << "one separator: a four-field record's arity is not proven by its head";
    EXPECT_FALSE(is_health_app_prefix("20171223-22:15:29:606|Step_LSC|onStandStepChanged 3579"))
        << "two separators: the process-id field is absent, so the message body has no home";
    EXPECT_TRUE(is_health_app_prefix("20171223-22:15:29:606|||"))
        << "three separators with every field empty is a well-formed record, not a failure";
    EXPECT_TRUE(is_health_app_prefix("20171223-22:15:29:606|c|1|a|b|c"))
        << "separators inside the message body are content, not extra fields";

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

// invariant: inputs longer than one vector block exercise the SIMD loop, and short inputs exercise
// the scalar tail.
TEST(FastGatesScan, SvTakeTokenScalarAndSimdPaths)
{
    std::string_view sv{"alpha  beta\tgamma"};
    EXPECT_EQ(sv_take_token(sv), "alpha");
    EXPECT_EQ(sv_take_token(sv), "beta");
    EXPECT_EQ(sv_take_token(sv), "gamma");
    EXPECT_EQ(sv_take_token(sv), "") << "exhausted view yields empty tokens";
    EXPECT_TRUE(sv.empty());

    // invariant: more than a block of leading whitespace followed by a longer-than-a-block token,
    // so BOTH vector loops run.
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

// invariant: the per-token byte profile collapses THREE scans the masker's dispatch used to run per
// token into a SINGLE byte walk.
// invariant: it is otherwise exercised only TRANSITIVELY through the masker suite, so a refactor of
// the walk could silently diverge one field from the predicate it replaced.
// invariant: these lock the primitive DIRECTLY — each field asserted byte-exact against an
// INDEPENDENT reference oracle.
// invariant: the oracle is deliberately spelled with a DIFFERENT idiom than the production scan, a
// plain range test instead of the unsigned-subtraction one.
// invariant: that is what makes it a real CROSS-CHECK rather than a tautology of the code under
// test.
namespace
{
// invariant: an independent reference per field, mirroring the SPEC of the predicate it replaced
// and NOT its code.
struct ShapeOracle
{
    bool empty{};
    bool all_digits{};
    bool digit_leading{};
    bool has_separator{};
};

[[nodiscard]] constexpr bool ascii_digit(char chr) noexcept
{
    return chr >= '0' && chr <= '9';
}

[[nodiscard]] ShapeOracle reference_shape(std::string_view tok) noexcept
{
    ShapeOracle ref{};
    ref.empty = tok.empty();
    if (ref.empty)
        return ref;

    ref.all_digits = true;
    for (const char chr : tok)
        ref.all_digits = ref.all_digits && ascii_digit(chr);

    const std::size_t lead{(tok[0] == '+' || tok[0] == '-') ? 1U : 0U};
    ref.digit_leading = lead < tok.size() && ascii_digit(tok[lead]);

    ref.has_separator = tok.find_first_of(":/[#-=") != std::string_view::npos;
    return ref;
}
} // namespace

TEST(FastGatesTokenShape, FieldsAreByteExactWithReplacedPredicates)
{
    // invariant: the edge cases the handoff calls out, plus the byte-trap rows the scan must
    // survive.
    const std::string_view cases[]{
        "",
        "+",
        "-",
        "5",
        "0",
        "12345",
        "+5",
        "-42",
        "+a",
        "-=",
        "a1",
        "1a",
        ":",
        "/",
        "[",
        "#",
        "=",
        "10:15:00",
        "512MB",
        "6.2s",
        "0.25.5-3",
        "user-name",
        "v1.2",
        "\xB0\xB0",
        "\xFF"
        "9",
    };

    for (const std::string_view tok : cases)
    {
        const ShapeOracle ref{reference_shape(tok)};
        const TokenShape got{tok};
        SCOPED_TRACE(::testing::Message() << "token=\"" << tok << "\" (len=" << tok.size() << ")");
        EXPECT_EQ(got.empty, ref.empty) << "empty mismatch";
        EXPECT_EQ(got.all_digits, ref.all_digits) << "all_digits mismatch (== is_all_digits)";
        EXPECT_EQ(got.digit_leading, ref.digit_leading)
            << "digit_leading mismatch (== is_digit_leading)";
        EXPECT_EQ(got.has_separator, ref.has_separator)
            << "has_separator mismatch (== maybe_composite)";
    }
}

TEST(FastGatesTokenShape, SignOnlyTokenIsSeparatorButNotDigitLeading)
{
    // invariant: the subtle COLLISION the handoff flags.
    // invariant: a lone sign is sign-only, so digit-leading must be false since there is no digit
    // after it, AND it is itself a separator byte.
    const TokenShape minus{"-"};
    EXPECT_FALSE(minus.empty);
    EXPECT_FALSE(minus.all_digits);
    EXPECT_FALSE(minus.digit_leading) << "a lone sign has no digit after it";
    EXPECT_TRUE(minus.has_separator) << "'-' is in the composite separator set";

    const TokenShape plus{"+"};
    EXPECT_FALSE(plus.digit_leading);
    EXPECT_FALSE(plus.has_separator) << "'+' is a sign but NOT a composite separator";
}

TEST(FastGatesTokenShape, EmptyTokenIsAllFalseExceptEmpty)
{
    const TokenShape empty{""};
    EXPECT_TRUE(empty.empty);
    EXPECT_FALSE(empty.all_digits) << "an empty token is not all-digits (matches is_all_digits)";
    EXPECT_FALSE(empty.digit_leading);
    EXPECT_FALSE(empty.has_separator);
}
