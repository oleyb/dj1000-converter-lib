#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

[[nodiscard]] std::vector<std::uint8_t> build_post_rgb_stage_3b00_lut(double scalar);

void apply_post_rgb_stage_3b00(
    std::span<std::uint8_t> plane0,
    std::span<std::uint8_t> plane1,
    std::span<std::uint8_t> plane2,
    int width,
    int height,
    int level,
    double scalar
);

}  // namespace dj1000
