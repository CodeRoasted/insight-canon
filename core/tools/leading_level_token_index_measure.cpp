// leading_level_token_index_measure — the standing G-L11 instrument (ADR-16.D7, ROADMAP N98).
//
// WHAT IT MEASURES. infer_leading_log_level's Stage 1 read the producer's own level word only
// when that word STARTED within kLeadingScanHead{40} raw bytes, until ROADMAP N98 replaced that
// byte head with the token budget kLeadingScanTokens{8} (time_utils.cpp) on this instrument's
// numbers. ADR-16.D7 rules that a budget deciding whether the producer's kind word is read at all
// is part of the CLAIM and must be counted in significant TOKENS, and that its VALUE is a
// measurement — the token index at which the leading level word sits, over the corpora, cut with a
// declared residual — never a judgment. This tool takes that measurement: per corpus root it
// prints the index distribution, what the replaced 40-byte head reached, and what every candidate
// token budget gains, loses and leaves against that head — with the residual's classes, so the
// cut is declared the way kKeywordHead{128} declares its own, and a re-derivation after the
// landing still reads against the same baseline. Per root it also prints the pipeline's LEVEL
// histogram: run at two commits over the same roots, the diff of that line is the classification
// delta at count grain (the G-L2 leg DN-54.D10 owes), which the index histogram — budget-blind by
// construction — cannot show.
//
// WHY A CLI TOOL AND NOT A TEST: the f13_cardinality_measure ruling (DN-18.D1 § 3.3). The
// population is whatever the operator mounts, so the report DECLARES it (files, lines, doors, the
// model's own control) rather than asserting on it, and no gate may depend on it.
//
// THE PREDICATE IS THE SHIPPED ONE, NOT A COPY. for_each_token is the tokenizer Stage 1 scans
// with and parse_log_level is the vocabulary it matches; both are public, and this tool calls them
// with scan_limit 0 (the whole remainder) and reports the index of the first hit. The replaced
// byte head's reach is reproduced without re-implementing its scan: for_each_token visits exactly
// the tokens whose START lies within a byte limit, so "the first level token starts at byte < 40"
// IS "the 40-byte head saw a level token".
//
// THE DOOR IS MODELLED, AND THE MODEL IS MEASURED. No strategy hands the scanner the line: each
// hands a REMAINDER (rfc3339_text.cpp scans after the stamp token, raw_text.cpp after a left-trim,
// syslog.cpp / bgl.cpp after their headers), so a raw-line index over-counts by whatever the door
// peeled. Every line is routed by the public pipeline (Tokenizer::process_line under a zero-package
// composition — a core tool may not link the vocabulary packages, SRC-SP-1), and the two doors that
// carry the CI corpora are modelled byte-for-byte from their source: RawText = trim ' '/'\t'
// (raw_text.cpp); Rfc3339Text = skip the first [^ \t]+ run and the [ \t]* after it
// (rfc3339_text.cpp via sv_take_token, whose whitespace set is space and tab). Every other door is
// reported UNMODELLED, its index taken on the whole stripped line as an UPPER BOUND. The model is
// checked against the pipeline on every modelled line — infer_leading_log_level(remainder) must
// equal the event's level — and the disagreement count is printed beside the histogram, so a model
// that peels the wrong bytes shows up as a number rather than as a silent shift of every other
// number.
//
// The pipeline runs stage 1 (the ANSI strip) before routing, and for_each_token treats an escape
// run as a delimiter, so the token index is presentation-invariant; only the BYTE offset moves.
// Both offsets are reported: on the stripped bytes (the parse_line door) and on the raw bytes
// (the parse_stable door, which strips nothing — ADR-16.D7).
//
// NOTHING FROM A LINE IS PRINTED. Prefix SHAPES (one class letter per token) and counts only — the
// private corpora may be measured here and published as counts, never as bytes.

// Textual, not `import std`-provided: `stderr` is a macro whose expansion needs the FILE*
// declaration visible in this TU, which only the header brings.
#include <cstdio>

import std;
import insight.canon;

namespace
{
using insight::LogFormat;
using insight::LogLevel;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

constexpr std::size_t kArenaBytes{std::size_t{8} * 1024 * 1024};
// The byte head the token budget REPLACED, quoted: kLeadingScanHead{40} was function-local to
// infer_leading_log_level and no longer exists in the tree. It stays the fixed baseline every
// budget column is read against, so a run after the landing still reports what the landing gained
// and lost, in the same columns the value was decided on.
constexpr std::size_t kReplacedByteHead{40};
constexpr std::size_t kHistogramWidth{24}; // indices printed one per row up to here, then pooled
constexpr std::size_t kPrefixShapeMaxTokens{8};
constexpr std::size_t kTopShapesShown{6};
constexpr std::size_t kFormatCount{static_cast<std::size_t>(LogFormat::Unknown) + 1U};
constexpr std::size_t kLevelCount{static_cast<std::size_t>(LogLevel::Unknown) + 1U};
// The budgets tabulated for every root, besides the coverage cuts the data itself produces. 7 is
// the pre-registered expectation (DN-54.D22 / ADR-16.D7: mass at index <= 6) and 8 is the landed
// value (kLeadingScanTokens, time_utils.cpp — Eqya's ruling of 2026-09-02 on the nested-record
// residual); the detailed cuts print the residual / gained / lost CLASSES for the candidates a
// ruling chooses between.
constexpr std::array<std::size_t, 10> kCandidateBudgets{2, 3, 4, 5, 6, 7, 8, 10, 12, 16};
constexpr std::array<std::size_t, 4> kDetailedBudgets{7, 8, 12, 16};
constexpr std::size_t kPreRegisteredBudget{7};
constexpr double kPercent{100.0};
constexpr double kCoverage99{0.99};
constexpr double kCoverage999{0.999};

constexpr int kExitOk{0};
constexpr int kExitEmptyPopulation{1};
constexpr int kExitUsage{2};
constexpr int kExitSelfTestFailed{3};
// The run died on a thrown exception — the only code that can fire after report rows have been
// printed: it says the report is partial.
constexpr int kExitFatal{4};

// ── The register PROXY, and it is a proxy ─────────────────────────────────────────────────────
// canon's is_verdict_anchored needs the whole-line kind-slot walk; this instrument classifies the
// level token by its two neighbouring bytes and its case only, so that the residual's rows can be
// read as verdict-shaped vs prose-shaped. First match wins, in this order.
enum class RegisterProxy : std::uint8_t
{
    Bracketed,  // [error] / ##[warning] — the byte before the token is '[' and the byte after ']'
    ColonAfter, // error: — the byte after the token is ':'
    Caps,       // ERROR — every byte an upper-case letter (two or more)
    Bare,       // error — anything else: the prose shape
};
constexpr std::size_t kProxyCount{4};
constexpr std::array<std::string_view, kProxyCount> kProxyNames{"bracketed", "colon", "caps",
                                                                "bare"};

// Which remainder rule the routed door hands the scanner — the two modelled doors, or none.
enum class DoorModel : std::uint8_t
{
    RawText,
    Rfc3339Text,
    Unmodelled,
};

struct LevelHit
{
    std::size_t token_index{0};
    std::size_t byte_start{0}; // of the token within the scanned bytes
    std::size_t token_size{0};
    LogLevel level{LogLevel::Unknown};
    RegisterProxy proxy{RegisterProxy::Bare};
    // No token follows the level word on the line — the one register in which an UNANCHORED
    // level word is still authoritative (time_utils.cpp: "the terminal / sole significant
    // token"), so a bare residual word that is terminal is a verdict a budget decides, and a bare
    // one that is not falls through to Stage 2 whatever the budget.
    bool terminal{false};
    // One class letter per prefix token (N digits · D date · Z digit-led ending in Z · P path ·
    // W letters · M mixed), '+' in the last slot when the prefix is longer than the buffer.
    std::array<char, kPrefixShapeMaxTokens + 1> shape{};
};

[[nodiscard]] constexpr bool is_digit(char chr) noexcept
{
    return chr >= '0' && chr <= '9';
}
[[nodiscard]] constexpr bool is_upper(char chr) noexcept
{
    return chr >= 'A' && chr <= 'Z';
}
[[nodiscard]] constexpr bool is_alpha(char chr) noexcept
{
    return is_upper(chr) || (chr >= 'a' && chr <= 'z');
}

[[nodiscard]] char token_shape_class(std::string_view token) noexcept
{
    constexpr std::size_t kDateWidth{10}; // YYYY-MM-DD
    if (std::ranges::all_of(token, is_digit))
        return 'N';
    if (token.size() >= kDateWidth && is_digit(token[0]) && is_digit(token[1]) &&
        is_digit(token[2]) && is_digit(token[3]) && token[4] == '-' && is_digit(token[5]) &&
        is_digit(token[6]) && token[7] == '-' && is_digit(token[8]) && is_digit(token[9]))
        return 'D';
    if (is_digit(token.front()) && token.back() == 'Z')
        return 'Z';
    if (token.find('/') != std::string_view::npos || token.find('\\') != std::string_view::npos)
        return 'P';
    if (std::ranges::all_of(token, is_alpha))
        return 'W';
    return 'M';
}

[[nodiscard]] RegisterProxy register_proxy_of(std::string_view scanned, std::string_view token,
                                              std::size_t byte_start) noexcept
{
    const std::size_t after_at{byte_start + token.size()};
    const char before{byte_start > 0 ? scanned[byte_start - 1] : '\0'};
    const char after{after_at < scanned.size() ? scanned[after_at] : '\0'};
    if (before == '[' && after == ']')
        return RegisterProxy::Bracketed;
    if (after == ':')
        return RegisterProxy::ColonAfter;
    if (token.size() >= 2 && std::ranges::all_of(token, is_upper))
        return RegisterProxy::Caps;
    return RegisterProxy::Bare;
}

// The predicate under measurement: the first token of `scanned`, under the shared canon
// tokenisation, that parse_log_level accepts — its index, its byte start, and its class.
// Only throw path is for_each_token's substr (begin <= size); the noexcept body cannot throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] std::optional<LevelHit> first_level_token(std::string_view scanned) noexcept
{
    std::optional<LevelHit> hit;
    std::size_t index{0};
    std::array<char, kPrefixShapeMaxTokens + 1> shape{};
    (void)insight::utils::for_each_token(
        scanned, 0U,
        [&](std::string_view token) noexcept
        {
            const LogLevel level{insight::utils::parse_log_level(token)};
            if (level == LogLevel::Unknown)
            {
                if (index < kPrefixShapeMaxTokens)
                    shape[index] = token_shape_class(token);
                else
                    shape[kPrefixShapeMaxTokens - 1] = '+';
                ++index;
                return false;
            }
            const std::size_t byte_start{static_cast<std::size_t>(token.data() - scanned.data())};
            hit = LevelHit{.token_index = index,
                           .byte_start = byte_start,
                           .token_size = token.size(),
                           .level = level,
                           .proxy = register_proxy_of(scanned, token, byte_start),
                           .shape = shape};
            return true;
        });
    if (hit)
        hit->terminal =
            !insight::utils::for_each_token(scanned.substr(hit->byte_start + hit->token_size), 0U,
                                            [](std::string_view) noexcept { return true; });
    return hit;
}

// ── The door models, byte-for-byte from the strategies' own source ────────────────────────────
[[nodiscard]] constexpr bool is_strategy_ws(char chr) noexcept // sv_take_token's whitespace set
{
    return chr == ' ' || chr == '\t';
}

// raw_text.cpp: "Trim leading ASCII whitespace" — ' ' and '\t' — then scan `content`.
// substr(pos) with pos <= size cannot throw; the noexcept body is total.
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] std::string_view raw_text_remainder(std::string_view line) noexcept
{
    std::size_t start{0};
    while (start < line.size() && is_strategy_ws(line[start]))
        ++start;
    return line.substr(start);
}

// rfc3339_text.cpp: `rest = line; sv_take_token(rest)` — skip leading ws, the stamp token, and
// the ws after it — then scan `rest`.
// substr(pos) with pos <= size cannot throw; the noexcept body is total.
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] std::string_view rfc3339_text_remainder(std::string_view line) noexcept
{
    std::size_t pos{0};
    while (pos < line.size() && is_strategy_ws(line[pos]))
        ++pos;
    while (pos < line.size() && !is_strategy_ws(line[pos]))
        ++pos;
    while (pos < line.size() && is_strategy_ws(line[pos]))
        ++pos;
    return line.substr(pos);
}

[[nodiscard]] constexpr DoorModel door_model_of(LogFormat routed) noexcept
{
    switch (routed)
    {
    case LogFormat::RawText:
        return DoorModel::RawText;
    case LogFormat::Rfc3339Text:
        return DoorModel::Rfc3339Text;
    default:
        return DoorModel::Unmodelled;
    }
}

[[nodiscard]] std::string_view remainder_for(DoorModel door, std::string_view bytes) noexcept
{
    switch (door)
    {
    case DoorModel::RawText:
        return raw_text_remainder(bytes);
    case DoorModel::Rfc3339Text:
        return rfc3339_text_remainder(bytes);
    case DoorModel::Unmodelled:
        return bytes;
    }
    return bytes;
}

// The GitLab runner's stream tag (`NNO ` / `NNE `, or `NNO+` on a continuation line) that the
// package door peels together with the stamp (gitlab_strategy.cpp: kTransportPrefixWidth) and the
// core Rfc3339Text door leaves in place as one extra token. Counted so the package-door index of
// a stamped GitLab line can be stated as "core-door index − 1" over a measured population.
enum class StreamTag : std::uint8_t
{
    None,
    NewLine,      // `NNO ` — one extra token under the core door, exactly
    Continuation, // `NNO+` — the '+' glues the tag to the next word; the shift is not one token
};
[[nodiscard]] StreamTag stream_tag_of(std::string_view remainder) noexcept
{
    constexpr std::size_t kTagWidth{4};
    if (remainder.size() < kTagWidth || !is_digit(remainder[0]) || !is_digit(remainder[1]) ||
        (remainder[2] != 'O' && remainder[2] != 'E'))
        return StreamTag::None;
    if (remainder[3] == ' ')
        return StreamTag::NewLine;
    if (remainder[3] == '+')
        return StreamTag::Continuation;
    return StreamTag::None;
}

// ── Aggregates ────────────────────────────────────────────────────────────────────────────────
// What a residual, a gain or a loss IS: its level words, its register shapes, how many of its
// level words are terminal, and the shapes of the prefixes in front of them.
struct Classes
{
    std::uint64_t lines{0};
    std::array<std::uint64_t, kLevelCount> by_level{};
    std::array<std::uint64_t, kProxyCount> by_proxy{};
    std::uint64_t terminal{0};
    std::uint64_t bare_terminal{0}; // the sub-class a budget DECIDES: unanchored yet authoritative
    std::map<std::string, std::uint64_t> shapes;

    void add(const LevelHit& hit)
    {
        ++lines;
        ++by_level[static_cast<std::size_t>(hit.level)];
        ++by_proxy[static_cast<std::size_t>(hit.proxy)];
        if (hit.terminal)
            ++terminal;
        if (hit.terminal && hit.proxy == RegisterProxy::Bare)
            ++bare_terminal;
        if (hit.token_index > 0)
            ++shapes[std::string{hit.shape.data()}];
    }
    void merge(const Classes& other)
    {
        lines += other.lines;
        for (std::size_t level{0}; level < kLevelCount; ++level)
            by_level[level] += other.by_level[level];
        for (std::size_t proxy{0}; proxy < kProxyCount; ++proxy)
            by_proxy[proxy] += other.by_proxy[proxy];
        terminal += other.terminal;
        bare_terminal += other.bare_terminal;
        for (const auto& [shape, count] : other.shapes)
            shapes[shape] += count;
    }
};

struct IndexStats
{
    // By exact token index, split on whether the level word STARTS inside the replaced 40-byte
    // head on the stripped bytes — the two populations a budget trades against that head.
    std::vector<Classes> in_head;           // byte start < 40: the byte head read the word
    std::vector<Classes> beyond_head;       // byte start >= 40: it did not
    std::vector<std::uint64_t> in_head_raw; // byte start < 40 on the RAW bytes (parse_stable)
    std::uint64_t lines{0};

    void record(const LevelHit& hit, bool in_byte_head, bool in_byte_head_raw)
    {
        if (hit.token_index >= in_head.size())
        {
            in_head.resize(hit.token_index + 1);
            beyond_head.resize(hit.token_index + 1);
            in_head_raw.resize(hit.token_index + 1, 0);
        }
        ++lines;
        (in_byte_head ? in_head : beyond_head)[hit.token_index].add(hit);
        if (in_byte_head_raw)
            ++in_head_raw[hit.token_index];
    }

    [[nodiscard]] std::size_t max_index() const noexcept
    {
        return in_head.empty() ? 0 : in_head.size() - 1;
    }
    [[nodiscard]] std::uint64_t total_at(std::size_t index) const noexcept
    {
        return in_head[index].lines + beyond_head[index].lines;
    }
    [[nodiscard]] std::uint64_t covered_by(std::size_t budget) const noexcept
    {
        std::uint64_t sum{0};
        for (std::size_t index{0}; index < std::min(budget, in_head.size()); ++index)
            sum += total_at(index);
        return sum;
    }
    [[nodiscard]] std::uint64_t byte_head_total() const noexcept
    {
        std::uint64_t sum{0};
        for (const Classes& classes : in_head)
            sum += classes.lines;
        return sum;
    }
    [[nodiscard]] std::uint64_t head_raw_total() const noexcept
    {
        return std::accumulate(in_head_raw.begin(), in_head_raw.end(), std::uint64_t{0});
    }
    // Lines the replaced 40-byte head read and a token budget does NOT: index >= budget, byte < 40.
    [[nodiscard]] Classes lost_vs_byte_head(std::size_t budget) const
    {
        Classes lost;
        for (std::size_t index{budget}; index < in_head.size(); ++index)
            lost.merge(in_head[index]);
        return lost;
    }
    // Lines a token budget reads and the replaced 40-byte head did NOT: index < budget, byte >= 40.
    [[nodiscard]] Classes gained_vs_byte_head(std::size_t budget) const
    {
        Classes gained;
        for (std::size_t index{0}; index < std::min(budget, beyond_head.size()); ++index)
            gained.merge(beyond_head[index]);
        return gained;
    }
    // Lines NO budget of `budget` tokens reads: index >= budget, whichever side of the head.
    [[nodiscard]] Classes residual_beyond(std::size_t budget) const
    {
        Classes residual;
        for (std::size_t index{budget}; index < in_head.size(); ++index)
        {
            residual.merge(in_head[index]);
            residual.merge(beyond_head[index]);
        }
        return residual;
    }
    // The smallest budget N whose coverage (index < N) reaches `fraction` of the level-bearing
    // lines; 0 when the population is empty.
    [[nodiscard]] std::size_t budget_covering(double fraction) const noexcept
    {
        if (lines == 0)
            return 0;
        std::uint64_t sum{0};
        for (std::size_t index{0}; index < in_head.size(); ++index)
        {
            sum += total_at(index);
            if (static_cast<double>(sum) >= fraction * static_cast<double>(lines))
                return index + 1;
        }
        return in_head.size();
    }
};

struct RootReport
{
    std::string label;
    std::filesystem::path dir;
    std::size_t files{0};
    std::uint64_t lines{0};
    std::uint64_t nonempty{0};
    std::uint64_t declined{0}; // the pipeline produced no event (a blank-after-peel line)
    std::array<std::uint64_t, kFormatCount> routed{};
    // Every routed event's level, declared or inferred, on every door: the one line of this report
    // that MOVES when Stage 1's budget moves. Diffed between two builds over the same root it is
    // the count-grain classification delta; the index histogram cannot show it, because the index
    // of a level word does not depend on whether the pipeline read it.
    std::array<std::uint64_t, kLevelCount> pipeline_levels{};
    std::uint64_t modelled{0};
    std::uint64_t unmodelled{0};
    std::uint64_t control_agree{0};
    std::uint64_t control_disagree{0};
    std::uint64_t control_declared{0}; // a modelled door with a declared level: outside the control
    std::array<std::uint64_t, kFormatCount> disagree_by_door{};
    std::uint64_t stream_tag_newline{0};
    std::uint64_t stream_tag_continuation{0};
    IndexStats modelled_stats;
    IndexStats unmodelled_stats;
};

[[nodiscard]] double percent(std::uint64_t part, std::uint64_t whole) noexcept
{
    return whole == 0 ? 0.0 : kPercent * static_cast<double>(part) / static_cast<double>(whole);
}

// ── The self-test: pre-registered rows the predicate and the door models must reproduce ───────
// Runs BEFORE the walk and refuses the run on any disagreement: a zero from an instrument that
// never demonstrated it can see the phenomenon is not a measurement. Rows 2 and 3 are ADR-16.D7's
// own pre-registered shapes; row 3 is also its "real path, ANSI stripped" witness row, whose level
// word starts at byte 43 — past the replaced 40-byte head with no escape byte anywhere.
struct SelfTestRow
{
    std::string_view name;
    std::string_view scanned;
    std::size_t token_index;
    std::size_t byte_start;
    LogLevel level;
    RegisterProxy proxy;
    std::string_view shape;
    bool terminal;
};

[[nodiscard]] bool run_self_test()
{
    static const std::string kPadded{"[" + std::string(41, 'a') +
                                     "] Failed to resolve action download info. Error: boom"};
    const std::array<SelfTestRow, 7> rows{{
        {.name = "bare level first",
         .scanned = "INFO starting worker",
         .token_index = 0,
         .byte_start = 0,
         .level = LogLevel::Info,
         .proxy = RegisterProxy::Caps,
         .shape = "",
         .terminal = false},
        {.name = "ISO comma-millis stamp (pre-registered: index 5)",
         .scanned = "2026-05-29 10:00:00,123 WARN pool exhausted",
         .token_index = 5,
         .byte_start = 24,
         .level = LogLevel::Warn,
         .proxy = RegisterProxy::Caps,
         .shape = "DNNNN",
         .terminal = false},
        {.name = "<path>:<line>:<col>: diagnostic (pre-registered: index 3; byte 43 > 40)",
         .scanned = "/builds/acme/widget/src/parser.cpp:210:21: warning: unused variable",
         .token_index = 3,
         .byte_start = 43,
         .level = LogLevel::Warn,
         .proxy = RegisterProxy::ColonAfter,
         .shape = "PNN",
         .terminal = false},
        {.name = "SGR-wrapped level, raw bytes (index invariant, byte offset spent by the escape)",
         .scanned = "\x1b[31mERROR\x1b[0m boom",
         .token_index = 0,
         .byte_start = 5,
         .level = LogLevel::Error,
         .proxy = RegisterProxy::Caps,
         .shape = "",
         .terminal = false},
        {.name = "41-byte bracket prefix, prose level word (the kind-slot test's shape)",
         .scanned = kPadded,
         .token_index = 6,
         .byte_start = 78,
         .level = LogLevel::Info,
         .proxy = RegisterProxy::Bare,
         .shape = "WWWWWW",
         .terminal = false},
        {.name = "##[error] workflow command",
         .scanned = "##[error]Process completed with exit code 1.",
         .token_index = 0,
         .byte_start = 3,
         .level = LogLevel::Error,
         .proxy = RegisterProxy::Bracketed,
         .shape = "",
         .terminal = false},
        {.name = "terminal bare level word (the register a budget decides)",
         .scanned = "the build finished with an error",
         .token_index = 5,
         .byte_start = 27,
         .level = LogLevel::Error,
         .proxy = RegisterProxy::Bare,
         .shape = "WWWWW",
         .terminal = true},
    }};

    bool all_ok{true};
    std::println("=== self-test: the predicate on pre-registered rows ===");
    for (const SelfTestRow& row : rows)
    {
        const std::optional<LevelHit> hit{first_level_token(row.scanned)};
        const bool row_ok{hit.has_value() && hit->token_index == row.token_index &&
                          hit->byte_start == row.byte_start && hit->level == row.level &&
                          hit->proxy == row.proxy &&
                          std::string_view{hit->shape.data()} == row.shape &&
                          hit->terminal == row.terminal};
        all_ok = all_ok && row_ok;
        if (hit)
            std::println("  [{}] {}: expected idx={} byte={} level={} proxy={} shape=\"{}\" "
                         "terminal={}  got idx={} byte={} level={} proxy={} shape=\"{}\" "
                         "terminal={}",
                         row_ok ? "ok" : "FAIL", row.name, row.token_index, row.byte_start,
                         insight::to_string(row.level),
                         kProxyNames[static_cast<std::size_t>(row.proxy)], row.shape, row.terminal,
                         hit->token_index, hit->byte_start, insight::to_string(hit->level),
                         kProxyNames[static_cast<std::size_t>(hit->proxy)], hit->shape.data(),
                         hit->terminal);
        else
            std::println("  [FAIL] {}: expected idx={} — got NO level token", row.name,
                         row.token_index);
    }

    // The door models, and the stamp shift they remove: the same GHA-shaped line is index 3 on
    // the raw line and index 0 on the Rfc3339Text door's remainder.
    constexpr std::string_view kStamped{"2026-05-27T15:42:03.4000004Z  ERROR boom"};
    const std::string_view rfc_rest{rfc3339_text_remainder(kStamped)};
    const std::optional<LevelHit> raw_hit{first_level_token(kStamped)};
    const std::optional<LevelHit> rest_hit{first_level_token(rfc_rest)};
    const bool rfc_ok{rfc_rest == "ERROR boom" && raw_hit && raw_hit->token_index == 3 &&
                      rest_hit && rest_hit->token_index == 0};
    all_ok = all_ok && rfc_ok;
    std::println("  [{}] Rfc3339Text door: remainder=\"{}\" raw-line idx={} door idx={} (expected "
                 "\"ERROR boom\", 3, 0)",
                 rfc_ok ? "ok" : "FAIL", rfc_rest, raw_hit ? raw_hit->token_index : 0,
                 rest_hit ? rest_hit->token_index : 0);
    const std::string_view raw_rest{raw_text_remainder("  \terror: x")};
    const bool raw_ok{raw_rest == "error: x"};
    all_ok = all_ok && raw_ok;
    std::println(R"(  [{}] RawText door: remainder="{}" (expected "error: x"))",
                 raw_ok ? "ok" : "FAIL", raw_rest);
    // The stripped-vs-raw byte offset the parse_line / parse_stable doors differ by.
    std::string scratch;
    const std::string_view stripped{
        insight::tokenization::normalize(rows[3].scanned, scratch).bytes()};
    const std::optional<LevelHit> stripped_hit{first_level_token(stripped)};
    const bool strip_ok{stripped_hit && stripped_hit->byte_start == 0 &&
                        stripped_hit->token_index == 0};
    all_ok = all_ok && strip_ok;
    std::println(
        "  [{}] stage-1 strip: the SGR row's level token starts at byte {} stripped, {} raw",
        strip_ok ? "ok" : "FAIL", stripped_hit ? stripped_hit->byte_start : 0, rows[3].byte_start);
    const bool tag_ok{stream_tag_of("00O section_start:1:x") == StreamTag::NewLine &&
                      stream_tag_of("00E+more") == StreamTag::Continuation &&
                      stream_tag_of("0O0 x") == StreamTag::None};
    all_ok = all_ok && tag_ok;
    std::println("  [{}] GitLab stream tag: `NNO ` new-line, `NNE+` continuation, else none",
                 tag_ok ? "ok" : "FAIL");
    std::println("self-test: {}", all_ok ? "PASS" : "FAIL — the run is refused");
    return all_ok;
}

// ── The walk ──────────────────────────────────────────────────────────────────────────────────
void measure_file(const std::filesystem::path& file, RootReport& report, ArenaAllocator& arena,
                  const insight::semantic::ComposedSemantics& composed)
{
    // A fresh Tokenizer per file: the format detector's latch is per STREAM in production, and a
    // corpus file is one stream.
    Tokenizer tokenizer{arena, MaskConfig{}, composed};
    std::ifstream input{file, std::ios::binary};
    std::string raw;
    std::string scratch;
    while (std::getline(input, raw))
    {
        ++report.lines;
        if (raw.empty())
            continue;
        ++report.nonempty;

        const auto event{tokenizer.process_line(raw)};
        if (!event.has_value())
        {
            ++report.declined;
            arena.reset();
            continue;
        }
        const LogFormat routed{event->format};
        const LogLevel pipeline_level{event->level};
        const bool declared{event->declared_level};
        arena.reset(); // every view the event held is dead from here; only values are read below

        ++report.routed[static_cast<std::size_t>(routed)];
        ++report.pipeline_levels[static_cast<std::size_t>(pipeline_level)];
        const DoorModel door{door_model_of(routed)};
        const std::string_view stripped{insight::tokenization::normalize(raw, scratch).bytes()};
        const std::string_view stripped_scan{remainder_for(door, stripped)};
        const std::string_view raw_scan{remainder_for(door, raw)};

        if (door == DoorModel::Unmodelled)
            ++report.unmodelled;
        else
        {
            ++report.modelled;
            if (declared)
                ++report.control_declared;
            else if (insight::utils::infer_leading_log_level(stripped_scan).value() ==
                     pipeline_level)
                ++report.control_agree;
            else
            {
                ++report.control_disagree;
                ++report.disagree_by_door[static_cast<std::size_t>(routed)];
            }
        }
        if (door == DoorModel::Rfc3339Text)
        {
            switch (stream_tag_of(stripped_scan))
            {
            case StreamTag::NewLine:
                ++report.stream_tag_newline;
                break;
            case StreamTag::Continuation:
                ++report.stream_tag_continuation;
                break;
            case StreamTag::None:
                break;
            }
        }

        const std::optional<LevelHit> hit{first_level_token(stripped_scan)};
        if (!hit)
            continue;
        const std::optional<LevelHit> raw_hit{first_level_token(raw_scan)};
        const bool in_byte_head{hit->byte_start < kReplacedByteHead};
        const bool in_byte_head_raw{raw_hit.has_value() &&
                                    raw_hit->byte_start < kReplacedByteHead};
        IndexStats& stats{door == DoorModel::Unmodelled ? report.unmodelled_stats
                                                        : report.modelled_stats};
        stats.record(*hit, in_byte_head, in_byte_head_raw);
    }
}

void print_histogram(const IndexStats& stats)
{
    std::println("  {:>5} {:>11} {:>8} {:>12} {:>12}", "idx", "lines", "cum%", "head<40(str)",
                 "head<40(raw)");
    std::uint64_t cumulative{0};
    for (std::size_t index{0}; index < std::min(stats.in_head.size(), kHistogramWidth); ++index)
    {
        cumulative += stats.total_at(index);
        std::println("  {:>5} {:>11} {:>8.3f} {:>12} {:>12}", index, stats.total_at(index),
                     percent(cumulative, stats.lines), stats.in_head[index].lines,
                     stats.in_head_raw[index]);
    }
    if (stats.in_head.size() > kHistogramWidth)
    {
        std::uint64_t pooled{0};
        std::uint64_t pooled_head{0};
        std::uint64_t pooled_raw{0};
        for (std::size_t index{kHistogramWidth}; index < stats.in_head.size(); ++index)
        {
            pooled += stats.total_at(index);
            pooled_head += stats.in_head[index].lines;
            pooled_raw += stats.in_head_raw[index];
        }
        std::println("  {:>4}+ {:>11} {:>8.3f} {:>12} {:>12}", kHistogramWidth, pooled,
                     percent(stats.lines, stats.lines), pooled_head, pooled_raw);
    }
    std::println("  max index: {}", stats.max_index());
}

void print_classes(std::string_view heading, const Classes& classes, std::uint64_t population)
{
    std::println("  {} : {} lines ({:.4f}% of level-bearing)", heading, classes.lines,
                 percent(classes.lines, population));
    if (classes.lines == 0)
        return;
    std::string levels;
    for (std::size_t level{0}; level < kLevelCount; ++level)
        if (classes.by_level[level] != 0)
            levels += std::format(" {}={}", insight::to_string(static_cast<LogLevel>(level)),
                                  classes.by_level[level]);
    std::println("    by level word  :{}", levels);
    std::string proxies;
    for (std::size_t proxy{0}; proxy < kProxyCount; ++proxy)
        proxies += std::format(" {}={}", kProxyNames[proxy], classes.by_proxy[proxy]);
    std::println("    by register    :{}", proxies);
    std::println("    terminal       : {} (of which bare = {} — unanchored yet authoritative)",
                 classes.terminal, classes.bare_terminal);
    std::vector<std::pair<std::string, std::uint64_t>> ranked{classes.shapes.begin(),
                                                              classes.shapes.end()};
    std::ranges::sort(
        ranked, [](const auto& lhs, const auto& rhs)
        { return lhs.second != rhs.second ? lhs.second > rhs.second : lhs.first < rhs.first; });
    std::string top;
    for (std::size_t rank{0}; rank < std::min(kTopShapesShown, ranked.size()); ++rank)
        top += std::format(" \"{}\"×{}", ranked[rank].first, ranked[rank].second);
    std::println("    prefix shapes  :{}", top.empty() ? " (index 0 — no prefix)" : top);
}

// Everything a candidate budget N decides, in one block: what stays unread under N, what N reads
// that the 40-byte head does not, and what the head reads that N would not.
void print_cut(const IndexStats& stats, std::size_t budget)
{
    print_classes(std::format("residual beyond N={}", budget), stats.residual_beyond(budget),
                  stats.lines);
    print_classes(std::format("gained at N={} vs the 40-byte head", budget),
                  stats.gained_vs_byte_head(budget), stats.lines);
    print_classes(std::format("lost at N={} vs the 40-byte head", budget),
                  stats.lost_vs_byte_head(budget), stats.lines);
}

void print_budget_table(const IndexStats& stats)
{
    const std::uint64_t byte_head{stats.byte_head_total()};
    std::println("  40-byte head, REPLACED (stripped bytes): reached {} of {} ({:.4f}%), missed {}",
                 byte_head, stats.lines, percent(byte_head, stats.lines),
                 stats.lines - byte_head);
    std::println("  40-byte head on the raw bytes          : reached {} of {} ({:.4f}%)",
                 stats.head_raw_total(), stats.lines, percent(stats.head_raw_total(), stats.lines));
    std::println("  {:>4} {:>11} {:>9} {:>11} {:>14} {:>13}", "N", "covered", "cum%", "missed",
                 "gained-vs-40B", "lost-vs-40B");
    for (const std::size_t budget : kCandidateBudgets)
    {
        const std::uint64_t covered{stats.covered_by(budget)};
        std::println("  {:>4} {:>11} {:>9.4f} {:>11} {:>14} {:>13}", budget, covered,
                     percent(covered, stats.lines), stats.lines - covered,
                     stats.gained_vs_byte_head(budget).lines,
                     stats.lost_vs_byte_head(budget).lines);
    }
    std::println("  coverage cuts: N(99%)={}  N(99.9%)={}  N(100%)={}",
                 stats.budget_covering(kCoverage99), stats.budget_covering(kCoverage999),
                 stats.max_index() + 1);
}

void print_root(const RootReport& report)
{
    std::println("");
    std::println("=== {} ===", report.label);
    std::println("population   : {} *.log files, {} lines, {} non-empty, {} declined by the "
                 "pipeline (blank after the peel) — under {} (recursive, sorted walk)",
                 report.files, report.lines, report.nonempty, report.declined, report.dir.string());
    std::string routed;
    for (std::size_t format{0}; format < kFormatCount; ++format)
        if (report.routed[format] != 0)
            routed += std::format(" {}={}", insight::to_string(static_cast<LogFormat>(format)),
                                  report.routed[format]);
    std::println("routed       :{}", routed.empty() ? " (nothing)" : routed);
    std::println("door model   : modelled={} (RawText, Rfc3339Text)  unmodelled={} (index on the "
                 "whole stripped line — an UPPER BOUND)",
                 report.modelled, report.unmodelled);
    std::string by_door;
    for (std::size_t format{0}; format < kFormatCount; ++format)
        if (report.disagree_by_door[format] != 0)
            by_door += std::format(" {}={}", insight::to_string(static_cast<LogFormat>(format)),
                                   report.disagree_by_door[format]);
    std::println("model control: infer_leading_log_level(remainder) == pipeline level on {} of {} "
                 "modelled lines ({:.4f}%); disagree {}{}; declared-level (outside the control) {}",
                 report.control_agree, report.control_agree + report.control_disagree,
                 percent(report.control_agree, report.control_agree + report.control_disagree),
                 report.control_disagree, by_door.empty() ? "" : " (by door:" + by_door + ")",
                 report.control_declared);
    std::println("gitlab shift : of the Rfc3339Text lines, {} carry a runner stream tag `NNO ` "
                 "(package-door index = core-door index − 1) and {} a `NNO+` continuation (shift "
                 "not modelled)",
                 report.stream_tag_newline, report.stream_tag_continuation);
    std::println("level-bearing: modelled {} ({:.3f}% of modelled)  unmodelled {} ({:.3f}% of "
                 "unmodelled)",
                 report.modelled_stats.lines, percent(report.modelled_stats.lines, report.modelled),
                 report.unmodelled_stats.lines,
                 percent(report.unmodelled_stats.lines, report.unmodelled));
    std::string levels;
    for (std::size_t level{0}; level < kLevelCount; ++level)
        levels += std::format(" {}={}", insight::to_string(static_cast<LogLevel>(level)),
                              report.pipeline_levels[level]);
    std::println("pipeline level:{}  (every routed event, declared or inferred, all doors — the "
                 "line that moves with Stage 1's budget: diff it between two builds over this root)",
                 levels);

    std::println("--- token index of the leading level word — MODELLED doors ({} lines) ---",
                 report.modelled_stats.lines);
    print_histogram(report.modelled_stats);
    std::println("--- the replaced 40-byte head vs a token budget N — MODELLED doors ---");
    print_budget_table(report.modelled_stats);
    for (const std::size_t budget : kDetailedBudgets)
        print_cut(report.modelled_stats, budget);
    print_classes(
        std::format("residual beyond N(99%)={}",
                    report.modelled_stats.budget_covering(kCoverage99)),
        report.modelled_stats.residual_beyond(report.modelled_stats.budget_covering(kCoverage99)),
        report.modelled_stats.lines);

    if (report.unmodelled_stats.lines != 0)
    {
        std::println("--- token index — UNMODELLED doors ({} lines, whole stripped line: an upper "
                     "bound) ---",
                     report.unmodelled_stats.lines);
        print_histogram(report.unmodelled_stats);
        print_classes(std::format("residual beyond N={}", kPreRegisteredBudget),
                      report.unmodelled_stats.residual_beyond(kPreRegisteredBudget),
                      report.unmodelled_stats.lines);
    }
}

void print_usage(std::string_view program_name)
{
    std::println(stderr, "usage: {} <label>=<corpus-dir> [<label>=<corpus-dir> ...]", program_name);
    std::println(stderr,
                 "  each <corpus-dir> is walked RECURSIVELY (sorted) for *.log files; the report "
                 "prints counts and prefix shapes only, never a line");
}
} // namespace

// A FUNCTION-TRY-BLOCK, as in f13_cardinality_measure: the body prints as it goes, so the
// handler's job is to make a partial report SAY it is partial. The handlers use std::fputs, not
// std::println — a diagnostic that can itself throw would leave this function throwing after all.
int main(int argc, char** argv)
try
{
    const std::span<char*> arguments{argv, static_cast<std::size_t>(argc)};
    if (arguments.size() < 2)
    {
        print_usage(arguments.empty() ? "leading_level_token_index_measure" : arguments[0]);
        return kExitUsage;
    }

    namespace fs = std::filesystem;
    std::vector<RootReport> reports;
    for (std::size_t index{1}; index < arguments.size(); ++index)
    {
        const std::string_view argument{arguments[index]};
        const std::size_t equals{argument.find('=')};
        if (equals == std::string_view::npos || equals == 0 || equals + 1 == argument.size())
        {
            std::println(stderr, "argument must be <label>=<corpus-dir>, got '{}'", argument);
            return kExitUsage;
        }
        RootReport report{.label = std::string{argument.substr(0, equals)},
                          .dir = fs::path{std::string{argument.substr(equals + 1)}}};
        std::error_code dir_error;
        if (!fs::is_directory(report.dir, dir_error))
        {
            std::println(stderr, "not a directory: {}", report.dir.string());
            return kExitUsage;
        }
        reports.push_back(std::move(report));
    }

    if (!run_self_test())
        return kExitSelfTestFailed;
    std::println("baseline head: the REPLACED kLeadingScanHead = {} raw bytes (N98 landed "
                 "kLeadingScanTokens{{8}} in time_utils.cpp; the byte figure is quoted, the "
                 "constant no longer exists)",
                 kReplacedByteHead);

    // Generic corpus routing is semantic-unaware — a degenerate (zero-package) composition.
    // `composed` precedes every Tokenizer so it outlives the const-ref each one holds.
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose({})};
    ArenaAllocator arena{kArenaBytes};

    std::uint64_t population{0};
    for (RootReport& report : reports)
    {
        std::vector<fs::path> files;
        for (const auto& entry : fs::recursive_directory_iterator{report.dir})
            if (entry.is_regular_file() && entry.path().extension() == ".log")
                files.push_back(entry.path());
        std::ranges::sort(files); // deterministic order for a fixed tree (order-independent anyway)
        report.files = files.size();
        for (const fs::path& file : files)
            measure_file(file, report, arena, composed);
        population += report.nonempty;
        print_root(report);
    }

    if (population == 0)
    {
        std::println(stderr, "no non-empty line in any *.log file under the given roots");
        return kExitEmptyPopulation;
    }

    std::println("");
    std::println("=== summary (MODELLED doors) ===");
    std::println("  {:<18} {:>11} {:>9} {:>8} {:>5} {:>6} {:>6} {:>9} {:>8} {:>9} {:>8}", "root",
                 "level-bear.", "head<40", "miss<40%", "N99", "N99.9", "N100", "gain@N=7",
                 "lost@N=7", "gain@N=8", "lost@N=8");
    for (const RootReport& report : reports)
    {
        const IndexStats& stats{report.modelled_stats};
        std::println("  {:<18} {:>11} {:>9} {:>8.4f} {:>5} {:>6} {:>6} {:>9} {:>8} {:>9} {:>8}",
                     report.label, stats.lines, stats.byte_head_total(),
                     percent(stats.lines - stats.byte_head_total(), stats.lines),
                     stats.budget_covering(kCoverage99), stats.budget_covering(kCoverage999),
                     stats.max_index() + 1, stats.gained_vs_byte_head(kPreRegisteredBudget).lines,
                     stats.lost_vs_byte_head(kPreRegisteredBudget).lines,
                     stats.gained_vs_byte_head(kPreRegisteredBudget + 1).lines,
                     stats.lost_vs_byte_head(kPreRegisteredBudget + 1).lines);
    }
    return kExitOk;
}
catch (const std::exception& error)
{
    std::fputs("fatal: ", stderr);
    std::fputs(error.what(), stderr);
    std::fputs("\nfatal: the report above is PARTIAL and may not be cited\n", stderr);
    return kExitFatal;
}
catch (...)
{
    std::fputs("fatal: unknown exception\n", stderr);
    std::fputs("fatal: the report above is PARTIAL and may not be cited\n", stderr);
    return kExitFatal;
}
