#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

inline constexpr int kGeometrySourceWidth = 504;
inline constexpr int kGeometrySourceHeight = 384;
inline constexpr int kGeometrySourceStride = 324;

inline constexpr int kPreviewStageWidth = 80;
inline constexpr int kPreviewStageHeight = 64;

inline constexpr int kNormalStageWidth = 320;
inline constexpr int kNormalStageHeight = 246;
inline constexpr int kNormalStageLeftCrop = 2;

inline constexpr int kLargeStageWidth = 504;
inline constexpr int kLargeStageHeight = 378;

[[nodiscard]] std::vector<std::uint8_t> copy_preview_stage_plane(
    std::span<const std::uint8_t> plane
);

[[nodiscard]] std::vector<std::uint8_t> copy_normal_stage_plane(
    std::span<const std::uint8_t> plane
);

[[nodiscard]] std::vector<std::uint8_t> copy_large_stage_plane(
    std::span<const std::uint8_t> plane
);

}  // namespace dj1000
