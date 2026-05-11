#pragma once

#include "dj1000/post_geometry_prepare.hpp"
#include "dj1000/post_geometry_rgb_convert.hpp"

#include <cstdint>
#include <span>

namespace dj1000 {

[[nodiscard]] PostGeometryPlanes build_nonlarge_post_geometry_planes_from_source(
    std::span<const std::uint8_t> source,
    bool preview_mode
);

[[nodiscard]] RgbBytePlanes build_nonlarge_rgb_planes_from_source(
    std::span<const std::uint8_t> source,
    bool preview_mode
);

}  // namespace dj1000
