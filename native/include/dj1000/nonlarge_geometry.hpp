#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

inline constexpr int kNormalSourceRowStride = 0x200;
inline constexpr int kNormalSourceRowOffset = 1;
inline constexpr int kNormalSourceRowBytes = 0x1F8;
inline constexpr int kNormalResampledRows = 0x0F4;
inline constexpr int kNormalIntermediateStride = 0x144;
inline constexpr int kNormalIntermediateRows = 0x0F6;

inline constexpr int kPreviewSourceRowStride = 0x80;
inline constexpr int kPreviewSourceRowOffset = 1;
inline constexpr int kPreviewSourceRowBytes = 0x7E;
inline constexpr int kPreviewResampledRows = 0x40;
inline constexpr int kPreviewIntermediateStride = 0x50;
inline constexpr int kPreviewIntermediateRows = 0x40;

[[nodiscard]] std::vector<std::uint8_t> build_nonlarge_intermediate_plane(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2,
    int channel_index,
    bool preview_mode
);

[[nodiscard]] std::vector<std::uint8_t> build_nonlarge_stage_plane(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2,
    int channel_index,
    bool preview_mode
);

}  // namespace dj1000
