#include "dj1000/pregeometry_pipeline.hpp"

#include "dj1000/pregeometry_normalize.hpp"
#include "dj1000/source_stage.hpp"

namespace dj1000 {

SourceSeedPlanes build_pregeometry_pipeline(std::span<const std::uint8_t> source, bool preview_mode) {
    auto planes = build_source_stage_planes(source, preview_mode);
    normalize_pregeometry_planes(planes.plane0, planes.plane1, planes.plane2, preview_mode);
    return planes;
}

}  // namespace dj1000
