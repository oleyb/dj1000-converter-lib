#include "dj1000/post_rgb_stage_3b00.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace dj1000 {

namespace {

struct LevelTransform {
    int numerator;
    int bias;
    int denominator;
};

constexpr std::array<LevelTransform, 7> kLevelTransforms{{
    {17, 0, 20},
    {9, 0, 10},
    {19, 0, 20},
    {1, 0, 1},
    {21, 0, 20},
    {11, 0, 10},
    {238, 4, 207},
}};

constexpr double kBreakpoint0 = static_cast<double>(50.0f);
constexpr double kBreakpoint1 = static_cast<double>(100.0f);
constexpr double kBreakpoint2 = static_cast<double>(127.0f);
constexpr double kSegment0Target = static_cast<double>(150.44999694824219f);
constexpr double kSegment1Base = static_cast<double>(150.44999694824219f);
constexpr double kSegment1Slope = static_cast<double>(1.0710002183914185f);
constexpr double kSegment2Base = static_cast<double>(204.0f);
constexpr double kSegment2Slope = static_cast<double>(0.9444441199302673f);
constexpr double kSegment3Base = static_cast<double>(229.5f);
constexpr double kSegment3Slope = static_cast<double>(0.19921879470348358f);

void validate_plane_sizes(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post-rgb stage 0x3b00 dimensions must be positive");
    }
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane0.size() < required || plane1.size() < required || plane2.size() < required) {
        throw std::runtime_error("post-rgb stage 0x3b00 planes are too small");
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

std::uint8_t transform_level_input(std::uint8_t sample, int level) {
    const auto transform = kLevelTransforms[static_cast<std::size_t>(level)];
    const int transformed =
        ((transform.numerator * static_cast<int>(sample)) + transform.bias) /
        transform.denominator;
    return static_cast<std::uint8_t>(std::clamp(transformed, 0, 255));
}

}  // namespace

std::vector<std::uint8_t> build_post_rgb_stage_3b00_lut(double scalar) {
    std::vector<std::uint8_t> lut(256, 0);
    const double segment0_slope =
        (kSegment0Target - scalar) * static_cast<double>(0.019999999552965164f);

    for (int input = 0; input <= 255; ++input) {
        const double x = static_cast<double>(input);
        double output = 0.0;
        if (input <= static_cast<int>(kBreakpoint0)) {
            output = scalar + (segment0_slope * x);
        } else if (input <= static_cast<int>(kBreakpoint1)) {
            output = kSegment1Base + ((x - kBreakpoint0) * kSegment1Slope);
        } else if (input <= static_cast<int>(kBreakpoint2)) {
            output = kSegment2Base + ((x - kBreakpoint1) * kSegment2Slope);
        } else {
            output = kSegment3Base + ((x - kBreakpoint2) * kSegment3Slope);
        }
        lut[static_cast<std::size_t>(input)] = trunc_clamp_byte(output);
    }

    return lut;
}

void apply_post_rgb_stage_3b00(
    std::span<std::uint8_t> plane0,
    std::span<std::uint8_t> plane1,
    std::span<std::uint8_t> plane2,
    int width,
    int height,
    int level,
    double scalar
) {
    validate_plane_sizes(plane0, plane1, plane2, width, height);
    if (level < 0 || level >= static_cast<int>(kLevelTransforms.size())) {
        throw std::runtime_error("post-rgb stage 0x3b00 level is out of range");
    }

    const std::size_t sample_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const auto active_plane0 = plane0.subspan(0, sample_count);
    const auto active_plane1 = plane1.subspan(0, sample_count);
    const auto active_plane2 = plane2.subspan(0, sample_count);

    const auto lut = build_post_rgb_stage_3b00_lut(scalar);
    for (std::size_t index = 0; index < sample_count; ++index) {
        active_plane0[index] = lut[transform_level_input(active_plane0[index], level)];
        active_plane1[index] = lut[transform_level_input(active_plane1[index], level)];
        active_plane2[index] = lut[transform_level_input(active_plane2[index], level)];
    }
}

}  // namespace dj1000
