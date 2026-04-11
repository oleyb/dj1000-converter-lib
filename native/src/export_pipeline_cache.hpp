#pragma once

#include "dj1000/dat_file.hpp"
#include "dj1000/large_export_pipeline.hpp"
#include "dj1000/normal_export_pipeline.hpp"
#include "dj1000/post_geometry_prepare.hpp"
#include "dj1000/post_geometry_rgb_convert.hpp"

#include <optional>

namespace dj1000 {

struct CachedPostGeometryStage {
    double source_gain = 1.0;
    PostGeometryPlanes prepared;
};

[[nodiscard]] CachedPostGeometryStage build_cached_nonlarge_post_geometry_stage(
    const DatFile& dat,
    std::optional<double> source_gain_override = std::nullopt
);

[[nodiscard]] CachedPostGeometryStage build_cached_large_post_geometry_stage(
    const DatFile& dat,
    std::optional<double> source_gain_override = std::nullopt
);

[[nodiscard]] RgbBytePlanes render_default_nonlarge_export_from_cached_stage(
    const CachedPostGeometryStage& cached_stage,
    bool small_export,
    NormalExportDebugState* debug_state = nullptr,
    const NormalExportOverrides* overrides = nullptr
);

[[nodiscard]] RgbBytePlanes render_default_large_export_from_cached_stage(
    const CachedPostGeometryStage& cached_stage,
    LargeExportDebugState* debug_state = nullptr,
    const LargeExportOverrides* overrides = nullptr
);

}  // namespace dj1000
