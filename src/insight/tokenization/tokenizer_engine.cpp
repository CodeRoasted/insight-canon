// src/1_tokenization/tokenizer_engine.cpp
//
// Tokenizer: Phase 1 facade that converts raw log lines into CanonicalEvents.
//
// Data flow per line:
//   raw_line
//     →  LogParser::parse_line()  →  ParsedLine { level, timestamp, component,
//                                                  content }   (all arena-stable)
//     →  Drain::match_into_arena(content, arena_)
//                                →  ArenaMatchResult { template_id, template_str,
//                                                      params[], new_cluster }
//     →  CanonicalEvent
//
// Ownership: the arena is external; all string_views in CanonicalEvent point
// into arena-managed memory and are valid until arena.reset() or destruction.

#include "insight/tokenization/tokenizer_engine.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/canonical_event.hpp"
#include "insight/tokenization/drain.hpp"
#include "insight/tokenization/drain_config.hpp"
#include "insight/tokenization/log_parser.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include "insight/utils/logger.hpp"
#include "insight/utils/result.hpp"

namespace insight::tokenization
{

namespace
{

    constexpr std::size_t kProgressLogInterval{1000};

} // namespace

struct Tokenizer::Impl
{
    ArenaAllocator& arena; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members): tokenizer
                           // shares the caller-managed arena for stable string_views.
    LogParser parser;
    Drain drain;
    EventID next_id{0};
    std::size_t produced{0};

    Impl(ArenaAllocator& arena_ref, DrainConfig drain_config)
        : arena{arena_ref}, parser{arena_ref}, drain{drain_config}
    {
    }

    [[nodiscard]] insight::Result<CanonicalEvent> make_event(insight::Result<ParsedLine> parsed)
    {
        if (!parsed)
        {
            INSIGHT_LOG_WARN(logging::tokenizer_logger(), "parse failed: {}", parsed.error());
            return insight::Result<CanonicalEvent>{parsed.error()};
        }
        const ParsedLine& parsed_line = parsed.value();

        const Drain::ArenaMatchResult match{drain.match_into_arena(parsed_line.content, arena)};

        CanonicalEvent event;
        event.id = next_id++;
        event.template_id = match.template_id;
        event.timestamp = parsed_line.timestamp.value_or(Timestamp{});
        event.level = parsed_line.level;
        event.component = parsed_line.component;
        event.template_str = match.template_str;
        event.params = match.params;

        ++produced;

        INSIGHT_LOG_TRACE(logging::tokenizer_logger(), "event: id={} tmpl_id={} params={}",
                          event.id, event.template_id, event.params.size());

        if constexpr (logging::kDebugLogsEnabled)
        {
            if (produced % kProgressLogInterval == 0)
            {
                INSIGHT_LOG_DEBUG(logging::tokenizer_logger(), "progress: events={} clusters={}",
                                  produced, drain.cluster_count());
            }
        }
        return insight::Result<CanonicalEvent>{event};
    }
};

Tokenizer::Tokenizer(ArenaAllocator& arena, DrainConfig drain_config)
    : impl_{std::make_unique<Impl>(arena, drain_config)}
{
    INSIGHT_LOG_INFO(logging::tokenizer_logger(), "tokenizer init");
}

Tokenizer::~Tokenizer() = default;
Tokenizer::Tokenizer(Tokenizer&&) noexcept = default;
Tokenizer& Tokenizer::operator=(Tokenizer&&) noexcept = default;

insight::Result<CanonicalEvent> Tokenizer::process_line(std::string_view raw_line)
{
    return impl_->make_event(impl_->parser.parse_line(raw_line));
}

insight::Result<CanonicalEvent> Tokenizer::process_stable_line(std::string_view stable_line)
{
    return impl_->make_event(impl_->parser.parse_stable(stable_line));
}

std::vector<insight::Result<CanonicalEvent>>
Tokenizer::process_batch(std::span<const std::string_view> lines)
{
    std::vector<insight::Result<CanonicalEvent>> out;
    out.reserve(lines.size());
    for (auto line : lines)
        out.push_back(process_line(line));
    return out;
}

std::size_t Tokenizer::events_produced() const noexcept
{
    return impl_->produced;
}
std::size_t Tokenizer::lines_parsed() const noexcept
{
    return impl_->parser.lines_parsed();
}
std::size_t Tokenizer::cluster_count() const noexcept
{
    return impl_->drain.cluster_count();
}

} // namespace insight::tokenization
