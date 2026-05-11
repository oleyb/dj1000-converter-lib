#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

struct RgbBytePlanes {
    std::vector<std::uint8_t> plane0;
    std::vector<std::uint8_t> plane1;
    std::vector<std::uint8_t> plane2;
};

[[nodiscard]] RgbBytePlanes convert_post_geometry_rgb_planes(
    std::span<const double> plane0,
    std::span<const double> plane1,
    std::span<const double> plane2,
    int width,
    int height
);

}  // namespace dj1000
