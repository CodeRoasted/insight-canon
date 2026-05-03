#pragma once
#include <span>
#include <string_view>

#include "insight/core/types.hpp"

namespace insight::tokenization
{

// CanonicalEvent — Phase 1 output. Every string view points into the
// arena that was passed to the Tokenizer; lifetimes are bounded by
// `arena.reset()` or arena destruction.
//
// `params` is a span over an arena-allocated array of string_views. This
// keeps the event a fixed-size POD with zero per-event heap allocations
// on the tokenizer hot path. Downstream consumers iterate or index it
// the same way they would a vector.
//
// `session_key` is an opaque producer-defined session identifier
// (request id, trace id, user session, partition key, etc). When 0
// the event is treated as session-agnostic (the historical behaviour
// before MetaLog spec v0.2.0) and downstream stages keep their fast
// global-stream path. Non-zero values opt in to per-session sequence
// scoping (MetaLog SPEC §14).
struct CanonicalEvent
{
    EventID id{};
    TemplateID template_id{};
    Timestamp timestamp;
    LogLevel level{LogLevel::Unknown};
    std::string_view component;
    std::string_view template_str;            // "Connection from <*> port <*>"
    std::span<const std::string_view> params; // ["192.168.1.1", "22"]
    SessionID session_key{0};                 // 0 = session-agnostic (default)
};

} // namespace insight::tokenization
