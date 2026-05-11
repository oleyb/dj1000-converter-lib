#include "dj1000/post_geometry_stage_3890.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

constexpr double kHundredth = static_cast<double>(0.01f);
constexpr std::array<int, 7> kPlane0ScaleHundredths{{60, 76, 95, 120, 151, 190, 240}};
constexpr std::array<int, 7> kPlane1FromPlane0Hundredths{{-12, -15, -19, -24, -30, -38, -48}};

void validate_plane(std::span<const double> plane, int width, int height, const char* label) {
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " is too small for post geometry stage 3890");
    }
}

double clamp_upper_255(double value) {
    return value > 255.0 ? 255.0 : value;
}

}  // namespace

PostGeometryStage3890Coefficients get_post_geometry_stage_3890_coefficients(int level) {
    if (level < 0 || level >= static_cast<int>(kPlane0ScaleHundredths.size())) {
        throw std::runtime_error("post geometry stage 3890 level must be in the 0..6 range");
    }
    const std::size_t index = static_cast<std::size_t>(level);
    const double plane0_scale = static_cast<double>(kPlane0ScaleHundredths[index]) * kHundredth;
    const double plane1_from_plane0 =
        static_cast<double>(kPlane1FromPlane0Hundredths[index]) * kHundredth;
    return {
        .plane0_scale = plane0_scale,
        .plane1_from_plane0 = plane1_from_plane0,
        .plane1_scale = plane0_scale,
    };
}

void apply_post_geometry_stage_3890(
    std::span<double> plane0,
    std::span<double> plane1,
    int width,
    int height,
    int level
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post geometry stage 3890 dimensions must be positive");
    }
    validate_plane(plane0, width, height, "plane0");
    validate_plane(plane1, width, height, "plane1");

    const auto coefficients = get_post_geometry_stage_3890_coefficients(level);
    const double plane1_ratio =
        coefficients.plane1_scale != 0.0
            ? (coefficients.plane1_from_plane0 / coefficients.plane1_scale)
            : 0.0;
    const std::size_t sample_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (std::size_t index = 0; index < sample_count; ++index) {
        const double input0 = plane0[index];
        const double input1 = plane1[index];
        plane0[index] = clamp_upper_255(input0 * coefficients.plane0_scale);
        const volatile double plane0_contribution = input0 * plane1_ratio;
        const volatile double mixed = plane0_contribution + input1;
        const volatile double scaled = mixed * coefficients.plane1_scale;
        plane1[index] = clamp_upper_255(scaled);
    }
}

}  // namespace dj1000
