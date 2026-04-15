#include "modern_export_pipeline.hpp"

#include "dj1000/modern_sensor_frame.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace dj1000 {

namespace {

constexpr int kModernLargeWidth = 504;
constexpr int kModernLargeHeight = 378;
constexpr int kModernNormalWidth = 320;
constexpr int kModernNormalHeight = 240;

int reflect_index(int index, int limit) {
    if (limit <= 1) {
        return 0;
    }
    while (index < 0 || index >= limit) {
        if (index < 0) {
            index = -index;
        } else {
            index = (limit - 1) - (index - (limit - 1));
        }
    }
    return index;
}

void resize_plane_bicubic_in_place(
    std::span<const float> source,
    int source_width,
    int source_height,
    int target_width,
    int target_height,
    std::vector<float>& target
);

std::vector<float> guided_smooth_plane(
    std::span<const float> plane,
    std::span<const float> guide,
    int width,
    int height,
    double guide_sensitivity
) {
    const std::size_t sample_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane.size() < sample_count || guide.size() < sample_count) {
        throw std::runtime_error("modern guided smoothing input is too small");
    }

    std::vector<float> output(sample_count, 0.0f);
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const std::size_t index =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(col);
            const float guide_center = guide[index];

            double weighted_sum = 0.0;
            double weight_total = 0.0;
            for (int dy = -1; dy <= 1; ++dy) {
                const int sample_row = reflect_index(row + dy, height);
                for (int dx = -1; dx <= 1; ++dx) {
                    const int sample_col = reflect_index(col + dx, width);
                    const std::size_t sample_index =
                        static_cast<std::size_t>(sample_row) * static_cast<std::size_t>(width) +
                        static_cast<std::size_t>(sample_col);
                    const double spatial_weight =
                        (dx == 0 && dy == 0) ? 1.0 : ((dx == 0 || dy == 0) ? 0.7 : 0.45);
                    const double guide_delta =
                        std::fabs(static_cast<double>(guide[sample_index]) - static_cast<double>(guide_center));
                    const double range_weight = 1.0 / (1.0 + guide_delta * guide_sensitivity);
                    const double weight = spatial_weight * range_weight;
                    weighted_sum += static_cast<double>(plane[sample_index]) * weight;
                    weight_total += weight;
                }
            }

            output[index] = static_cast<float>(weighted_sum / weight_total);
        }
    }

    return output;
}

float smoothstep01(float edge0, float edge1, float value) {
    if (!(edge1 > edge0)) {
        return value >= edge1 ? 1.0f : 0.0f;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float percentile(std::vector<float> values, double fraction) {
    if (values.empty()) {
        return 0.0f;
    }
    fraction = std::clamp(fraction, 0.0, 1.0);
    const std::size_t index = static_cast<std::size_t>(
        std::clamp(
            std::llround(fraction * static_cast<double>(values.size() - 1)),
            0LL,
            static_cast<long long>(values.size() - 1)
        )
    );
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());
    return values[index];
}

struct WhiteBalanceScales {
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
};

WhiteBalanceScales compute_white_balance(
    std::span<const float> red,
    std::span<const float> green,
    std::span<const float> blue
) {
    const std::size_t sample_count = red.size();
    if (green.size() != sample_count || blue.size() != sample_count) {
        throw std::runtime_error("modern white balance planes must have matching sizes");
    }

    double sum_red = 0.0;
    double sum_green = 0.0;
    double sum_blue = 0.0;
    std::size_t count = 0;

    for (std::size_t index = 0; index < sample_count; ++index) {
        const double luminance =
            static_cast<double>(red[index]) * 0.2126 +
            static_cast<double>(green[index]) * 0.7152 +
            static_cast<double>(blue[index]) * 0.0722;
        if (luminance < 0.05 || luminance > 0.95) {
            continue;
        }
        sum_red += red[index];
        sum_green += green[index];
        sum_blue += blue[index];
        ++count;
    }

    if (count == 0) {
        return {};
    }

    const double mean_red = sum_red / static_cast<double>(count);
    const double mean_green = sum_green / static_cast<double>(count);
    const double mean_blue = sum_blue / static_cast<double>(count);
    const double target = (mean_red + mean_green + mean_blue) / 3.0;

    return {
        .red = static_cast<float>(std::clamp(target / std::max(mean_red, 1.0e-6), 0.5, 2.0)),
        .green = static_cast<float>(std::clamp(target / std::max(mean_green, 1.0e-6), 0.5, 2.0)),
        .blue = static_cast<float>(std::clamp(target / std::max(mean_blue, 1.0e-6), 0.5, 2.0)),
    };
}

CfaColor color_at(
    const ModernSensorFrame& frame,
    const std::array<CfaColor, 4>& pattern,
    int row,
    int col
) {
    const int absolute_row = frame.active_y + reflect_index(row, frame.active_height);
    const int absolute_col = frame.active_x + reflect_index(col, frame.active_width);
    const std::size_t parity = static_cast<std::size_t>(((absolute_row & 1) << 1) | (absolute_col & 1));
    return pattern[parity];
}

float active_sample(const ModernSensorFrame& frame, int row, int col) {
    const int absolute_row = frame.active_y + reflect_index(row, frame.active_height);
    const int absolute_col = frame.active_x + reflect_index(col, frame.active_width);
    return frame.at(absolute_row, absolute_col);
}

ComplementarySite complementary_site_at(
    const ModernSensorFrame& frame,
    const std::array<ComplementarySite, 4>& pattern,
    int row,
    int col
) {
    const int absolute_row = frame.active_y + reflect_index(row, frame.active_height);
    const int absolute_col = frame.active_x + reflect_index(col, frame.active_width);
    const std::size_t parity = static_cast<std::size_t>(((absolute_row & 1) << 1) | (absolute_col & 1));
    return pattern[parity];
}

bool uses_datasheet_complementary_layout(const ModernSensorFrame& frame) {
    if (!frame.complementary_pattern_locked) {
        return false;
    }
    const auto pattern = resolve_modern_complementary_pattern(frame);
    return pattern[0] == ComplementarySite::A &&
           pattern[1] == ComplementarySite::C &&
           pattern[2] == ComplementarySite::D &&
        pattern[3] == ComplementarySite::E;
}

float complementary_luma_response(ComplementarySite site) {
    switch (site) {
        case ComplementarySite::A:
        case ComplementarySite::E:
            return 5.0f;
        case ComplementarySite::C:
        case ComplementarySite::D:
            return 3.0f;
        case ComplementarySite::Unknown:
            return 4.0f;
    }
    return 4.0f;
}

float average_matching_cross_neighbors(
    const ModernSensorFrame& frame,
    const std::array<CfaColor, 4>& pattern,
    int row,
    int col,
    CfaColor target
) {
    double sum = 0.0;
    int count = 0;
    constexpr int offsets[4][2] = {
        {0, -1},
        {0, 1},
        {-1, 0},
        {1, 0},
    };
    for (const auto& offset : offsets) {
        const int sample_row = row + offset[0];
        const int sample_col = col + offset[1];
        if (color_at(frame, pattern, sample_row, sample_col) != target) {
            continue;
        }
        sum += static_cast<double>(active_sample(frame, sample_row, sample_col));
        ++count;
    }
    return count == 0 ? active_sample(frame, row, col) : static_cast<float>(sum / static_cast<double>(count));
}

float average_matching_diagonal_neighbors(
    const ModernSensorFrame& frame,
    const std::array<CfaColor, 4>& pattern,
    int row,
    int col,
    CfaColor target
) {
    double sum = 0.0;
    int count = 0;
    constexpr int offsets[4][2] = {
        {-1, -1},
        {-1, 1},
        {1, -1},
        {1, 1},
    };
    for (const auto& offset : offsets) {
        const int sample_row = row + offset[0];
        const int sample_col = col + offset[1];
        if (color_at(frame, pattern, sample_row, sample_col) != target) {
            continue;
        }
        sum += static_cast<double>(active_sample(frame, sample_row, sample_col));
        ++count;
    }
    return count == 0 ? active_sample(frame, row, col) : static_cast<float>(sum / static_cast<double>(count));
}

float interpolate_green_at_non_green_site(const ModernSensorFrame& frame, int row, int col) {
    const double left = static_cast<double>(active_sample(frame, row, col - 1));
    const double right = static_cast<double>(active_sample(frame, row, col + 1));
    const double up = static_cast<double>(active_sample(frame, row - 1, col));
    const double down = static_cast<double>(active_sample(frame, row + 1, col));
    const double far_left = static_cast<double>(active_sample(frame, row, col - 2));
    const double far_right = static_cast<double>(active_sample(frame, row, col + 2));
    const double far_up = static_cast<double>(active_sample(frame, row - 2, col));
    const double far_down = static_cast<double>(active_sample(frame, row + 2, col));

    const double horizontal_gradient = std::fabs(left - right) + 0.5 * std::fabs(far_left - far_right);
    const double vertical_gradient = std::fabs(up - down) + 0.5 * std::fabs(far_up - far_down);
    if (horizontal_gradient + 1.0e-6 < vertical_gradient) {
        return static_cast<float>(0.5 * (left + right));
    }
    if (vertical_gradient + 1.0e-6 < horizontal_gradient) {
        return static_cast<float>(0.5 * (up + down));
    }
    return static_cast<float>(0.25 * (left + right + up + down));
}

void apply_source_gain(ModernSensorFrame& frame, double gain) {
    if (!std::isfinite(gain) || gain <= 0.0) {
        throw std::runtime_error("modern source gain must be positive");
    }
    if (std::fabs(gain - 1.0) < 1.0e-6) {
        return;
    }

    for (int row = frame.active_y; row < frame.active_y + frame.active_height; ++row) {
        for (int col = frame.active_x; col < frame.active_x + frame.active_width; ++col) {
            const std::size_t index =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(frame.full_width) +
                static_cast<std::size_t>(col);
            frame.normalized_mosaic[index] = static_cast<float>(std::clamp(
                static_cast<double>(frame.normalized_mosaic[index]) * gain,
                0.0,
                1.0
            ));
        }
    }
}

CachedModernStage demosaic_sensor_frame(
    const ModernSensorFrame& sensor_frame,
    double source_gain
) {
    if (uses_datasheet_complementary_layout(sensor_frame)) {
        const auto pattern = resolve_modern_complementary_pattern(sensor_frame);
        const int width = sensor_frame.active_width;
        const int height = sensor_frame.active_height;
        const int block_width = width / 2;
        const int block_height = height / 2;
        const std::size_t block_count =
            static_cast<std::size_t>(block_width) * static_cast<std::size_t>(block_height);

        std::vector<float> block_red(block_count, 0.0f);
        std::vector<float> block_green(block_count, 0.0f);
        std::vector<float> block_blue(block_count, 0.0f);

        for (int block_row = 0; block_row < block_height; ++block_row) {
            for (int block_col = 0; block_col < block_width; ++block_col) {
                const int row = block_row * 2;
                const int col = block_col * 2;
                const float a = active_sample(sensor_frame, row, col);
                const float c = active_sample(sensor_frame, row, col + 1);
                const float d = active_sample(sensor_frame, row + 1, col);
                const float e = active_sample(sensor_frame, row + 1, col + 1);

                const std::size_t index =
                    static_cast<std::size_t>(block_row) * static_cast<std::size_t>(block_width) +
                    static_cast<std::size_t>(block_col);

                if (complementary_site_at(sensor_frame, pattern, row, col) != ComplementarySite::A ||
                    complementary_site_at(sensor_frame, pattern, row, col + 1) != ComplementarySite::C ||
                    complementary_site_at(sensor_frame, pattern, row + 1, col) != ComplementarySite::D ||
                    complementary_site_at(sensor_frame, pattern, row + 1, col + 1) != ComplementarySite::E) {
                    throw std::runtime_error("modern complementary sensor layout does not match the LC9997M tile");
                }

                const float red = std::max(0.0f, 0.5f * (e - d));
                const float blue = std::max(0.0f, 0.5f * (a - c));
                const float green_from_c = std::max(0.0f, 0.5f * (c - red));
                const float green_from_d = std::max(0.0f, 0.5f * (d - blue));
                const float green_from_average = std::max(0.0f, 0.125f * ((-a) + 3.0f * c + 3.0f * d - e));
                const float green = std::clamp(
                    (green_from_c + green_from_d + green_from_average) / 3.0f,
                    0.0f,
                    1.0f
                );

                block_red[index] = std::clamp(red, 0.0f, 1.0f);
                block_green[index] = std::clamp(green, 0.0f, 1.0f);
                block_blue[index] = std::clamp(blue, 0.0f, 1.0f);
            }
        }

        CachedModernStage cached_stage;
        cached_stage.source_gain = source_gain;
        cached_stage.width = width;
        cached_stage.height = height;
        resize_plane_bicubic_in_place(block_red, block_width, block_height, width, height, cached_stage.plane0);
        resize_plane_bicubic_in_place(block_green, block_width, block_height, width, height, cached_stage.plane1);
        resize_plane_bicubic_in_place(block_blue, block_width, block_height, width, height, cached_stage.plane2);

        const std::size_t sample_count =
            static_cast<std::size_t>(cached_stage.width) * static_cast<std::size_t>(cached_stage.height);
        std::vector<float> base_luma(sample_count, 0.0f);
        std::vector<float> sensor_luma(sample_count, 0.0f);
        double base_luma_sum = 0.0;
        double sensor_luma_sum = 0.0;
        std::size_t luma_scale_count = 0;
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                const std::size_t index =
                    static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(col);
                const float luma =
                    cached_stage.plane0[index] * 0.2126f +
                    cached_stage.plane1[index] * 0.7152f +
                    cached_stage.plane2[index] * 0.0722f;
                base_luma[index] = luma;

                const ComplementarySite site = complementary_site_at(sensor_frame, pattern, row, col);
                const float normalized_site_luma =
                    active_sample(sensor_frame, row, col) / complementary_luma_response(site);
                sensor_luma[index] = normalized_site_luma;
                if (luma > 0.02f && luma < 0.98f && normalized_site_luma > 1.0e-5f) {
                    base_luma_sum += static_cast<double>(luma);
                    sensor_luma_sum += static_cast<double>(normalized_site_luma);
                    ++luma_scale_count;
                }
            }
        }

        const float sensor_luma_scale = luma_scale_count == 0 || !(sensor_luma_sum > 0.0)
            ? 1.0f
            : static_cast<float>(base_luma_sum / sensor_luma_sum);
        for (float& value : sensor_luma) {
            value = std::clamp(value * sensor_luma_scale, 0.0f, 1.0f);
        }

        auto smoothed_sensor_luma = guided_smooth_plane(sensor_luma, sensor_luma, width, height, 6.0);
        std::vector<float> detail_luma(sample_count, 0.0f);
        std::vector<float> red_chroma(sample_count, 0.0f);
        std::vector<float> blue_chroma(sample_count, 0.0f);
        for (std::size_t index = 0; index < sample_count; ++index) {
            detail_luma[index] = std::clamp(
                smoothed_sensor_luma[index] * 0.85f + base_luma[index] * 0.15f,
                0.0f,
                1.0f
            );
            red_chroma[index] = cached_stage.plane0[index] - base_luma[index];
            blue_chroma[index] = cached_stage.plane2[index] - base_luma[index];
        }

        auto smoothed_red_chroma = guided_smooth_plane(red_chroma, detail_luma, width, height, 24.0);
        auto smoothed_blue_chroma = guided_smooth_plane(blue_chroma, detail_luma, width, height, 24.0);
        std::vector<float> edge_strength(sample_count, 0.0f);
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                const int left = reflect_index(col - 1, width);
                const int right = reflect_index(col + 1, width);
                const int up = reflect_index(row - 1, height);
                const int down = reflect_index(row + 1, height);
                const std::size_t index =
                    static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(col);
                const float center = detail_luma[index];
                const float horizontal = std::max(
                    std::fabs(center - detail_luma[static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                                                   static_cast<std::size_t>(left)]),
                    std::fabs(center - detail_luma[static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                                                   static_cast<std::size_t>(right)])
                );
                const float vertical = std::max(
                    std::fabs(center - detail_luma[static_cast<std::size_t>(up) * static_cast<std::size_t>(width) +
                                                   static_cast<std::size_t>(col)]),
                    std::fabs(center - detail_luma[static_cast<std::size_t>(down) * static_cast<std::size_t>(width) +
                                                   static_cast<std::size_t>(col)])
                );
                edge_strength[index] = std::max(horizontal, vertical);
            }
        }

        for (std::size_t index = 0; index < sample_count; ++index) {
            const float luma = detail_luma[index];
            const float highlight_chroma_scale = 1.0f - 0.78f * smoothstep01(0.72f, 0.98f, luma);
            const float edge_chroma_scale = 1.0f - 0.6f * smoothstep01(0.035f, 0.18f, edge_strength[index]);
            const float chroma_scale = 0.82f * highlight_chroma_scale * edge_chroma_scale;
            const float red =
                std::clamp(luma + smoothed_red_chroma[index] * chroma_scale, 0.0f, 1.0f);
            const float blue =
                std::clamp(luma + smoothed_blue_chroma[index] * chroma_scale, 0.0f, 1.0f);
            const float green = std::clamp(
                (luma - red * 0.2126f - blue * 0.0722f) / 0.7152f,
                0.0f,
                1.0f
            );
            cached_stage.plane0[index] = red;
            cached_stage.plane1[index] = green;
            cached_stage.plane2[index] = blue;
        }

        return cached_stage;
    }

    const auto pattern = resolve_modern_cfa_pattern(sensor_frame);
    const int width = sensor_frame.active_width;
    const int height = sensor_frame.active_height;
    const std::size_t sample_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    CachedModernStage cached_stage;
    cached_stage.source_gain = source_gain;
    cached_stage.width = width;
    cached_stage.height = height;
    cached_stage.plane0.resize(sample_count, 0.0f);
    cached_stage.plane1.resize(sample_count, 0.0f);
    cached_stage.plane2.resize(sample_count, 0.0f);

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const std::size_t index =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(col);
            const CfaColor color = color_at(sensor_frame, pattern, row, col);
            const float sample = active_sample(sensor_frame, row, col);
            float red = sample;
            float green = sample;
            float blue = sample;

            switch (color) {
                case CfaColor::Red:
                    green = interpolate_green_at_non_green_site(sensor_frame, row, col);
                    blue = average_matching_diagonal_neighbors(sensor_frame, pattern, row, col, CfaColor::Blue);
                    break;
                case CfaColor::Blue:
                    green = interpolate_green_at_non_green_site(sensor_frame, row, col);
                    red = average_matching_diagonal_neighbors(sensor_frame, pattern, row, col, CfaColor::Red);
                    break;
                case CfaColor::Green:
                    red = average_matching_cross_neighbors(sensor_frame, pattern, row, col, CfaColor::Red);
                    blue = average_matching_cross_neighbors(sensor_frame, pattern, row, col, CfaColor::Blue);
                    break;
                case CfaColor::Unknown:
                    green = average_matching_cross_neighbors(sensor_frame, pattern, row, col, CfaColor::Green);
                    red = average_matching_cross_neighbors(sensor_frame, pattern, row, col, CfaColor::Red);
                    blue = average_matching_cross_neighbors(sensor_frame, pattern, row, col, CfaColor::Blue);
                    break;
            }

            cached_stage.plane0[index] = std::clamp(red, 0.0f, 1.0f);
            cached_stage.plane1[index] = std::clamp(green, 0.0f, 1.0f);
            cached_stage.plane2[index] = std::clamp(blue, 0.0f, 1.0f);
        }
    }

    std::vector<float> red_chroma(sample_count, 0.0f);
    std::vector<float> blue_chroma(sample_count, 0.0f);
    for (std::size_t index = 0; index < sample_count; ++index) {
        red_chroma[index] = cached_stage.plane0[index] - cached_stage.plane1[index];
        blue_chroma[index] = cached_stage.plane2[index] - cached_stage.plane1[index];
    }

    auto smoothed_green = guided_smooth_plane(
        cached_stage.plane1,
        cached_stage.plane1,
        width,
        height,
        6.0
    );
    auto smoothed_red_chroma = guided_smooth_plane(red_chroma, cached_stage.plane1, width, height, 18.0);
    auto smoothed_blue_chroma = guided_smooth_plane(blue_chroma, cached_stage.plane1, width, height, 18.0);

    for (std::size_t index = 0; index < sample_count; ++index) {
        cached_stage.plane1[index] = std::clamp(smoothed_green[index], 0.0f, 1.0f);
        cached_stage.plane0[index] =
            std::clamp(cached_stage.plane1[index] + smoothed_red_chroma[index], 0.0f, 1.0f);
        cached_stage.plane2[index] =
            std::clamp(cached_stage.plane1[index] + smoothed_blue_chroma[index], 0.0f, 1.0f);
    }

    return cached_stage;
}

float aces_filmic(float value) {
    if (!std::isfinite(value) || !(value > 0.0f)) {
        return 0.0f;
    }
    const float numerator = value * (2.51f * value + 0.03f);
    const float denominator = value * (2.43f * value + 0.59f) + 0.14f;
    return std::clamp(numerator / denominator, 0.0f, 1.0f);
}

void resize_plane_bicubic_in_place(
    std::span<const float> source,
    int source_width,
    int source_height,
    int target_width,
    int target_height,
    std::vector<float>& target
) {
    const std::size_t source_count =
        static_cast<std::size_t>(source_width) * static_cast<std::size_t>(source_height);
    if (source.size() < source_count) {
        throw std::runtime_error("modern resize source plane is too small");
    }

    const auto cubic_weight = [](double x) {
        constexpr double a = -0.5;
        x = std::fabs(x);
        if (x <= 1.0) {
            return (a + 2.0) * x * x * x - (a + 3.0) * x * x + 1.0;
        }
        if (x < 2.0) {
            return a * x * x * x - 5.0 * a * x * x + 8.0 * a * x - 4.0 * a;
        }
        return 0.0;
    };

    target.assign(static_cast<std::size_t>(target_width) * static_cast<std::size_t>(target_height), 0.0f);

    const double scale_x = static_cast<double>(source_width) / static_cast<double>(target_width);
    const double scale_y = static_cast<double>(source_height) / static_cast<double>(target_height);

    for (int y = 0; y < target_height; ++y) {
        const double source_y = (static_cast<double>(y) + 0.5) * scale_y - 0.5;
        const int base_y = static_cast<int>(std::floor(source_y));
        for (int x = 0; x < target_width; ++x) {
            const double source_x = (static_cast<double>(x) + 0.5) * scale_x - 0.5;
            const int base_x = static_cast<int>(std::floor(source_x));

            double weighted_sum = 0.0;
            double weight_total = 0.0;
            for (int sample_y = base_y - 1; sample_y <= base_y + 2; ++sample_y) {
                const double weight_y = cubic_weight(source_y - static_cast<double>(sample_y));
                const int reflected_y = reflect_index(sample_y, source_height);
                for (int sample_x = base_x - 1; sample_x <= base_x + 2; ++sample_x) {
                    const double weight_x = cubic_weight(source_x - static_cast<double>(sample_x));
                    const int reflected_x = reflect_index(sample_x, source_width);
                    const double weight = weight_x * weight_y;
                    const std::size_t sample_index =
                        static_cast<std::size_t>(reflected_y) * static_cast<std::size_t>(source_width) +
                        static_cast<std::size_t>(reflected_x);
                    weighted_sum += static_cast<double>(source[sample_index]) * weight;
                    weight_total += weight;
                }
            }

            const std::size_t target_index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(target_width) +
                static_cast<std::size_t>(x);
            target[target_index] = static_cast<float>(weighted_sum / weight_total);
        }
    }
}

void apply_detail_adjustment(
    std::vector<float>& plane0,
    std::vector<float>& plane1,
    std::vector<float>& plane2,
    int width,
    int height,
    int sharpness
) {
    const double amount = 0.14 * static_cast<double>(sharpness - 3);
    if (std::fabs(amount) < 1.0e-6) {
        return;
    }

    const auto blur_plane = [width, height](std::span<const float> source) {
        std::vector<float> blurred(source.size(), 0.0f);
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                double weighted_sum = 0.0;
                double weight_total = 0.0;
                for (int dy = -1; dy <= 1; ++dy) {
                    const int sample_row = reflect_index(row + dy, height);
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int sample_col = reflect_index(col + dx, width);
                        const std::size_t sample_index =
                            static_cast<std::size_t>(sample_row) * static_cast<std::size_t>(width) +
                            static_cast<std::size_t>(sample_col);
                        const double weight =
                            (dx == 0 && dy == 0) ? 4.0 : ((dx == 0 || dy == 0) ? 2.0 : 1.0);
                        weighted_sum += static_cast<double>(source[sample_index]) * weight;
                        weight_total += weight;
                    }
                }
                const std::size_t index =
                    static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(col);
                blurred[index] = static_cast<float>(weighted_sum / weight_total);
            }
        }
        return blurred;
    };

    std::vector<float> luma(plane0.size(), 0.0f);
    for (std::size_t index = 0; index < plane0.size(); ++index) {
        luma[index] =
            plane0[index] * 0.2126f +
            plane1[index] * 0.7152f +
            plane2[index] * 0.0722f;
    }
    std::vector<float> blurred_luma = blur_plane(luma);

    const std::size_t sample_count = plane0.size();
    for (std::size_t index = 0; index < sample_count; ++index) {
        float adjusted_luma = luma[index];
        if (amount >= 0.0) {
            adjusted_luma = std::clamp(
                luma[index] + static_cast<float>((luma[index] - blurred_luma[index]) * amount),
                0.0f,
                1.0f
            );
        } else {
            const float blend = static_cast<float>(-amount);
            adjusted_luma = std::clamp(luma[index] * (1.0f - blend) + blurred_luma[index] * blend, 0.0f, 1.0f);
        }

        const float scale = adjusted_luma / std::max(luma[index], 1.0e-5f);
        plane0[index] = std::clamp(plane0[index] * scale, 0.0f, 1.0f);
        plane1[index] = std::clamp(plane1[index] * scale, 0.0f, 1.0f);
        plane2[index] = std::clamp(plane2[index] * scale, 0.0f, 1.0f);
    }
}

RgbBytePlanes quantize_rgb_planes(
    std::span<const float> plane0,
    std::span<const float> plane1,
    std::span<const float> plane2
) {
    if (plane0.size() != plane1.size() || plane0.size() != plane2.size()) {
        throw std::runtime_error("modern output planes must have matching sizes");
    }

    const auto quantize = [](float value) {
        if (!std::isfinite(value) || !(value > 0.0f)) {
            return static_cast<std::uint8_t>(0);
        }
        if (value >= 1.0f) {
            return static_cast<std::uint8_t>(255);
        }
        return static_cast<std::uint8_t>(std::clamp(std::lround(value * 255.0f), 0L, 255L));
    };

    RgbBytePlanes output{
        .plane0 = std::vector<std::uint8_t>(plane0.size(), 0),
        .plane1 = std::vector<std::uint8_t>(plane1.size(), 0),
        .plane2 = std::vector<std::uint8_t>(plane2.size(), 0),
    };
    for (std::size_t index = 0; index < plane0.size(); ++index) {
        output.plane0[index] = quantize(plane0[index]);
        output.plane1[index] = quantize(plane1[index]);
        output.plane2[index] = quantize(plane2[index]);
    }
    return output;
}

}  // namespace

CachedModernStage build_cached_modern_stage(
    const DatFile& dat,
    std::optional<double> source_gain_override
) {
    ModernSensorFrame sensor_frame = build_modern_sensor_frame(dat);
    const double source_gain = source_gain_override.value_or(1.0);
    apply_source_gain(sensor_frame, source_gain);
    return demosaic_sensor_frame(sensor_frame, source_gain);
}

RenderedModernFloatImage render_modern_linear_from_cached_stage(
    const CachedModernStage& cached_stage,
    ExportSize size,
    const SliderSettings& sliders,
    ModernExportDebugState* debug_state
) {
    if (cached_stage.width <= 0 || cached_stage.height <= 0) {
        throw std::runtime_error("modern cached stage is empty");
    }

    std::vector<float> plane0 = cached_stage.plane0;
    std::vector<float> plane1 = cached_stage.plane1;
    std::vector<float> plane2 = cached_stage.plane2;

    const auto auto_balance = compute_white_balance(plane0, plane1, plane2);
    const float user_red = static_cast<float>(sliders.red_balance) / 100.0f;
    const float user_green = static_cast<float>(sliders.green_balance) / 100.0f;
    const float user_blue = static_cast<float>(sliders.blue_balance) / 100.0f;
    const float red_scale = auto_balance.red * user_red;
    const float green_scale = auto_balance.green * user_green;
    const float blue_scale = auto_balance.blue * user_blue;

    std::vector<float> luminance;
    luminance.reserve(plane0.size());
    for (std::size_t index = 0; index < plane0.size(); ++index) {
        plane0[index] *= red_scale;
        plane1[index] *= green_scale;
        plane2[index] *= blue_scale;
        luminance.push_back(
            plane0[index] * 0.2126f +
            plane1[index] * 0.7152f +
            plane2[index] * 0.0722f
        );
    }

    const float black_point = percentile(luminance, 0.01);
    const float white_point = std::max(percentile(luminance, 0.995), black_point + 1.0e-5f);
    const float exposure =
        static_cast<float>(1.25 * std::exp2(static_cast<double>(sliders.brightness - 3) * 0.2)) /
        std::max(white_point - black_point, 1.0e-5f);
    const float contrast_scale = std::clamp(
        1.0f + (0.12f * static_cast<float>(sliders.contrast - 3)),
        0.5f,
        1.75f
    );
    const float vividness_scale = std::clamp(
        1.0f + (0.18f * static_cast<float>(sliders.vividness - 3)),
        0.0f,
        2.0f
    );

    for (std::size_t index = 0; index < plane0.size(); ++index) {
        plane0[index] = aces_filmic(std::max(0.0f, (plane0[index] - black_point) * exposure));
        plane1[index] = aces_filmic(std::max(0.0f, (plane1[index] - black_point) * exposure));
        plane2[index] = aces_filmic(std::max(0.0f, (plane2[index] - black_point) * exposure));

        const float luma =
            plane0[index] * 0.2126f +
            plane1[index] * 0.7152f +
            plane2[index] * 0.0722f;
        plane0[index] = std::clamp(luma + (plane0[index] - luma) * vividness_scale, 0.0f, 1.0f);
        plane1[index] = std::clamp(luma + (plane1[index] - luma) * vividness_scale, 0.0f, 1.0f);
        plane2[index] = std::clamp(luma + (plane2[index] - luma) * vividness_scale, 0.0f, 1.0f);

        plane0[index] = std::clamp(0.5f + (plane0[index] - 0.5f) * contrast_scale, 0.0f, 1.0f);
        plane1[index] = std::clamp(0.5f + (plane1[index] - 0.5f) * contrast_scale, 0.0f, 1.0f);
        plane2[index] = std::clamp(0.5f + (plane2[index] - 0.5f) * contrast_scale, 0.0f, 1.0f);
    }

    int output_width = cached_stage.width;
    int output_height = cached_stage.height;
    int target_width = cached_stage.width;
    int target_height = cached_stage.height;
    if (size == ExportSize::Large) {
        target_width = kModernLargeWidth;
        target_height = kModernLargeHeight;
    } else {
        target_width = kModernNormalWidth;
        target_height = kModernNormalHeight;
    }

    if (target_width != cached_stage.width || target_height != cached_stage.height) {
        std::vector<float> resized0;
        std::vector<float> resized1;
        std::vector<float> resized2;
        resize_plane_bicubic_in_place(
            plane0,
            cached_stage.width,
            cached_stage.height,
            target_width,
            target_height,
            resized0
        );
        resize_plane_bicubic_in_place(
            plane1,
            cached_stage.width,
            cached_stage.height,
            target_width,
            target_height,
            resized1
        );
        resize_plane_bicubic_in_place(
            plane2,
            cached_stage.width,
            cached_stage.height,
            target_width,
            target_height,
            resized2
        );
        plane0 = std::move(resized0);
        plane1 = std::move(resized1);
        plane2 = std::move(resized2);
        output_width = target_width;
        output_height = target_height;
    }

    apply_detail_adjustment(plane0, plane1, plane2, output_width, output_height, sliders.sharpness);

    if (debug_state != nullptr) {
        debug_state->source_gain = cached_stage.source_gain;
        debug_state->exposure = exposure;
        debug_state->red_scale = red_scale;
        debug_state->green_scale = green_scale;
        debug_state->blue_scale = blue_scale;
    }

    return {
        .width = output_width,
        .height = output_height,
        .plane0 = std::move(plane0),
        .plane1 = std::move(plane1),
        .plane2 = std::move(plane2),
    };
}

RgbBytePlanes render_modern_export_from_cached_stage(
    const CachedModernStage& cached_stage,
    ExportSize size,
    const SliderSettings& sliders,
    ModernExportDebugState* debug_state
) {
    const auto rendered = render_modern_linear_from_cached_stage(cached_stage, size, sliders, debug_state);
    return quantize_rgb_planes(rendered.plane0, rendered.plane1, rendered.plane2);
}

RgbBytePlanes build_modern_export_planes(
    const DatFile& dat,
    ExportSize size,
    const SliderSettings& sliders,
    ModernExportDebugState* debug_state,
    std::optional<double> source_gain_override
) {
    const auto cached_stage = build_cached_modern_stage(dat, source_gain_override);
    return render_modern_export_from_cached_stage(cached_stage, size, sliders, debug_state);
}

}  // namespace dj1000
