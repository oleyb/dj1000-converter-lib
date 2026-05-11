#pragma once

#include <span>

namespace dj1000 {

void apply_post_geometry_center_scale(
    std::span<double> delta0,
    std::span<double> center,
    std::span<double> delta2,
    int width,
    int height
);

}  // namespace dj1000
