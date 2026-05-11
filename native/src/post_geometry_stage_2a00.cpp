#include "dj1000/post_geometry_stage_2a00.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

constexpr double kVerticalWeight = 2.0;
constexpr double kCenterWeight = 2.0;
constexpr double kOutputScale = 0.125;

void validate_plane(std::span<const double> plane, int width, int height, const char* label) {
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " is too small for post-geometry stage 0x2a00");
    }
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
    std::copy_n(padded.begin() + static_cast<std::ptrdiff_t>(row_size * 4), padded_width, padded.begin());
    std::copy_n(padded.begin() + static_cast<std::ptrdiff_t>(row_size * 3), padded_width, padded.begin() + static_cast<std::ptrdiff_t>(row_size));
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

std::vector<double> build_post_geometry_stage_2a00_output(
    std::span<const double> plane,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post-geometry stage 0x2a00 dimensions must be positive");
    }
    validate_plane(plane, width, height, "plane");

    const auto padded = build_padded_plane(plane, width, height);
    const int padded_width = width + 4;
    std::vector<double> output(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
        0.0
    );

    for (int row = 0; row < height; ++row) {
        const std::size_t output_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        const std::size_t top_offset =
            (static_cast<std::size_t>(row) + 1) * static_cast<std::size_t>(padded_width) + 2;
        const std::size_t center_offset =
            (static_cast<std::size_t>(row) + 2) * static_cast<std::size_t>(padded_width) + 2;
        const std::size_t bottom_offset =
            (static_cast<std::size_t>(row) + 3) * static_cast<std::size_t>(padded_width) + 2;

        for (int col = 0; col < width; ++col) {
            const std::size_t x = static_cast<std::size_t>(col);
            const double left = padded[center_offset + x - 1];
            const double right = padded[center_offset + x + 1];
            const double top = padded[top_offset + x];
            const double bottom = padded[bottom_offset + x];
            const double center = padded[center_offset + x];

            const volatile double top_weighted = top * kVerticalWeight;
            const volatile double bottom_weighted = bottom * kVerticalWeight;
            const volatile double center_weighted = center * kCenterWeight;

            double vertical_sum = top_weighted;
            vertical_sum += bottom_weighted;
            double horizontal_sum = left;
            horizontal_sum += right;
            double value = vertical_sum;
            value += horizontal_sum;
            value += center_weighted;
            const volatile double scaled_value = value * kOutputScale;
            output[output_offset + x] = scaled_value;
        }
    }

    return output;
}

void apply_post_geometry_stage_2a00(
    std::span<double> plane,
    int width,
    int height
) {
    const auto output = build_post_geometry_stage_2a00_output(plane, width, height);
    std::copy(output.begin(), output.end(), plane.begin());
}

}  // namespace dj1000
