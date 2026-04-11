#pragma once

#include "dj1000/dat_file.hpp"
#include "dj1000/post_geometry_rgb_convert.hpp"

#include <filesystem>
#include <optional>

namespace dj1000 {

struct NormalExportDebugState {
    double source_gain = 1.0;
    double sharpness_scalar = 1.0;
    double stage3060_scalar = 0.0;
    int stage3060_param0 = 0;
    int stage3060_param1 = 0;
    int stage3060_threshold = 0;
    double post_rgb_scalar = 0.0;
};

struct NormalExportOverrides {
    std::optional<double> source_gain;
    std::optional<int> red_balance;
    std::optional<int> green_balance;
    std::optional<int> blue_balance;
    std::optional<int> contrast;
    std::optional<int> brightness;
    std::optional<int> vividness;
    std::optional<int> sharpness;
    std::optional<int> crop_top_rows;
    std::optional<int> stage3060_param0;
    std::optional<int> stage3060_param1;
    std::optional<double> stage3060_scalar;
    std::optional<int> stage3060_threshold;
};

[[nodiscard]] RgbBytePlanes build_default_normal_export_bgr_planes(
    const DatFile& dat,
    NormalExportDebugState* debug_state = nullptr,
    const NormalExportOverrides* overrides = nullptr
);

[[nodiscard]] RgbBytePlanes build_default_small_export_bgr_planes(
    const DatFile& dat,
    NormalExportDebugState* debug_state = nullptr,
    const NormalExportOverrides* overrides = nullptr
);

void write_default_normal_export_bmp(
    const DatFile& dat,
    const std::filesystem::path& output_path,
    NormalExportDebugState* debug_state = nullptr,
    const NormalExportOverrides* overrides = nullptr
);

void write_default_small_export_bmp(
    const DatFile& dat,
    const std::filesystem::path& output_path,
    NormalExportDebugState* debug_state = nullptr,
    const NormalExportOverrides* overrides = nullptr
);

}  // namespace dj1000
