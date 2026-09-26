// refs: DN-43.D20
// invariant: the one UTF-8 replacement rule every JSON egress runs, proven once: measured vectors,
// the identity on well-formed boundary code points, and an exhaustive sweep.
// note: expected values come from two independent decoders; the sweep's judge is simdjson.
#include <gtest/gtest.h>
#include <simdjson.h>
#include <utf8/well_formed.hpp>

import insight.canon.test;

namespace
{

using insight::utf8::replace_ill_formed;

[[nodiscard]] std::string bytes_of(std::string_view hex)
{
    std::string out;
    for (std::size_t at{0}; at + 1 < hex.size(); at += 2)
        out.push_back(static_cast<char>(std::stoi(std::string{hex.substr(at, 2)}, nullptr, 16)));
    return out;
}

[[nodiscard]] std::string hex_of(std::string_view bytes)
{
    constexpr std::string_view kDigits{"0123456789ABCDEF"};
    std::string out;
    for (const char byte : bytes)
    {
        const auto value{static_cast<std::uint8_t>(byte)};
        out.push_back(kDigits[value >> 4U]);
        out.push_back(kDigits[value & 0x0FU]);
    }
    return out;
}

[[nodiscard]] std::string ascii_subsequence(std::string_view bytes)
{
    std::string out;
    for (const char byte : bytes)
        if (static_cast<std::uint8_t>(byte) < 0x80U)
            out.push_back(byte);
    return out;
}

[[nodiscard]] bool well_formed(std::string_view bytes)
{
    return simdjson::validate_utf8(bytes.data(), bytes.size());
}

struct MeasuredVector
{
    std::string_view name;
    std::string_view input_hex;
    std::string_view expected_hex;
};

// note: expected bytes from CPython 3.12.3 and Node 18.19.1 (Buffer and TextDecoder), 2026-09-26.
constexpr std::array kMeasured{
    MeasuredVector{.name = "the standard's worked example",
                   .input_hex = "61F18080E180C262806380BF64",
                   .expected_hex = "61EFBFBDEFBFBDEFBFBD62EFBFBD63EFBFBDEFBFBD64"},
    MeasuredVector{.name = "a lone continuation byte", .input_hex = "80", .expected_hex = "EFBFBD"},
    MeasuredVector{
        .name = "a lead byte before ASCII", .input_hex = "C341", .expected_hex = "EFBFBD41"},
    MeasuredVector{
        .name = "an overlong two-byte form", .input_hex = "C0AF", .expected_hex = "EFBFBDEFBFBD"},
    MeasuredVector{.name = "an overlong three-byte form",
                   .input_hex = "E080AF",
                   .expected_hex = "EFBFBDEFBFBDEFBFBD"},
    MeasuredVector{.name = "an encoded surrogate",
                   .input_hex = "EDA080",
                   .expected_hex = "EFBFBDEFBFBDEFBFBD"},
    MeasuredVector{.name = "a code point above U+10FFFF",
                   .input_hex = "F4908080",
                   .expected_hex = "EFBFBDEFBFBDEFBFBDEFBFBD"},
    MeasuredVector{.name = "truncated four- and three-byte forms",
                   .input_hex = "F09F9878E282",
                   .expected_hex = "EFBFBD78EFBFBD"},
};

TEST(Utf8Replacement, EachMeasuredVectorMatchesTwoIndependentDecoders)
{
    for (const MeasuredVector& vector : kMeasured)
    {
        const std::string actual{replace_ill_formed(bytes_of(vector.input_hex))};
        EXPECT_EQ(hex_of(actual), vector.expected_hex)
            << vector.name << ": input " << vector.input_hex << ", expected " << vector.expected_hex
            << ", actual " << hex_of(actual);
    }
}

TEST(Utf8Replacement, WellFormedBoundaryCodePointsPassThroughByteIdentical)
{
    constexpr std::array<std::string_view, 10> kBoundaries{
        "7F",     "C280",   "DFBF",   "E0A080",   "ED9FBF",
        "EE8080", "EFBFBD", "EFBFBF", "F0908080", "F48FBFBF"};
    std::string all;
    for (const std::string_view hex : kBoundaries)
    {
        const std::string input{"<" + bytes_of(hex) + ">"};
        all += input;
        EXPECT_EQ(hex_of(replace_ill_formed(input)), hex_of(input))
            << "the well-formed code point " << hex << " did not pass through";
    }
    EXPECT_EQ(hex_of(replace_ill_formed(all)), hex_of(all))
        << "the ten boundary code points in one buffer did not pass through";
}

// invariant: every 1-byte and 2-byte input, and every 3-byte input whose lead is E0 or above.
TEST(Utf8Replacement, EveryShortInputComesOutWellFormedWithItsAsciiBytesInPlace)
{
    constexpr std::size_t kByteValues{256};
    constexpr std::size_t kFirstThreeByteLead{0xE0};
    constexpr std::size_t kReportedFailures{12};
    std::size_t swept{0};
    std::size_t failed{0};
    std::string report;
    const auto judge = [&](const std::string& input)
    {
        ++swept;
        const std::string output{replace_ill_formed(input)};
        std::string reason;
        if (!well_formed(output))
            reason = "the output is not well-formed UTF-8";
        else if (ascii_subsequence(output) != ascii_subsequence(input))
            reason = "the bytes below 0x80 moved";
        else if (well_formed(input) && output != input)
            reason = "a well-formed input was changed";
        else if (replace_ill_formed(output) != output)
            reason = "a second pass changed the output";
        if (reason.empty())
            return;
        if (++failed <= kReportedFailures)
            report += "  input " + hex_of(input) + " -> " + hex_of(output) + ": " + reason + "\n";
    };

    std::string input;
    for (std::size_t first{0}; first < kByteValues; ++first)
    {
        input.assign(1, static_cast<char>(first));
        judge(input);
        for (std::size_t second{0}; second < kByteValues; ++second)
        {
            input.assign({static_cast<char>(first), static_cast<char>(second)});
            judge(input);
            if (first < kFirstThreeByteLead)
                continue;
            for (std::size_t third{0}; third < kByteValues; ++third)
            {
                input.assign({static_cast<char>(first), static_cast<char>(second),
                              static_cast<char>(third)});
                judge(input);
            }
        }
    }

    constexpr std::size_t kExpectedSwept{
        kByteValues + (kByteValues * kByteValues) +
        ((kByteValues - kFirstThreeByteLead) * kByteValues * kByteValues)};
    EXPECT_EQ(swept, kExpectedSwept) << "the sweep did not cover its declared population";
    EXPECT_EQ(failed, 0U) << failed << " of " << swept << " inputs failed; the first "
                          << std::min(failed, kReportedFailures) << ":\n"
                          << report;
}

} // namespace
