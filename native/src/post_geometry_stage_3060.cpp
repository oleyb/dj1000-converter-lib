#include "dj1000/post_geometry_stage_3060.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

constexpr double kHundredth = 0.1;
constexpr double kPositiveBiasScale = 1.5;
constexpr double kCorrectionClampMin = -255.0;
constexpr double kCorrectionClampMax = 255.0;
constexpr double kSmoothingAverageScale = 0.2;
constexpr double kOutputClampMin = 0.0;
constexpr double kOutputClampMax = 255.0;
constexpr double kScalarThresholdHigh = 2.0;
constexpr double kScalarThresholdLow = 1.0;
constexpr double kScalarFactorHigh = 0.2;
constexpr double kScalarLinearSlope = -0.8;
constexpr double kScalarLinearIntercept = 1.8;
constexpr double kNegativeCorrectionScale = 0.4;
constexpr double kHighlightStart = 160.0;
constexpr double kHighlightRangeReciprocal = 1.0 / 95.0;
constexpr double kShadowStart = 10.0;
constexpr double kShadowScale = 0.2;

void validate_edge_plane(
    std::span<const std::int32_t> plane,
    int width,
    int height,
    const char* label
) {
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " is too small for post-geometry stage 0x3060");
    }
}

void validate_center_plane(
    std::span<const double> plane,
    int width,
    int height,
    const char* label
) {
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " is too small for post-geometry stage 0x3060");
    }
}

double round_to_stored_double(long double value) {
    return static_cast<double>(value);
}

double clamp_correction(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, kCorrectionClampMin, kCorrectionClampMax);
}

double clamp_output(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, kOutputClampMin, kOutputClampMax);
}

int truncated_abs_difference(double left, double right) {
    const double difference = left - right;
    if (!std::isfinite(difference)) {
        return 0;
    }
    const int truncated = static_cast<int>(std::trunc(difference));
    return truncated < 0 ? -truncated : truncated;
}

double build_scalar_factor(double scalar) {
    if (!(scalar < kScalarThresholdHigh)) {
        return kScalarFactorHigh;
    }
    if (!(scalar > kScalarThresholdLow)) {
        return 1.0;
    }
    return round_to_stored_double(
        (static_cast<long double>(scalar) * kScalarLinearSlope) + kScalarLinearIntercept
    );
}

std::vector<double> build_thresholded_edge_plane(
    std::span<const std::int32_t> edge_response,
    int width,
    int height,
    int stage_param0,
    int stage_param1,
    double scalar
) {
    const std::size_t sample_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<double> output(sample_count, 0.0);

    const double positive_bias = round_to_stored_double(
        static_cast<long double>(stage_param1) * kHundredth
    );
    const double stage_param0_scaled = round_to_stored_double(
        static_cast<long double>(stage_param0) * kHundredth
    );
    const double overall_gain = round_to_stored_double(
        static_cast<long double>(build_scalar_factor(scalar)) * stage_param0_scaled
    );

    for (std::size_t index = 0; index < sample_count; ++index) {
        const double scaled_input = round_to_stored_double(
            static_cast<long double>(edge_response[index]) * kHundredth
        );
        double adjusted = 0.0;
        if (scaled_input < 0.0) {
            adjusted = round_to_stored_double(
                static_cast<long double>(scaled_input) + positive_bias
            );
            if (adjusted > 0.0) {
                adjusted = 0.0;
            }
        } else {
            adjusted = round_to_stored_double(
                static_cast<long double>(scaled_input) -
                (static_cast<long double>(positive_bias) * kPositiveBiasScale)
            );
            if (adjusted < 0.0) {
                adjusted = 0.0;
            }
        }
        output[index] = std::isfinite(adjusted)
            ? round_to_stored_double(static_cast<long double>(adjusted) * overall_gain)
            : 0.0;
    }

    return output;
}

std::vector<double> build_smoothed_edge_plane(
    std::span<const double> thresholded,
    int width,
    int height,
    int threshold
) {
    const std::size_t sample_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<double> output(sample_count, 0.0);
    std::copy(
        thresholded.begin(),
        thresholded.begin() + static_cast<std::ptrdiff_t>(sample_count),
        output.begin()
    );

    if (width <= 0 || height <= 2) {
        return output;
    }

    const int padded_stride = width + 2;
    const int padded_rows = height + 2;
    std::vector<double> padded(
        static_cast<std::size_t>(padded_stride) * static_cast<std::size_t>(padded_rows),
        0.0
    );

    for (int row = 0; row < height; ++row) {
        const std::size_t source_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        const std::size_t dest_offset =
            (static_cast<std::size_t>(row + 1) * static_cast<std::size_t>(padded_stride)) + 1;
        for (int col = 0; col < width; ++col) {
            padded[dest_offset + static_cast<std::size_t>(col)] =
                thresholded[source_offset + static_cast<std::size_t>(col)];
        }

        if (width > 1) {
            padded[dest_offset - 1] = thresholded[source_offset + 1];
            padded[dest_offset + static_cast<std::size_t>(width)] =
                thresholded[source_offset + static_cast<std::size_t>(width - 2)];
        } else {
            padded[dest_offset - 1] = thresholded[source_offset];
            padded[dest_offset + static_cast<std::size_t>(width)] = thresholded[source_offset];
        }
    }

    const std::size_t top_row_offset = 0;
    const std::size_t first_data_row_offset = static_cast<std::size_t>(padded_stride);
    for (int col = 0; col < padded_stride; ++col) {
        padded[top_row_offset + static_cast<std::size_t>(col)] =
            padded[first_data_row_offset + static_cast<std::size_t>(col)];
    }

    for (int row = 1; row < height - 1; ++row) {
        const std::size_t output_row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        const std::size_t center_offset =
            (static_cast<std::size_t>(row + 1) * static_cast<std::size_t>(padded_stride)) + 1;
        const std::size_t top_offset =
            (static_cast<std::size_t>(row) * static_cast<std::size_t>(padded_stride)) + 1;
        const std::size_t bottom_offset =
            (static_cast<std::size_t>(row + 2) * static_cast<std::size_t>(padded_stride)) + 1;

        for (int col = 0; col < width; ++col) {
            const double center = padded[center_offset + static_cast<std::size_t>(col)];
            const double left = padded[center_offset + static_cast<std::size_t>(col - 1 + 1) - 1];
            const double right = padded[center_offset + static_cast<std::size_t>(col + 1)];
            const double top = padded[top_offset + static_cast<std::size_t>(col)];
            const double bottom = padded[bottom_offset + static_cast<std::size_t>(col)];
            double value = center;

            int active_directions = 0;
            if (truncated_abs_difference(center, left) >= threshold) {
                ++active_directions;
            }
            if (truncated_abs_difference(center, right) >= threshold) {
                ++active_directions;
            }
            if (truncated_abs_difference(center, top) >= threshold) {
                ++active_directions;
            }
            if (truncated_abs_difference(center, bottom) >= threshold) {
                ++active_directions;
            }

            if (active_directions >= 3) {
                value = round_to_stored_double(
                    (static_cast<long double>(center) +
                     static_cast<long double>(left) +
                     static_cast<long double>(right) +
                     static_cast<long double>(top) +
                     static_cast<long double>(bottom)) *
                    kSmoothingAverageScale
                );
            }

            output[output_row_offset + static_cast<std::size_t>(col)] = clamp_correction(value);
        }
    }

    return output;
}

std::vector<double> smooth_thresholded_edge_plane(
    std::span<const double> thresholded,
    int width,
    int height,
    int threshold
) {
    return build_smoothed_edge_plane(thresholded, width, height, threshold);
}

}  // namespace

std::vector<double> build_post_geometry_stage_3060_output(
    std::span<const std::int32_t> edge_response,
    std::span<const double> center,
    int width,
    int height,
    int stage_param0,
    int stage_param1,
    double scalar,
    int threshold
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post-geometry stage 0x3060 dimensions must be positive");
    }
    validate_edge_plane(edge_response, width, height, "edge_response");
    validate_center_plane(center, width, height, "center");

    const auto thresholded = build_thresholded_edge_plane(
        edge_response,
        width,
        height,
        stage_param0,
        stage_param1,
        scalar
    );
    const auto smoothed = smooth_thresholded_edge_plane(thresholded, width, height, threshold);

    const std::size_t sample_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<double> output(sample_count, 0.0);
    for (std::size_t index = 0; index < sample_count; ++index) {
        const double current = std::isfinite(center[index]) ? center[index] : 0.0;
        double correction = smoothed[index];
        if (correction < 0.0) {
            correction = round_to_stored_double(
                static_cast<long double>(correction) * kNegativeCorrectionScale
            );
        }
        if (current > kHighlightStart) {
            correction = round_to_stored_double(
                static_cast<long double>(correction) *
                (1.0L - ((static_cast<long double>(current) - kHighlightStart) *
                         kHighlightRangeReciprocal))
            );
        }
        if (current < kShadowStart) {
            correction = round_to_stored_double(
                static_cast<long double>(correction) *
                (1.0L - ((kShadowStart - static_cast<long double>(current)) * kShadowScale))
            );
        }
        output[index] = clamp_output(
            round_to_stored_double(static_cast<long double>(current) + correction)
        );
    }

    return output;
}

void apply_post_geometry_stage_3060(
    std::span<const std::int32_t> edge_response,
    std::span<double> center,
    int width,
    int height,
    int stage_param0,
    int stage_param1,
    double scalar,
    int threshold
) {
    const auto output = build_post_geometry_stage_3060_output(
        edge_response,
        center,
        width,
        height,
        stage_param0,
        stage_param1,
        scalar,
        threshold
    );
    std::copy(output.begin(), output.end(), center.begin());
}

}  // namespace dj1000
