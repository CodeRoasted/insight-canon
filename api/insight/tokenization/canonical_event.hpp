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
struct CanonicalEvent
{
    EventID id{};
    TemplateID template_id{};
    Timestamp timestamp;
    LogLevel level{LogLevel::Unknown};
    std::string_view component;
    std::string_view template_str;            // "Connection from <*> port <*>"
    std::span<const std::string_view> params; // ["192.168.1.1", "22"]
    // What this line DOES in the sequence (announced role; F12 StructuralRole
    // registry). Orthogonal to template_id (what the line IS) and to the semantic
    // class of tokens inside it. Consumers: phase alignment + structural surprise
    // (Phase 2/4); None for the vast majority of lines.
    StructuralRole structural_role{StructuralRole::None};
};

} // namespace insight::tokenization
