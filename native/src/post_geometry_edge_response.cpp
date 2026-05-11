#include "dj1000/post_geometry_edge_response.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace dj1000 {

namespace {

constexpr double kHorizontalWeight = 3.0;
constexpr double kVerticalWeight = 2.0;
constexpr double kCenterWeight = 10.0;
constexpr double kOutputScale = 6.0;
constexpr double kClampMin = -2550.0;
constexpr double kClampMax = 2550.0;

void validate_plane(std::span<const double> plane, int width, int height) {
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane.size() < required) {
        throw std::runtime_error("post geometry edge response plane is too small");
    }
}

std::int32_t trunc_to_int32(double value) {
    if (!std::isfinite(value)) {
        return 0;
    }
    const double clamped = std::clamp(value, kClampMin, kClampMax);
    return static_cast<std::int32_t>(std::trunc(clamped));
}

std::vector<double> build_padded_plane(std::span<const double> plane, int width, int height) {
    const int padded_width = width + 4;
    const int padded_height = height + 4;
    std::vector<double> padded(
        static_cast<std::size_t>(padded_width) * static_cast<std::size_t>(padded_height),
        0.0
    );

    for (int row = 0; row < height; ++row) {
        const std::size_t source_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        const std::size_t padded_offset =
            (static_cast<std::size_t>(row) + 2) * static_cast<std::size_t>(padded_width);
        std::copy_n(
            plane.begin() + static_cast<std::ptrdiff_t>(source_offset),
            width,
            padded.begin() + static_cast<std::ptrdiff_t>(padded_offset + 2)
        );

        padded[padded_offset] = padded[padded_offset + 4];
        padded[padded_offset + 1] = padded[padded_offset + 3];
        padded[padded_offset + static_cast<std::size_t>(width) + 2] =
            padded[padded_offset + static_cast<std::size_t>(width)];
        padded[padded_offset + static_cast<std::size_t>(width) + 3] =
            padded[padded_offset + static_cast<std::size_t>(width) - 1];
    }

    const std::size_t row_size = static_cast<std::size_t>(padded_width);
    std::copy_n(padded.begin() + static_cast<std::ptrdiff_t>(row_size * 3), padded_width, padded.begin());
    std::copy_n(padded.begin() + static_cast<std::ptrdiff_t>(row_size * 2), padded_width, padded.begin() + static_cast<std::ptrdiff_t>(row_size));
    std::copy_n(
        padded.begin() + static_cast<std::ptrdiff_t>(row_size * static_cast<std::size_t>(height)),
        padded_width,
        padded.begin() + static_cast<std::ptrdiff_t>(row_size * static_cast<std::size_t>(height + 2))
    );
    std::copy_n(
        padded.begin() + static_cast<std::ptrdiff_t>(row_size * static_cast<std::size_t>(height - 1)),
        padded_width,
        padded.begin() + static_cast<std::ptrdiff_t>(row_size * static_cast<std::size_t>(height + 3))
    );

    return padded;
}

}  // namespace

std::vector<std::int32_t> build_post_geometry_edge_response(
    std::span<const double> plane,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post geometry edge response dimensions must be positive");
    }
    validate_plane(plane, width, height);

    std::vector<std::int32_t> output(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
        0
    );
    const auto padded = build_padded_plane(plane, width, height);
    const int padded_width = width + 4;
    for (int row = 0; row < height; ++row) {
        const std::size_t output_row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        const std::size_t top_row_offset =
            (static_cast<std::size_t>(row) + 1) * static_cast<std::size_t>(padded_width) + 2;
        const std::size_t center_row_offset =
            (static_cast<std::size_t>(row) + 2) * static_cast<std::size_t>(padded_width) + 2;
        const std::size_t bottom_row_offset =
            (static_cast<std::size_t>(row) + 3) * static_cast<std::size_t>(padded_width) + 2;

        for (int col = 0; col < width; ++col) {
            const std::size_t x = static_cast<std::size_t>(col);
            const double left = padded[center_row_offset + x - 1];
            const double top = padded[top_row_offset + x];
            const double right = padded[center_row_offset + x + 1];
            const double bottom = padded[bottom_row_offset + x];
            const double center = padded[center_row_offset + x];

            const volatile double left_weighted = left * kHorizontalWeight;
            const volatile double top_weighted = top * kVerticalWeight;
            const volatile double right_weighted = right * kHorizontalWeight;
            const volatile double bottom_weighted = bottom * kVerticalWeight;
            const volatile double center_weighted = center * kCenterWeight;

            double neighbors = left_weighted;
            neighbors += top_weighted;
            neighbors += right_weighted;
            neighbors += bottom_weighted;
            double response = center_weighted;
            response -= neighbors;
            const volatile double scaled_response = response * kOutputScale;
            output[output_row_offset + static_cast<std::size_t>(col)] = trunc_to_int32(scaled_response);
        }
    }

    return output;
}

}  // namespace dj1000
