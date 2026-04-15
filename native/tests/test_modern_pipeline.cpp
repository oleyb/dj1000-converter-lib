#include "dj1000/converter.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint8_t> make_sample_dat() {
    std::vector<std::uint8_t> bytes(dj1000::kExpectedDatSize);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>((index * 29U + 31U) & 0xFFU);
    }

    bytes[dj1000::kRawBlockSize + 0] = 0xC4;
    bytes[dj1000::kRawBlockSize + 1] = 0xB2;
    bytes[dj1000::kRawBlockSize + 2] = 0xE3;
    bytes[dj1000::kRawBlockSize + 3] = 0x22;
    bytes[dj1000::kRawBlockSize + 8] = 0;
    bytes[dj1000::kRawBlockSize + 10] = 0;
    bytes[dj1000::kRawBlockSize + 11] = 1;
    return bytes;
}

}  // namespace

int main() {
    const auto bytes = make_sample_dat();

    dj1000::ConvertOptions legacy_options;
    legacy_options.size = dj1000::ExportSize::Large;

    dj1000::ConvertOptions modern_options;
    modern_options.pipeline = dj1000::ConversionPipeline::Modern;
    modern_options.size = dj1000::ExportSize::Large;
    modern_options.sliders.vividness = 5;
    modern_options.sliders.sharpness = 5;

    const auto legacy_large = dj1000::convert_dat_bytes_to_bgr(bytes, legacy_options);
    const auto modern_large = dj1000::convert_dat_bytes_to_bgr(bytes, modern_options);
    assert(modern_large.width == 504);
    assert(modern_large.height == 378);
    assert(legacy_large.interleaved_rgba() != modern_large.interleaved_rgba());

    modern_options.size = dj1000::ExportSize::Normal;
    const auto modern_normal = dj1000::convert_dat_bytes_to_bgr(bytes, modern_options);
    assert(modern_normal.width == 320);
    assert(modern_normal.height == 240);

    modern_options.size = dj1000::ExportSize::Small;
    modern_options.sliders.brightness = 5;
    const auto modern_small = dj1000::convert_dat_bytes_to_bgr(bytes, modern_options);
    assert(modern_small.width == 320);
    assert(modern_small.height == 240);

    std::cout << "test_modern_pipeline passed\n";
    return 0;
}
