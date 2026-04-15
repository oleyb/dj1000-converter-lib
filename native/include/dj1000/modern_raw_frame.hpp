#pragma once

#include "dj1000/dat_file.hpp"
#include "dj1000/source_seed_stage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dj1000 {

inline constexpr int kModernRawFullWidth = kNormalSourceSeedInputStride;
inline constexpr int kModernRawFullHeight = static_cast<int>(kRawBlockSize / kModernRawFullWidth);
inline constexpr int kModernRawActiveX = 0;
inline constexpr int kModernRawActiveY = 0;
inline constexpr int kModernRawActiveWidth = kNormalSourceSeedActiveWidth;
inline constexpr int kModernRawActiveHeight = kNormalSourceSeedActiveRows;
inline constexpr int kLc9997EffectiveWidth = 508;
inline constexpr int kLc9997EffectiveHeight = 246;
inline constexpr int kLc9997OpticalBlackFrontColumns = 2;
inline constexpr int kLc9997OpticalBlackBackColumns = 22;

enum class CfaColor {
    Unknown,
    Red,
    Green,
    Blue,
};

enum class ComplementarySite {
    Unknown,
    A,
    C,
    D,
    E,
};

struct RawRegionStats {
    std::size_t count = 0;
    std::uint16_t minimum = 0;
    std::uint16_t maximum = 0;
    double mean = 0.0;
};

struct LinearRegionStats {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double mean = 0.0;
};

struct ModernRawFrame {
    int full_width = kModernRawFullWidth;
    int full_height = kModernRawFullHeight;
    int active_x = kModernRawActiveX;
    int active_y = kModernRawActiveY;
    int active_width = kModernRawActiveWidth;
    int active_height = kModernRawActiveHeight;
    int cfa_repeat_x = 2;
    int cfa_repeat_y = 2;
    std::array<CfaColor, 4> cfa_pattern = {
        CfaColor::Unknown,
        CfaColor::Green,
        CfaColor::Green,
        CfaColor::Unknown,
    };
    bool cfa_pattern_locked = false;
    std::array<ComplementarySite, 4> complementary_pattern = {
        ComplementarySite::A,
        ComplementarySite::C,
        ComplementarySite::D,
        ComplementarySite::E,
    };
    bool complementary_pattern_locked = true;
    std::array<double, 4> black_levels = {0.0, 0.0, 0.0, 0.0};
    std::array<double, 4> white_levels = {255.0, 255.0, 255.0, 255.0};
    std::vector<std::uint16_t> mosaic;

    [[nodiscard]] std::uint16_t at(int row, int col) const;
};

struct ModernRawFrameStats {
    RawRegionStats active;
    RawRegionStats right_margin;
    RawRegionStats bottom_margin;
    RawRegionStats inactive;
    std::array<RawRegionStats, 4> parity_sites{};
    std::vector<double> full_row_means;
    std::vector<double> active_row_means;
    std::vector<double> full_column_means;
    std::vector<double> active_column_means;
    std::vector<double> right_margin_column_means;
    std::vector<double> bottom_margin_row_means;
    std::vector<std::size_t> right_margin_column_nonzero_counts;
    std::vector<std::size_t> bottom_margin_row_nonzero_counts;
    std::vector<double> right_margin_column_correlations;
    std::vector<double> bottom_margin_row_correlations;
    int leading_structured_right_margin_columns = 0;
    int leading_structured_bottom_margin_rows = 0;
    int dark_reference_right_margin_columns = 0;
    int dark_reference_bottom_margin_rows = 0;
    int trailing_hard_zero_right_margin_columns = 0;
    int trailing_hard_zero_bottom_margin_rows = 0;
    int trailing_zero_right_margin_columns = 0;
    int trailing_zero_bottom_margin_rows = 0;
};

struct ModernRawCalibration {
    std::array<double, 4> black_levels = {0.0, 0.0, 0.0, 0.0};
    std::vector<double> active_row_offsets;
    std::vector<double> full_column_offsets;
    int dark_reference_right_margin_columns = 0;
    int dark_reference_bottom_margin_rows = 0;
    int row_offset_reference_right_margin_columns = 0;
    int column_offset_reference_bottom_margin_rows = 0;
    std::size_t black_reference_sample_count = 0;
};

struct ModernCalibratedRawFrame {
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
    std::array<ComplementarySite, 4> complementary_pattern = {
        ComplementarySite::Unknown,
        ComplementarySite::Unknown,
        ComplementarySite::Unknown,
        ComplementarySite::Unknown,
    };
    bool complementary_pattern_locked = false;
    std::array<double, 4> black_levels = {0.0, 0.0, 0.0, 0.0};
    std::vector<float> mosaic;

    [[nodiscard]] float at(int row, int col) const;
};

struct ModernCalibratedRawFrameStats {
    LinearRegionStats active;
    std::array<LinearRegionStats, 4> parity_sites{};
    std::vector<double> active_row_means;
    std::vector<double> active_column_means;
};

[[nodiscard]] ModernRawFrame build_modern_raw_frame(const DatFile& dat);
[[nodiscard]] ModernRawFrameStats analyze_modern_raw_frame(const ModernRawFrame& frame);
[[nodiscard]] ModernRawCalibration estimate_modern_raw_calibration(const ModernRawFrame& frame);
[[nodiscard]] ModernCalibratedRawFrame build_calibrated_modern_raw_frame(
    const ModernRawFrame& frame,
    const ModernRawCalibration& calibration
);
[[nodiscard]] ModernCalibratedRawFrameStats analyze_modern_calibrated_raw_frame(
    const ModernCalibratedRawFrame& frame
);

}  // namespace dj1000
