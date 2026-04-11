#pragma once

#include <span>

namespace dj1000 {

struct PostGeometryStage3890Coefficients {
    double plane0_scale;
    double plane1_from_plane0;
    double plane1_scale;
};

[[nodiscard]] PostGeometryStage3890Coefficients get_post_geometry_stage_3890_coefficients(int level);

void apply_post_geometry_stage_3890(
    std::span<double> plane0,
    std::span<double> plane1,
    int width,
    int height,
    int level
);

}  // namespace dj1000
