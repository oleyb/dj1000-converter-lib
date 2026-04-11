#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

inline constexpr int kNonLargePhaseResetCounter = 0x18F;
inline constexpr int kNormalRowLutWidthParameter = 0x101;
inline constexpr int kNormalRowWidthLimit = 0x1F9;
inline constexpr int kNormalRowOutputLength = 0x144;

inline constexpr int kPreviewRowLutWidthParameter = 0x103;
inline constexpr int kPreviewRowWidthLimit = 0x7C;
inline constexpr int kPreviewRowOutputLength = 0x50;

[[nodiscard]] std::vector<std::uint8_t> resample_nonlarge_row(
    std::span<const std::uint8_t> source_row,
    int lut_width_parameter,
    int width_limit,
    int output_length
);

}  // namespace dj1000
