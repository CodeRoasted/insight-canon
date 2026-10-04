module insight.canon.api;
import insight.canon.internal;
import insight.canon.detail.scan;

namespace insight::tokenization
{

std::optional<std::string_view> complete_shell_core(std::string_view token) noexcept
{
    if (token.size() < kMinShellTokenLen || !is_wrapper_open(token.front()))
        return std::nullopt;
    const char open{token.front()};
    const char close{wrapper_closer_of(open)};
    std::size_t end{token.size()};
    for (std::size_t taken{0};
         taken < kMaxShellTrailBytes && is_shell_trailing_punct(token[end - 1U]); ++taken)
        --end;
    if (end < kMinShellTokenLen || token[end - 1U] != close)
        return std::nullopt;
    std::string_view core{token};
    core.remove_suffix(token.size() - end + 1U);
    core.remove_prefix(1U);
    if (core.contains(open) || core.contains(close))
        return std::nullopt;
    return core;
}

} // namespace insight::tokenization
