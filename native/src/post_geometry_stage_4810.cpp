#include "dj1000/post_geometry_stage_4810.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace dj1000 {

namespace {

constexpr double kHighThreshold = 143.0;
constexpr double kLowThreshold = 140.0;

void validate_plane(std::span<const double> plane, int width, int height, const char* label) {
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " is too small for post geometry stage 4810");
    }
}

int reflect_index(int value, int length) {
    if (length <= 1) {
        return 0;
    }
    while (value < 0 || value >= length) {
        if (value < 0) {
            value = -value;
        } else {
            value = ((length - 1) * 2) - value;
        }
    }
    return value;
}

std::vector<double> build_full_reflect_pad(std::span<const double> plane, int width, int height) {
    const int padded_width = width + 4;
    const int padded_height = height + 4;
    std::vector<double> padded(
        static_cast<std::size_t>(padded_width) * static_cast<std::size_t>(padded_height),
        0.0
    );
    for (int padded_row = 0; padded_row < padded_height; ++padded_row) {
        const int source_row = reflect_index(padded_row - 2, height);
        const std::size_t padded_row_offset =
            static_cast<std::size_t>(padded_row) * static_cast<std::size_t>(padded_width);
        const std::size_t source_row_offset =
            static_cast<std::size_t>(source_row) * static_cast<std::size_t>(width);
        for (int padded_col = 0; padded_col < padded_width; ++padded_col) {
            const int source_col = reflect_index(padded_col - 2, width);
            padded[padded_row_offset + static_cast<std::size_t>(padded_col)] =
                plane[source_row_offset + static_cast<std::size_t>(source_col)];
        }
    }
    return padded;
}

std::vector<double> build_vertical_stage_buffer(std::span<const double> plane, int width, int height) {
    const int stride = width + 4;
    std::vector<double> padded(
        static_cast<std::size_t>(height + 4) * static_cast<std::size_t>(stride),
        0.0
    );
    const std::size_t total_samples =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    for (int row = 0; row < height; ++row) {
        const std::size_t source_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        const std::size_t dest_offset =
            (static_cast<std::size_t>(row + 2) * static_cast<std::size_t>(stride)) + 2;
        for (int col = 0; col < stride; ++col) {
            const std::size_t source_index = source_offset + static_cast<std::size_t>(col);
            padded[dest_offset + static_cast<std::size_t>(col)] =
                (source_index < total_samples) ? plane[source_index] : 0.0;
        }
    }

    if (height > 2) {
        const std::size_t row_bytes = static_cast<std::size_t>(stride);
        const std::size_t row0 = 0;
        const std::size_t row1 = row_bytes;
        const std::size_t row3 = 3 * row_bytes;
        const std::size_t row4 = 4 * row_bytes;
        const std::size_t row_h = static_cast<std::size_t>(height) * row_bytes;
        const std::size_t row_h_minus_1 = static_cast<std::size_t>(height - 1) * row_bytes;
        const std::size_t row_h_plus_2 = static_cast<std::size_t>(height + 2) * row_bytes;
        const std::size_t row_h_plus_3 = static_cast<std::size_t>(height + 3) * row_bytes;

        std::copy_n(padded.begin() + static_cast<std::ptrdiff_t>(row4), stride, padded.begin() + static_cast<std::ptrdiff_t>(row0));
        std::copy_n(padded.begin() + static_cast<std::ptrdiff_t>(row3), stride, padded.begin() + static_cast<std::ptrdiff_t>(row1));
        std::copy_n(
            padded.begin() + static_cast<std::ptrdiff_t>(row_h),
            stride,
            padded.begin() + static_cast<std::ptrdiff_t>(row_h_plus_2)
        );
        std::copy_n(
            padded.begin() + static_cast<std::ptrdiff_t>(row_h_minus_1),
            stride,
            padded.begin() + static_cast<std::ptrdiff_t>(row_h_plus_3)
        );
    }

    return padded;
}

void apply_horizontal_repair(
    std::span<double> plane0,
    std::span<const double> center,
    std::span<double> plane2,
    int width,
    int height
) {
    const auto center_padded = build_full_reflect_pad(center, width, height);
    const auto plane0_source = build_full_reflect_pad(plane0, width, height);
    const auto plane2_source = build_full_reflect_pad(plane2, width, height);
    const int padded_width = width + 4;
    const std::size_t total_samples =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    const int repair_rows = (height > 2) ? (height - 2) : 0;
    for (int row = 0; row < repair_rows; ++row) {
        const std::size_t output_row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        const std::size_t padded_row_offset =
            static_cast<std::size_t>(row + 2) * static_cast<std::size_t>(padded_width);

        // The DLL walks two extra positions past the visible row end on the flat output
        // buffer. Those extra iterations are what repair the following row's left edge.
        for (int column = 0; column < (width + 2); ++column) {
            const double left = center_padded[padded_row_offset + static_cast<std::size_t>(column + 1)];
            const double right = center_padded[padded_row_offset + static_cast<std::size_t>(column + 3)];
            const std::size_t output_index = output_row_offset + static_cast<std::size_t>(column);

            if (left >= kHighThreshold && right < kLowThreshold) {
                const double replacement0 =
                    plane0_source[padded_row_offset + static_cast<std::size_t>(column + 4)];
                const double replacement2 =
                    plane2_source[padded_row_offset + static_cast<std::size_t>(column + 4)];
                if (output_index < total_samples) {
                    plane0[output_index] = replacement0;
                    plane2[output_index] = replacement2;
                }
                if ((column + 1) < width) {
                    const std::size_t next_index = output_index + 1;
                    if (next_index < total_samples) {
                        plane0[next_index] = replacement0;
                        plane2[next_index] = replacement2;
                    }
                }
            } else if (right >= kHighThreshold && left < kLowThreshold) {
                const double replacement0 =
                    plane0_source[padded_row_offset + static_cast<std::size_t>(column)];
                const double replacement2 =
                    plane2_source[padded_row_offset + static_cast<std::size_t>(column)];
                if (output_index < total_samples) {
                    plane0[output_index] = replacement0;
                    plane2[output_index] = replacement2;
                }
                if (column == 0 && output_index > 0) {
                    plane0[output_index - 1] = replacement0;
                    plane2[output_index - 1] = replacement2;
                }
            }
        }
    }
}

void apply_vertical_repair(
    std::span<double> plane0,
    std::span<const double> center,
    std::span<double> plane2,
    int width,
    int height
) {
    const auto center_padded = build_full_reflect_pad(center, width, height);
    const auto plane0_source = build_vertical_stage_buffer(plane0, width, height);
    const auto plane2_source = build_vertical_stage_buffer(plane2, width, height);
    const int padded_width = width + 4;
    const std::size_t total_samples =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    const int repair_rows = (height > 1) ? (height - 1) : 0;
    for (int row = 0; row < repair_rows; ++row) {
        const std::size_t output_row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        const std::size_t top_offset =
            (static_cast<std::size_t>(row + 1) * static_cast<std::size_t>(padded_width)) + 2;
        const std::size_t bottom_offset =
            (static_cast<std::size_t>(row + 3) * static_cast<std::size_t>(padded_width)) + 2;
        const std::size_t source_above_offset =
            (static_cast<std::size_t>(row) * static_cast<std::size_t>(padded_width)) + 2;
        const std::size_t source_below_offset =
            (static_cast<std::size_t>(row + 4) * static_cast<std::size_t>(padded_width)) + 2;

        // The vertical pass also walks two extra flat positions per row and can
        // spill into the next row's left edge before later stages consume it.
        for (int col = 0; col < (width + 2); ++col) {
            const double top = center_padded[top_offset + static_cast<std::size_t>(col)];
            const double bottom = center_padded[bottom_offset + static_cast<std::size_t>(col)];
            const std::size_t output_index = output_row_offset + static_cast<std::size_t>(col);

            if (top >= kHighThreshold && bottom < kLowThreshold) {
                const std::size_t source_index = source_below_offset + static_cast<std::size_t>(col);
                const double replacement0 = plane0_source[source_index];
                const double replacement2 = plane2_source[source_index];
                if (output_index < total_samples) {
                    plane0[output_index] = replacement0;
                    plane2[output_index] = replacement2;
                }
                if ((row + 1) < height) {
                    const std::size_t next_index = output_index + static_cast<std::size_t>(width);
                    if (next_index < total_samples) {
                        plane0[next_index] = replacement0;
                        plane2[next_index] = replacement2;
                    }
                }
            } else if (bottom >= kHighThreshold && top < kLowThreshold) {
                const std::size_t source_index = source_above_offset + static_cast<std::size_t>(col);
                const double replacement0 = plane0_source[source_index];
                const double replacement2 = plane2_source[source_index];
                if (output_index < total_samples) {
                    plane0[output_index] = replacement0;
                    plane2[output_index] = replacement2;
                }
                if (row > 0) {
                    const std::size_t previous_index = output_index - static_cast<std::size_t>(width);
                    if (previous_index < total_samples) {
                        plane0[previous_index] = replacement0;
                        plane2[previous_index] = replacement2;
                    }
                }
            }
        }
    }
}

}  // namespace

void apply_post_geometry_stage_4810_horizontal(
    std::span<double> plane0,
    std::span<const double> plane1,
    std::span<double> plane2,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post geometry stage 4810 dimensions must be positive");
    }
    validate_plane(plane0, width, height, "plane0");
    validate_plane(plane1, width, height, "plane1");
    validate_plane(plane2, width, height, "plane2");

    apply_horizontal_repair(plane0, plane1, plane2, width, height);
}

void apply_post_geometry_stage_4810_vertical(
    std::span<double> plane0,
    std::span<const double> plane1,
    std::span<double> plane2,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post geometry stage 4810 dimensions must be positive");
    }
    validate_plane(plane0, width, height, "plane0");
    validate_plane(plane1, width, height, "plane1");
    validate_plane(plane2, width, height, "plane2");

    apply_vertical_repair(plane0, plane1, plane2, width, height);
}

void apply_post_geometry_stage_4810(
    std::span<double> plane0,
    std::span<double> plane1,
    std::span<double> plane2,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post geometry stage 4810 dimensions must be positive");
    }
    validate_plane(plane0, width, height, "plane0");
    validate_plane(plane1, width, height, "plane1");
    validate_plane(plane2, width, height, "plane2");

    apply_post_geometry_stage_4810_horizontal(plane0, plane1, plane2, width, height);
    apply_post_geometry_stage_4810_vertical(plane0, plane1, plane2, width, height);
}

}  // namespace dj1000
