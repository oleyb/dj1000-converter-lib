#include "dj1000/c_api.h"

#include "dj1000/converter.hpp"
#include "dj1000/dat_file.hpp"

#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

struct dj1000_session {
    explicit dj1000_session(dj1000::Session session_value)
        : session(std::move(session_value)) {}

    dj1000::Session session;
};

namespace {

char* duplicate_c_string(std::string_view text) {
    char* value = static_cast<char*>(std::malloc(text.size() + 1));
    if (value == nullptr) {
        return nullptr;
    }
    std::memcpy(value, text.data(), text.size());
    value[text.size()] = '\0';
    return value;
}

void reset_image(dj1000_image* image) {
    if (image == nullptr) {
        return;
    }
    image->width = 0;
    image->height = 0;
    image->channels = 0;
    image->stride = 0;
    image->byte_count = 0;
    image->pixel_format = DJ1000_PIXEL_FORMAT_RGBA;
    image->pixels = nullptr;
}

void set_error_message(char** out_error_message, std::string_view text) {
    if (out_error_message == nullptr) {
        return;
    }
    *out_error_message = duplicate_c_string(text);
}

bool is_valid_export_size(dj1000_export_size size) {
    switch (size) {
        case DJ1000_EXPORT_SIZE_SMALL:
        case DJ1000_EXPORT_SIZE_NORMAL:
        case DJ1000_EXPORT_SIZE_LARGE:
            return true;
    }
    return false;
}

bool is_valid_pipeline(dj1000_pipeline pipeline) {
    switch (pipeline) {
        case DJ1000_PIPELINE_LEGACY:
        case DJ1000_PIPELINE_MODERN:
            return true;
    }
    return false;
}

bool is_valid_pixel_format(dj1000_pixel_format format) {
    switch (format) {
        case DJ1000_PIXEL_FORMAT_BGR:
        case DJ1000_PIXEL_FORMAT_RGB:
        case DJ1000_PIXEL_FORMAT_RGBA:
            return true;
    }
    return false;
}

dj1000::ExportSize to_cpp_export_size(dj1000_export_size size) {
    switch (size) {
        case DJ1000_EXPORT_SIZE_SMALL:
            return dj1000::ExportSize::Small;
        case DJ1000_EXPORT_SIZE_NORMAL:
            return dj1000::ExportSize::Normal;
        case DJ1000_EXPORT_SIZE_LARGE:
            return dj1000::ExportSize::Large;
    }
    throw std::runtime_error("unsupported export size");
}

dj1000::ConversionPipeline to_cpp_pipeline(dj1000_pipeline pipeline) {
    switch (pipeline) {
        case DJ1000_PIPELINE_LEGACY:
            return dj1000::ConversionPipeline::Legacy;
        case DJ1000_PIPELINE_MODERN:
            return dj1000::ConversionPipeline::Modern;
    }
    throw std::runtime_error("unsupported conversion pipeline");
}

dj1000::ConvertOptions to_cpp_options(const dj1000_convert_options& options) {
    dj1000::ConvertOptions converted;
    converted.size = to_cpp_export_size(options.size);
    converted.pipeline = to_cpp_pipeline(options.pipeline);
    converted.sliders.red_balance = options.sliders.red_balance;
    converted.sliders.green_balance = options.sliders.green_balance;
    converted.sliders.blue_balance = options.sliders.blue_balance;
    converted.sliders.contrast = options.sliders.contrast;
    converted.sliders.brightness = options.sliders.brightness;
    converted.sliders.vividness = options.sliders.vividness;
    converted.sliders.sharpness = options.sliders.sharpness;
    if (options.has_source_gain != 0) {
        converted.source_gain = options.source_gain;
    }
    return converted;
}

bool prepare_convert_options(
    const dj1000_convert_options* options,
    dj1000_convert_options* effective_options,
    char** out_error_message
) {
    dj1000_init_convert_options(effective_options);
    if (options != nullptr) {
        *effective_options = *options;
    }

    if (!is_valid_export_size(effective_options->size)) {
        set_error_message(out_error_message, "invalid export size");
        return false;
    }
    if (!is_valid_pipeline(effective_options->pipeline)) {
        set_error_message(out_error_message, "invalid conversion pipeline");
        return false;
    }
    if (!is_valid_pixel_format(effective_options->pixel_format)) {
        set_error_message(out_error_message, "invalid pixel format");
        return false;
    }
    return true;
}

std::vector<std::uint8_t> extract_pixels(
    const dj1000::ConvertedImage& image,
    dj1000_pixel_format pixel_format,
    int* out_channels
) {
    switch (pixel_format) {
        case DJ1000_PIXEL_FORMAT_BGR:
            *out_channels = 3;
            return image.interleaved_bgr();
        case DJ1000_PIXEL_FORMAT_RGB:
            *out_channels = 3;
            return image.interleaved_rgb();
        case DJ1000_PIXEL_FORMAT_RGBA:
            *out_channels = 4;
            return image.interleaved_rgba();
    }
    throw std::runtime_error("unsupported pixel format");
}

dj1000_status copy_converted_image(
    const dj1000::ConvertedImage& converted,
    dj1000_pixel_format pixel_format,
    dj1000_image* out_image,
    char** out_error_message
) {
    int channels = 0;
    const auto pixels = extract_pixels(converted, pixel_format, &channels);
    auto* owned_pixels = static_cast<std::uint8_t*>(std::malloc(pixels.size()));
    if (owned_pixels == nullptr) {
        set_error_message(out_error_message, "out of memory allocating output image");
        return DJ1000_STATUS_OUT_OF_MEMORY;
    }

    std::memcpy(owned_pixels, pixels.data(), pixels.size());
    out_image->width = converted.width;
    out_image->height = converted.height;
    out_image->channels = channels;
    out_image->stride = static_cast<size_t>(converted.width * channels);
    out_image->byte_count = pixels.size();
    out_image->pixel_format = pixel_format;
    out_image->pixels = owned_pixels;
    return DJ1000_STATUS_OK;
}

}  // namespace

extern "C" {

void dj1000_init_convert_options(dj1000_convert_options* options) {
    if (options == nullptr) {
        return;
    }

    options->size = DJ1000_EXPORT_SIZE_LARGE;
    options->pipeline = DJ1000_PIPELINE_LEGACY;
    options->sliders.red_balance = 100;
    options->sliders.green_balance = 100;
    options->sliders.blue_balance = 100;
    options->sliders.contrast = 3;
    options->sliders.brightness = 3;
    options->sliders.vividness = 3;
    options->sliders.sharpness = 3;
    options->has_source_gain = 0;
    options->source_gain = 1.0;
    options->pixel_format = DJ1000_PIXEL_FORMAT_RGBA;
}

void dj1000_init_image(dj1000_image* image) {
    reset_image(image);
}

dj1000_status dj1000_convert_dat(
    const uint8_t* dat_bytes,
    size_t dat_size,
    const dj1000_convert_options* options,
    dj1000_image* out_image,
    char** out_error_message
) {
    if (out_error_message != nullptr) {
        *out_error_message = nullptr;
    }
    if (out_image == nullptr) {
        set_error_message(out_error_message, "out_image must not be null");
        return DJ1000_STATUS_INVALID_ARGUMENT;
    }

    reset_image(out_image);

    if (dat_bytes == nullptr) {
        set_error_message(out_error_message, "dat_bytes must not be null");
        return DJ1000_STATUS_INVALID_ARGUMENT;
    }
    if (dat_size != dj1000::kExpectedDatSize) {
        set_error_message(out_error_message, "DAT input must be exactly 0x20000 bytes");
        return DJ1000_STATUS_INVALID_DAT;
    }

    dj1000_convert_options effective_options;
    if (!prepare_convert_options(options, &effective_options, out_error_message)) {
        return DJ1000_STATUS_INVALID_ARGUMENT;
    }

    try {
        const auto converted = dj1000::convert_dat_bytes_to_bgr(
            std::span<const std::uint8_t>(dat_bytes, dat_size),
            to_cpp_options(effective_options)
        );
        return copy_converted_image(
            converted,
            effective_options.pixel_format,
            out_image,
            out_error_message
        );
    } catch (const std::bad_alloc&) {
        set_error_message(out_error_message, "out of memory");
        return DJ1000_STATUS_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error_message(out_error_message, error.what());
        return DJ1000_STATUS_CONVERSION_ERROR;
    }
}

dj1000_status dj1000_session_open(
    const uint8_t* dat_bytes,
    size_t dat_size,
    dj1000_session** out_session,
    char** out_error_message
) {
    if (out_error_message != nullptr) {
        *out_error_message = nullptr;
    }
    if (out_session == nullptr) {
        set_error_message(out_error_message, "out_session must not be null");
        return DJ1000_STATUS_INVALID_ARGUMENT;
    }
    *out_session = nullptr;

    if (dat_bytes == nullptr) {
        set_error_message(out_error_message, "dat_bytes must not be null");
        return DJ1000_STATUS_INVALID_ARGUMENT;
    }
    if (dat_size != dj1000::kExpectedDatSize) {
        set_error_message(out_error_message, "DAT input must be exactly 0x20000 bytes");
        return DJ1000_STATUS_INVALID_DAT;
    }

    try {
        auto session = dj1000::Session::open(std::span<const std::uint8_t>(dat_bytes, dat_size));
        auto* owned_session = new (std::nothrow) dj1000_session(std::move(session));
        if (owned_session == nullptr) {
            set_error_message(out_error_message, "out of memory allocating session");
            return DJ1000_STATUS_OUT_OF_MEMORY;
        }
        *out_session = owned_session;
        return DJ1000_STATUS_OK;
    } catch (const std::bad_alloc&) {
        set_error_message(out_error_message, "out of memory");
        return DJ1000_STATUS_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error_message(out_error_message, error.what());
        return DJ1000_STATUS_CONVERSION_ERROR;
    }
}

dj1000_status dj1000_session_render(
    const dj1000_session* session,
    const dj1000_convert_options* options,
    dj1000_image* out_image,
    char** out_error_message
) {
    if (out_error_message != nullptr) {
        *out_error_message = nullptr;
    }
    if (session == nullptr) {
        set_error_message(out_error_message, "session must not be null");
        return DJ1000_STATUS_INVALID_ARGUMENT;
    }
    if (out_image == nullptr) {
        set_error_message(out_error_message, "out_image must not be null");
        return DJ1000_STATUS_INVALID_ARGUMENT;
    }

    reset_image(out_image);

    dj1000_convert_options effective_options;
    if (!prepare_convert_options(options, &effective_options, out_error_message)) {
        return DJ1000_STATUS_INVALID_ARGUMENT;
    }

    try {
        const auto converted = session->session.render(to_cpp_options(effective_options));
        return copy_converted_image(
            converted,
            effective_options.pixel_format,
            out_image,
            out_error_message
        );
    } catch (const std::bad_alloc&) {
        set_error_message(out_error_message, "out of memory");
        return DJ1000_STATUS_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        set_error_message(out_error_message, error.what());
        return DJ1000_STATUS_CONVERSION_ERROR;
    }
}

void dj1000_session_free(dj1000_session* session) {
    delete session;
}

void dj1000_image_free(dj1000_image* image) {
    if (image == nullptr) {
        return;
    }
    std::free(image->pixels);
    reset_image(image);
}

void dj1000_free_string(char* value) {
    std::free(value);
}

const char* dj1000_status_string(dj1000_status status) {
    switch (status) {
        case DJ1000_STATUS_OK:
            return "ok";
        case DJ1000_STATUS_INVALID_ARGUMENT:
            return "invalid_argument";
        case DJ1000_STATUS_INVALID_DAT:
            return "invalid_dat";
        case DJ1000_STATUS_CONVERSION_ERROR:
            return "conversion_error";
        case DJ1000_STATUS_OUT_OF_MEMORY:
            return "out_of_memory";
    }
    return "unknown";
}

}  // extern "C"
