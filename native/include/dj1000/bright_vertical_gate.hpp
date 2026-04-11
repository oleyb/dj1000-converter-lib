#pragma once

#include <span>
#include <vector>

namespace dj1000 {

[[nodiscard]] std::vector<float> apply_bright_vertical_gate(
    std::span<const float> plane,
    bool preview_mode
);

}  // namespace dj1000
