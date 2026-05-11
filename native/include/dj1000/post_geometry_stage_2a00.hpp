#pragma once

#include <span>
#include <vector>

namespace dj1000 {

void apply_post_geometry_stage_2a00(
    std::span<double> plane,
    int width,
    int height
);

[[nodiscard]] std::vector<double> build_post_geometry_stage_2a00_output(
    std::span<const double> plane,
    int width,
    int height
);

}  // namespace dj1000
