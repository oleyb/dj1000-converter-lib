#include "dj1000/post_geometry_stage_2dd0.hpp"

#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

constexpr long double kStage2dd0NeighborWeight = 2.0L;
constexpr long double kStage2dd0Scale = 0.07142857142857142L;

void validate_plane(std::span<const double> plane, int width, int height, const char* label) {
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " is too small for post-geometry stage 0x2dd0");
    }
}

std::vector<double> build_padded_plane(std::span<const double> plane, int width, int height) {
    const int padded_width = width + 6;
    const int padded_height = height + 2;
    std::vector<double> padded(
        static_cast<std::size_t>(padded_width) * static_cast<std::size_t>(padded_height),
        0.0
    );

    for (int row = 0; row < height; ++row) {
        const std::size_t source_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        const std::size_t padded_offset =
            (static_cast<std::size_t>(row) + 1) * static_cast<std::size_t>(padded_width);
        for (int col = 0; col < width; ++col) {
            padded[padded_offset + static_cast<std::size_t>(col) + 3] =
                plane[source_offset + static_cast<std::size_t>(col)];
        }

        padded[padded_offset] = padded[padded_offset + 6];
        padded[padded_offset + 1] = padded[padded_offset + 5];
        padded[padded_offset + 2] = padded[padded_offset + 4];
        padded[padded_offset + static_cast<std::size_t>(width) + 3] =
            padded[padded_offset + static_cast<std::size_t>(width) + 1];
        padded[padded_offset + static_cast<std::size_t>(width) + 4] =
            padded[padded_offset + static_cast<std::size_t>(width)];
        padded[padded_offset + static_cast<std::size_t>(width) + 5] =
            padded[padded_offset + static_cast<std::size_t>(width) - 1];
    }

    const std::size_t row_size = static_cast<std::size_t>(padded_width);
    std::copy_n(padded.begin() + row_size, row_size, padded.begin());
    std::copy_n(
        padded.begin() + (static_cast<std::size_t>(height - 1) * row_size),
        row_size,
        padded.begin() + (static_cast<std::size_t>(height + 1) * row_size)
    );

    return padded;
}

}  // namespace

std::vector<double> build_post_geometry_stage_2dd0_plane_output(
    std::span<const double> plane,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post-geometry stage 0x2dd0 dimensions must be positive");
    }
    validate_plane(plane, width, height, "plane");

    const auto padded = build_padded_plane(plane, width, height);
    const int padded_width = width + 6;
    std::vector<double> output(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
        0.0
    );

    for (int row = 0; row < height; ++row) {
        const std::size_t output_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        const std::size_t top_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(padded_width) + 3;
        const std::size_t center_offset =
            (static_cast<std::size_t>(row) + 1) * static_cast<std::size_t>(padded_width) + 3;
        const std::size_t bottom_offset =
            (static_cast<std::size_t>(row) + 2) * static_cast<std::size_t>(padded_width) + 3;

        for (int col = 0; col < width; ++col) {
            const std::size_t x = static_cast<std::size_t>(col);
            long double accum =
                static_cast<long double>(padded[center_offset + x - 1]) *
                kStage2dd0NeighborWeight;
            accum +=
                static_cast<long double>(padded[center_offset + x + 1]) *
                kStage2dd0NeighborWeight;
            accum +=
                static_cast<long double>(padded[center_offset + x - 2]) *
                kStage2dd0NeighborWeight;
            accum +=
                static_cast<long double>(padded[center_offset + x + 2]) *
                kStage2dd0NeighborWeight;
            accum += static_cast<long double>(padded[center_offset + x + 3]);
            accum += static_cast<long double>(padded[center_offset + x - 3]);
            accum += static_cast<long double>(padded[top_offset + x]);
            accum += static_cast<long double>(padded[bottom_offset + x]);
            accum +=
                static_cast<long double>(padded[center_offset + x]) *
                kStage2dd0NeighborWeight;
            output[output_offset + x] = static_cast<double>(accum * kStage2dd0Scale);
        }
    }

    return output;
}

void apply_post_geometry_stage_2dd0(
    std::span<double> plane0,
    std::span<double> plane1,
    int width,
    int height
) {
    const auto output0 = build_post_geometry_stage_2dd0_plane_output(plane0, width, height);
    const auto output1 = build_post_geometry_stage_2dd0_plane_output(plane1, width, height);
    std::copy(output0.begin(), output0.end(), plane0.begin());
    std::copy(output1.begin(), output1.end(), plane1.begin());
}

}  // namespace dj1000
