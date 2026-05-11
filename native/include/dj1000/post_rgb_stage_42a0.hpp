#pragma once

#include <cstdint>
#include <span>

namespace dj1000 {

void apply_post_rgb_stage_42a0(
    std::span<std::uint8_t> plane0,
    std::span<std::uint8_t> plane1,
    std::span<std::uint8_t> plane2,
    int width,
    int height,
    int scale0,
    int scale1,
    int scale2
);

}  // namespace dj1000
