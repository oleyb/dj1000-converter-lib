#pragma once

#include "dj1000/modern_raw_frame.hpp"

#include <array>
#include <vector>

namespace dj1000 {

struct ModernSensorFrame {
    int full_width = 0;
    int full_height = 0;
    int active_x = 0;
    int active_y = 0;
    int active_width = 0;
    int active_height = 0;
    int cfa_repeat_x = 0;
    int cfa_repeat_y = 0;
    std::array<CfaColor, 4> cfa_pattern = {
        CfaColor::Unknown,
        CfaColor::Unknown,
        CfaColor::Unknown,
        CfaColor::Unknown,
    };
    bool cfa_pattern_locked = false;
    std::array<ComplementarySite, 4> complementary_pattern = {
        ComplementarySite::Unknown,
        ComplementarySite::Unknown,
        ComplementarySite::Unknown,
        ComplementarySite::Unknown,
    };
    bool complementary_pattern_locked = false;
    std::array<double, 4> black_levels = {0.0, 0.0, 0.0, 0.0};
    std::array<double, 4> white_levels = {1.0, 1.0, 1.0, 1.0};
    double shared_white_level = 1.0;
    std::vector<float> normalized_mosaic;

    [[nodiscard]] float at(int row, int col) const;
};

struct ModernSensorFrameStats {
    LinearRegionStats active;
    std::array<LinearRegionStats, 4> parity_sites{};
    std::array<double, 4> clip_fractions = {0.0, 0.0, 0.0, 0.0};
    std::vector<double> active_row_means;
    std::vector<double> active_column_means;
};

[[nodiscard]] std::array<double, 4> estimate_modern_white_levels(const ModernCalibratedRawFrame& frame);
[[nodiscard]] double estimate_modern_shared_white_level(const ModernCalibratedRawFrame& frame);
[[nodiscard]] std::array<CfaColor, 4> resolve_modern_cfa_pattern(const ModernSensorFrame& frame);
[[nodiscard]] std::array<ComplementarySite, 4> resolve_modern_complementary_pattern(const ModernSensorFrame& frame);
[[nodiscard]] ModernSensorFrame build_modern_sensor_frame(const DatFile& dat);
[[nodiscard]] ModernSensorFrame build_modern_sensor_frame(
    const ModernRawFrame& raw_frame,
    const ModernRawCalibration& calibration,
    const ModernCalibratedRawFrame& corrected_frame
);
[[nodiscard]] ModernSensorFrameStats analyze_modern_sensor_frame(const ModernSensorFrame& frame);

}  // namespace dj1000
