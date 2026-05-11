#pragma once

#include <span>

namespace dj1000 {

void apply_post_geometry_dual_scale(
    std::span<double> plane0,
    std::span<double> plane1,
    int width,
    int height,
    double scalar
);

}  // namespace dj1000
