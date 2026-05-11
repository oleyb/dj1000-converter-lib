#include "dj1000/nonlarge_source_pipeline.hpp"

#include "dj1000/nonlarge_geometry.hpp"
#include "dj1000/pregeometry_pipeline.hpp"

namespace dj1000 {

QuantizedBytePlanes build_nonlarge_quantized_planes_from_source(
    std::span<const std::uint8_t> source,
    bool preview_mode
) {
    const auto pregeometry = build_pregeometry_pipeline(source, preview_mode);
    return quantize_pregeometry_planes(
        pregeometry.plane0,
        pregeometry.plane1,
        pregeometry.plane2
    );
}

NonlargeStagePlanes build_nonlarge_stage_planes_from_source(
    std::span<const std::uint8_t> source,
    bool preview_mode
) {
    const auto quantized = build_nonlarge_quantized_planes_from_source(source, preview_mode);
    return {
        .plane0 = build_nonlarge_stage_plane(
            quantized.plane0,
            quantized.plane1,
            quantized.plane2,
            0,
            preview_mode
        ),
        .plane1 = build_nonlarge_stage_plane(
            quantized.plane0,
            quantized.plane1,
            quantized.plane2,
            1,
            preview_mode
        ),
        .plane2 = build_nonlarge_stage_plane(
            quantized.plane0,
            quantized.plane1,
            quantized.plane2,
            2,
            preview_mode
        ),
    };
}

}  // namespace dj1000
