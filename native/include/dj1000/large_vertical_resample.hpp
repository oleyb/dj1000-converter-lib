#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

inline constexpr int kLargeVerticalWidth = 504;
inline constexpr int kLargeVerticalSourceStride = 512;
inline constexpr int kLargeVerticalSourceRows = 244;
inline constexpr int kLargeVerticalOutputRows = 378;
inline constexpr int kLargeVerticalLutWidthParameter = 0x26C;
inline constexpr int kLargeVerticalLutResetCounter = kLargeVerticalLutWidthParameter - 1;

[[nodiscard]] std::vector<std::uint8_t> resample_large_vertical_plane(
    std::span<const std::uint8_t> source_plane
);

}  // namespace dj1000
