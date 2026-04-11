#include "dj1000/post_geometry_filter.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace dj1000 {

namespace {

constexpr double kDifferenceThreshold = 5.0;
constexpr double kBrightThreshold = 160.0;

void validate_plane(std::span<const double> plane, int width, int height, const char* label) {
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " is too small for post geometry filter");
    }
}

}  // namespace

void apply_post_geometry_delta_filter(
    std::span<double> plane,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post geometry filter dimensions must be positive");
    }
    validate_plane(plane, width, height, "delta plane");
    if (width <= 1) {
        return;
    }

    std::vector<double> reflected_row(static_cast<std::size_t>(width) + 2, 0.0);
    for (int row = 0; row < height; ++row) {
        const auto row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        for (int col = 0; col < width; ++col) {
            reflected_row[static_cast<std::size_t>(col) + 1] =
                plane[row_offset + static_cast<std::size_t>(col)];
        }

        reflected_row[0] = reflected_row[2];
        reflected_row[static_cast<std::size_t>(width) + 1] =
            reflected_row[static_cast<std::size_t>(width) - 1];

        for (int col = 0; col < width; ++col) {
            const double left = reflected_row[static_cast<std::size_t>(col)];
            const double current = reflected_row[static_cast<std::size_t>(col) + 1];
            const double right = reflected_row[static_cast<std::size_t>(col) + 2];
            const double left_minus_right = left - right;

            if (left_minus_right >= kDifferenceThreshold && left >= kBrightThreshold) {
                plane[row_offset + static_cast<std::size_t>(col)] = right;
            } else if (left_minus_right <= -kDifferenceThreshold && right >= kBrightThreshold) {
                plane[row_offset + static_cast<std::size_t>(col)] = left;
            } else {
                plane[row_offset + static_cast<std::size_t>(col)] = current;
            }
        }
    }
}

void apply_post_geometry_delta_filters(
    std::span<double> delta0,
    std::span<double> delta2,
    int width,
    int height
) {
    validate_plane(delta0, width, height, "delta0");
    validate_plane(delta2, width, height, "delta2");
    apply_post_geometry_delta_filter(delta0, width, height);
    apply_post_geometry_delta_filter(delta2, width, height);
}

}  // namespace dj1000
