#pragma once

#include <optional>
#include <string_view>

#include "insight/core/types.hpp"

namespace insight::tokenization
{

// Intermediate representation produced by a format strategy.
//
// All string_view fields point into arena-managed storage. They remain
// valid until the owning ArenaAllocator is reset or destroyed.
//
// raw_line   — the original line, copied into the arena by LogParser before
//              the strategy is invoked.
// component  — component / tag extracted by the strategy and stored into the
//              arena via ArenaAllocator::store_string().
// content    — message body fed to the Drain tokeniser, also arena-stored.
struct ParsedLine
{
    std::string_view raw_line;
    std::optional<Timestamp> timestamp;
    LogLevel level{LogLevel::Unknown};
    std::string_view component;
    std::string_view content;
};

} // namespace insight::tokenization
