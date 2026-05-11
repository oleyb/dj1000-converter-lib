#include "dj1000/post_geometry_rgb_convert.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace dj1000 {

namespace {

constexpr double kPlane0Contribution = 0.3;
constexpr double kPlane2Contribution = 0.11;
constexpr double kCenterScale = 1.0 / (1.0 - kPlane0Contribution - kPlane2Contribution);

void validate_planes(
    std::span<const double> plane0,
    std::span<const double> plane1,
    std::span<const double> plane2,
    int width,
    int height
) {
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane0.size() < required || plane1.size() < required || plane2.size() < required) {
        throw std::runtime_error("post geometry rgb convert planes are too small");
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

double clamp_channel(double value) {
    if (!std::isfinite(value) || !(value > 0.0)) {
        return 0.0;
    }
    if (value >= 255.0) {
        return 255.0;
    }
    return value;
}

}  // namespace

RgbBytePlanes convert_post_geometry_rgb_planes(
    std::span<const double> plane0,
    std::span<const double> plane1,
    std::span<const double> plane2,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post geometry rgb convert dimensions must be positive");
    }
    validate_planes(plane0, plane1, plane2, width, height);

    const std::size_t sample_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    RgbBytePlanes output{
        .plane0 = std::vector<std::uint8_t>(sample_count, 0),
        .plane1 = std::vector<std::uint8_t>(sample_count, 0),
        .plane2 = std::vector<std::uint8_t>(sample_count, 0),
    };

    for (std::size_t index = 0; index < sample_count; ++index) {
        const double input0 = plane0[index];
        const double input1 = plane1[index];
        const double input2 = plane2[index];

        double output0_value = input1;
        output0_value += input0;
        const double output0 = clamp_channel(output0_value);

        double output2_value = input1;
        output2_value += input2;
        const double output2 = clamp_channel(output2_value);

        const volatile double output0_weighted = output0 * kPlane0Contribution;
        const volatile double output2_weighted = output2 * kPlane2Contribution;
        double output1_value = input1;
        output1_value -= output0_weighted;
        output1_value -= output2_weighted;
        const volatile double output1_scaled = output1_value * kCenterScale;
        const double output1 = clamp_channel(output1_scaled);

        output.plane0[index] = trunc_clamp_byte(output0);
        output.plane1[index] = trunc_clamp_byte(output1);
        output.plane2[index] = trunc_clamp_byte(output2);
    }

    return output;
}

}  // namespace dj1000
