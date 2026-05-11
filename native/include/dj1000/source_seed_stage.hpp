#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

inline constexpr int kNormalSourceSeedInputStride = 0x200;
inline constexpr int kNormalSourceSeedActiveWidth = 0x1F8;
inline constexpr int kNormalSourceSeedActiveRows = 0x0F4;

inline constexpr int kPreviewSourceSeedInputStride = 0x80;
inline constexpr int kPreviewSourceSeedActiveWidth = 0x80;
inline constexpr int kPreviewSourceSeedActiveRows = 0x40;

struct SourceSeedPlanes {
    std::vector<float> plane0;
    std::vector<float> plane1;
    std::vector<float> plane2;
};

[[nodiscard]] std::size_t expected_source_seed_input_byte_count(bool preview_mode);

[[nodiscard]] SourceSeedPlanes build_source_seed_planes(
    std::span<const std::uint8_t> source,
    bool preview_mode
);

}  // namespace dj1000
