#include "dj1000/source_stage.hpp"

#include "dj1000/bright_vertical_gate.hpp"
#include "dj1000/pregeometry_normalize.hpp"
#include "dj1000/source_seed_stage.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace dj1000 {

namespace {

struct SourceStageConfig {
    int input_stride;
    int active_width;
    int active_rows;
    int output_stride;
};

SourceStageConfig config_for_mode(bool preview_mode) {
    if (preview_mode) {
        return {
            .input_stride = kPreviewSourceSeedInputStride,
            .active_width = kPreviewSourceSeedActiveWidth,
            .active_rows = kPreviewSourceSeedActiveRows,
            .output_stride = kPreviewFloatPlaneStride,
        };
    }
    return {
        .input_stride = kNormalSourceSeedInputStride,
        .active_width = kNormalSourceSeedActiveWidth,
        .active_rows = kNormalSourceSeedActiveRows,
        .output_stride = kNormalFloatPlaneStride,
    };
}

int reflect_index(int index, int limit) {
    if (limit <= 1) {
        return 0;
    }
    while (index < 0 || index >= limit) {
        if (index < 0) {
            index = -index;
        } else {
            index = (limit - 1) - (index - (limit - 1));
        }
    }
    return index;
}

float sample_source(
    std::span<const std::uint8_t> source,
    const SourceStageConfig& config,
    int row,
    int col
) {
    const int reflected_col = reflect_index(col, config.active_width);
    const std::size_t index =
        static_cast<std::size_t>(row) * static_cast<std::size_t>(config.input_stride) +
        static_cast<std::size_t>(reflected_col);
    return static_cast<float>(source[index]);
}

float clamp_0_255(double value) {
    if (!(value > 0.0)) {
        return 0.0f;
    }
    if (value > 255.0) {
        return 255.0f;
    }
    return static_cast<float>(value);
}

}  // namespace

SourceSeedPlanes build_source_stage_planes(std::span<const std::uint8_t> source, bool preview_mode) {
    const auto config = config_for_mode(preview_mode);
    if (source.size() < expected_source_seed_input_byte_count(preview_mode)) {
        throw std::runtime_error("source stage input is too small");
    }

    constexpr float kDifferenceThreshold = 2.0f;
    constexpr float kValidityThreshold = 250.0f;

    SourceSeedPlanes planes{
        .plane0 = std::vector<float>(expected_pregeometry_float_count(preview_mode), 0.0f),
        .plane1 = std::vector<float>(expected_pregeometry_float_count(preview_mode), 0.0f),
        .plane2 = std::vector<float>(expected_pregeometry_float_count(preview_mode), 0.0f),
    };

    std::vector<float> adjusted(expected_pregeometry_float_count(preview_mode), 0.0f);
    std::vector<float> current(expected_pregeometry_float_count(preview_mode), 0.0f);
    std::vector<float> plane0_seed(expected_pregeometry_float_count(preview_mode), 0.0f);
    std::vector<float> plane2_seed(expected_pregeometry_float_count(preview_mode), 0.0f);
    std::vector<double> ratio(expected_pregeometry_float_count(preview_mode), 0.0);

    for (int row = 0; row < config.active_rows; ++row) {
        const std::size_t row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(config.output_stride);
        for (int col = 0; col < config.active_width; ++col) {
            const float current_sample = sample_source(source, config, row, col);
            const float left = sample_source(source, config, row, col - 1);
            const float right = sample_source(source, config, row, col + 1);
            const float far_left = sample_source(source, config, row, col - 2);
            const float far_right = sample_source(source, config, row, col + 2);

            int far_count = 1;
            double far_sum = current_sample;
            if (far_left < kValidityThreshold) {
                far_sum += far_left;
                far_count += 1;
            }
            if (far_right < kValidityThreshold) {
                far_sum += far_right;
                far_count += 1;
            }

            int near_count = 0;
            double near_sum = 0.0;
            if (left < kValidityThreshold) {
                near_sum += left;
                near_count += 1;
            }
            if (right < kValidityThreshold) {
                near_sum += right;
                near_count += 1;
            }

            float adjusted_sample =
                static_cast<float>((static_cast<double>(left) + static_cast<double>(right)) * 0.5);
            if (std::fabs(left - right) >= kDifferenceThreshold && far_count >= 3 && near_count >= 2) {
                const double far_avg = far_sum / static_cast<double>(far_count);
                if (far_avg > 0.0) {
                    const double near_avg = near_sum / static_cast<double>(near_count);
                    adjusted_sample = clamp_0_255(
                        static_cast<double>(current_sample) * near_avg / far_avg
                    );
                } else {
                    adjusted_sample = 0.0f;
                }
            }

            const std::size_t index = row_offset + static_cast<std::size_t>(col);
            current[index] = current_sample;
            adjusted[index] = adjusted_sample;
            planes.plane1[index] = static_cast<float>(
                (static_cast<double>(current_sample) + static_cast<double>(adjusted_sample)) * 0.5
            );

            const double diff = static_cast<double>(current_sample) - static_cast<double>(adjusted_sample);
            const double signed_diff = (((row ^ col) & 1) == 0) ? diff : -diff;
            const float seed = clamp_0_255(signed_diff * 0.5);
            if ((row & 1) == 0) {
                plane0_seed[index] = seed;
            } else {
                plane2_seed[index] = seed;
            }
        }
    }

    planes.plane1 = apply_bright_vertical_gate(planes.plane1, preview_mode);

    for (int row = 0; row < config.active_rows; ++row) {
        const int top_row = reflect_index(row - 1, config.active_rows);
        const int bottom_row = reflect_index(row + 1, config.active_rows);
        const std::size_t row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(config.output_stride);
        const std::size_t top_row_offset =
            static_cast<std::size_t>(top_row) * static_cast<std::size_t>(config.output_stride);
        const std::size_t bottom_row_offset =
            static_cast<std::size_t>(bottom_row) * static_cast<std::size_t>(config.output_stride);

        for (int col = 0; col < config.active_width; ++col) {
            const std::size_t index = row_offset + static_cast<std::size_t>(col);
            const double vertical_average =
                (static_cast<double>(planes.plane1[top_row_offset + static_cast<std::size_t>(col)]) +
                 static_cast<double>(planes.plane1[bottom_row_offset + static_cast<std::size_t>(col)])) *
                0.5;

            if (vertical_average <= 0.0) {
                ratio[index] = 0.0;
            } else {
                ratio[index] = std::clamp(
                    static_cast<double>(planes.plane1[index]) / vertical_average,
                    0.0,
                    255.0
                );
            }
        }
    }

    for (int row = 0; row < config.active_rows; ++row) {
        const int top_row = reflect_index(row - 1, config.active_rows);
        const int bottom_row = reflect_index(row + 1, config.active_rows);
        const std::size_t row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(config.output_stride);
        const std::size_t top_row_offset =
            static_cast<std::size_t>(top_row) * static_cast<std::size_t>(config.output_stride);
        const std::size_t bottom_row_offset =
            static_cast<std::size_t>(bottom_row) * static_cast<std::size_t>(config.output_stride);

        for (int col = 0; col < config.active_width; ++col) {
            const std::size_t index = row_offset + static_cast<std::size_t>(col);
            if ((row & 1) == 0) {
                planes.plane0[index] = plane0_seed[index];
                const double fill =
                    (static_cast<double>(plane2_seed[top_row_offset + static_cast<std::size_t>(col)]) +
                     static_cast<double>(plane2_seed[bottom_row_offset + static_cast<std::size_t>(col)])) *
                    0.5;
                planes.plane2[index] = clamp_0_255(ratio[index] * fill);
            } else {
                planes.plane2[index] = plane2_seed[index];
                const double fill =
                    (static_cast<double>(plane0_seed[top_row_offset + static_cast<std::size_t>(col)]) +
                     static_cast<double>(plane0_seed[bottom_row_offset + static_cast<std::size_t>(col)])) *
                    0.5;
                planes.plane0[index] = clamp_0_255(ratio[index] * fill);
            }
        }
    }

    return planes;
}

}  // namespace dj1000
