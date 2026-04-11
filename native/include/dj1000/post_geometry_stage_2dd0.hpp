#pragma once

#include <span>
#include <vector>

namespace dj1000 {

void apply_post_geometry_stage_2dd0(
    std::span<double> plane0,
    std::span<double> plane1,
    int width,
    int height
);

[[nodiscard]] std::vector<double> build_post_geometry_stage_2dd0_plane_output(
    std::span<const double> plane,
    int width,
    int height
);

}  // namespace dj1000
