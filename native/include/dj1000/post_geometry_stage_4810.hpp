#pragma once

#include <span>

namespace dj1000 {

void apply_post_geometry_stage_4810_horizontal(
    std::span<double> plane0,
    std::span<const double> plane1,
    std::span<double> plane2,
    int width,
    int height
);

void apply_post_geometry_stage_4810_vertical(
    std::span<double> plane0,
    std::span<const double> plane1,
    std::span<double> plane2,
    int width,
    int height
);

void apply_post_geometry_stage_4810(
    std::span<double> plane0,
    std::span<double> plane1,
    std::span<double> plane2,
    int width,
    int height
);

}  // namespace dj1000
