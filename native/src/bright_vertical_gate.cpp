#include "dj1000/bright_vertical_gate.hpp"

#include "dj1000/pregeometry_normalize.hpp"

#include <algorithm>
#include <stdexcept>

namespace dj1000 {

namespace {

struct BrightVerticalGateConfig {
    int stride;
    int active_width;
    int active_rows;
};

BrightVerticalGateConfig config_for_mode(bool preview_mode) {
    if (preview_mode) {
        return {
            .stride = kPreviewFloatPlaneStride,
            .active_width = kPreviewFloatPlaneActiveWidth,
            .active_rows = kPreviewFloatPlaneActiveRows,
        };
    }

    return {
        .stride = kNormalFloatPlaneStride,
        .active_width = kNormalFloatPlaneActiveWidth,
        .active_rows = kNormalFloatPlaneActiveRows,
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

}  // namespace

std::vector<float> apply_bright_vertical_gate(std::span<const float> plane, bool preview_mode) {
    if (plane.size() != expected_pregeometry_float_count(preview_mode)) {
        throw std::runtime_error("bright vertical gate input size does not match mode");
    }

    constexpr float kBrightThreshold = 160.0f;
    constexpr float kVerticalDeltaThreshold = 3.0f;
    constexpr float kMinValue = 0.0f;
    constexpr float kMaxValue = 255.0f;

    const auto config = config_for_mode(preview_mode);
    std::vector<float> output(plane.begin(), plane.end());

    for (int row = 0; row < config.active_rows; ++row) {
        const int top_row = reflect_index(row - 1, config.active_rows);
        const int bottom_row = reflect_index(row + 1, config.active_rows);

        const std::size_t row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(config.stride);
        const std::size_t top_row_offset =
            static_cast<std::size_t>(top_row) * static_cast<std::size_t>(config.stride);
        const std::size_t bottom_row_offset =
            static_cast<std::size_t>(bottom_row) * static_cast<std::size_t>(config.stride);

        for (int col = 0; col < config.active_width; ++col) {
            const std::size_t index = row_offset + static_cast<std::size_t>(col);
            const float value = plane[index];
            const float vertical_average =
                (plane[top_row_offset + static_cast<std::size_t>(col)] +
                 plane[bottom_row_offset + static_cast<std::size_t>(col)]) *
                0.5f;

            float repaired = value;
            if (value > kBrightThreshold && (vertical_average - value) > kVerticalDeltaThreshold) {
                repaired = vertical_average;
            }

            output[index] = std::clamp(repaired, kMinValue, kMaxValue);
        }
    }

    return output;
}

}  // namespace dj1000
