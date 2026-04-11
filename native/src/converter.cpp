#include "dj1000/converter.hpp"

#include "dj1000/bmp.hpp"
#include "dj1000/large_export_pipeline.hpp"
#include "dj1000/normal_export_pipeline.hpp"

#include <stdexcept>

namespace dj1000 {

namespace {

NormalExportOverrides build_normal_overrides(const ConvertOptions& options) {
    NormalExportOverrides overrides;
    overrides.source_gain = options.source_gain;
    overrides.red_balance = options.sliders.red_balance;
    overrides.green_balance = options.sliders.green_balance;
    overrides.blue_balance = options.sliders.blue_balance;
    overrides.contrast = options.sliders.contrast;
    overrides.brightness = options.sliders.brightness;
    overrides.vividness = options.sliders.vividness;
    overrides.sharpness = options.sliders.sharpness;
    return overrides;
}

LargeExportOverrides build_large_overrides(const ConvertOptions& options) {
    LargeExportOverrides overrides;
    overrides.source_gain = options.source_gain;
    overrides.red_balance = options.sliders.red_balance;
    overrides.green_balance = options.sliders.green_balance;
    overrides.blue_balance = options.sliders.blue_balance;
    overrides.contrast = options.sliders.contrast;
    overrides.brightness = options.sliders.brightness;
    overrides.vividness = options.sliders.vividness;
    overrides.sharpness = options.sliders.sharpness;
    return overrides;
}

void populate_debug_state(ConvertDebugState* target, const NormalExportDebugState& source) {
    if (target == nullptr) {
        return;
    }
    target->source_gain = source.source_gain;
    target->sharpness_scalar = source.sharpness_scalar;
    target->stage3060_scalar = source.stage3060_scalar;
    target->stage3060_param0 = source.stage3060_param0;
    target->stage3060_param1 = source.stage3060_param1;
    target->stage3060_threshold = source.stage3060_threshold;
    target->post_rgb_scalar = source.post_rgb_scalar;
}

void populate_debug_state(ConvertDebugState* target, const LargeExportDebugState& source) {
    if (target == nullptr) {
        return;
    }
    target->source_gain = source.source_gain;
    target->sharpness_scalar = source.sharpness_scalar;
    target->stage3060_scalar = source.stage3060_scalar;
    target->stage3060_param0 = source.stage3060_param0;
    target->stage3060_param1 = source.stage3060_param1;
    target->stage3060_threshold = source.stage3060_threshold;
    target->post_rgb_scalar = source.post_rgb_scalar;
}

}  // namespace

std::vector<std::uint8_t> ConvertedImage::interleaved_bgr() const {
    if (planes.plane0.size() != planes.plane1.size() || planes.plane0.size() != planes.plane2.size()) {
        throw std::runtime_error("converted image planes must have the same size");
    }

    std::vector<std::uint8_t> output;
    output.reserve(planes.plane0.size() * 3);
    for (std::size_t index = 0; index < planes.plane0.size(); ++index) {
        output.push_back(planes.plane0[index]);
        output.push_back(planes.plane1[index]);
        output.push_back(planes.plane2[index]);
    }
    return output;
}

ConvertedImage convert_dat_to_bgr(
    const DatFile& dat,
    const ConvertOptions& options,
    ConvertDebugState* debug_state
) {
    if (options.size == ExportSize::Large) {
        LargeExportDebugState large_debug;
        const auto overrides = build_large_overrides(options);
        auto image = build_default_large_export_bgr_planes(
            dat,
            debug_state != nullptr ? &large_debug : nullptr,
            &overrides
        );
        populate_debug_state(debug_state, large_debug);
        return {
            .width = 504,
            .height = 378,
            .planes = std::move(image),
        };
    }

    NormalExportDebugState normal_debug;
    const auto overrides = build_normal_overrides(options);
    auto image =
        options.size == ExportSize::Small
            ? build_default_small_export_bgr_planes(
                  dat,
                  debug_state != nullptr ? &normal_debug : nullptr,
                  &overrides
              )
            : build_default_normal_export_bgr_planes(
                  dat,
                  debug_state != nullptr ? &normal_debug : nullptr,
                  &overrides
              );
    populate_debug_state(debug_state, normal_debug);
    return {
        .width = 320,
        .height = 240,
        .planes = std::move(image),
    };
}

void write_bmp(
    const DatFile& dat,
    const std::filesystem::path& output_path,
    const ConvertOptions& options,
    ConvertDebugState* debug_state
) {
    const auto image = convert_dat_to_bgr(dat, options, debug_state);
    const auto pixels = image.interleaved_bgr();
    write_bmp24_bgr(output_path, image.width, image.height, pixels);
}

}  // namespace dj1000
