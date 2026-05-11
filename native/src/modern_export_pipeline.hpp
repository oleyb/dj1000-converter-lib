#pragma once

#include "dj1000/converter.hpp"
#include "dj1000/dat_file.hpp"
#include "dj1000/post_geometry_rgb_convert.hpp"

#include <optional>
#include <vector>

namespace dj1000 {

struct ModernExportDebugState {
    double source_gain = 1.0;
    double exposure = 1.0;
    double red_scale = 1.0;
    double green_scale = 1.0;
    double blue_scale = 1.0;
};

struct CachedModernStage {
    double source_gain = 1.0;
    int width = 0;
    int height = 0;
    std::vector<float> plane0;
    std::vector<float> plane1;
    std::vector<float> plane2;
};

struct RenderedModernFloatImage {
    int width = 0;
    int height = 0;
    std::vector<float> plane0;
    std::vector<float> plane1;
    std::vector<float> plane2;
};

[[nodiscard]] CachedModernStage build_cached_modern_stage(
    const DatFile& dat,
    std::optional<double> source_gain_override = std::nullopt
);

[[nodiscard]] RgbBytePlanes render_modern_export_from_cached_stage(
    const CachedModernStage& cached_stage,
    ExportSize size,
    const SliderSettings& sliders,
    ModernExportDebugState* debug_state = nullptr
);

[[nodiscard]] RenderedModernFloatImage render_modern_linear_from_cached_stage(
    const CachedModernStage& cached_stage,
    ExportSize size,
    const SliderSettings& sliders,
    ModernExportDebugState* debug_state = nullptr
);

[[nodiscard]] RgbBytePlanes build_modern_export_planes(
    const DatFile& dat,
    ExportSize size,
    const SliderSettings& sliders,
    ModernExportDebugState* debug_state = nullptr,
    std::optional<double> source_gain_override = std::nullopt
);

}  // namespace dj1000
