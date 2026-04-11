#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

struct QuantizedBytePlanes {
    std::vector<std::uint8_t> plane0;
    std::vector<std::uint8_t> plane1;
    std::vector<std::uint8_t> plane2;
};

[[nodiscard]] std::uint8_t quantize_stage_sample(float value);

[[nodiscard]] std::vector<std::uint8_t> quantize_stage_plane(
    std::span<const float> plane
);

[[nodiscard]] QuantizedBytePlanes quantize_pregeometry_planes(
    std::span<const float> plane0,
    std::span<const float> plane1,
    std::span<const float> plane2
);

}  // namespace dj1000
