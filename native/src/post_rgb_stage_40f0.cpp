#include "dj1000/post_rgb_stage_40f0.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>

namespace dj1000 {

namespace {

struct OffsetAdjustment {
    int sign;
    int magnitude;
};

constexpr std::array<OffsetAdjustment, 6> kSelector0Adjustments{{
    {1, 0x15},
    {1, 0x0E},
    {1, 0x07},
    {1, 0x00},
    {-1, 0x02},
    {-1, 0x04},
}};

constexpr std::array<OffsetAdjustment, 6> kSelector1Adjustments{{
    {-1, 0x15},
    {-1, 0x0E},
    {-1, 0x07},
    {1, 0x00},
    {1, 0x07},
    {1, 0x0E},
}};

constexpr OffsetAdjustment kSelector0DefaultAdjustment{-1, 0x06};
constexpr OffsetAdjustment kSelector1DefaultAdjustment{1, 0x15};

void validate_plane_sizes(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post-rgb stage 0x40f0 dimensions must be positive");
    }
    const std::size_t required =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane0.size() < required || plane1.size() < required || plane2.size() < required) {
        throw std::runtime_error("post-rgb stage 0x40f0 planes are too small");
    }
}

OffsetAdjustment select_adjustment(int level, int selector) {
    if (level == 3) {
        return OffsetAdjustment{1, 0};
    }
    if (selector == 0) {
        if (level >= 0 && level < static_cast<int>(kSelector0Adjustments.size())) {
            return kSelector0Adjustments[static_cast<std::size_t>(level)];
        }
        return kSelector0DefaultAdjustment;
    }
    if (selector == 1) {
        if (level >= 0 && level < static_cast<int>(kSelector1Adjustments.size())) {
            return kSelector1Adjustments[static_cast<std::size_t>(level)];
        }
        return kSelector1DefaultAdjustment;
    }
    throw std::runtime_error("post-rgb stage 0x40f0 selector must be 0 or 1");
}

void apply_adjustment(std::span<std::uint8_t> plane, int adjustment) {
    if (adjustment == 0) {
        return;
    }

    for (std::uint8_t& sample : plane) {
        const int adjusted = std::clamp(static_cast<int>(sample) + adjustment, 0, 255);
        sample = static_cast<std::uint8_t>(adjusted);
    }
}

}  // namespace

void apply_post_rgb_stage_40f0(
    std::span<std::uint8_t> plane0,
    std::span<std::uint8_t> plane1,
    std::span<std::uint8_t> plane2,
    int width,
    int height,
    int level,
    int selector
) {
    validate_plane_sizes(plane0, plane1, plane2, width, height);

    const std::size_t sample_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const auto active_plane0 = plane0.subspan(0, sample_count);
    const auto active_plane1 = plane1.subspan(0, sample_count);
    const auto active_plane2 = plane2.subspan(0, sample_count);

    const OffsetAdjustment adjustment = select_adjustment(level, selector);
    const int signed_offset = adjustment.sign * adjustment.magnitude;
    apply_adjustment(active_plane0, signed_offset);
    apply_adjustment(active_plane1, signed_offset);
    apply_adjustment(active_plane2, signed_offset);
}

}  // namespace dj1000
