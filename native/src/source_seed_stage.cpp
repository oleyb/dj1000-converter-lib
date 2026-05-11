#include "dj1000/source_seed_stage.hpp"

#include "dj1000/pregeometry_normalize.hpp"

#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

struct SourceSeedConfig {
    int input_stride;
    int active_width;
    int active_rows;
    int output_stride;
    int output_rows;
};

SourceSeedConfig config_for_mode(bool preview_mode) {
    if (preview_mode) {
        return {
            .input_stride = kPreviewSourceSeedInputStride,
            .active_width = kPreviewSourceSeedActiveWidth,
            .active_rows = kPreviewSourceSeedActiveRows,
            .output_stride = kPreviewFloatPlaneStride,
            .output_rows = kPreviewFloatPlaneRows,
        };
    }
    return {
        .input_stride = kNormalSourceSeedInputStride,
        .active_width = kNormalSourceSeedActiveWidth,
        .active_rows = kNormalSourceSeedActiveRows,
        .output_stride = kNormalFloatPlaneStride,
        .output_rows = kNormalFloatPlaneRows,
    };
}

void validate_source(std::span<const std::uint8_t> source, const SourceSeedConfig& config) {
    const std::size_t required = static_cast<std::size_t>(config.input_stride) *
                                 static_cast<std::size_t>(config.active_rows);
    if (source.size() < required) {
        throw std::runtime_error("source seed input is too small");
    }
}

float smoothed_row_sample(std::span<const std::uint8_t> source, const SourceSeedConfig& config, int row, int col) {
    const std::size_t row_offset =
        static_cast<std::size_t>(row) * static_cast<std::size_t>(config.input_stride);
    const auto sample = [&](int sample_col) {
        return static_cast<float>(source[row_offset + static_cast<std::size_t>(sample_col)]);
    };

    if (config.active_width == 1) {
        return sample(0);
    }
    if (col == 0) {
        return sample(0) * 0.5f + sample(1) * 0.5f;
    }
    if (col == config.active_width - 1) {
        return sample(col - 1) * 0.5f + sample(col) * 0.5f;
    }
    return sample(col - 1) * 0.25f + sample(col) * 0.5f + sample(col + 1) * 0.25f;
}

void spread_residual(std::vector<float>& plane, const SourceSeedConfig& config, int row, int col, float residual) {
    const std::size_t row_offset =
        static_cast<std::size_t>(row) * static_cast<std::size_t>(config.output_stride);
    if (col > 0) {
        plane[row_offset + static_cast<std::size_t>(col - 1)] += residual * 0.25f;
    }
    plane[row_offset + static_cast<std::size_t>(col)] += residual * 0.5f;
    if (col + 1 < config.active_width) {
        plane[row_offset + static_cast<std::size_t>(col + 1)] += residual * 0.25f;
    }
}

}  // namespace

std::size_t expected_source_seed_input_byte_count(bool preview_mode) {
    const auto config = config_for_mode(preview_mode);
    return static_cast<std::size_t>(config.input_stride) * static_cast<std::size_t>(config.active_rows);
}

SourceSeedPlanes build_source_seed_planes(std::span<const std::uint8_t> source, bool preview_mode) {
    const auto config = config_for_mode(preview_mode);
    validate_source(source, config);

    SourceSeedPlanes planes{
        .plane0 = std::vector<float>(expected_pregeometry_float_count(preview_mode), 0.0f),
        .plane1 = std::vector<float>(expected_pregeometry_float_count(preview_mode), 0.0f),
        .plane2 = std::vector<float>(expected_pregeometry_float_count(preview_mode), 0.0f),
    };

    for (int row = 0; row < config.active_rows; ++row) {
        const std::size_t output_row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(config.output_stride);
        for (int col = 0; col < config.active_width; ++col) {
            planes.plane1[output_row_offset + static_cast<std::size_t>(col)] =
                smoothed_row_sample(source, config, row, col);
        }

        // This currently reproduces the impulse-exact seed we have isolated
        // so far. Later neighborhood/cancellation passes inside 0x10005eb0
        // are still upstream work.
        for (int col = 0; col < config.active_width; ++col) {
            const std::size_t input_index =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(config.input_stride) +
                static_cast<std::size_t>(col);
            const float site_value = static_cast<float>(source[input_index]);
            if (((row & 1) == 0) && ((col & 1) == 0)) {
                spread_residual(planes.plane0, config, row, col, site_value);
            } else if (((row & 1) == 1) && ((col & 1) == 1)) {
                spread_residual(planes.plane2, config, row, col, site_value);
            }
        }
    }

    return planes;
}

}  // namespace dj1000
