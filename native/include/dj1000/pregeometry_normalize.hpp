#pragma once

#include <cstddef>
#include <span>

namespace dj1000 {

inline constexpr int kNormalFloatPlaneStride = 0x200;
inline constexpr int kNormalFloatPlaneRows = 0x0F6;
inline constexpr int kNormalFloatPlaneActiveWidth = 0x1F8;
inline constexpr int kNormalFloatPlaneActiveRows = 0x0F4;

inline constexpr int kPreviewFloatPlaneStride = 0x80;
inline constexpr int kPreviewFloatPlaneRows = 0x40;
inline constexpr int kPreviewFloatPlaneActiveWidth = 0x80;
inline constexpr int kPreviewFloatPlaneActiveRows = 0x40;

[[nodiscard]] std::size_t expected_pregeometry_float_count(bool preview_mode);

void normalize_pregeometry_planes(
    std::span<float> plane0,
    std::span<float> plane1,
    std::span<float> plane2,
    bool preview_mode
);

}  // namespace dj1000
