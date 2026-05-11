#include "dng_jxl.h"

#include "dng_exceptions.h"

bool ParseJXL(dng_host&, dng_stream&, dng_info&, bool, bool) {
    ThrowNotYetImplemented("JPEG XL support is disabled in this dj1000 Adobe DNG SDK build");
}

dng_jxl_decoder::~dng_jxl_decoder() = default;

void dng_jxl_decoder::Decode(dng_host&, dng_stream&) {
    ThrowNotYetImplemented("JPEG XL support is disabled in this dj1000 Adobe DNG SDK build");
}

void dng_jxl_decoder::ProcessExifBox(dng_host&, const std::vector<uint8>&) {
}

void dng_jxl_decoder::ProcessXMPBox(dng_host&, const std::vector<uint8>&) {
}

void dng_jxl_decoder::ProcessBox(dng_host&, const dng_string&, const std::vector<uint8>&) {
}

void EncodeJXL_Tile(
    dng_host&,
    dng_stream&,
    const dng_pixel_buffer&,
    const dng_jxl_color_space_info&,
    const dng_jxl_encode_settings&
) {
    ThrowNotYetImplemented("JPEG XL support is disabled in this dj1000 Adobe DNG SDK build");
}

void EncodeJXL_Tile(
    dng_host&,
    dng_stream&,
    const dng_image&,
    const dng_jxl_color_space_info&,
    const dng_jxl_encode_settings&
) {
    ThrowNotYetImplemented("JPEG XL support is disabled in this dj1000 Adobe DNG SDK build");
}

void EncodeJXL_Container(
    dng_host&,
    dng_stream&,
    const dng_image&,
    const dng_jxl_encode_settings&,
    const dng_jxl_color_space_info&,
    const dng_metadata*,
    bool,
    bool,
    bool,
    const dng_bmff_box_list*
) {
    ThrowNotYetImplemented("JPEG XL support is disabled in this dj1000 Adobe DNG SDK build");
}

void EncodeJXL_Container(
    dng_host&,
    dng_stream&,
    const dng_pixel_buffer&,
    const dng_jxl_encode_settings&,
    const dng_jxl_color_space_info&,
    const dng_metadata*,
    bool,
    bool,
    bool,
    const dng_bmff_box_list*
) {
    ThrowNotYetImplemented("JPEG XL support is disabled in this dj1000 Adobe DNG SDK build");
}

real32 JXLQualityToDistance(uint32 quality) {
    if (quality >= 13) {
        return 0.0f;
    }
    if (quality >= 12) {
        return 0.1f;
    }
    if (quality >= 9) {
        return 1.0f;
    }
    if (quality >= 6) {
        return 2.0f;
    }
    return 3.0f;
}

dng_jxl_encode_settings* JXLQualityToSettings(uint32 quality) {
    auto* settings = new dng_jxl_encode_settings();
    settings->SetDistance(JXLQualityToDistance(quality));
    return settings;
}

void PreviewColorSpaceToJXLEncoding(
    const PreviewColorSpaceEnum,
    const uint32,
    dng_jxl_color_space_info& info
) {
    info.fJxlColorEncoding.Reset();
    info.fICCProfile.Reset();
    info.fIntensityTargetNits = 0.0f;
}

bool SupportsJXL(const dng_image&) {
    return false;
}
