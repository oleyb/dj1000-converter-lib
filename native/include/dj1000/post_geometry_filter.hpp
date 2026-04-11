#pragma once

#include <span>
#include <vector>

namespace dj1000 {

void apply_post_geometry_delta_filter(
    std::span<double> plane,
    int width,
    int height
);

void apply_post_geometry_delta_filters(
    std::span<double> delta0,
    std::span<double> delta2,
    int width,
    int height
);

}  // namespace dj1000
