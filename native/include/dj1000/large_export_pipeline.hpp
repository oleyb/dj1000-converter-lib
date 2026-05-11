#pragma once

#include "dj1000/dat_file.hpp"
#include "dj1000/post_geometry_rgb_convert.hpp"

#include <filesystem>
#include <optional>

namespace dj1000 {

struct LargeExportDebugState {
    double source_gain = 1.0;
    double sharpness_scalar = 1.0;
    double stage3060_scalar = 0.0;
    int stage3060_param0 = 0;
    int stage3060_param1 = 0;
    int stage3060_threshold = 0;
    double post_rgb_scalar = 0.0;
};

struct LargeExportOverrides {
    std::optional<double> source_gain;
    std::optional<int> red_balance;
    std::optional<int> green_balance;
    std::optional<int> blue_balance;
    std::optional<int> contrast;
    std::optional<int> brightness;
    std::optional<int> vividness;
    std::optional<int> sharpness;
    std::optional<int> stage3060_param0;
    std::optional<int> stage3060_param1;
    std::optional<double> stage3060_scalar;
    std::optional<int> stage3060_threshold;
};

[[nodiscard]] RgbBytePlanes build_default_large_export_bgr_planes(
    const DatFile& dat,
    LargeExportDebugState* debug_state = nullptr,
    const LargeExportOverrides* overrides = nullptr
);

void write_default_large_export_bmp(
    const DatFile& dat,
    const std::filesystem::path& output_path,
    LargeExportDebugState* debug_state = nullptr,
    const LargeExportOverrides* overrides = nullptr
);

}  // namespace dj1000
