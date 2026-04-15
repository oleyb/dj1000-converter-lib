#include "dj1000/modern_sensor_frame.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace dj1000 {

namespace {

double compute_float_percentile(std::vector<float> values, double fraction) {
    if (values.empty()) {
        return 1.0;
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
    return static_cast<double>(values[index]);
}

LinearRegionStats accumulate_linear_region(
    const ModernSensorFrame& frame,
    int start_x,
    int start_y,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        return {};
    }

    LinearRegionStats stats;
    stats.minimum = std::numeric_limits<double>::infinity();
    stats.maximum = -std::numeric_limits<double>::infinity();

    double sum = 0.0;
    for (int row = start_y; row < start_y + height; ++row) {
        for (int col = start_x; col < start_x + width; ++col) {
            const double value = static_cast<double>(frame.at(row, col));
            stats.minimum = std::min(stats.minimum, value);
            stats.maximum = std::max(stats.maximum, value);
            sum += value;
            stats.count += 1;
        }
    }

    stats.mean = stats.count == 0 ? 0.0 : (sum / static_cast<double>(stats.count));
    if (stats.count == 0) {
        stats.minimum = 0.0;
        stats.maximum = 0.0;
    }
    return stats;
}

LinearRegionStats accumulate_linear_parity_site(
    const ModernSensorFrame& frame,
    int parity_row,
    int parity_col
) {
    LinearRegionStats stats;
    stats.minimum = std::numeric_limits<double>::infinity();
    stats.maximum = -std::numeric_limits<double>::infinity();

    double sum = 0.0;
    for (int row = frame.active_y + parity_row; row < frame.active_y + frame.active_height; row += 2) {
        for (int col = frame.active_x + parity_col; col < frame.active_x + frame.active_width; col += 2) {
            const double value = static_cast<double>(frame.at(row, col));
            stats.minimum = std::min(stats.minimum, value);
            stats.maximum = std::max(stats.maximum, value);
            sum += value;
            stats.count += 1;
        }
    }

    stats.mean = stats.count == 0 ? 0.0 : (sum / static_cast<double>(stats.count));
    if (stats.count == 0) {
        stats.minimum = 0.0;
        stats.maximum = 0.0;
    }
    return stats;
}

std::vector<double> compute_active_row_means(const ModernSensorFrame& frame) {
    std::vector<double> means(static_cast<std::size_t>(frame.active_height), 0.0);
    for (int row = 0; row < frame.active_height; ++row) {
        double sum = 0.0;
        for (int col = 0; col < frame.active_width; ++col) {
            sum += static_cast<double>(frame.at(frame.active_y + row, frame.active_x + col));
        }
        means[static_cast<std::size_t>(row)] = sum / static_cast<double>(frame.active_width);
    }
    return means;
}

std::vector<double> compute_active_column_means(const ModernSensorFrame& frame) {
    std::vector<double> means(static_cast<std::size_t>(frame.active_width), 0.0);
    for (int col = 0; col < frame.active_width; ++col) {
        double sum = 0.0;
        for (int row = 0; row < frame.active_height; ++row) {
            sum += static_cast<double>(frame.at(frame.active_y + row, frame.active_x + col));
        }
        means[static_cast<std::size_t>(col)] = sum / static_cast<double>(frame.active_height);
    }
    return means;
}

int parity_index(int row, int col) {
    return ((row & 1) << 1) | (col & 1);
}

}  // namespace

float ModernSensorFrame::at(int row, int col) const {
    if (row < 0 || row >= full_height || col < 0 || col >= full_width) {
        throw std::runtime_error("modern sensor frame sample is out of bounds");
    }
    const std::size_t index =
        static_cast<std::size_t>(row) * static_cast<std::size_t>(full_width) +
        static_cast<std::size_t>(col);
    return normalized_mosaic[index];
}

std::array<double, 4> estimate_modern_white_levels(const ModernCalibratedRawFrame& frame) {
    std::array<std::vector<float>, 4> parity_samples;
    for (int row = frame.active_y; row < frame.active_y + frame.active_height; ++row) {
        for (int col = frame.active_x; col < frame.active_x + frame.active_width; ++col) {
            const float value = frame.at(row, col);
            if (!(value > 0.0f)) {
                continue;
            }
            const std::size_t parity = static_cast<std::size_t>(parity_index(row, col));
            parity_samples[parity].push_back(value);
        }
    }

    std::array<double, 4> white_levels = {1.0, 1.0, 1.0, 1.0};
    for (std::size_t index = 0; index < white_levels.size(); ++index) {
        if (parity_samples[index].empty()) {
            white_levels[index] = 1.0;
            continue;
        }
        const double percentile = compute_float_percentile(parity_samples[index], 0.998);
        const auto max_it = std::max_element(parity_samples[index].begin(), parity_samples[index].end());
        const double maximum = max_it == parity_samples[index].end() ? 1.0 : static_cast<double>(*max_it);
        white_levels[index] = std::clamp(std::max(percentile, maximum * 0.9), 1.0, 255.0);
    }
    return white_levels;
}

double estimate_modern_shared_white_level(const ModernCalibratedRawFrame& frame) {
    std::vector<float> samples;
    for (int row = frame.active_y; row < frame.active_y + frame.active_height; ++row) {
        for (int col = frame.active_x; col < frame.active_x + frame.active_width; ++col) {
            const float value = frame.at(row, col);
            if (value > 0.0f) {
                samples.push_back(value);
            }
        }
    }

    if (samples.empty()) {
        return 1.0;
    }

    const double percentile = compute_float_percentile(samples, 0.998);
    const auto max_it = std::max_element(samples.begin(), samples.end());
    const double maximum = max_it == samples.end() ? 1.0 : static_cast<double>(*max_it);
    return std::clamp(std::max(percentile, maximum * 0.9), 1.0, 255.0);
}

std::array<CfaColor, 4> resolve_modern_cfa_pattern(const ModernSensorFrame& frame) {
    std::array<CfaColor, 4> pattern = frame.cfa_pattern;
    if (pattern[0] == CfaColor::Unknown && pattern[1] == CfaColor::Green &&
        pattern[2] == CfaColor::Green && pattern[3] == CfaColor::Unknown) {
        pattern[0] = CfaColor::Red;
        pattern[3] = CfaColor::Blue;
    }
    if (pattern[0] == CfaColor::Unknown) {
        pattern[0] = CfaColor::Red;
    }
    if (pattern[3] == CfaColor::Unknown) {
        pattern[3] = CfaColor::Blue;
    }
    return pattern;
}

std::array<ComplementarySite, 4> resolve_modern_complementary_pattern(const ModernSensorFrame& frame) {
    std::array<ComplementarySite, 4> pattern = frame.complementary_pattern;
    if (pattern[0] == ComplementarySite::Unknown &&
        pattern[1] == ComplementarySite::Unknown &&
        pattern[2] == ComplementarySite::Unknown &&
        pattern[3] == ComplementarySite::Unknown) {
        pattern = {
            ComplementarySite::A,
            ComplementarySite::C,
            ComplementarySite::D,
            ComplementarySite::E,
        };
    }
    return pattern;
}

ModernSensorFrame build_modern_sensor_frame(const DatFile& dat) {
    const auto raw_frame = build_modern_raw_frame(dat);
    const auto calibration = estimate_modern_raw_calibration(raw_frame);
    const auto corrected_frame = build_calibrated_modern_raw_frame(raw_frame, calibration);
    return build_modern_sensor_frame(raw_frame, calibration, corrected_frame);
}

ModernSensorFrame build_modern_sensor_frame(
    const ModernRawFrame& raw_frame,
    const ModernRawCalibration& calibration,
    const ModernCalibratedRawFrame& corrected_frame
) {
    if (corrected_frame.full_width != raw_frame.full_width || corrected_frame.full_height != raw_frame.full_height) {
        throw std::runtime_error("modern sensor frame requires matching raw and corrected geometry");
    }

    ModernSensorFrame sensor_frame;
    sensor_frame.full_width = raw_frame.full_width;
    sensor_frame.full_height = raw_frame.full_height;
    sensor_frame.active_x = raw_frame.active_x;
    sensor_frame.active_y = raw_frame.active_y;
    sensor_frame.active_width = raw_frame.active_width;
    sensor_frame.active_height = raw_frame.active_height;
    sensor_frame.cfa_repeat_x = raw_frame.cfa_repeat_x;
    sensor_frame.cfa_repeat_y = raw_frame.cfa_repeat_y;
    sensor_frame.cfa_pattern = raw_frame.cfa_pattern;
    sensor_frame.cfa_pattern_locked = raw_frame.cfa_pattern_locked;
    sensor_frame.complementary_pattern = raw_frame.complementary_pattern;
    sensor_frame.complementary_pattern_locked = raw_frame.complementary_pattern_locked;
    sensor_frame.black_levels = calibration.black_levels;
    sensor_frame.white_levels = estimate_modern_white_levels(corrected_frame);
    sensor_frame.shared_white_level = estimate_modern_shared_white_level(corrected_frame);
    sensor_frame.normalized_mosaic.resize(corrected_frame.mosaic.size(), 0.0f);

    const bool use_shared_white = sensor_frame.complementary_pattern_locked;
    const double shared_white = std::max(sensor_frame.shared_white_level, 1.0);

    for (int row = 0; row < corrected_frame.full_height; ++row) {
        for (int col = 0; col < corrected_frame.full_width; ++col) {
            const std::size_t parity = static_cast<std::size_t>(parity_index(row, col));
            const double white = use_shared_white
                ? shared_white
                : std::max(sensor_frame.white_levels[parity], 1.0);
            const double normalized = static_cast<double>(corrected_frame.at(row, col)) / white;
            const std::size_t index =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(corrected_frame.full_width) +
                static_cast<std::size_t>(col);
            sensor_frame.normalized_mosaic[index] = static_cast<float>(std::clamp(normalized, 0.0, 1.0));
        }
    }

    return sensor_frame;
}

ModernSensorFrameStats analyze_modern_sensor_frame(const ModernSensorFrame& frame) {
    ModernSensorFrameStats stats;
    stats.active = accumulate_linear_region(
        frame,
        frame.active_x,
        frame.active_y,
        frame.active_width,
        frame.active_height
    );
    stats.parity_sites[0] = accumulate_linear_parity_site(frame, 0, 0);
    stats.parity_sites[1] = accumulate_linear_parity_site(frame, 0, 1);
    stats.parity_sites[2] = accumulate_linear_parity_site(frame, 1, 0);
    stats.parity_sites[3] = accumulate_linear_parity_site(frame, 1, 1);
    stats.active_row_means = compute_active_row_means(frame);
    stats.active_column_means = compute_active_column_means(frame);

    std::array<std::size_t, 4> parity_counts = {0, 0, 0, 0};
    std::array<std::size_t, 4> clipped_counts = {0, 0, 0, 0};
    for (int row = frame.active_y; row < frame.active_y + frame.active_height; ++row) {
        for (int col = frame.active_x; col < frame.active_x + frame.active_width; ++col) {
            const std::size_t parity = static_cast<std::size_t>(parity_index(row, col));
            parity_counts[parity] += 1;
            if (frame.at(row, col) >= 0.999f) {
                clipped_counts[parity] += 1;
            }
        }
    }
    for (std::size_t index = 0; index < stats.clip_fractions.size(); ++index) {
        stats.clip_fractions[index] =
            parity_counts[index] == 0
                ? 0.0
                : (static_cast<double>(clipped_counts[index]) / static_cast<double>(parity_counts[index]));
    }

    return stats;
}

}  // namespace dj1000
