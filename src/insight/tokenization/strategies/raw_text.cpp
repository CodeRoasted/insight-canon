// src/1_tokenization/strategies/raw_text.cpp
//
// RawTextStrategy — last-resort catch-all for unstructured text.
//
// Zero-copy: the input is already arena-stable when parse() is invoked, so the
// message body is a subview of it after a pointer-arithmetic left-trim. Drain
// (with its built-in numeric/IP/hex masking) does the templating downstream.

#include "insight/tokenization/strategies/raw_text.hpp"

#include <string_view>

#include <expected>

#include "insight/core/types.hpp"
#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include "insight/utils/time_utils.hpp"

namespace insight::tokenization
{

std::expected<ParsedLine, std::string> RawTextStrategy::parse(std::string_view line,
                                                              ArenaAllocator& /*arena*/) const
{
    // Trim leading ASCII whitespace so indented continuation lines (e.g. Python
    // traceback frames) group with their peers. Pure pointer arithmetic — no copy.
    std::size_t start{0};
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
        ++start;

    ParsedLine parsed;
    parsed.raw_line = line;
    parsed.timestamp = std::nullopt;
    parsed.component = {};
    // Subview of the arena-stable input: no store_string(), no allocation.
    parsed.content = line.substr(start);
    // Even unstructured lines usually lead with a level / marker (ERROR, WARN,
    // FAILED, ##[error]); recover it so the dominant-level signal survives into
    // MetaLog for downstream severity-aware ranking (Sift) and detection (Eidos).
    parsed.level = utils::infer_leading_log_level(parsed.content);
    return parsed;
}

LogFormat RawTextStrategy::format() const noexcept
{
    return LogFormat::RawText;
}

double RawTextStrategy::confidence(std::string_view /*line*/) const noexcept
{
    // Always 0.0: never wins the structured majority vote, and never latches the
    // sticky fast-path. Reached only via FormatDetector's explicit fallback.
    return 0.0;
}

} // namespace insight::tokenization
