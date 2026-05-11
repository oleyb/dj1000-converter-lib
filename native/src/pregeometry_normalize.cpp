#include "dj1000/pregeometry_normalize.hpp"

#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

struct NormalizeConfig {
    int stored_stride;
    int stored_rows;
    int active_width;
    int active_rows;
};

constexpr float kReferenceLow = 20.0f;
constexpr float kReferenceHigh = 127.0f;
constexpr float kOutputHigh = 255.0f;

NormalizeConfig config_for_mode(bool preview_mode) {
    if (preview_mode) {
        return {
            .stored_stride = kPreviewFloatPlaneStride,
            .stored_rows = kPreviewFloatPlaneRows,
            .active_width = kPreviewFloatPlaneActiveWidth,
            .active_rows = kPreviewFloatPlaneActiveRows,
        };
    }
    return {
        .stored_stride = kNormalFloatPlaneStride,
        .stored_rows = kNormalFloatPlaneRows,
        .active_width = kNormalFloatPlaneActiveWidth,
        .active_rows = kNormalFloatPlaneActiveRows,
    };
}

void validate_plane(std::span<const float> plane, const NormalizeConfig& config, const char* label) {
    const std::size_t required =
        static_cast<std::size_t>(config.stored_stride) * static_cast<std::size_t>(config.stored_rows);
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " float plane is too small");
    }
}

float clamp_stage_sample(float value) {
    if (value > kOutputHigh) {
        value = kOutputHigh;
    }
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    if (bits > 0x80000000u) {
        return 0.0f;
    }
    return value;
}

bool is_reference_sample_valid(float sample) {
    return sample > kReferenceLow && sample < kReferenceHigh;
}

}  // namespace

std::size_t expected_pregeometry_float_count(bool preview_mode) {
    const auto config = config_for_mode(preview_mode);
    return static_cast<std::size_t>(config.stored_stride) * static_cast<std::size_t>(config.stored_rows);
}

void normalize_pregeometry_planes(
    std::span<float> plane0,
    std::span<float> plane1,
    std::span<float> plane2,
    bool preview_mode
) {
    const auto config = config_for_mode(preview_mode);
    validate_plane(plane0, config, "plane0");
    validate_plane(plane1, config, "plane1");
    validate_plane(plane2, config, "plane2");

    double plane0_sum = 0.0;
    double plane1_sum = 0.0;
    double plane2_sum = 0.0;
    int plane0_count = 0;
    int plane1_count = 0;
    int plane2_count = 0;

    for (int row = 0; row < config.active_rows; ++row) {
        const std::size_t row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(config.stored_stride);
        for (int col = 0; col < config.active_width; ++col) {
            const std::size_t index = row_offset + static_cast<std::size_t>(col);
            const float reference_sample = plane1[index];
            if (!is_reference_sample_valid(reference_sample)) {
                continue;
            }
            plane0_sum += plane0[index];
            plane1_sum += reference_sample;
            plane2_sum += plane2[index];
            ++plane0_count;
            ++plane1_count;
            ++plane2_count;
        }
    }

    const double plane0_average = plane0_count > 0 ? plane0_sum / static_cast<double>(plane0_count) : 0.0;
    const double plane1_average = plane1_count > 0 ? plane1_sum / static_cast<double>(plane1_count) : 0.0;
    const double plane2_average = plane2_count > 0 ? plane2_sum / static_cast<double>(plane2_count) : 0.0;

    if (plane0_average == 0.0 && plane1_average == 0.0 && plane2_average == 0.0) {
        return;
    }

    const double plane0_scale = plane0_average > 0.0 ? plane1_average / plane0_average : 1.0;
    const double plane2_scale = plane2_average > 0.0 ? plane1_average / plane2_average : 1.0;

    const std::size_t total_samples = expected_pregeometry_float_count(preview_mode);
    for (std::size_t index = 0; index < total_samples; ++index) {
        plane0[index] = clamp_stage_sample(static_cast<float>(static_cast<double>(plane0[index]) * plane0_scale));
        plane1[index] = clamp_stage_sample(plane1[index]);
        plane2[index] = clamp_stage_sample(static_cast<float>(static_cast<double>(plane2[index]) * plane2_scale));
    }
}

}  // namespace dj1000
