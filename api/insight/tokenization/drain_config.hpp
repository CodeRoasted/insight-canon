#pragma once
#include <cstddef>

namespace insight::tokenization
{

struct DrainConfig
{
    std::size_t max_depth{4};
    static constexpr double kDefaultSimilarityThreshold{0.4};
    static constexpr std::size_t kDefaultMaxClusters{10'000};
    double similarity_threshold{kDefaultSimilarityThreshold};
    std::size_t max_clusters{kDefaultMaxClusters};

    // Token masking: structurally variable tokens are replaced with "<*>" before
    // template matching so they never fossilise into the cluster template.
    bool mask_ip_addresses{true};  // IPv4 address tokens (e.g. "192.168.1.1:")
    bool mask_hex_addresses{true}; // hex address tokens  (e.g. "0xdeadbeef")
};

} // namespace insight::tokenization
