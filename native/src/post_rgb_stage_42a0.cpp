#include "dj1000/post_rgb_stage_42a0.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace dj1000 {

namespace {

constexpr double kScaleFactor = 0.01;

void validate_plane_sizes(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post-rgb stage 0x42a0 dimensions must be positive");
    }
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane0.size() < required || plane1.size() < required || plane2.size() < required) {
        throw std::runtime_error("post-rgb stage 0x42a0 planes are too small");
    }
}

std::uint8_t trunc_clamp_byte(double value) {
    if (!std::isfinite(value) || !(value > 0.0)) {
        return 0;
    }
    if (value >= 255.0) {
        return 255;
    }
    return static_cast<std::uint8_t>(std::trunc(value));
}

void apply_scale(std::span<std::uint8_t> plane, int scale) {
    if (scale == 100) {
        return;
    }

    const double multiplier = static_cast<double>(scale) * kScaleFactor;
    for (std::uint8_t& sample : plane) {
        sample = trunc_clamp_byte(static_cast<double>(sample) * multiplier);
    }
}

}  // namespace

void apply_post_rgb_stage_42a0(
    std::span<std::uint8_t> plane0,
    std::span<std::uint8_t> plane1,
    std::span<std::uint8_t> plane2,
    int width,
    int height,
    int scale0,
    int scale1,
    int scale2
) {
    validate_plane_sizes(plane0, plane1, plane2, width, height);

    const std::size_t sample_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const auto active_plane0 = plane0.subspan(0, sample_count);
    const auto active_plane1 = plane1.subspan(0, sample_count);
    const auto active_plane2 = plane2.subspan(0, sample_count);

    apply_scale(active_plane0, scale0);
    apply_scale(active_plane1, scale1);
    apply_scale(active_plane2, scale2);
}

}  // namespace dj1000
