#include "dj1000/normal_export_pipeline.hpp"

#include "dj1000/bmp.hpp"
#include "dj1000/nonlarge_post_geometry_pipeline.hpp"
#include "dj1000/post_geometry_center_scale.hpp"
#include "dj1000/post_geometry_dual_scale.hpp"
#include "dj1000/post_geometry_edge_response.hpp"
#include "dj1000/post_geometry_prepare.hpp"
#include "dj1000/post_geometry_stage_2a00.hpp"
#include "dj1000/post_geometry_stage_2dd0.hpp"
#include "dj1000/post_geometry_stage_3060.hpp"
#include "dj1000/post_geometry_stage_3600.hpp"
#include "dj1000/post_geometry_stage_3890.hpp"
#include "dj1000/post_geometry_stage_4810.hpp"
#include "dj1000/post_rgb_stage_40f0.hpp"
#include "dj1000/post_rgb_stage_42a0.hpp"
#include "dj1000/post_rgb_stage_3b00.hpp"
#include "dj1000/source_seed_stage.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace dj1000 {

namespace {

constexpr int kSmallExportMode = 0;
constexpr int kNormalExportMode = 2;
constexpr int kNormalColorBalanceDefault = 100;
constexpr int kNormalContrastDefault = 3;
constexpr int kNormalBrightnessDefault = 3;
constexpr int kNormalVividnessDefault = 3;
constexpr int kNormalSharpnessDefault = 3;
constexpr int kNormalOutputWidth = 320;
constexpr int kNormalWorkingHeight = 244;
constexpr int kNormalOutputHeight = 240;
constexpr int kNormalCropTopRows = 3;
constexpr std::array<int, 7> kNormalSharpnessStage3060Param0 = {0, 4, 8, 16, 28, 44, 64};
constexpr std::array<int, 7> kSmallSharpnessStage3060Param0 = {0, 2, 4, 8, 14, 22, 32};
constexpr std::array<int, 7> kSharpnessStage3060Param1 = {40, 40, 40, 40, 43, 46, 50};
constexpr std::array<int, 7> kSharpnessStage3060Threshold = {0, 30, 25, 20, 15, 10, 5};
constexpr int kSourceGainTarget = 18;
constexpr double kSourceGainScale = 0.01 * 255.0;
constexpr double kSourceGainMax = 5.0;
constexpr int kSourceGainRegionOffset = 0x8494;
constexpr int kSourceGainRegionWidth = 0x0EC;
constexpr int kSourceGainRegionRows = 0x077;
constexpr int kSourceGainRegionStride = 0x200;
constexpr int kSourceGainWritableWidth = 0x1FA;
constexpr int kSourceGainWritableRows = 0x0F4;

struct DefaultSettingsBlock {
    std::array<int, 12> values{};
};

int get_override_or_default(const std::optional<int>& value, int fallback) {
    return value.has_value() ? *value : fallback;
}

void validate_balance_value(int value, const char* label) {
    if (value < 0 || value > 200) {
        throw std::runtime_error(std::string(label) + " must be in the range 0..200");
    }
}

void validate_level_value(int value, const char* label) {
    if (value < 0 || value > 6) {
        throw std::runtime_error(std::string(label) + " must be in the range 0..6");
    }
}

DefaultSettingsBlock build_default_settings_block(
    const DatFile& dat,
    int export_mode,
    const NormalExportOverrides* overrides
) {
    DefaultSettingsBlock settings;
    settings.values[0] = get_override_or_default(
        overrides != nullptr ? overrides->green_balance : std::nullopt,
        kNormalColorBalanceDefault
    );
    settings.values[1] = get_override_or_default(
        overrides != nullptr ? overrides->red_balance : std::nullopt,
        kNormalColorBalanceDefault
    );
    settings.values[2] = get_override_or_default(
        overrides != nullptr ? overrides->blue_balance : std::nullopt,
        kNormalColorBalanceDefault
    );
    settings.values[3] = get_override_or_default(
        overrides != nullptr ? overrides->contrast : std::nullopt,
        kNormalContrastDefault
    );
    settings.values[4] = get_override_or_default(
        overrides != nullptr ? overrides->brightness : std::nullopt,
        kNormalBrightnessDefault
    );
    settings.values[5] = get_override_or_default(
        overrides != nullptr ? overrides->vividness : std::nullopt,
        kNormalVividnessDefault
    );
    settings.values[6] = get_override_or_default(
        overrides != nullptr ? overrides->sharpness : std::nullopt,
        kNormalSharpnessDefault
    );
    settings.values[7] = export_mode;
    settings.values[8] = dat.metadata.trailer[9];
    settings.values[9] = dat.metadata.trailer[8];
    settings.values[10] =
        static_cast<int>(dat.metadata.trailer[11]) +
        (10 * static_cast<int>(dat.metadata.trailer[10]));
    settings.values[11] = 1;

    validate_balance_value(settings.values[0], "green_balance");
    validate_balance_value(settings.values[1], "red_balance");
    validate_balance_value(settings.values[2], "blue_balance");
    validate_level_value(settings.values[3], "contrast");
    validate_level_value(settings.values[4], "brightness");
    validate_level_value(settings.values[5], "vividness");
    validate_level_value(settings.values[6], "sharpness");

    return settings;
}

std::vector<std::uint8_t> extract_normal_source_payload(const DatFile& dat) {
    const std::size_t source_bytes = expected_source_seed_input_byte_count(false);
    if (dat.raw_payload().size() < source_bytes) {
        throw std::runtime_error("DAT payload is too small for the normal export pipeline");
    }
    return std::vector<std::uint8_t>(
        dat.raw_payload().begin(),
        dat.raw_payload().begin() + static_cast<std::ptrdiff_t>(source_bytes)
    );
}

double compute_source_gain(std::span<const std::uint8_t> source, int target) {
    if (source.size() < expected_source_seed_input_byte_count(false)) {
        throw std::runtime_error("source payload is too small for source-gain analysis");
    }

    std::uint64_t region_sum = 0;
    for (int row = 0; row < kSourceGainRegionRows; ++row) {
        const std::size_t row_offset =
            static_cast<std::size_t>(kSourceGainRegionOffset) +
            (static_cast<std::size_t>(row) * static_cast<std::size_t>(kSourceGainRegionStride));
        for (int col = 0; col < kSourceGainRegionWidth; ++col) {
            region_sum += source[row_offset + static_cast<std::size_t>(col)];
        }
    }

    const std::uint64_t region_average =
        region_sum /
        static_cast<std::uint64_t>(kSourceGainRegionWidth * kSourceGainRegionRows);
    const double average = static_cast<double>(region_average);
    const double target_level = static_cast<double>(target) * kSourceGainScale;
    if (average >= target_level) {
        return 1.0;
    }
    if (!(average > 0.0)) {
        return kSourceGainMax;
    }
    return std::min(kSourceGainMax, target_level / average);
}

int trunc_to_int(double value) {
    if (!std::isfinite(value)) {
        return 0;
    }
    return static_cast<int>(std::trunc(value));
}

void apply_source_gain(std::span<std::uint8_t> source, double gain) {
    for (int row = 0; row < kSourceGainWritableRows; ++row) {
        const std::size_t row_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(kNormalSourceSeedInputStride);
        for (int col = 0; col < kSourceGainWritableWidth; ++col) {
            const std::size_t index = row_offset + static_cast<std::size_t>(col);
            const int scaled = trunc_to_int(static_cast<double>(source[index]) * gain);
            source[index] = static_cast<std::uint8_t>(std::clamp(scaled, 0, 255));
        }
    }
}

double compute_stage_3060_scalar(double source_gain) {
    if (source_gain < 1.5) {
        return 0.0;
    }
    if (source_gain >= 4.0) {
        return 30.0;
    }
    return (source_gain - 1.5) * 12.0;
}

std::vector<std::uint8_t> crop_output_plane_with_top(
    std::span<const std::uint8_t> plane,
    int crop_top_rows
) {
    const std::size_t required =
        static_cast<std::size_t>(kNormalOutputWidth) * static_cast<std::size_t>(kNormalWorkingHeight);
    if (plane.size() < required) {
        throw std::runtime_error("working RGB plane is too small for final normal-export crop");
    }
    if (crop_top_rows < 0 || crop_top_rows + kNormalOutputHeight > kNormalWorkingHeight) {
        throw std::runtime_error("normal-export crop_top_rows is out of range");
    }

    std::vector<std::uint8_t> output(
        static_cast<std::size_t>(kNormalOutputWidth) * static_cast<std::size_t>(kNormalOutputHeight),
        0
    );
    for (int row = 0; row < kNormalOutputHeight; ++row) {
        const std::size_t source_row =
            static_cast<std::size_t>(row + crop_top_rows) * static_cast<std::size_t>(kNormalOutputWidth);
        const std::size_t output_row =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(kNormalOutputWidth);
        std::copy_n(
            plane.begin() + static_cast<std::ptrdiff_t>(source_row),
            kNormalOutputWidth,
            output.begin() + static_cast<std::ptrdiff_t>(output_row)
        );
    }
    return output;
}

std::vector<std::uint8_t> interleave_planes_bgr(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2
) {
    if (plane0.size() != plane1.size() || plane0.size() != plane2.size()) {
        throw std::runtime_error("cropped RGB planes must all have the same size");
    }

    std::vector<std::uint8_t> output;
    output.reserve(plane0.size() * 3);
    for (std::size_t index = 0; index < plane0.size(); ++index) {
        output.push_back(plane0[index]);
        output.push_back(plane1[index]);
        output.push_back(plane2[index]);
    }
    return output;
}

RgbBytePlanes build_default_nonlarge_export_bgr_planes(
    const DatFile& dat,
    int export_mode,
    NormalExportDebugState* debug_state,
    const NormalExportOverrides* overrides
) {
    const auto settings = build_default_settings_block(dat, export_mode, overrides);
    if (settings.values[10] == 0 && settings.values[11] == 1) {
        throw std::runtime_error("the native normal export path does not yet implement the 0x4570 pre-pass");
    }

    auto source = extract_normal_source_payload(dat);
    double source_gain = compute_source_gain(source, kSourceGainTarget);
    if (overrides != nullptr && overrides->source_gain.has_value()) {
        source_gain = *overrides->source_gain;
    }
    apply_source_gain(source, source_gain);

    const double sharpness_scalar = std::clamp(source_gain, 1.0, 1.5);
    const double post_rgb_scalar = compute_stage_3060_scalar(source_gain);
    double stage3060_scalar = compute_stage_3060_scalar(source_gain);
    const auto& sharpness_param0_table = export_mode == kSmallExportMode
        ? kSmallSharpnessStage3060Param0
        : kNormalSharpnessStage3060Param0;
    int stage3060_param0 = sharpness_param0_table[settings.values[6]];
    int stage3060_param1 = kSharpnessStage3060Param1[settings.values[6]];
    int stage3060_threshold = kSharpnessStage3060Threshold[settings.values[6]];
    int crop_top_rows = kNormalCropTopRows;

    if (overrides != nullptr) {
        if (overrides->crop_top_rows.has_value()) {
            crop_top_rows = *overrides->crop_top_rows;
        }
        if (overrides->stage3060_param0.has_value()) {
            stage3060_param0 = *overrides->stage3060_param0;
        }
        if (overrides->stage3060_param1.has_value()) {
            stage3060_param1 = *overrides->stage3060_param1;
        }
        if (overrides->stage3060_scalar.has_value()) {
            stage3060_scalar = *overrides->stage3060_scalar;
        }
        if (overrides->stage3060_threshold.has_value()) {
            stage3060_threshold = *overrides->stage3060_threshold;
        }
    }

    auto prepared = build_nonlarge_post_geometry_planes_from_source(source, false);
    apply_post_geometry_stage_4810(
        prepared.delta0,
        prepared.center,
        prepared.delta2,
        kNormalOutputWidth,
        kNormalWorkingHeight
    );
    if (settings.values[6] != 0) {
        apply_post_geometry_stage_3600(prepared.center, kNormalOutputWidth, kNormalWorkingHeight);
    }
    const auto edge_response =
        build_post_geometry_edge_response(prepared.center, kNormalOutputWidth, kNormalWorkingHeight);
    apply_post_geometry_stage_2a00(prepared.center, kNormalOutputWidth, kNormalWorkingHeight);
    apply_post_geometry_center_scale(
        prepared.delta0,
        prepared.center,
        prepared.delta2,
        kNormalOutputWidth,
        kNormalWorkingHeight
    );
    apply_post_geometry_stage_2dd0(
        prepared.delta0,
        prepared.delta2,
        kNormalOutputWidth,
        kNormalWorkingHeight
    );
    apply_post_geometry_stage_3890(
        prepared.delta0,
        prepared.delta2,
        kNormalOutputWidth,
        kNormalWorkingHeight,
        settings.values[5]
    );
    apply_post_geometry_stage_3060(
        edge_response,
        prepared.center,
        kNormalOutputWidth,
        kNormalWorkingHeight,
        stage3060_param0,
        stage3060_param1,
        stage3060_scalar,
        stage3060_threshold
    );
    apply_post_geometry_dual_scale(
        prepared.delta0,
        prepared.delta2,
        kNormalOutputWidth,
        kNormalWorkingHeight,
        sharpness_scalar
    );

    auto output = convert_post_geometry_rgb_planes(
        prepared.delta0,
        prepared.center,
        prepared.delta2,
        kNormalOutputWidth,
        kNormalWorkingHeight
    );

    apply_post_rgb_stage_42a0(
        output.plane0,
        output.plane1,
        output.plane2,
        kNormalOutputWidth,
        kNormalWorkingHeight,
        settings.values[1],
        settings.values[0],
        settings.values[2]
    );

    apply_post_rgb_stage_3b00(
        output.plane0,
        output.plane1,
        output.plane2,
        kNormalOutputWidth,
        kNormalWorkingHeight,
        settings.values[3],
        post_rgb_scalar
    );

    apply_post_rgb_stage_40f0(
        output.plane0,
        output.plane1,
        output.plane2,
        kNormalOutputWidth,
        kNormalWorkingHeight,
        settings.values[3],
        0
    );
    apply_post_rgb_stage_40f0(
        output.plane0,
        output.plane1,
        output.plane2,
        kNormalOutputWidth,
        kNormalWorkingHeight,
        settings.values[4],
        1
    );

    output.plane0 = crop_output_plane_with_top(output.plane0, crop_top_rows);
    output.plane1 = crop_output_plane_with_top(output.plane1, crop_top_rows);
    output.plane2 = crop_output_plane_with_top(output.plane2, crop_top_rows);

    if (debug_state != nullptr) {
        debug_state->source_gain = source_gain;
        debug_state->sharpness_scalar = sharpness_scalar;
        debug_state->stage3060_scalar = stage3060_scalar;
        debug_state->stage3060_param0 = stage3060_param0;
        debug_state->stage3060_param1 = stage3060_param1;
        debug_state->stage3060_threshold = stage3060_threshold;
        debug_state->post_rgb_scalar = post_rgb_scalar;
    }

    return output;
}

}  // namespace

RgbBytePlanes build_default_normal_export_bgr_planes(
    const DatFile& dat,
    NormalExportDebugState* debug_state,
    const NormalExportOverrides* overrides
) {
    return build_default_nonlarge_export_bgr_planes(
        dat,
        kNormalExportMode,
        debug_state,
        overrides
    );
}

RgbBytePlanes build_default_small_export_bgr_planes(
    const DatFile& dat,
    NormalExportDebugState* debug_state,
    const NormalExportOverrides* overrides
) {
    return build_default_nonlarge_export_bgr_planes(
        dat,
        kSmallExportMode,
        debug_state,
        overrides
    );
}

void write_default_normal_export_bmp(
    const DatFile& dat,
    const std::filesystem::path& output_path,
    NormalExportDebugState* debug_state,
    const NormalExportOverrides* overrides
) {
    const auto output = build_default_normal_export_bgr_planes(dat, debug_state, overrides);
    const auto bgr_pixels = interleave_planes_bgr(output.plane2, output.plane1, output.plane0);
    write_bmp24_bgr(output_path, kNormalOutputWidth, kNormalOutputHeight, bgr_pixels);
}

void write_default_small_export_bmp(
    const DatFile& dat,
    const std::filesystem::path& output_path,
    NormalExportDebugState* debug_state,
    const NormalExportOverrides* overrides
) {
    const auto output = build_default_small_export_bgr_planes(dat, debug_state, overrides);
    const auto bgr_pixels = interleave_planes_bgr(output.plane2, output.plane1, output.plane0);
    write_bmp24_bgr(output_path, kNormalOutputWidth, kNormalOutputHeight, bgr_pixels);
}

}  // namespace dj1000
