#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

struct PostGeometryPlanes {
    std::vector<double> center;
    std::vector<double> delta0;
    std::vector<double> delta2;
};

[[nodiscard]] PostGeometryPlanes build_post_geometry_planes(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2,
    int width,
    int height
);

}  // namespace dj1000
