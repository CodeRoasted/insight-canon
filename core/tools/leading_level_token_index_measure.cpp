// refs: ADR-16.D7, ADR-16.D8, ADR-21.D4, DN-18.D1, ADR-7.D3, SRC-SP-1
// invariant: prints counts, prefix shape LETTERS and the matched level word — never any other
// byte of a corpus line.
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
// note: quotes the RETIRED kLeadingScanHead{40}; no such constant is in the tree.
constexpr std::size_t kReplacedByteHead{40};
constexpr std::size_t kHistogramWidth{24};
constexpr std::size_t kPrefixShapeMaxTokens{8};
constexpr std::size_t kTopShapesShown{6};
constexpr std::size_t kFormatCount{static_cast<std::size_t>(LogFormat::Unknown) + 1U};
constexpr std::size_t kLevelCount{static_cast<std::size_t>(LogLevel::Unknown) + 1U};
// refs: DN-54.D22, ADR-16.D7
constexpr std::array<std::size_t, 10> kCandidateBudgets{2, 3, 4, 5, 6, 7, 8, 10, 12, 16};
constexpr std::array<std::size_t, 4> kDetailedBudgets{7, 8, 12, 16};
constexpr std::size_t kPreRegisteredBudget{7};
// note: hand-copied — both are function-local there, so they can drift in silence.
// refs: F-SRC-insight-canon:time_utils.cpp:infer_leading_log_level
constexpr std::size_t kLandedBudget{8};
constexpr std::size_t kQuotedCueHead{128};
static_assert(kLandedBudget == kPreRegisteredBudget + 1);
constexpr std::size_t kNestedBandEnd{16};
constexpr std::size_t kNestedBandCount{2};
constexpr std::array<std::string_view, kNestedBandCount> kNestedBandNames{"index 8-15",
                                                                          "index 16+"};
constexpr double kPercent{100.0};
constexpr double kCoverage99{0.99};
constexpr double kCoverage999{0.999};

constexpr int kExitOk{0};
constexpr int kExitEmptyPopulation{1};
constexpr int kExitUsage{2};
constexpr int kExitSelfTestFailed{3};
constexpr int kExitFatal{4};

// note: a two-neighbouring-bytes PROXY, never canon's is_verdict_anchored kind-slot walk.
enum class RegisterProxy : std::uint8_t
{
    Bracketed,
    ColonAfter,
    Caps,
    Bare,
};
constexpr std::size_t kProxyCount{4};
constexpr std::array<std::string_view, kProxyCount> kProxyNames{"bracketed", "colon", "caps",
                                                                "bare"};

// invariant: a file with no row is Undeclared — never defaulted into a bucket.
enum class Outcome : std::uint8_t
{
    Passed,
    Failed,
    Unstable,
    Other,
    Unrecognised,
    Undeclared,
};
constexpr std::size_t kOutcomeCount{6};
constexpr std::array<std::string_view, kOutcomeCount> kOutcomeNames{
    "passed", "failed", "unstable", "other", "unrecognised", "undeclared"};

[[nodiscard]] Outcome outcome_of(std::string_view word) noexcept
{
    if (word == "success" || word == "SUCCESS")
        return Outcome::Passed;
    if (word == "failure" || word == "FAILURE" || word == "failed")
        return Outcome::Failed;
    if (word == "UNSTABLE")
        return Outcome::Unstable;
    if (word == "cancelled" || word == "canceled" || word == "skipped" || word == "ABORTED")
        return Outcome::Other;
    return Outcome::Unrecognised;
}

// invariant: `word_by_path` keys are the path relative to the root, '/'-separated.
struct OutcomeTable
{
    std::map<std::string, std::string> word_by_path;
    std::size_t duplicate_rows{0};
};

// pre: every row is `<path>\t<word>` — exactly one tab, both fields non-empty.
// post: a malformed row is returned as an error, never skipped.
[[nodiscard]] std::expected<OutcomeTable, std::string>
read_outcome_table(const std::filesystem::path& tsv)
{
    std::ifstream input{tsv};
    if (!input)
        return std::unexpected(std::format("cannot open outcomes file {}", tsv.string()));
    OutcomeTable table;
    std::string row;
    std::size_t row_number{0};
    while (std::getline(input, row))
    {
        ++row_number;
        if (!row.empty() && row.back() == '\r')
            row.pop_back();
        const std::size_t tab{row.find('\t')};
        if (tab == std::string::npos || tab == 0 || tab + 1 == row.size() ||
            row.find('\t', tab + 1) != std::string::npos)
            return std::unexpected(std::format(
                "{}:{}: expected exactly one `<path>\\t<word>` row with both fields non-empty",
                tsv.string(), row_number));
        const auto [position,
                    inserted]{table.word_by_path.emplace(row.substr(0, tab), row.substr(tab + 1))};
        if (!inserted)
            ++table.duplicate_rows;
    }
    return table;
}

enum class DoorModel : std::uint8_t
{
    RawText,
    Rfc3339Text,
    Unmodelled,
};

// invariant: `byte_start` is an offset within the SCANNED bytes, never the raw line.
struct LevelHit
{
    std::size_t token_index{0};
    std::size_t byte_start{0};
    std::size_t token_size{0};
    LogLevel level{LogLevel::Unknown};
    RegisterProxy proxy{RegisterProxy::Bare};
    bool terminal{false};
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
    constexpr std::size_t kDateWidth{10};
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

// post: the first token of `scanned` that parse_log_level accepts, with its index.
// note: for_each_token's substr cannot throw here (begin <= size), so the body is total.
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

// invariant: matches sv_take_token's whitespace set — space and tab only.
[[nodiscard]] constexpr bool is_strategy_ws(char chr) noexcept
{
    return chr == ' ' || chr == '\t';
}

// invariant: models raw_text.cpp's door — trim leading space and tab, then scan.
// refs: F-SRC-insight-canon:raw_text.cpp
// note: substr(pos) with pos <= size cannot throw, so the noexcept body is total.
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] std::string_view raw_text_remainder(std::string_view line) noexcept
{
    std::size_t start{0};
    while (start < line.size() && is_strategy_ws(line[start]))
        ++start;
    return line.substr(start);
}

// invariant: models rfc3339_text.cpp's door — skip leading ws, the stamp token, the ws after.
// refs: F-SRC-insight-canon:rfc3339_text.cpp
// note: substr(pos) with pos <= size cannot throw, so the noexcept body is total.
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

// note: counted so a stamped GitLab line's package-door index reads as core-door minus one.
// invariant: a Continuation tag glues to the next word, so its shift is NOT one token.
// refs: F-SRC-insight-canon:gitlab_strategy.cpp:kTransportPrefixWidth
enum class StreamTag : std::uint8_t
{
    None,
    NewLine,
    Continuation,
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

struct Classes
{
    std::uint64_t lines{0};
    std::array<std::uint64_t, kLevelCount> by_level{};
    std::array<std::uint64_t, kProxyCount> by_proxy{};
    std::uint64_t terminal{0};
    std::uint64_t bare_terminal{0};
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

// invariant: `in_head` and `beyond_head` split on whether the word starts inside the replaced
// 40-byte head, on the stripped bytes.
struct IndexStats
{
    std::vector<Classes> in_head;
    std::vector<Classes> beyond_head;
    std::vector<std::uint64_t> in_head_raw;
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
    // post: read by the replaced 40-byte head and NOT by this budget: index >= budget, byte < 40.
    [[nodiscard]] Classes lost_vs_byte_head(std::size_t budget) const
    {
        Classes lost;
        for (std::size_t index{budget}; index < in_head.size(); ++index)
            lost.merge(in_head[index]);
        return lost;
    }
    // post: read by this budget and NOT by the replaced 40-byte head: index < budget, byte >= 40.
    [[nodiscard]] Classes gained_vs_byte_head(std::size_t budget) const
    {
        Classes gained;
        for (std::size_t index{0}; index < std::min(budget, beyond_head.size()); ++index)
            gained.merge(beyond_head[index]);
        return gained;
    }
    // post: what no budget of this size reads: index >= budget, either side of the head.
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
    // post: the smallest budget whose coverage reaches `fraction`; 0 on an empty population.
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

[[nodiscard]] constexpr bool is_error_class(LogLevel level) noexcept
{
    return level == LogLevel::Error || level == LogLevel::Fatal;
}

// post: the SHIPPED verdict for `scanned` with the hit's bytes blanked to spaces.
// note: spaces are a delimiter run, so every other token keeps its index and neighbours.
[[nodiscard]] LogLevel level_with_word_blanked(std::string_view scanned, const LevelHit& hit)
{
    std::string blanked{scanned};
    std::fill_n(blanked.begin() + static_cast<std::ptrdiff_t>(hit.byte_start), hit.token_size, ' ');
    return insight::utils::infer_leading_log_level(blanked).value();
}

// refs: ADR-21.D4, DN-54.D23
struct NestedLine
{
    LogLevel word{LogLevel::Unknown};
    RegisterProxy proxy{RegisterProxy::Bare};
    bool in_cue_head{false};
    bool in_cue_head_raw{false};
    LogLevel pipeline{LogLevel::Unknown};
    LogLevel stable{LogLevel::Unknown};
    bool promoted{false};
    bool promoted_by_word{false};
    std::string_view lexeme{};
    // invariant: the product's own predicate, never a re-listing — a local copy goes stale in
    // silence the day a word is added.
    // refs: DN-37.D20
    bool in_stage2_lexicon{false};
    // invariant: meaningful only where `in_stage2_lexicon` holds — the kernel is reached only
    // after a lexicon match.
    bool verdict_anchored{false};
};

// invariant: the three classes are disjoint BY THE ORDER OF THE TESTS — head, then lexicon, then
// anchor.
// refs: DN-54.D23, ADR-16.D8, ADR-16.D7, SRC-D-OUT-4c
enum class UnreadClass : std::uint8_t
{
    R1,
    R2,
    R3,
    NotInPopulation
};
constexpr std::size_t kUnreadClassCount{3};
constexpr std::array<std::string_view, kUnreadClassCount> kUnreadClassNames{
    "R1 not-in-lexicon", "R2 lexicon-declined", "R3 past-cue-head"};

// post: NotInPopulation unless the word is error-class and the pipeline did not promote.
[[nodiscard]] UnreadClass unread_class_of(const NestedLine& line) noexcept
{
    if (!is_error_class(line.word) || line.promoted)
        return UnreadClass::NotInPopulation;
    if (!line.in_cue_head)
        return UnreadClass::R3;
    return line.in_stage2_lexicon ? UnreadClass::R2 : UnreadClass::R1;
}

// post: an ASCII-lowercased histogram KEY — presentation, never a predicate input.
// note: is_failure_lexicon_word is itself case-insensitive, so the fold changes no answer.
[[nodiscard]] std::string casefold(std::string_view token)
{
    std::string folded{token};
    for (char& chr : folded)
        if (chr >= 'A' && chr <= 'Z')
            chr = static_cast<char>(chr + ('a' - 'A'));
    return folded;
}

[[nodiscard]] NestedLine classify_nested(std::string_view stripped_scan, std::string_view raw_scan,
                                         const LevelHit& hit,
                                         const std::optional<LevelHit>& raw_hit, LogLevel pipeline)
{
    // assert: `lexeme` is a sub-view of `stripped_scan` — is_verdict_anchored recovers the
    // surrounding bytes by pointer arithmetic.
    const std::string_view lexeme{stripped_scan.substr(hit.byte_start, hit.token_size)};
    NestedLine line{.word = hit.level,
                    .proxy = hit.proxy,
                    .in_cue_head = hit.byte_start < kQuotedCueHead,
                    .in_cue_head_raw = raw_hit.has_value() && raw_hit->byte_start < kQuotedCueHead,
                    .pipeline = pipeline,
                    .stable = insight::utils::infer_leading_log_level(raw_scan).value(),
                    .promoted = is_error_class(pipeline),
                    .promoted_by_word = false,
                    .lexeme = lexeme,
                    .in_stage2_lexicon = insight::utils::detail::is_failure_lexicon_word(lexeme),
                    .verdict_anchored =
                        insight::utils::detail::is_verdict_anchored(stripped_scan, lexeme)};
    if (line.promoted)
        line.promoted_by_word = !is_error_class(level_with_word_blanked(stripped_scan, hit));
    return line;
}

struct NestedResidual
{
    std::uint64_t lines{0};
    std::array<std::uint64_t, kLevelCount> by_word{};
    std::array<std::uint64_t, kOutcomeCount> lines_by_outcome{};
    std::uint64_t error_class{0};
    std::uint64_t error_in_head{0};
    std::uint64_t error_in_head_raw{0};
    std::array<std::uint64_t, kLevelCount> pipeline_of_error_in_head{};
    // refs: SRC-D-OUT-4c
    std::array<std::uint64_t, kProxyCount> promoted_by_proxy{};
    std::array<std::uint64_t, kProxyCount> unpromoted_by_proxy{};
    std::uint64_t promoted{0};
    std::uint64_t promoted_by_word{0};
    std::uint64_t stable_promoted{0};
    std::uint64_t promoted_beyond_head{0};
    std::uint64_t promoted_other_word{0};
    std::array<std::uint64_t, kOutcomeCount> promoted_by_outcome{};
    std::array<std::uint64_t, kOutcomeCount> promoted_by_word_by_outcome{};
    std::array<std::uint64_t, kOutcomeCount> error_unpromoted_by_outcome{};
    std::array<std::uint64_t, kOutcomeCount> files_with_promotion_by_outcome{};
    std::array<std::uint64_t, kOutcomeCount> files_with_word_promotion_by_outcome{};
    // invariant: unread == r1 + r2 + r3 by construction; the report prints the identity rather than
    // assuming it.
    std::uint64_t unread{0};
    std::array<std::uint64_t, kUnreadClassCount> by_unread_class{};
    std::array<std::uint64_t, kOutcomeCount> r1_by_outcome{};
    std::array<std::uint64_t, kOutcomeCount> r2_by_outcome{};
    std::array<std::uint64_t, kOutcomeCount> r3_by_outcome{};
    std::uint64_t r2_register_declined{0};
    std::uint64_t r2_anchored_yet_unread{0};
    // invariant: report-only — no gate, no assertion and no behaviour turns on these counters.
    // refs: ADR-16.D8
    std::uint64_t r3_in_stage2_lexicon{0};
    std::uint64_t r3_verdict_anchored{0};
    std::uint64_t r3_in_lexicon_and_anchored{0};
    std::map<std::string, std::uint64_t> r1_lexemes;
    std::map<std::string, std::uint64_t> r2_lexemes;
    std::map<std::string, std::uint64_t> r3_lexemes;

    void record_unread_class(const NestedLine& line, std::size_t bucket)
    {
        const UnreadClass unread_class{unread_class_of(line)};
        if (unread_class == UnreadClass::NotInPopulation)
            return;
        ++unread;
        ++by_unread_class[static_cast<std::size_t>(unread_class)];
        const std::string lexeme{casefold(line.lexeme)};
        switch (unread_class)
        {
        case UnreadClass::R1:
            ++r1_by_outcome[bucket];
            ++r1_lexemes[lexeme];
            break;
        case UnreadClass::R2:
            ++r2_by_outcome[bucket];
            ++r2_lexemes[lexeme];
            ++(line.verdict_anchored ? r2_anchored_yet_unread : r2_register_declined);
            break;
        case UnreadClass::R3:
            ++r3_by_outcome[bucket];
            ++r3_lexemes[lexeme];
            if (line.in_stage2_lexicon)
                ++r3_in_stage2_lexicon;
            if (line.verdict_anchored)
                ++r3_verdict_anchored;
            if (line.in_stage2_lexicon && line.verdict_anchored)
                ++r3_in_lexicon_and_anchored;
            break;
        case UnreadClass::NotInPopulation:
            break;
        }
    }

    void add(const NestedLine& line, Outcome outcome)
    {
        const std::size_t bucket{static_cast<std::size_t>(outcome)};
        ++lines;
        ++by_word[static_cast<std::size_t>(line.word)];
        ++lines_by_outcome[bucket];
        if (!is_error_class(line.word))
        {
            if (line.promoted)
                ++promoted_other_word;
            return;
        }
        ++error_class;
        if (!line.promoted)
            ++error_unpromoted_by_outcome[bucket];
        // note: recorded BEFORE the early return; R3 lives past it and would read as empty.
        record_unread_class(line, bucket);
        if (line.in_cue_head_raw)
            ++error_in_head_raw;
        if (!line.in_cue_head)
        {
            if (line.promoted)
                ++promoted_beyond_head;
            return;
        }
        ++error_in_head;
        ++pipeline_of_error_in_head[static_cast<std::size_t>(line.pipeline)];
        ++(line.promoted ? promoted_by_proxy
                         : unpromoted_by_proxy)[static_cast<std::size_t>(line.proxy)];
        if (!line.promoted)
            return;
        ++promoted;
        ++promoted_by_outcome[bucket];
        if (is_error_class(line.stable))
            ++stable_promoted;
        if (line.promoted_by_word)
        {
            ++promoted_by_word;
            ++promoted_by_word_by_outcome[bucket];
        }
    }
};

struct NestedFileFlags
{
    bool promoted{false};
    bool promoted_by_word{false};
};

struct RootReport
{
    std::string label;
    std::filesystem::path dir;
    std::optional<std::filesystem::path> outcomes_path;
    OutcomeTable outcomes;
    std::uint64_t files_joined{0};
    std::map<std::string, std::uint64_t> outcome_words;
    std::array<std::uint64_t, kOutcomeCount> files_by_outcome{};
    std::array<NestedResidual, kNestedBandCount> nested{};
    std::size_t files{0};
    std::uint64_t lines{0};
    std::uint64_t nonempty{0};
    std::uint64_t declined{0};
    std::array<std::uint64_t, kFormatCount> routed{};
    // note: diff it across two builds over one root: the count-grain classification delta.
    // refs: DN-54.D10
    std::array<std::uint64_t, kLevelCount> pipeline_levels{};
    std::uint64_t modelled{0};
    std::uint64_t unmodelled{0};
    std::uint64_t control_agree{0};
    std::uint64_t control_disagree{0};
    std::uint64_t control_declared{0};
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

// refs: ADR-16.D7
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

struct NestedSelfTestRow
{
    std::string_view name;
    std::string_view scanned;
    bool class_member;
    bool error_class;
    bool in_cue_head;
    LogLevel inferred;
    bool promoted_by_word;
    // invariant: every row pre-registers its unread class, including the rows in NO class.
    UnreadClass unread_class;
    bool in_stage2_lexicon;
    // invariant: the anchor answer is pre-registered per row, never merely printed beside it.
    bool verdict_anchored;
};

[[nodiscard]] constexpr std::string_view unread_class_name(UnreadClass unread_class) noexcept
{
    return unread_class == UnreadClass::NotInPopulation
               ? std::string_view{"(not in the unread population)"}
               : kUnreadClassNames[static_cast<std::size_t>(unread_class)];
}

[[nodiscard]] bool run_nested_self_test()
{
    static const std::string kFarNested{"[" + std::string(100, 'a') +
                                        "] [runner-7] [job-42] [step-3] [attempt-1] May api-1 "
                                        "kernel: ERROR: worker died"};
    static const std::string kFarNestedInLexicon{"[" + std::string(100, 'a') +
                                                 "] [runner-7] [job-42] [step-3] [attempt-1] May "
                                                 "api-1 kernel: error: worker died"};
    const std::array<NestedSelfTestRow, 9> rows{{
        {.name = "error-class nested word at index 9, inside the cue head: promoted BY the word",
         .scanned = "[2026-05-29 10:00:00] [runner-7] [job-42] May api-1 kernel: ERROR: worker "
                    "died",
         .class_member = true,
         .error_class = true,
         .in_cue_head = true,
         .inferred = LogLevel::Error,
         .promoted_by_word = true,
         .unread_class = UnreadClass::NotInPopulation,
         .in_stage2_lexicon = true,
         .verdict_anchored = true},
        {.name = "info-class nested word at index 9: the residual, not promoted",
         .scanned = "[2026-05-29 10:00:00] [runner-7] [job-42] May api-1 kernel: INFO: worker "
                    "restarted",
         .class_member = true,
         .error_class = false,
         .in_cue_head = true,
         .inferred = LogLevel::Unknown,
         .promoted_by_word = false,
         .unread_class = UnreadClass::NotInPopulation,
         .in_stage2_lexicon = false,
         .verdict_anchored = true},
        {.name = "error-class nested word at index 8 starting past byte 128: outside the cue "
                 "head, not promoted",
         .scanned = kFarNested,
         .class_member = true,
         .error_class = true,
         .in_cue_head = false,
         .inferred = LogLevel::Unknown,
         .promoted_by_word = false,
         .unread_class = UnreadClass::R3,
         .in_stage2_lexicon = true,
         .verdict_anchored = true},
        {.name = "error-class nested word beside a `failed` cue: promoted, NOT by the word",
         .scanned = "[2026-05-29 10:00:00] [runner-7] [job-42] May api-1 kernel: ERROR: build "
                    "failed",
         .class_member = true,
         .error_class = true,
         .in_cue_head = true,
         .inferred = LogLevel::Error,
         .promoted_by_word = false,
         .unread_class = UnreadClass::NotInPopulation,
         .in_stage2_lexicon = true,
         .verdict_anchored = true},
        {.name = "bare nested word at index 10: prose-shaped, outside the class",
         .scanned = "the job on the runner in the pool hit an error while syncing",
         .class_member = false,
         .error_class = true,
         .in_cue_head = true,
         .inferred = LogLevel::Unknown,
         .promoted_by_word = false,
         .unread_class = UnreadClass::NotInPopulation,
         .in_stage2_lexicon = true,
         .verdict_anchored = false},
        {.name = "lowercase colon-anchored `error:` at index 9: in the class, in the head, and "
                 "DECLINED by the cue path (the kind slot is broken by the bare outer tokens)",
         .scanned = "[2026-05-29 10:00:00] [runner-7] [job-42] May api-1 kernel: error: worker "
                    "died",
         .class_member = true,
         .error_class = true,
         .in_cue_head = true,
         .inferred = LogLevel::Unknown,
         .promoted_by_word = false,
         .unread_class = UnreadClass::R2,
         .in_stage2_lexicon = true,
         .verdict_anchored = false},
        {.name = "R2 — lowercase colon-anchored `fatal:` at index 9: in Stage 2's lexicon, inside "
                 "the cue head, and the kind-slot walk declines it",
         .scanned = "[2026-05-29 10:00:00] [runner-7] [job-42] May api-1 kernel: fatal: disk "
                    "offline",
         .class_member = true,
         .error_class = true,
         .in_cue_head = true,
         .inferred = LogLevel::Unknown,
         .promoted_by_word = false,
         .unread_class = UnreadClass::R2,
         .in_stage2_lexicon = true,
         .verdict_anchored = false},
        {.name = "R1 — lowercase colon-anchored `severe:` at index 9, the same position as the "
                 "R2 row: error-class for Stage 1, ABSENT from Stage 2's lexicon",
         .scanned = "[2026-05-29 10:00:00] [runner-7] [job-42] May api-1 kernel: severe: disk "
                    "offline",
         .class_member = true,
         .error_class = true,
         .in_cue_head = true,
         .inferred = LogLevel::Unknown,
         .promoted_by_word = false,
         .unread_class = UnreadClass::R1,
         .in_stage2_lexicon = false,
         .verdict_anchored = false},
        {.name = "R3 — lowercase `error:` starting past byte 128: in Stage 2's lexicon, but the "
                 "byte head is checked FIRST, so the budget owns it",
         .scanned = kFarNestedInLexicon,
         .class_member = true,
         .error_class = true,
         .in_cue_head = false,
         .inferred = LogLevel::Unknown,
         .promoted_by_word = false,
         .unread_class = UnreadClass::R3,
         .in_stage2_lexicon = true,
         .verdict_anchored = false},
    }};
    bool all_ok{true};
    for (const NestedSelfTestRow& row : rows)
    {
        const std::optional<LevelHit> hit{first_level_token(row.scanned)};
        if (!hit)
        {
            all_ok = false;
            std::println("  [FAIL] {}: got NO level token", row.name);
            continue;
        }
        const bool member{hit->token_index >= kLandedBudget && hit->proxy != RegisterProxy::Bare};
        const NestedLine line{
            classify_nested(row.scanned, row.scanned, *hit, hit,
                            insight::utils::infer_leading_log_level(row.scanned).value())};
        // assert: the walk's own composition — the membership gate runs before the partition.
        const UnreadClass unread_class{member ? unread_class_of(line)
                                              : UnreadClass::NotInPopulation};
        const bool row_ok{
            member == row.class_member && is_error_class(line.word) == row.error_class &&
            line.in_cue_head == row.in_cue_head && line.pipeline == row.inferred &&
            line.promoted_by_word == row.promoted_by_word &&
            line.in_stage2_lexicon == row.in_stage2_lexicon &&
            line.verdict_anchored == row.verdict_anchored && unread_class == row.unread_class};
        all_ok = all_ok && row_ok;
        std::println("  [{}] {}: expected member={} error-class={} in-head={} level={} by-word={} "
                     "lexicon={} anchored={} class={}  got member={} error-class={} in-head={} "
                     "level={} by-word={} lexicon={} anchored={} class={} (lexeme='{}' idx={} "
                     "byte={})",
                     row_ok ? "ok" : "FAIL", row.name, row.class_member, row.error_class,
                     row.in_cue_head, insight::to_string(row.inferred), row.promoted_by_word,
                     row.in_stage2_lexicon, row.verdict_anchored,
                     unread_class_name(row.unread_class), member, is_error_class(line.word),
                     line.in_cue_head, insight::to_string(line.pipeline), line.promoted_by_word,
                     line.in_stage2_lexicon, line.verdict_anchored, unread_class_name(unread_class),
                     casefold(line.lexeme), hit->token_index, hit->byte_start);
    }
    return all_ok;
}

[[nodiscard]] bool run_outcome_rule_self_test()
{
    constexpr std::array<std::pair<std::string_view, Outcome>, 11> kOutcomeRows{{
        {"success", Outcome::Passed},
        {"SUCCESS", Outcome::Passed},
        {"failure", Outcome::Failed},
        {"FAILURE", Outcome::Failed},
        {"failed", Outcome::Failed},
        {"UNSTABLE", Outcome::Unstable},
        {"cancelled", Outcome::Other},
        {"canceled", Outcome::Other},
        {"skipped", Outcome::Other},
        {"ABORTED", Outcome::Other},
        {"neutral", Outcome::Unrecognised},
    }};
    bool all_ok{true};
    for (const auto& [word, bucket] : kOutcomeRows)
        all_ok = all_ok && outcome_of(word) == bucket;
    std::println("  [{}] outcome rule: the three producers' verdict words map to their buckets, an "
                 "unknown word to `unrecognised`",
                 all_ok ? "ok" : "FAIL");
    return all_ok;
}

[[nodiscard]] bool run_predicate_self_test()
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
    return all_ok;
}

// invariant: every leg runs unconditionally and BEFORE the walk; any disagreement refuses the run,
// and every row prints before the one verdict.
// note: a zero from an instrument never shown to see the phenomenon is not a measurement.
[[nodiscard]] bool run_self_test()
{
    const bool predicate_ok{run_predicate_self_test()};
    const bool nested_ok{run_nested_self_test()};
    const bool outcome_rule_ok{run_outcome_rule_self_test()};
    const bool all_ok{predicate_ok && nested_ok && outcome_rule_ok};
    std::println("self-test: {}", all_ok ? "PASS" : "FAIL — the run is refused");
    return all_ok;
}

void record_nested_line(RootReport& report, Outcome file_outcome, std::string_view stripped_scan,
                        std::string_view raw_scan, const LevelHit& hit,
                        const std::optional<LevelHit>& raw_hit, LogLevel pipeline_level,
                        std::array<NestedFileFlags, kNestedBandCount>& flags)
{
    if (hit.token_index < kLandedBudget || hit.proxy == RegisterProxy::Bare)
        return;
    const std::size_t band{hit.token_index < kNestedBandEnd ? 0U : 1U};
    const NestedLine nested{classify_nested(stripped_scan, raw_scan, hit, raw_hit, pipeline_level)};
    report.nested[band].add(nested, file_outcome);
    flags[band].promoted = flags[band].promoted || nested.promoted;
    flags[band].promoted_by_word = flags[band].promoted_by_word || nested.promoted_by_word;
}

[[nodiscard]] std::array<NestedFileFlags, kNestedBandCount>
measure_file(const std::filesystem::path& file, Outcome file_outcome, RootReport& report,
             ArenaAllocator& arena, const insight::semantic::ComposedSemantics& composed)
{
    std::array<NestedFileFlags, kNestedBandCount> flags{};
    // note: a fresh Tokenizer per file — the format latch is per stream, and a file is one.
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
        // assert: the reset below kills every view the event held; only values are read after it.
        arena.reset();

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
        const bool in_byte_head_raw{raw_hit.has_value() && raw_hit->byte_start < kReplacedByteHead};
        IndexStats& stats{door == DoorModel::Unmodelled ? report.unmodelled_stats
                                                        : report.modelled_stats};
        stats.record(*hit, in_byte_head, in_byte_head_raw);
        if (door != DoorModel::Unmodelled)
            record_nested_line(report, file_outcome, stripped_scan, raw_scan, *hit, raw_hit,
                               pipeline_level, flags);
    }
    return flags;
}

void walk_root(RootReport& report, ArenaAllocator& arena,
               const insight::semantic::ComposedSemantics& composed)
{
    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator{report.dir})
        if (entry.is_regular_file() && entry.path().extension() == ".log")
            files.push_back(entry.path());
    // note: sorted for a fixed tree; the counts are order-independent anyway.
    std::ranges::sort(files);
    report.files = files.size();
    for (const fs::path& file : files)
    {
        Outcome file_outcome{Outcome::Undeclared};
        if (const auto row{
                report.outcomes.word_by_path.find(fs::relative(file, report.dir).generic_string())};
            row != report.outcomes.word_by_path.end())
        {
            ++report.files_joined;
            ++report.outcome_words[row->second];
            file_outcome = outcome_of(row->second);
        }
        const std::size_t bucket{static_cast<std::size_t>(file_outcome)};
        ++report.files_by_outcome[bucket];
        const std::array<NestedFileFlags, kNestedBandCount> flags{
            measure_file(file, file_outcome, report, arena, composed)};
        for (std::size_t band{0}; band < kNestedBandCount; ++band)
        {
            report.nested[band].files_with_promotion_by_outcome[bucket] +=
                flags[band].promoted ? 1U : 0U;
            report.nested[band].files_with_word_promotion_by_outcome[bucket] +=
                flags[band].promoted_by_word ? 1U : 0U;
        }
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
                 byte_head, stats.lines, percent(byte_head, stats.lines), stats.lines - byte_head);
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

[[nodiscard]] std::string outcome_cells(const std::array<std::uint64_t, kOutcomeCount>& counts)
{
    std::string cells;
    for (std::size_t bucket{0}; bucket < kOutcomeCount; ++bucket)
        cells += std::format(" {}={}", kOutcomeNames[bucket], counts[bucket]);
    return cells;
}

// post: cells in the map's own sorted order, so two runs print identical bytes.
// note: prints `none` rather than an empty tail — absent and unprinted must not read alike.
[[nodiscard]] std::string lexeme_cells(const std::map<std::string, std::uint64_t>& counts)
{
    if (counts.empty())
        return " none";
    std::string cells;
    for (const auto& [lexeme, count] : counts)
        cells += std::format(" {}={}", lexeme, count);
    return cells;
}

void print_unread_partition(const NestedResidual& nested)
{
    const std::uint64_t r1{nested.by_unread_class[static_cast<std::size_t>(UnreadClass::R1)]};
    const std::uint64_t r2{nested.by_unread_class[static_cast<std::size_t>(UnreadClass::R2)]};
    const std::uint64_t r3{nested.by_unread_class[static_cast<std::size_t>(UnreadClass::R3)]};
    std::println("    -- DN-54.D23: the UNREAD population (error-class inner word, line NOT "
                 "Error/Fatal), partitioned by WHICH of Stage 2's three conditions fails --");
    std::println("      unread total : {}   (identity r1+r2+r3 == unread: {})", nested.unread,
                 (r1 + r2 + r3 == nested.unread) ? "holds" : "BROKEN — do not cite this run");
    std::println("      R3 past the {}-byte cue head, condition (a), checked FIRST : {} ({:.2f}%)"
                 "  — a BUDGET, already owned by ADR-16.D7; by outcome:{}",
                 kQuotedCueHead, r3, percent(r3, nested.unread),
                 outcome_cells(nested.r3_by_outcome));
    std::println("      R1 not in Stage 2's lexicon, condition (b)                  : {} ({:.2f}%)"
                 "  — reaches NO register: declined by nothing, invisible; by outcome:{}",
                 r1, percent(r1, nested.unread), outcome_cells(nested.r1_by_outcome));
    std::println("      R2 in the lexicon, still unread, condition (c)              : {} ({:.2f}%)"
                 "  — the REGISTER RULE's population (SRC-D-OUT-4c); by outcome:{}",
                 r2, percent(r2, nested.unread), outcome_cells(nested.r2_by_outcome));
    std::println("        of the {} R2 — kind-slot walk REFUSED the anchor: {}; verdict-anchored "
                 "yet still unread (a count / NOTE register, or a leading pass glyph — NOT the "
                 "register rule): {}",
                 r2, nested.r2_register_declined, nested.r2_anchored_yet_unread);
    std::println("        of the {} R3 — ADR-16.D8's DISCHARGE CONDITION, report-only: in Stage "
                 "2's lexicon (b): {} ({:.2f}%); verdict-anchored (c): {} ({:.2f}%); BOTH — the "
                 "share a longer kKeywordHead would actually read: {} ({:.2f}%). A material share "
                 "means the byte head is buying something and its value returns as a cost "
                 "question; a negligible one closes the last lever on measurement. (b) and (c) are "
                 "never ASKED of R3 by the product path — (a) is checked first — so these are "
                 "counterfactuals, and the CONJUNCTION is the figure the condition turns on",
                 r3, nested.r3_in_stage2_lexicon, percent(nested.r3_in_stage2_lexicon, r3),
                 nested.r3_verdict_anchored, percent(nested.r3_verdict_anchored, r3),
                 nested.r3_in_lexicon_and_anchored, percent(nested.r3_in_lexicon_and_anchored, r3));
    std::println("      lexeme histograms (casefolded; membership by the SHIPPED "
                 "kFailureLexicon test, never a re-listing — DN-37.D20)");
    std::println("        R1:{}", lexeme_cells(nested.r1_lexemes));
    std::println("        R2:{}", lexeme_cells(nested.r2_lexemes));
    std::println("        R3:{}", lexeme_cells(nested.r3_lexemes));
}

void print_nested(const RootReport& report)
{
    std::println(
        "--- N112: the NESTED-RECORD residual — a verdict-shaped level word at token index "
        ">= {} on a modelled door — against the run's DECLARED outcome ---",
        kLandedBudget);
    if (report.outcomes_path)
    {
        std::string words;
        for (const auto& [word, count] : report.outcome_words)
            words += std::format(" {}={}", word, count);
        std::println("  outcomes      : {} — {} rows ({} duplicate), joined to {} of {} files; "
                     "files by bucket:{}; raw words as joined:{}",
                     report.outcomes_path->string(), report.outcomes.word_by_path.size(),
                     report.outcomes.duplicate_rows, report.files_joined, report.files,
                     outcome_cells(report.files_by_outcome), words);
    }
    else
        std::println("  outcomes      : none given (add <label>.outcomes=<path.tsv>) — every file "
                     "is undeclared; files by bucket:{}",
                     outcome_cells(report.files_by_outcome));
    for (std::size_t band{0}; band < kNestedBandCount; ++band)
    {
        const NestedResidual& nested{report.nested[band]};
        std::string words;
        for (std::size_t level{0}; level < kLevelCount; ++level)
            if (nested.by_word[level] != 0)
                words += std::format(" {}={}", insight::to_string(static_cast<LogLevel>(level)),
                                     nested.by_word[level]);
        std::println("  band {:<10}: {} lines ({:.4f}% of level-bearing) — inner word:{}; by "
                     "outcome:{}",
                     kNestedBandNames[band], nested.lines,
                     percent(nested.lines, report.modelled_stats.lines), words,
                     outcome_cells(nested.lines_by_outcome));
        if (nested.lines == 0)
            continue;
        std::string pipeline;
        for (std::size_t level{0}; level < kLevelCount; ++level)
            if (nested.pipeline_of_error_in_head[level] != 0)
                pipeline += std::format(" {}={}", insight::to_string(static_cast<LogLevel>(level)),
                                        nested.pipeline_of_error_in_head[level]);
        std::println("    error-class inner word          : {} ({:.4f}% of the band)",
                     nested.error_class, percent(nested.error_class, nested.lines));
        std::println("      starting inside the {}-byte cue head: {} on the stripped remainder "
                     "(parse_line), {} on the raw remainder (parse_stable)",
                     kQuotedCueHead, nested.error_in_head, nested.error_in_head_raw);
        std::println("      of the {} inside (stripped) — pipeline level:{}", nested.error_in_head,
                     pipeline);
        std::println("      PROMOTED (pipeline Error/Fatal)  : {} ({:.4f}% of those inside); "
                     "attributable to the inner word (blanking it drops the verdict): {}; the raw "
                     "remainder reads Error/Fatal on {} of the {}",
                     nested.promoted, percent(nested.promoted, nested.error_in_head),
                     nested.promoted_by_word, nested.stable_promoted, nested.promoted);
        std::println("      error-class OUTSIDE the head yet Error/Fatal (another cue): {}; "
                     "non-error-class inner word yet Error/Fatal: {}",
                     nested.promoted_beyond_head, nested.promoted_other_word);
        std::string promoted_proxies;
        std::string unpromoted_proxies;
        for (std::size_t proxy{0}; proxy < kProxyCount; ++proxy)
        {
            promoted_proxies +=
                std::format(" {}={}", kProxyNames[proxy], nested.promoted_by_proxy[proxy]);
            unpromoted_proxies +=
                std::format(" {}={}", kProxyNames[proxy], nested.unpromoted_by_proxy[proxy]);
        }
        std::println("      by the inner word's register — promoted:{}; declined:{}",
                     promoted_proxies, unpromoted_proxies);
        std::println("    promoted lines, by outcome        :{}",
                     outcome_cells(nested.promoted_by_outcome));
        std::println("    promoted BY THE WORD, by outcome  :{}",
                     outcome_cells(nested.promoted_by_word_by_outcome));
        std::println("    error-class NOT promoted, by outcome:{}  (the recall side: outside the "
                     "head, or inside it and declined)",
                     outcome_cells(nested.error_unpromoted_by_outcome));
        std::println("    files with >= 1 promoted line, by outcome:{}",
                     outcome_cells(nested.files_with_promotion_by_outcome));
        std::println("    files with >= 1 line promoted BY THE WORD, by outcome:{}  (of the "
                     "root's files by bucket:{})",
                     outcome_cells(nested.files_with_word_promotion_by_outcome),
                     outcome_cells(report.files_by_outcome));
        print_unread_partition(nested);
    }
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
    std::println(
        "pipeline level:{}  (every routed event, declared or inferred, all doors — the "
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
    print_nested(report);

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
    std::println(
        stderr,
        "usage: {} <label>=<corpus-dir> [<label>.outcomes=<path.tsv>] [<label>=<corpus-dir> "
        "...]",
        program_name);
    std::println(stderr,
                 "  each <corpus-dir> is walked RECURSIVELY (sorted) for *.log files; the report "
                 "prints counts and prefix shapes only, never a line");
    std::println(stderr,
                 "  <label>.outcomes=<path.tsv>: one `<file path relative to the corpus-dir>\\t"
                 "<outcome word>` row per file, transcribed verbatim from the corpus manifest "
                 "(the header names the transcription per corpus); the label must already have "
                 "its <corpus-dir>. Files with no row count as undeclared; a malformed row is an "
                 "error.");
}
} // namespace

// pre: each root is `<label>=<corpus-dir>`, optionally followed by `<label>.outcomes=<tsv>`.
// post: on failure the error is the exit code to return, after the reason is printed.
[[nodiscard]] std::expected<std::vector<RootReport>, int>
parse_arguments(std::span<char*> arguments)
{
    namespace fs = std::filesystem;
    constexpr std::string_view kOutcomesSuffix{".outcomes"};
    std::vector<RootReport> reports;
    for (std::size_t index{1}; index < arguments.size(); ++index)
    {
        const std::string_view argument{arguments[index]};
        const std::size_t equals{argument.find('=')};
        if (equals == std::string_view::npos || equals == 0 || equals + 1 == argument.size())
        {
            std::println(stderr,
                         "argument must be <label>=<corpus-dir> or <label>.outcomes=<path.tsv>, "
                         "got '{}'",
                         argument);
            return std::unexpected(kExitUsage);
        }
        const std::string_view key{argument.substr(0, equals)};
        const std::string_view value{argument.substr(equals + 1)};
        if (key.ends_with(kOutcomesSuffix) && key.size() > kOutcomesSuffix.size())
        {
            const std::string_view label{key.substr(0, key.size() - kOutcomesSuffix.size())};
            const auto owner{std::ranges::find(reports, label, &RootReport::label)};
            if (owner == reports.end())
            {
                std::println(stderr,
                             "'{}' names no root given before it (give <label>=<dir> first)",
                             argument);
                return std::unexpected(kExitUsage);
            }
            owner->outcomes_path = fs::path{std::string{value}};
            auto table{read_outcome_table(*owner->outcomes_path)};
            if (!table)
            {
                std::println(stderr, "{}", table.error());
                return std::unexpected(kExitUsage);
            }
            owner->outcomes = std::move(*table);
            continue;
        }
        RootReport report{.label = std::string{key}, .dir = fs::path{std::string{value}}};
        std::error_code dir_error;
        if (!fs::is_directory(report.dir, dir_error))
        {
            std::println(stderr, "not a directory: {}", report.dir.string());
            return std::unexpected(kExitUsage);
        }
        reports.push_back(std::move(report));
    }
    return reports;
}

void print_summary(const std::vector<RootReport>& reports)
{
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
}

void print_nested_summary(const std::vector<RootReport>& reports)
{
    std::println("");
    std::println(
        "=== N112 summary — the nested-record residual at {} (MODELLED doors): error-class "
        "inner words, the cue head's promotions, and the run's declared outcome ===",
        kNestedBandNames[0]);
    std::println("  {:<18} {:>7} {:>7} {:>7} {:>8} {:>7} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8}",
                 "root", "band", "err-cls", "in-head", "promoted", "by-word", "pr:pass", "pr:fail",
                 "pr:unst", "pr:other", "bw:pass", "bw:fail", "np:fail");
    for (const RootReport& report : reports)
    {
        const NestedResidual& nested{report.nested[0]};
        const auto& promoted{nested.promoted_by_outcome};
        const auto& by_word{nested.promoted_by_word_by_outcome};
        std::println(
            "  {:<18} {:>7} {:>7} {:>7} {:>8} {:>7} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8}",
            report.label, nested.lines, nested.error_class, nested.error_in_head, nested.promoted,
            nested.promoted_by_word, promoted[static_cast<std::size_t>(Outcome::Passed)],
            promoted[static_cast<std::size_t>(Outcome::Failed)],
            promoted[static_cast<std::size_t>(Outcome::Unstable)],
            promoted[static_cast<std::size_t>(Outcome::Other)],
            by_word[static_cast<std::size_t>(Outcome::Passed)],
            by_word[static_cast<std::size_t>(Outcome::Failed)],
            nested.error_unpromoted_by_outcome[static_cast<std::size_t>(Outcome::Failed)]);
    }
}

// post: the report DECLARES its population and asserts nothing; no gate may depend on it.
int main(int argc, char** argv)
try
{
    const std::span<char*> arguments{argv, static_cast<std::size_t>(argc)};
    if (arguments.size() < 2)
    {
        print_usage(arguments.empty() ? "leading_level_token_index_measure" : arguments[0]);
        return kExitUsage;
    }
    auto parsed{parse_arguments(arguments)};
    if (!parsed)
        return parsed.error();
    std::vector<RootReport>& reports{*parsed};

    if (!run_self_test())
        return kExitSelfTestFailed;
    std::println("baseline head: the REPLACED kLeadingScanHead = {} raw bytes (N98 landed "
                 "kLeadingScanTokens{{8}} in time_utils.cpp; the byte figure is quoted, the "
                 "constant no longer exists)",
                 kReplacedByteHead);

    // note: `composed` precedes every Tokenizer so it outlives the const-ref each one holds.
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose({})};
    ArenaAllocator arena{kArenaBytes};

    std::uint64_t population{0};
    for (RootReport& report : reports)
    {
        walk_root(report, arena, composed);
        population += report.nonempty;
        print_root(report);
    }

    if (population == 0)
    {
        std::println(stderr, "no non-empty line in any *.log file under the given roots");
        return kExitEmptyPopulation;
    }

    print_summary(reports);
    print_nested_summary(reports);
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
