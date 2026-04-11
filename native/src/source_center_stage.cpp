#include "dj1000/source_center_stage.hpp"

#include "dj1000/bright_vertical_gate.hpp"
#include "dj1000/pregeometry_normalize.hpp"
#include "dj1000/source_seed_stage.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace dj1000 {

namespace {

struct SourceCenterConfig {
    int input_stride;
    int active_width;
    int active_rows;
    int output_stride;
};

SourceCenterConfig config_for_mode(bool preview_mode) {
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

float source_sample(
    std::span<const std::uint8_t> source,
    const SourceCenterConfig& config,
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
    if (value < 0.0) {
        return 0.0f;
    }
    if (value > 255.0) {
        return 255.0f;
    }
    return static_cast<float>(value);
}

}  // namespace

std::vector<float> build_source_center_plane(std::span<const std::uint8_t> source, bool preview_mode) {
    const auto config = config_for_mode(preview_mode);
    if (source.size() < expected_source_seed_input_byte_count(preview_mode)) {
        throw std::runtime_error("source center input is too small");
    }

    constexpr float kDifferenceThreshold = 2.0f;
    constexpr float kValidityThreshold = 250.0f;

    std::vector<float> pair0(expected_pregeometry_float_count(preview_mode), 0.0f);
    std::vector<float> pair1(expected_pregeometry_float_count(preview_mode), 0.0f);
    std::vector<float> pair2(expected_pregeometry_float_count(preview_mode), 0.0f);
    std::vector<float> pair3(expected_pregeometry_float_count(preview_mode), 0.0f);
    std::vector<float> center(expected_pregeometry_float_count(preview_mode), 0.0f);

    for (int row = 0; row < config.active_rows; ++row) {
        const std::size_t row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(config.output_stride);
        for (int col = 0; col < config.active_width; ++col) {
            const float current = source_sample(source, config, row, col);
            const float left = source_sample(source, config, row, col - 1);
            const float right = source_sample(source, config, row, col + 1);
            const float far_left = source_sample(source, config, row, col - 2);
            const float far_right = source_sample(source, config, row, col + 2);

            int far_count = 1;
            double far_sum = current;
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

            float adjusted = static_cast<float>((static_cast<double>(left) + static_cast<double>(right)) * 0.5);
            if (std::fabs(left - right) >= kDifferenceThreshold && far_count >= 3 && near_count >= 2) {
                const double far_avg = far_sum / static_cast<double>(far_count);
                if (far_avg > 0.0) {
                    const double near_avg = near_sum / static_cast<double>(near_count);
                    adjusted = clamp_0_255(static_cast<double>(current) * near_avg / far_avg);
                } else {
                    adjusted = 0.0f;
                }
            }

            const std::size_t index = row_offset + static_cast<std::size_t>(col);
            if ((row & 1) == 0) {
                if ((col & 1) == 0) {
                    pair3[index] = current;
                    pair2[index] = adjusted;
                } else {
                    pair2[index] = current;
                    pair3[index] = adjusted;
                }
                center[index] = static_cast<float>(
                    (static_cast<double>(pair2[index]) + static_cast<double>(pair3[index])) * 0.5
                );
            } else {
                if ((col & 1) == 0) {
                    pair1[index] = current;
                    pair0[index] = adjusted;
                } else {
                    pair0[index] = current;
                    pair1[index] = adjusted;
                }
                center[index] = static_cast<float>(
                    (static_cast<double>(pair0[index]) + static_cast<double>(pair1[index])) * 0.5
                );
            }
        }
    }

    return apply_bright_vertical_gate(center, preview_mode);
}

}  // namespace dj1000
