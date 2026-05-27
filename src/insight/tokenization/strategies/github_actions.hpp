#pragma once

#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include <expected>

namespace insight::tokenization
{

// GitHubActionsStrategy — parses GitHub Actions / Azure Pipelines log lines.
//
// Every line is prefixed with an RFC 3339 UTC timestamp at .NET 100-ns
// precision (exactly 7 fractional digits + 'Z'), then a single space, then the
// raw message — which may carry workflow-command annotations
// ("##[error]", "##[warning]", "##[group]", "##[endgroup]", legacy "::error::").
//
// Without this strategy these lines are claimed by SyslogStrategy (they share
// the RFC 3339 prefix), which mis-parses them — eating the first message token
// as a fake hostname and collapsing timestamp-only lines into the empty
// template. CI logs are a primary product input, so GHA is first-class here:
//   - the timestamp is stripped and the full message is templated (real shape
//     preserved, e.g. "CODEROAST_IPC_REPO: <*>");
//   - the level is lifted from any "##[error]"/"##[warning]"/"##[notice]"/
//     "##[debug]" (or legacy "::error::") marker;
//   - a timestamp-only line is a blank line: parse() declines it so it is
//     dropped, never inflating an empty "" template cluster.
class GitHubActionsStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization
