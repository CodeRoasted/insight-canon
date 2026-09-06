module;

module insight.semantic.github;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;

// refs: ADR-17, ADR-22, ADR-23
// invariant: nothing here DETECTS — the per-line RFC 3339 stamp is GitHub's DELIVERY, declared as
// `api-rfc3339-line-prefix` and unwound by `TransportStack::peel` before canon sees a line.
// refs: ADR-8, F-SRC-insight-canon:test_transport_peel_equivalence_gate.cpp
// note: that frozen gate scores the declared peel over 4 082 logs / 22 490 937 lines
namespace insight::semantic::github
{
namespace
{

    [[nodiscard]] constexpr bool is_digit(char chr) noexcept
    {
        return static_cast<unsigned>(chr) - '0' < 10U;
    }
    [[nodiscard]] constexpr bool is_space(char chr) noexcept
    {
        return chr == ' ' || chr == '\t';
    }

    constexpr unsigned char kEsc{0x1bU};
    inline constexpr std::array<std::string_view, 2> kCommandEchoSgrParams{
        std::string_view{"36;1"}, std::string_view{"1;36"}};
    inline constexpr std::array<std::string_view, 3> kSgrResetParams{
        std::string_view{"0"}, std::string_view{}, std::string_view{"39"}};

    // refs: BIB:determinism_model
    // post: on success `pos` sits past the final `m`; on `nullopt` it is left where it was.
    // note: the suppression below is load-bearing: `params_begin <= cur`, so `substr` cannot throw
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] std::optional<std::string_view> parse_sgr_params(std::string_view line,
                                                                   std::size_t& pos) noexcept
    {
        if (pos + 1U >= line.size() || static_cast<unsigned char>(line[pos]) != kEsc ||
            line[pos + 1U] != '[')
            return std::nullopt;
        const std::size_t params_begin{pos + 2U};
        std::size_t cur{params_begin};
        while (cur < line.size() && (is_digit(line[cur]) || line[cur] == ';'))
            ++cur;
        if (cur >= line.size() || line[cur] != 'm')
            return std::nullopt;
        const std::string_view params{line.substr(params_begin, cur - params_begin)};
        pos = cur + 1U;
        return params;
    }

} // namespace

// refs: SRC-D-PROV-1, BIB:determinism_model
// pre: `line` carries its ANSI intact and its declared transport already peeled — the SGR wrapper
// is the only signal and stage 1 would destroy it.
// post: true iff the whole visible content is ONE command-echo SGR span — open (`36;1`/`1;36`), a
// content run, a reset (`0`/empty/`39`) — with no visible byte outside it.
bool is_echoed_source(std::string_view line) noexcept
{
    std::size_t pos{0};
    const std::optional<std::string_view> open{parse_sgr_params(line, pos)};
    if (!open || std::ranges::find(kCommandEchoSgrParams, *open) == kCommandEchoSgrParams.end())
        return false;
    while (pos < line.size() && static_cast<unsigned char>(line[pos]) != kEsc)
        ++pos;
    if (pos >= line.size())
        return false;
    const std::optional<std::string_view> close{parse_sgr_params(line, pos)};
    if (!close || std::ranges::find(kSgrResetParams, *close) == kSgrResetParams.end())
        return false;
    while (pos < line.size() && (is_space(line[pos]) || line[pos] == '\r' || line[pos] == '\n'))
        ++pos;
    return pos == line.size();
}

} // namespace insight::semantic::github
