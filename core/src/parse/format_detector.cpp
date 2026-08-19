module;
#include "utils/log_macros.hpp" // textual macro layer (ADR-3.D4)

module insight.canon.detail.parse;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;             // StrategyFactory
import insight.canon.compose;         // ComposedSemantics
import insight.canon.detail.strategy; // the 20 core representation strategies registered here

// FormatDetector: registers all built-in format strategies and selects the
// best match for a given line or sample batch.
//
// The constructor auto-registers built-in strategies. Additional strategies
// can be injected at runtime via register_strategy().

namespace insight::tokenization
{

namespace
{
    constexpr std::size_t kDateLength{10};
    constexpr std::size_t kShortDateLength{8};
    constexpr std::size_t kMonthFirstDigitIndex{5};
    constexpr std::size_t kMonthSecondDigitIndex{6};
    constexpr std::size_t kDaySeparatorIndex{7};
    constexpr std::size_t kDayFirstDigitIndex{8};
    constexpr std::size_t kDaySecondDigitIndex{9};
    constexpr std::size_t kMinCompactTimestampLength{18};

    struct CandidateList
    {
        static constexpr std::size_t kMaxCandidates{8};
        std::array<LogFormat, kMaxCandidates> formats{};
        std::size_t size{0};

        void add(LogFormat format) noexcept
        {
            if (size >= formats.size())
                return;
            for (std::size_t i = 0; i < size; ++i)
                if (formats[i] == format) // NOLINT
                    return;
            formats[size++] = format; // NOLINT
        }
    };

    // is_digit / is_alpha: the canonical char-class predicates from insight.canon.detail.scan
    // (imported by this module via the parse interface) — not re-declared here (A3 consolidation,
    // single source of truth). Behaviour-identical to the former local copies on every byte.

    [[nodiscard]] std::string_view trim_left(std::string_view line) noexcept
    {
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            line.remove_prefix(1);
        return line;
    }

    [[nodiscard]] bool looks_like_month_prefix(std::string_view line) noexcept
    {
        return line.size() >= 4 && is_alpha(line[0]) && is_alpha(line[1]) && is_alpha(line[2]) &&
               line[3] == ' ';
    }

    [[nodiscard]] bool looks_like_yyyy_mm_dd(std::string_view line) noexcept
    {
        return line.size() >= kDateLength && is_digit(line[0]) && is_digit(line[1]) &&
               is_digit(line[2]) && is_digit(line[3]) && line[4] == '-' &&
               is_digit(line[kMonthFirstDigitIndex]) && is_digit(line[kMonthSecondDigitIndex]) &&
               line[kDaySeparatorIndex] == '-' && is_digit(line[kDayFirstDigitIndex]) &&
               is_digit(line[kDaySecondDigitIndex]);
    }

    [[nodiscard]] bool looks_like_yyyy_slash_mm_slash_dd(std::string_view line) noexcept
    {
        return line.size() >= kDateLength && is_digit(line[0]) && is_digit(line[1]) &&
               is_digit(line[2]) && is_digit(line[3]) && line[4] == '/' &&
               is_digit(line[kMonthFirstDigitIndex]) && is_digit(line[kMonthSecondDigitIndex]) &&
               line[kDaySeparatorIndex] == '/' && is_digit(line[kDayFirstDigitIndex]) &&
               is_digit(line[kDaySecondDigitIndex]);
    }

    [[nodiscard]] bool looks_like_yy_slash_mm_slash_dd(std::string_view line) noexcept
    {
        return line.size() >= kShortDateLength && is_digit(line[0]) && is_digit(line[1]) &&
               line[2] == '/' && is_digit(line[3]) && is_digit(line[4]) &&
               line[kMonthFirstDigitIndex] == '/' && is_digit(line[kMonthSecondDigitIndex]) &&
               is_digit(line[kDaySeparatorIndex]);
    }

    [[nodiscard]] bool looks_like_health_app(std::string_view line) noexcept
    {
        static constexpr std::size_t kCompactDateSeparatorIndex{8};

        if (line.size() < kMinCompactTimestampLength || line[kCompactDateSeparatorIndex] != '-')
            return false;
        for (std::size_t i = 0; i < kShortDateLength; ++i)
            if (!is_digit(line[i]))
                return false;
        return true;
    }

    [[nodiscard]] bool looks_like_android_logcat(std::string_view line) noexcept
    {
        return line.size() >= kMinCompactTimestampLength && is_digit(line[0]) &&
               is_digit(line[1]) && line[2] == '-' && is_digit(line[3]) && is_digit(line[4]) &&
               line[kMonthFirstDigitIndex] == ' ';
    }

    [[nodiscard]] bool looks_like_clf(std::string_view line) noexcept
    {
        return line.contains('[') && line.contains(']') && line.contains('"') &&
               line.contains("HTTP/");
    }

    [[nodiscard]] bool looks_like_hpc(std::string_view line) noexcept
    {
        const auto first_space{line.find(' ')};
        if (first_space == std::string_view::npos || first_space == 0)
            return false;
        for (std::size_t i = 0; i < first_space; ++i)
            if (!is_digit(line[i]))
                return false;
        return line.contains(" node-") || line.contains(" unix.");
    }

    [[nodiscard]] bool contains_kv_marker(std::string_view line) noexcept
    {
        const auto equals_position{line.find('=')};
        if (equals_position == std::string_view::npos || equals_position == 0)
            return false;
        const char before = line[equals_position - 1U];
        return is_alpha(before) || is_digit(before) || before == '_' || before == '-' ||
               before == '.';
    }

    [[nodiscard]] CandidateList candidates_for(std::string_view raw_line) noexcept
    {
        static constexpr std::size_t kTimestampSeparatorIndex{10};

        CandidateList candidates;
        const std::string_view line = trim_left(raw_line);
        if (line.empty())
            return candidates;

        if (line.front() == '{')
        {
            candidates.add(LogFormat::SystemdJournal);
            candidates.add(LogFormat::CloudWatch);
            candidates.add(LogFormat::JSON);
            return candidates;
        }

        if (line.front() == '<')
            candidates.add(LogFormat::RFC5424);

        if (line.front() == '-')
            candidates.add(LogFormat::BGL);

        if (line.front() == '[')
        {
            if (line.size() > 1 && is_alpha(line[1]))
                candidates.add(LogFormat::ApacheError);
            if (line.size() > 3 && is_digit(line[1]) && line.contains('.'))
                candidates.add(LogFormat::Proxifier);
        }

        if (looks_like_month_prefix(line))
            candidates.add(LogFormat::Syslog);

        if (looks_like_yyyy_slash_mm_slash_dd(line))
            candidates.add(LogFormat::NginxError);

        if (looks_like_yy_slash_mm_slash_dd(line))
            candidates.add(LogFormat::SparkHDFS);

        if (looks_like_health_app(line))
            candidates.add(LogFormat::HealthApp);

        if (looks_like_android_logcat(line))
            candidates.add(LogFormat::AndroidLogcat);

        if (looks_like_yyyy_mm_dd(line))
        {
            if (line.size() > kTimestampSeparatorIndex && line[kTimestampSeparatorIndex] == 'T')
            {
                // BOTH core representation candidates for an RFC3339+T line, and they are
                // DISJOINT: Syslog claims it only if the syslog header holds, Rfc3339Text only if
                // it does not (DN-43.D4). Listing both is what makes the split reachable — the
                // detector scores candidates, and a builtin absent from this list is never probed.
                candidates.add(LogFormat::Syslog);
                candidates.add(LogFormat::Rfc3339Text);
            }
            else
            {
                candidates.add(LogFormat::WindowsCBS);
                candidates.add(LogFormat::IISW3C);
                candidates.add(LogFormat::Log4j);
            }
        }

        if (looks_like_clf(line))
            candidates.add(LogFormat::CLF);

        if (looks_like_hpc(line))
            candidates.add(LogFormat::HPC);

        if (contains_kv_marker(line))
            candidates.add(LogFormat::KeyValue);

        return candidates;
    }

    [[nodiscard]] std::size_t format_index(LogFormat format) noexcept
    {
        return static_cast<std::size_t>(format);
    }

} // namespace

FormatDetector::FormatDetector(const insight::semantic::ComposedSemantics& composed)
{
    auto add_builtin = [this](std::unique_ptr<IFormatStrategy> strategy)
    {
        const auto index{format_index(strategy->format())};
        strategies_.push_back(std::move(strategy));
        by_format_.at(index) = strategies_.back().get();
    };

    // The core REPRESENTATION-format strategies (semantic-unaware — how data is represented, not
    // what an ecosystem means). The GitHub-Actions DIALECT strategy is no longer a builtin; it
    // arrives via the composition below (ADR-17).
    add_builtin(std::make_unique<JsonStrategy>());
    add_builtin(std::make_unique<SyslogStrategy>());
    add_builtin(std::make_unique<Rfc3339TextStrategy>());
    add_builtin(std::make_unique<CLFStrategy>());
    add_builtin(std::make_unique<KVStrategy>());
    add_builtin(std::make_unique<HealthAppStrategy>());
    add_builtin(std::make_unique<BGLStrategy>());
    add_builtin(std::make_unique<AndroidLogcatStrategy>());
    add_builtin(std::make_unique<ProxifierStrategy>());
    add_builtin(std::make_unique<ApacheErrorLogStrategy>());
    add_builtin(std::make_unique<WindowsCBSStrategy>());
    add_builtin(std::make_unique<SparkHDFSStrategy>());
    add_builtin(std::make_unique<Log4jStrategy>());
    add_builtin(std::make_unique<HPCStrategy>());
    add_builtin(std::make_unique<NginxErrorStrategy>());
    add_builtin(std::make_unique<CloudWatchStrategy>());
    add_builtin(std::make_unique<IISW3CStrategy>());
    add_builtin(std::make_unique<RFC5424Strategy>());
    add_builtin(std::make_unique<SystemdJournalStrategy>());
    fallback_ = std::make_unique<RawTextStrategy>();

    // Composed DIALECT strategies (ADR-17): each factory the composition carries produces one
    // strategy, registered through the existing injection seam (probed once-per-line via
    // custom_strategies_). Canon core names no dialect: every dialect strategy arrives from a
    // semantic package. The factories are in canonical (package-sorted) order.
    for (const insight::semantic::StrategyFactory factory : composed.strategy_factories())
        register_strategy(factory());

    INSIGHT_LOG_INFO(logging::detector_logger(),
                     "format detector init: {} strategies registered (+ raw-text fallback)",
                     strategies_.size());
}

void FormatDetector::register_strategy(std::unique_ptr<IFormatStrategy> strategy)
{
    INSIGHT_LOG_INFO(logging::detector_logger(), "strategy registered: {}",
                     to_string(strategy->format()));
    const auto index{format_index(strategy->format())};
    strategies_.push_back(std::move(strategy));
    by_format_.at(index) = strategies_.back().get();
    custom_strategies_.push_back(strategies_.back().get());
}

// Returns the strategy with the highest confidence score for the given line.
// O(C + U) where C is the heuristic candidate count and U is custom strategy count.
IFormatStrategy* FormatDetector::detect(std::string_view line) const
{
    IFormatStrategy* best = nullptr;
    double best_score{0.0};
    const CandidateList candidates = candidates_for(line);

    for (std::size_t i = 0; i < candidates.size; ++i)
    {
        IFormatStrategy* strategy = by_format_[format_index(candidates.formats[i])]; // NOLINT
        if (strategy == nullptr)
            continue;
        const double score{strategy->confidence(line)};
        INSIGHT_LOG_TRACE(logging::detector_logger(), "  confidence: {}={:.3f}",
                          to_string(strategy->format()), score);
        if (score > best_score)
        {
            best_score = score;
            best = strategy;
        }
    }

    for (IFormatStrategy* strategy : custom_strategies_)
    {
        bool already_evaluated = false;
        for (std::size_t i = 0; i < candidates.size; ++i)
        {
            if (by_format_[format_index(candidates.formats[i])] == strategy) // NOLINT
            {
                already_evaluated = true;
                break;
            }
        }
        if (already_evaluated)
            continue;
        const double score{strategy->confidence(line)};
        INSIGHT_LOG_TRACE(logging::detector_logger(), "  confidence: {}={:.3f}",
                          to_string(strategy->format()), score);
        if (score > best_score)
        {
            best_score = score;
            best = strategy;
        }
    }

    if (best_score > 0.0)
    {
        INSIGHT_LOG_DEBUG(logging::detector_logger(), "detect: winner={} confidence={:.3f}",
                          to_string(best->format()), best_score);
        return best;
    }

    // No structured strategy matched: template the line as raw text rather than
    // dropping it (empty lines stay dropped). The fallback never enters the
    // sticky fast-path because its confidence is a constant 0.0.
    return trim_left(line).empty() ? nullptr : fallback_.get();
}

// Weighted-sum detection across a sample batch.
// For each line, heuristic candidates and custom strategies contribute scores.
// Returns the strategy with the highest cumulative confidence.
// O((C + U) * N) where N is sample size.
IFormatStrategy* FormatDetector::detect_from_batch(std::span<const std::string_view> sample) const
{
    if (sample.empty() || strategies_.empty())
        return nullptr;

    std::array<double, kFormatSlotCount> scores{};

    for (auto line : sample)
    {
        const CandidateList candidates = candidates_for(line);
        for (std::size_t i = 0; i < candidates.size; ++i)
        {
            const auto index{format_index(candidates.formats[i])};                  // NOLINT
            if (IFormatStrategy* strategy = by_format_[index]; strategy != nullptr) // NOLINT
                scores[index] += strategy->confidence(line);                        // NOLINT
        }
        for (IFormatStrategy* strategy : custom_strategies_)
        {
            bool already_evaluated = false;
            for (std::size_t i = 0; i < candidates.size; ++i)
            {
                if (by_format_[format_index(candidates.formats[i])] == strategy) // NOLINT
                {
                    already_evaluated = true;
                    break;
                }
            }
            if (already_evaluated)
                continue;
            scores[format_index(strategy->format())] += strategy->confidence(line); // NOLINT
        }
    }

    // `auto` (not `auto*`): max_element returns an ITERATOR — a raw pointer on libstdc++/libc++ but
    // a wrapper class on MSVC's STL, so forcing pointer deduction breaks there. *it and
    // std::distance below work for any random-access iterator. NOLINT: clang-tidy's qualified-auto
    // wants `auto*`, which is the very libstdc++-ism that broke MSVC — keep `auto`.
    // NOLINTNEXTLINE(readability-qualified-auto)
    const auto max_score_it{std::ranges::max_element(scores)};
    if (*max_score_it == 0.0)
    {
        // No structured format dominates the sample — fall back to raw text so a
        // batch of unstructured CI output is templated rather than dropped.
        const bool any_content{std::ranges::any_of(sample, [](std::string_view line)
                                                   { return !trim_left(line).empty(); })};
        return any_content ? fallback_.get() : nullptr;
    }

    const auto winning_index{static_cast<std::size_t>(std::distance(scores.begin(), max_score_it))};
    IFormatStrategy* winner = by_format_[winning_index]; // NOLINT
    if (winner == nullptr)
        return nullptr;
    INSIGHT_LOG_DEBUG(logging::detector_logger(),
                      "detect_from_batch: winner={} cumulative_score={:.2f} sample_size={}",
                      to_string(winner->format()), *max_score_it, sample.size());
    return winner;
}

std::span<const std::unique_ptr<IFormatStrategy>> FormatDetector::strategies() const noexcept
{
    return strategies_;
}

} // namespace insight::tokenization
