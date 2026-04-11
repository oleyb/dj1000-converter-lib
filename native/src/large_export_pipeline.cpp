#include "dj1000/large_export_pipeline.hpp"

#include "export_pipeline_cache.hpp"
#include "dj1000/bmp.hpp"
#include "dj1000/large_vertical_resample.hpp"
#include "dj1000/post_geometry_center_scale.hpp"
#include "dj1000/post_geometry_dual_scale.hpp"
#include "dj1000/post_geometry_edge_response.hpp"
#include "dj1000/post_geometry_filter.hpp"
#include "dj1000/post_geometry_prepare.hpp"
#include "dj1000/post_geometry_rgb_convert.hpp"
#include "dj1000/post_geometry_stage_2a00.hpp"
#include "dj1000/post_geometry_stage_2dd0.hpp"
#include "dj1000/post_geometry_stage_3060.hpp"
#include "dj1000/post_geometry_stage_3600.hpp"
#include "dj1000/post_geometry_stage_3890.hpp"
#include "dj1000/post_geometry_stage_4810.hpp"
#include "dj1000/post_rgb_stage_40f0.hpp"
#include "dj1000/post_rgb_stage_42a0.hpp"
#include "dj1000/post_rgb_stage_3b00.hpp"
#include "dj1000/pregeometry_pipeline.hpp"
#include "dj1000/quantize_stage.hpp"
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

constexpr int kLargeExportMode = 5;
constexpr int kLargeColorBalanceDefault = 100;
constexpr int kLargeContrastDefault = 3;
constexpr int kLargeBrightnessDefault = 3;
constexpr int kLargeVividnessDefault = 3;
constexpr int kLargeSharpnessDefault = 3;
constexpr int kLargeOutputWidth = 504;
constexpr int kLargeOutputHeight = 378;
constexpr std::array<int, 7> kLargeSharpnessStage3060Param0 = {0, 4, 8, 16, 28, 44, 64};
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
    const LargeExportOverrides* overrides
) {
    DefaultSettingsBlock settings;
    settings.values[0] = get_override_or_default(
        overrides != nullptr ? overrides->green_balance : std::nullopt,
        kLargeColorBalanceDefault
    );
    settings.values[1] = get_override_or_default(
        overrides != nullptr ? overrides->red_balance : std::nullopt,
        kLargeColorBalanceDefault
    );
    settings.values[2] = get_override_or_default(
        overrides != nullptr ? overrides->blue_balance : std::nullopt,
        kLargeColorBalanceDefault
    );
    settings.values[3] = get_override_or_default(
        overrides != nullptr ? overrides->contrast : std::nullopt,
        kLargeContrastDefault
    );
    settings.values[4] = get_override_or_default(
        overrides != nullptr ? overrides->brightness : std::nullopt,
        kLargeBrightnessDefault
    );
    settings.values[5] = get_override_or_default(
        overrides != nullptr ? overrides->vividness : std::nullopt,
        kLargeVividnessDefault
    );
    settings.values[6] = get_override_or_default(
        overrides != nullptr ? overrides->sharpness : std::nullopt,
        kLargeSharpnessDefault
    );
    settings.values[7] = kLargeExportMode;
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

DefaultSettingsBlock build_runtime_settings_block(const LargeExportOverrides* overrides) {
    DefaultSettingsBlock settings;
    settings.values[0] = get_override_or_default(
        overrides != nullptr ? overrides->green_balance : std::nullopt,
        kLargeColorBalanceDefault
    );
    settings.values[1] = get_override_or_default(
        overrides != nullptr ? overrides->red_balance : std::nullopt,
        kLargeColorBalanceDefault
    );
    settings.values[2] = get_override_or_default(
        overrides != nullptr ? overrides->blue_balance : std::nullopt,
        kLargeColorBalanceDefault
    );
    settings.values[3] = get_override_or_default(
        overrides != nullptr ? overrides->contrast : std::nullopt,
        kLargeContrastDefault
    );
    settings.values[4] = get_override_or_default(
        overrides != nullptr ? overrides->brightness : std::nullopt,
        kLargeBrightnessDefault
    );
    settings.values[5] = get_override_or_default(
        overrides != nullptr ? overrides->vividness : std::nullopt,
        kLargeVividnessDefault
    );
    settings.values[6] = get_override_or_default(
        overrides != nullptr ? overrides->sharpness : std::nullopt,
        kLargeSharpnessDefault
    );

    validate_balance_value(settings.values[0], "green_balance");
    validate_balance_value(settings.values[1], "red_balance");
    validate_balance_value(settings.values[2], "blue_balance");
    validate_level_value(settings.values[3], "contrast");
    validate_level_value(settings.values[4], "brightness");
    validate_level_value(settings.values[5], "vividness");
    validate_level_value(settings.values[6], "sharpness");

    return settings;
}

std::vector<std::uint8_t> extract_large_source_payload(const DatFile& dat) {
    const std::size_t source_bytes = expected_source_seed_input_byte_count(false);
    if (dat.raw_payload().size() < source_bytes) {
        throw std::runtime_error("DAT payload is too small for the large export pipeline");
    }
    return std::vector<std::uint8_t>(
        dat.raw_payload().begin(),
        dat.raw_payload().begin() + static_cast<std::ptrdiff_t>(source_bytes)
    );
}

int trunc_to_int(double value) {
    if (!std::isfinite(value)) {
        return 0;
    }
    return static_cast<int>(std::trunc(value));
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

int compute_low_gain_stage_3060_param1(double source_gain) {
    return std::max(20, trunc_to_int(source_gain * 20.0));
}

int compute_low_gain_stage_3060_threshold(int stage3060_param1) {
    return 25 - std::max(0, (stage3060_param1 - 20) / 2);
}

RgbBytePlanes build_large_stage_planes_from_source(std::span<const std::uint8_t> source) {
    const auto pregeometry = build_pregeometry_pipeline(source, false);
    const auto quantized = quantize_pregeometry_planes(
        pregeometry.plane0,
        pregeometry.plane1,
        pregeometry.plane2
    );

    return {
        .plane0 = resample_large_vertical_plane(quantized.plane0),
        .plane1 = resample_large_vertical_plane(quantized.plane1),
        .plane2 = resample_large_vertical_plane(quantized.plane2),
    };
}

std::vector<std::uint8_t> interleave_planes_bgr(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2
) {
    if (plane0.size() != plane1.size() || plane0.size() != plane2.size()) {
        throw std::runtime_error("large export RGB planes must all have the same size");
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

}  // namespace

CachedPostGeometryStage build_cached_large_post_geometry_stage(
    const DatFile& dat,
    std::optional<double> source_gain_override
) {
    const auto settings = build_default_settings_block(dat, nullptr);
    if (settings.values[10] == 0 && settings.values[11] == 1) {
        throw std::runtime_error("the native large export path does not yet implement the 0x4570 pre-pass");
    }

    auto source = extract_large_source_payload(dat);
    double source_gain = compute_source_gain(source, kSourceGainTarget);
    if (source_gain_override.has_value()) {
        source_gain = *source_gain_override;
    }
    apply_source_gain(source, source_gain);

    auto stage = build_large_stage_planes_from_source(source);
    auto prepared = build_post_geometry_planes(
        stage.plane0,
        stage.plane1,
        stage.plane2,
        kLargeOutputWidth,
        kLargeOutputHeight
    );
    apply_post_geometry_delta_filters(
        prepared.delta0,
        prepared.delta2,
        kLargeOutputWidth,
        kLargeOutputHeight
    );
    apply_post_geometry_stage_4810(
        prepared.delta0,
        prepared.center,
        prepared.delta2,
        kLargeOutputWidth,
        kLargeOutputHeight
    );
    return {
        .source_gain = source_gain,
        .prepared = std::move(prepared),
    };
}

RgbBytePlanes render_default_large_export_from_cached_stage(
    const CachedPostGeometryStage& cached_stage,
    LargeExportDebugState* debug_state,
    const LargeExportOverrides* overrides
) {
    const auto settings = build_runtime_settings_block(overrides);
    const double source_gain = cached_stage.source_gain;
    const double sharpness_scalar = std::clamp(source_gain, 1.0, 1.5);
    const double post_rgb_scalar = compute_stage_3060_scalar(source_gain);
    double stage3060_scalar = compute_stage_3060_scalar(source_gain);
    int stage3060_param0 = kLargeSharpnessStage3060Param0[settings.values[6]];
    int stage3060_param1 = kSharpnessStage3060Param1[settings.values[6]];
    int stage3060_threshold = kSharpnessStage3060Threshold[settings.values[6]];

    if (source_gain < 1.5 && settings.values[6] == kLargeSharpnessDefault) {
        stage3060_scalar = source_gain;
        stage3060_param1 = compute_low_gain_stage_3060_param1(source_gain);
        stage3060_threshold = compute_low_gain_stage_3060_threshold(stage3060_param1);
    }

    if (overrides != nullptr) {
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

    auto prepared = cached_stage.prepared;
    if (settings.values[6] != 0) {
        apply_post_geometry_stage_3600(prepared.center, kLargeOutputWidth, kLargeOutputHeight);
    }
    const auto edge_response =
        build_post_geometry_edge_response(prepared.center, kLargeOutputWidth, kLargeOutputHeight);
    apply_post_geometry_stage_2a00(prepared.center, kLargeOutputWidth, kLargeOutputHeight);
    apply_post_geometry_center_scale(
        prepared.delta0,
        prepared.center,
        prepared.delta2,
        kLargeOutputWidth,
        kLargeOutputHeight
    );
    apply_post_geometry_stage_2dd0(
        prepared.delta0,
        prepared.delta2,
        kLargeOutputWidth,
        kLargeOutputHeight
    );
    apply_post_geometry_stage_3890(
        prepared.delta0,
        prepared.delta2,
        kLargeOutputWidth,
        kLargeOutputHeight,
        settings.values[5]
    );
    apply_post_geometry_stage_3060(
        edge_response,
        prepared.center,
        kLargeOutputWidth,
        kLargeOutputHeight,
        stage3060_param0,
        stage3060_param1,
        stage3060_scalar,
        stage3060_threshold
    );
    apply_post_geometry_dual_scale(
        prepared.delta0,
        prepared.delta2,
        kLargeOutputWidth,
        kLargeOutputHeight,
        sharpness_scalar
    );

    auto output = convert_post_geometry_rgb_planes(
        prepared.delta0,
        prepared.center,
        prepared.delta2,
        kLargeOutputWidth,
        kLargeOutputHeight
    );

    apply_post_rgb_stage_42a0(
        output.plane0,
        output.plane1,
        output.plane2,
        kLargeOutputWidth,
        kLargeOutputHeight,
        settings.values[1],
        settings.values[0],
        settings.values[2]
    );

    apply_post_rgb_stage_3b00(
        output.plane0,
        output.plane1,
        output.plane2,
        kLargeOutputWidth,
        kLargeOutputHeight,
        settings.values[3],
        post_rgb_scalar
    );

    apply_post_rgb_stage_40f0(
        output.plane0,
        output.plane1,
        output.plane2,
        kLargeOutputWidth,
        kLargeOutputHeight,
        settings.values[3],
        0
    );
    apply_post_rgb_stage_40f0(
        output.plane0,
        output.plane1,
        output.plane2,
        kLargeOutputWidth,
        kLargeOutputHeight,
        settings.values[4],
        1
    );

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

RgbBytePlanes build_default_large_export_bgr_planes(
    const DatFile& dat,
    LargeExportDebugState* debug_state,
    const LargeExportOverrides* overrides
) {
    const auto cached_stage = build_cached_large_post_geometry_stage(
        dat,
        overrides != nullptr ? overrides->source_gain : std::nullopt
    );
    return render_default_large_export_from_cached_stage(
        cached_stage,
        debug_state,
        overrides
    );
}

void write_default_large_export_bmp(
    const DatFile& dat,
    const std::filesystem::path& output_path,
    LargeExportDebugState* debug_state,
    const LargeExportOverrides* overrides
) {
    const auto output = build_default_large_export_bgr_planes(dat, debug_state, overrides);
    const auto bgr_pixels = interleave_planes_bgr(output.plane2, output.plane1, output.plane0);
    write_bmp24_bgr(output_path, kLargeOutputWidth, kLargeOutputHeight, bgr_pixels);
}

}  // namespace dj1000
