#pragma once

#include "dj1000/source_seed_stage.hpp"

#include <cstdint>
#include <span>

namespace dj1000 {

[[nodiscard]] SourceSeedPlanes build_pregeometry_pipeline(
    std::span<const std::uint8_t> source,
    bool preview_mode
);

}  // namespace dj1000
