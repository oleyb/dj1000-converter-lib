#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dj1000_status {
    DJ1000_STATUS_OK = 0,
    DJ1000_STATUS_INVALID_ARGUMENT = 1,
    DJ1000_STATUS_INVALID_DAT = 2,
    DJ1000_STATUS_CONVERSION_ERROR = 3,
    DJ1000_STATUS_OUT_OF_MEMORY = 4
} dj1000_status;

typedef enum dj1000_export_size {
    DJ1000_EXPORT_SIZE_SMALL = 0,
    DJ1000_EXPORT_SIZE_NORMAL = 1,
    DJ1000_EXPORT_SIZE_LARGE = 2
} dj1000_export_size;

typedef enum dj1000_pixel_format {
    DJ1000_PIXEL_FORMAT_BGR = 0,
    DJ1000_PIXEL_FORMAT_RGB = 1,
    DJ1000_PIXEL_FORMAT_RGBA = 2
} dj1000_pixel_format;

typedef struct dj1000_slider_settings {
    int red_balance;
    int green_balance;
    int blue_balance;
    int contrast;
    int brightness;
    int vividness;
    int sharpness;
} dj1000_slider_settings;

typedef struct dj1000_convert_options {
    dj1000_export_size size;
    dj1000_slider_settings sliders;
    int has_source_gain;
    double source_gain;
    dj1000_pixel_format pixel_format;
} dj1000_convert_options;

typedef struct dj1000_image {
    int width;
    int height;
    int channels;
    size_t stride;
    size_t byte_count;
    dj1000_pixel_format pixel_format;
    uint8_t* pixels;
} dj1000_image;

typedef struct dj1000_session dj1000_session;

void dj1000_init_convert_options(dj1000_convert_options* options);
void dj1000_init_image(dj1000_image* image);

dj1000_status dj1000_convert_dat(
    const uint8_t* dat_bytes,
    size_t dat_size,
    const dj1000_convert_options* options,
    dj1000_image* out_image,
    char** out_error_message
);

dj1000_status dj1000_session_open(
    const uint8_t* dat_bytes,
    size_t dat_size,
    dj1000_session** out_session,
    char** out_error_message
);

dj1000_status dj1000_session_render(
    const dj1000_session* session,
    const dj1000_convert_options* options,
    dj1000_image* out_image,
    char** out_error_message
);

void dj1000_session_free(dj1000_session* session);
void dj1000_image_free(dj1000_image* image);
void dj1000_free_string(char* value);
const char* dj1000_status_string(dj1000_status status);

#ifdef __cplusplus
}
#endif
