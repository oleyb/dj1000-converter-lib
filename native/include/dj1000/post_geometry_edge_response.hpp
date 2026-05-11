#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

[[nodiscard]] std::vector<std::int32_t> build_post_geometry_edge_response(
    std::span<const double> plane,
    int width,
    int height
);

}  // namespace dj1000
