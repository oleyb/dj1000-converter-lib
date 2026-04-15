#include "dj1000/converter.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint8_t> make_sample_dat() {
    std::vector<std::uint8_t> bytes(dj1000::kExpectedDatSize);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>((index * 17U) & 0xFFU);
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
    const auto session = dj1000::Session::open(bytes);

    assert(session.dat_file().bytes.size() == dj1000::kExpectedDatSize);
    assert(session.dat_file().metadata.signature_matches);

    dj1000::ConvertOptions normal_options;
    normal_options.size = dj1000::ExportSize::Normal;
    normal_options.sliders.vividness = 5;
    const auto direct_normal = dj1000::convert_dat_bytes_to_bgr(bytes, normal_options);
    const auto session_normal = session.render(normal_options);
    assert(direct_normal.width == 320);
    assert(direct_normal.height == 240);
    assert(direct_normal.interleaved_rgb() == session_normal.interleaved_rgb());

    dj1000::ConvertOptions small_options;
    small_options.size = dj1000::ExportSize::Small;
    small_options.sliders.brightness = 6;
    const auto direct_small = dj1000::convert_dat_bytes_to_bgr(bytes, small_options);
    const auto session_small = session.render(small_options);
    assert(direct_small.width == 320);
    assert(direct_small.height == 240);
    assert(direct_small.interleaved_rgb() == session_small.interleaved_rgb());

    dj1000::ConvertOptions large_options;
    large_options.size = dj1000::ExportSize::Large;
    large_options.source_gain = 1.25;
    large_options.sliders.brightness = 6;
    const auto direct_large = dj1000::convert_dat_bytes_to_bgr(bytes, large_options);
    const auto session_large = session.render(large_options);
    assert(direct_large.width == session_large.width);
    assert(direct_large.height == session_large.height);
    assert(direct_large.interleaved_rgba() == session_large.interleaved_rgba());

    dj1000::ConvertOptions second_large_options = large_options;
    second_large_options.sliders.vividness = 6;
    const auto second_direct_large = dj1000::convert_dat_bytes_to_bgr(bytes, second_large_options);
    const auto second_session_large = session.render(second_large_options);
    assert(second_direct_large.interleaved_rgba() == second_session_large.interleaved_rgba());

    dj1000::ConvertOptions modern_options;
    modern_options.pipeline = dj1000::ConversionPipeline::Modern;
    modern_options.size = dj1000::ExportSize::Large;
    modern_options.sliders.vividness = 5;
    modern_options.sliders.sharpness = 5;
    const auto direct_modern = dj1000::convert_dat_bytes_to_bgr(bytes, modern_options);
    const auto session_modern = session.render(modern_options);
    assert(direct_modern.width == 504);
    assert(direct_modern.height == 378);
    assert(direct_modern.interleaved_rgba() == session_modern.interleaved_rgba());

    modern_options.size = dj1000::ExportSize::Normal;
    modern_options.sliders.brightness = 5;
    const auto direct_modern_normal = dj1000::convert_dat_bytes_to_bgr(bytes, modern_options);
    const auto session_modern_normal = session.render(modern_options);
    assert(direct_modern_normal.width == 320);
    assert(direct_modern_normal.height == 240);
    assert(direct_modern_normal.interleaved_rgb() == session_modern_normal.interleaved_rgb());

    std::cout << "test_session passed\n";
    return 0;
}
