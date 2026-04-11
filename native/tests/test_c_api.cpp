#include "dj1000/c_api.h"
#include "dj1000/converter.hpp"
#include "dj1000/dat_file.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <string>
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
    dj1000_convert_options options;
    dj1000_init_convert_options(&options);
    assert(options.size == DJ1000_EXPORT_SIZE_LARGE);
    assert(options.pixel_format == DJ1000_PIXEL_FORMAT_RGBA);
    assert(options.sliders.red_balance == 100);
    assert(options.sliders.contrast == 3);
    assert(options.has_source_gain == 0);

    dj1000_image image;
    dj1000_init_image(&image);
    assert(image.pixels == nullptr);
    assert(image.byte_count == 0);

    char* error_message = nullptr;
    const std::uint8_t invalid_dat[4] = {0, 1, 2, 3};
    const dj1000_status invalid_status = dj1000_convert_dat(
        invalid_dat,
        sizeof(invalid_dat),
        &options,
        &image,
        &error_message
    );
    assert(invalid_status == DJ1000_STATUS_INVALID_DAT);
    assert(error_message != nullptr);
    dj1000_free_string(error_message);
    error_message = nullptr;

    const auto bytes = make_sample_dat();
    options.size = DJ1000_EXPORT_SIZE_NORMAL;
    options.pixel_format = DJ1000_PIXEL_FORMAT_RGBA;
    const dj1000_status status = dj1000_convert_dat(
        bytes.data(),
        bytes.size(),
        &options,
        &image,
        &error_message
    );
    assert(status == DJ1000_STATUS_OK);
    assert(error_message == nullptr);
    assert(image.width == 320);
    assert(image.height == 240);
    assert(image.channels == 4);
    assert(image.byte_count == static_cast<std::size_t>(image.width * image.height * image.channels));

    dj1000::ConvertOptions cpp_options;
    cpp_options.size = dj1000::ExportSize::Normal;
    const auto cpp_image = dj1000::convert_dat_bytes_to_bgr(bytes, cpp_options);
    const auto expected_rgba = cpp_image.interleaved_rgba();
    assert(expected_rgba.size() == image.byte_count);
    assert(std::equal(expected_rgba.begin(), expected_rgba.end(), image.pixels));

    dj1000_image_free(&image);

    dj1000_session* session = nullptr;
    const dj1000_status session_open_status = dj1000_session_open(
        bytes.data(),
        bytes.size(),
        &session,
        &error_message
    );
    assert(session_open_status == DJ1000_STATUS_OK);
    assert(session != nullptr);
    assert(error_message == nullptr);

    options.size = DJ1000_EXPORT_SIZE_SMALL;
    options.pixel_format = DJ1000_PIXEL_FORMAT_RGB;
    options.sliders.brightness = 6;
    const dj1000_status session_render_status = dj1000_session_render(
        session,
        &options,
        &image,
        &error_message
    );
    assert(session_render_status == DJ1000_STATUS_OK);
    assert(error_message == nullptr);
    assert(image.width == 320);
    assert(image.height == 240);
    assert(image.channels == 3);

    cpp_options.size = dj1000::ExportSize::Small;
    cpp_options.sliders.brightness = 6;
    const auto session_expected = dj1000::convert_dat_bytes_to_bgr(bytes, cpp_options);
    const auto expected_rgb = session_expected.interleaved_rgb();
    assert(expected_rgb.size() == image.byte_count);
    assert(std::equal(expected_rgb.begin(), expected_rgb.end(), image.pixels));

    dj1000_image_free(&image);
    dj1000_session_free(session);

    std::cout << "test_c_api passed\n";
    return 0;
}
