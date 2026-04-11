#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace dj1000 {

[[nodiscard]] std::vector<float> build_source_center_plane(
    std::span<const std::uint8_t> source,
    bool preview_mode
);

}  // namespace dj1000
