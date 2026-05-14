#pragma once

// Deprecated: use std::expected<T, std::string> directly.
// This header provides a backward-compatibility type alias for existing callers.
#include <expected>
#include <string>

namespace insight
{

template <typename T>
using Result = std::expected<T, std::string>;

} // namespace insight
