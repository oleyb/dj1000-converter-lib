#include "dj1000/post_geometry_dual_scale.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

constexpr double kClampMin = -255.0;
constexpr double kClampMax = 255.0;
constexpr double kBaseScale = 2.0;

void validate_plane(std::span<const double> plane, int width, int height, const char* label) {
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " is too small for post geometry dual scale");
    }
}

double clamp_sample(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, kClampMin, kClampMax);
}

}  // namespace

void apply_post_geometry_dual_scale(
    std::span<double> plane0,
    std::span<double> plane1,
    int width,
    int height,
    double scalar
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post geometry dual scale dimensions must be positive");
    }
    validate_plane(plane0, width, height, "plane0");
    validate_plane(plane1, width, height, "plane1");

    const double factor = kBaseScale - scalar;
    const std::size_t sample_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (std::size_t index = 0; index < sample_count; ++index) {
        plane0[index] = clamp_sample(plane0[index] * factor);
        plane1[index] = clamp_sample(plane1[index] * factor);
    }
}

}  // namespace dj1000
