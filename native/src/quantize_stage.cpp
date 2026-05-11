#include "dj1000/quantize_stage.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dj1000 {

std::uint8_t quantize_stage_sample(float value) {
    if (!std::isfinite(value) || !(value > 0.0f)) {
        return 0;
    }

    if (value >= 255.0f) {
        return 255;
    }

    const double truncated = std::trunc(static_cast<double>(value));
    return static_cast<std::uint8_t>(std::clamp(truncated, 0.0, 255.0));
}

std::vector<std::uint8_t> quantize_stage_plane(std::span<const float> plane) {
    std::vector<std::uint8_t> output(plane.size(), 0);
    std::transform(plane.begin(), plane.end(), output.begin(), quantize_stage_sample);
    return output;
}

QuantizedBytePlanes quantize_pregeometry_planes(
    std::span<const float> plane0,
    std::span<const float> plane1,
    std::span<const float> plane2
) {
    if (plane0.size() != plane1.size() || plane0.size() != plane2.size()) {
        throw std::runtime_error("pregeometry planes must have matching sizes for quantization");
    }

    return {
        .plane0 = quantize_stage_plane(plane0),
        .plane1 = quantize_stage_plane(plane1),
        .plane2 = quantize_stage_plane(plane2),
    };
}

}  // namespace dj1000
