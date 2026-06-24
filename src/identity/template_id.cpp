module;
#include <picosha2.h>

module insight.canon.api;
import insight.canon.internal;

// template_id.cpp — the canon-owned template identity (insight_perf_template_id.md D-TIR-1).
//
// `template_id_of` is the SHA-256 body moved verbatim out of MetaLogEngine::compute_template_id
// (spec §3.2): first 16 bytes of SHA-256(masked template_str). `render` is the sole place the
// "h:"+hex string materialises (the serialize seam). `parse_template_id` is its inverse, a
// test/fixture helper only — the product wire is a one-way terminal render, never parsed back.

namespace insight
{
namespace
{
constexpr std::size_t kSha256Bytes{32};
constexpr std::size_t kTemplateIdBytes{16}; // spec §3.2: first 16 SHA-256 bytes
constexpr unsigned kNibbleMask{0xFU};
constexpr std::array<char, 16> kHexDigits{'0', '1', '2', '3', '4', '5', '6', '7',
                                          '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

[[nodiscard]] std::uint8_t hex_nibble(char chr) noexcept
{
    if (chr >= '0' && chr <= '9')
        return static_cast<std::uint8_t>(chr - '0');
    if (chr >= 'a' && chr <= 'f')
        return static_cast<std::uint8_t>(chr - 'a' + 10);
    if (chr >= 'A' && chr <= 'F')
        return static_cast<std::uint8_t>(chr - 'A' + 10);
    return 0;
}
} // namespace

TemplateId template_id_of(std::string_view canonical_template) noexcept
{
    std::array<unsigned char, kSha256Bytes> digest{};
    picosha2::hash256(canonical_template.begin(), canonical_template.end(), digest.begin(),
                      digest.end());
    TemplateId out;
    for (std::size_t i{0}; i < kTemplateIdBytes; ++i)
        out.bytes[i] = static_cast<std::uint8_t>(digest[i]);
    return out;
}

std::string render(TemplateId template_id)
{
    std::string out;
    out.reserve(2 + (2 * kTemplateIdBytes)); // "h:" + two hex chars per byte
    out.append("h:");
    for (const std::uint8_t byte : template_id.bytes)
    {
        out.push_back(kHexDigits[(static_cast<unsigned>(byte) >> 4) & kNibbleMask]);
        out.push_back(kHexDigits[static_cast<unsigned>(byte) & kNibbleMask]);
    }
    return out;
}

TemplateId parse_template_id(std::string_view rendered)
{
    TemplateId out;
    // Accept the "h:" prefix when present; tolerate a bare 32-hex string for fixtures.
    std::string_view hex{rendered};
    if (hex.size() >= 2 && hex[0] == 'h' && hex[1] == ':')
        hex.remove_prefix(2);
    for (std::size_t i{0}; i < kTemplateIdBytes && (2 * i + 1) < hex.size(); ++i)
        out.bytes[i] = static_cast<std::uint8_t>((hex_nibble(hex[2 * i]) << 4) |
                                                 hex_nibble(hex[2 * i + 1]));
    return out;
}

} // namespace insight
