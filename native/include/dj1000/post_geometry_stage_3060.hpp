#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

void apply_post_geometry_stage_3060(
    std::span<const std::int32_t> edge_response,
    std::span<double> center,
    int width,
    int height,
    int stage_param0,
    int stage_param1,
    double scalar,
    int threshold
);

[[nodiscard]] std::vector<double> build_post_geometry_stage_3060_output(
    std::span<const std::int32_t> edge_response,
    std::span<const double> center,
    int width,
    int height,
    int stage_param0,
    int stage_param1,
    double scalar,
    int threshold
);

}  // namespace dj1000
