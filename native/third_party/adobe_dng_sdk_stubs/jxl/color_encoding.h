#pragma once

struct JxlColorEncoding {
    int color_space = 0;
    int white_point = 0;
    int primaries = 0;
    int transfer_function = 0;
};

inline constexpr int JXL_COLOR_SPACE_GRAY = 1;
inline constexpr int JXL_COLOR_SPACE_RGB = 2;
inline constexpr int JXL_WHITE_POINT_D65 = 1;
inline constexpr int JXL_PRIMARIES_2100 = 1;
inline constexpr int JXL_TRANSFER_FUNCTION_SRGB = 13;
