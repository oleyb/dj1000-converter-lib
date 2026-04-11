#include "dj1000/c_api.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace {

char* duplicate_error_text(const char* message) {
    if (message == nullptr) {
        return nullptr;
    }
    const std::size_t length = std::strlen(message);
    auto* copy = static_cast<char*>(std::malloc(length + 1));
    if (copy == nullptr) {
        return nullptr;
    }
    std::memcpy(copy, message, length);
    copy[length] = '\0';
    return copy;
}

bool prepare_output_pointers(int* out_width, int* out_height, std::size_t* out_byte_count, char** out_error_message) {
    if (out_error_message != nullptr) {
        *out_error_message = nullptr;
    }
    if (out_width == nullptr || out_height == nullptr || out_byte_count == nullptr) {
        if (out_error_message != nullptr) {
            *out_error_message = duplicate_error_text("output pointers must not be null");
        }
        return false;
    }

    *out_width = 0;
    *out_height = 0;
    *out_byte_count = 0;
    return true;
}

dj1000_convert_options build_options(
    int export_size,
    int red_balance,
    int green_balance,
    int blue_balance,
    int contrast,
    int brightness,
    int vividness,
    int sharpness,
    int has_source_gain,
    double source_gain
) {
    dj1000_convert_options options;
    dj1000_init_convert_options(&options);
    options.size = static_cast<dj1000_export_size>(export_size);
    options.sliders.red_balance = red_balance;
    options.sliders.green_balance = green_balance;
    options.sliders.blue_balance = blue_balance;
    options.sliders.contrast = contrast;
    options.sliders.brightness = brightness;
    options.sliders.vividness = vividness;
    options.sliders.sharpness = sharpness;
    options.has_source_gain = has_source_gain;
    options.source_gain = source_gain;
    options.pixel_format = DJ1000_PIXEL_FORMAT_RGBA;
    return options;
}

std::uint8_t* take_image_pixels(
    dj1000_image* image,
    int* out_width,
    int* out_height,
    std::size_t* out_byte_count
) {
    *out_width = image->width;
    *out_height = image->height;
    *out_byte_count = image->byte_count;
    std::uint8_t* pixels = image->pixels;
    image->pixels = nullptr;
    dj1000_image_free(image);
    return pixels;
}

}  // namespace

extern "C" {

std::uint8_t* dj1000_wasm_convert_dat_rgba(
    const std::uint8_t* dat_bytes,
    std::size_t dat_size,
    int export_size,
    int red_balance,
    int green_balance,
    int blue_balance,
    int contrast,
    int brightness,
    int vividness,
    int sharpness,
    int has_source_gain,
    double source_gain,
    int* out_width,
    int* out_height,
    std::size_t* out_byte_count,
    char** out_error_message
) {
    if (!prepare_output_pointers(out_width, out_height, out_byte_count, out_error_message)) {
        return nullptr;
    }

    const auto options = build_options(
        export_size,
        red_balance,
        green_balance,
        blue_balance,
        contrast,
        brightness,
        vividness,
        sharpness,
        has_source_gain,
        source_gain
    );

    dj1000_image image;
    dj1000_init_image(&image);
    const dj1000_status status = dj1000_convert_dat(
        dat_bytes,
        dat_size,
        &options,
        &image,
        out_error_message
    );
    if (status != DJ1000_STATUS_OK) {
        dj1000_image_free(&image);
        return nullptr;
    }

    return take_image_pixels(&image, out_width, out_height, out_byte_count);
}

dj1000_session* dj1000_wasm_session_open(
    const std::uint8_t* dat_bytes,
    std::size_t dat_size,
    char** out_error_message
) {
    if (out_error_message != nullptr) {
        *out_error_message = nullptr;
    }

    dj1000_session* session = nullptr;
    const dj1000_status status = dj1000_session_open(
        dat_bytes,
        dat_size,
        &session,
        out_error_message
    );
    if (status != DJ1000_STATUS_OK) {
        return nullptr;
    }
    return session;
}

std::uint8_t* dj1000_wasm_session_render_rgba(
    const dj1000_session* session,
    int export_size,
    int red_balance,
    int green_balance,
    int blue_balance,
    int contrast,
    int brightness,
    int vividness,
    int sharpness,
    int has_source_gain,
    double source_gain,
    int* out_width,
    int* out_height,
    std::size_t* out_byte_count,
    char** out_error_message
) {
    if (!prepare_output_pointers(out_width, out_height, out_byte_count, out_error_message)) {
        return nullptr;
    }

    const auto options = build_options(
        export_size,
        red_balance,
        green_balance,
        blue_balance,
        contrast,
        brightness,
        vividness,
        sharpness,
        has_source_gain,
        source_gain
    );

    dj1000_image image;
    dj1000_init_image(&image);
    const dj1000_status status = dj1000_session_render(
        session,
        &options,
        &image,
        out_error_message
    );
    if (status != DJ1000_STATUS_OK) {
        dj1000_image_free(&image);
        return nullptr;
    }

    return take_image_pixels(&image, out_width, out_height, out_byte_count);
}

void dj1000_wasm_session_free(dj1000_session* session) {
    dj1000_session_free(session);
}

void dj1000_wasm_free_buffer(void* buffer) {
    std::free(buffer);
}

void dj1000_wasm_free_string(char* value) {
    dj1000_free_string(value);
}

}  // extern "C"
