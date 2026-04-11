#include "dj1000/post_geometry_center_scale.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

constexpr double kCenterMidpoint = 127.0;
constexpr double kHighScaleSlope = 1.0 / 23.0;
constexpr double kLowThreshold = 20.0;
constexpr double kLowScaleSlope = 0.1;

void validate_plane(std::span<const double> plane, int width, int height, const char* label) {
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " is too small for post geometry center scale");
    }
}

std::uint8_t quantize_center_sample(double value) {
    if (!std::isfinite(value)) {
        return 0;
    }
    const auto truncated = static_cast<std::int32_t>(value);
    return static_cast<std::uint8_t>(truncated & 0xff);
}

double compute_center_scale(double center_value) {
    if (center_value > kCenterMidpoint) {
        const double scaled = 1.0 - ((center_value - kCenterMidpoint) * kHighScaleSlope);
        return scaled > 0.0 ? scaled : 0.0;
    }
    if (center_value < kLowThreshold) {
        return 1.0 - ((kLowThreshold - center_value) * kLowScaleSlope);
    }
    return 1.0;
}

}  // namespace

void apply_post_geometry_center_scale(
    std::span<double> delta0,
    std::span<double> center,
    std::span<double> delta2,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post geometry center scale dimensions must be positive");
    }
    validate_plane(delta0, width, height, "delta0");
    validate_plane(center, width, height, "center");
    validate_plane(delta2, width, height, "delta2");

    const std::size_t sample_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (std::size_t index = 0; index < sample_count; ++index) {
        const double center_value = center[index];
        const double scale = compute_center_scale(center_value);
        delta0[index] *= scale;
        delta2[index] *= scale;
        center[index] = static_cast<double>(quantize_center_sample(center_value));
    }
}

}  // namespace dj1000
