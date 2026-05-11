#pragma once

#include "dj1000/quantize_stage.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

struct NonlargeStagePlanes {
    std::vector<std::uint8_t> plane0;
    std::vector<std::uint8_t> plane1;
    std::vector<std::uint8_t> plane2;
};

[[nodiscard]] QuantizedBytePlanes build_nonlarge_quantized_planes_from_source(
    std::span<const std::uint8_t> source,
    bool preview_mode
);

[[nodiscard]] NonlargeStagePlanes build_nonlarge_stage_planes_from_source(
    std::span<const std::uint8_t> source,
    bool preview_mode
);

}  // namespace dj1000
