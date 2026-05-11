#include "dj1000/dat_file.hpp"
#include "dj1000/modern_raw_frame.hpp"
#include "dj1000/modern_sensor_frame.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint8_t> make_sample_dat() {
    std::vector<std::uint8_t> bytes(dj1000::kExpectedDatSize, 0);

    for (int row = 0; row < dj1000::kModernRawFullHeight; ++row) {
        for (int col = 0; col < dj1000::kModernRawFullWidth; ++col) {
            std::uint8_t value = 0;
            if (row < dj1000::kModernRawActiveHeight && col < dj1000::kModernRawActiveWidth) {
                value = static_cast<std::uint8_t>(10 + (row % 5) + (col % 3));
            } else if (row < dj1000::kModernRawActiveHeight && col == dj1000::kModernRawActiveWidth) {
                value = bytes[static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kModernRawFullWidth) +
                              static_cast<std::size_t>(dj1000::kModernRawActiveWidth - 2)];
            } else if (row < dj1000::kModernRawActiveHeight && col == dj1000::kModernRawActiveWidth + 1) {
                value = 2;
            } else if (row == dj1000::kModernRawActiveHeight + 2 || row == dj1000::kModernRawActiveHeight + 3) {
                value = 2;
            }
            bytes[static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kModernRawFullWidth) +
                  static_cast<std::size_t>(col)] = value;
        }
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

bool almost_equal(double left, double right, double epsilon = 1.0e-6) {
    return std::fabs(left - right) < epsilon;
}

}  // namespace

int main() {
    const auto bytes = make_sample_dat();
    const auto dat = dj1000::make_dat_file(bytes);
    const auto raw_frame = dj1000::build_modern_raw_frame(dat);
    const auto calibration = dj1000::estimate_modern_raw_calibration(raw_frame);
    const auto corrected = dj1000::build_calibrated_modern_raw_frame(raw_frame, calibration);
    const auto sensor_frame = dj1000::build_modern_sensor_frame(raw_frame, calibration, corrected);
    const auto sensor_stats = dj1000::analyze_modern_sensor_frame(sensor_frame);

    assert(sensor_frame.full_width == raw_frame.full_width);
    assert(sensor_frame.full_height == raw_frame.full_height);
    assert(sensor_frame.active_width == raw_frame.active_width);
    assert(sensor_frame.active_height == raw_frame.active_height);
    assert(sensor_frame.normalized_mosaic.size() == corrected.mosaic.size());
    assert(sensor_frame.complementary_pattern_locked);
    assert(sensor_frame.complementary_pattern[0] == dj1000::ComplementarySite::A);
    assert(sensor_frame.complementary_pattern[1] == dj1000::ComplementarySite::C);
    assert(sensor_frame.complementary_pattern[2] == dj1000::ComplementarySite::D);
    assert(sensor_frame.complementary_pattern[3] == dj1000::ComplementarySite::E);

    assert(almost_equal(sensor_frame.black_levels[0], calibration.black_levels[0]));
    assert(almost_equal(sensor_frame.black_levels[1], calibration.black_levels[1]));
    assert(almost_equal(sensor_frame.black_levels[2], calibration.black_levels[2]));
    assert(almost_equal(sensor_frame.black_levels[3], calibration.black_levels[3]));

    for (const double white_level : sensor_frame.white_levels) {
        assert(white_level >= 12.0);
        assert(white_level <= 16.5);
    }

    assert(almost_equal(
        sensor_frame.at(0, 0),
        std::clamp(static_cast<double>(corrected.at(0, 0)) / sensor_frame.white_levels[0], 0.0, 1.0)
    ));
    assert(almost_equal(
        sensor_frame.at(0, 1),
        std::clamp(static_cast<double>(corrected.at(0, 1)) / sensor_frame.white_levels[1], 0.0, 1.0)
    ));
    assert(sensor_frame.at(0, 504) > 0.0f);
    assert(sensor_frame.at(0, 504) <= 1.0f);
    assert(sensor_frame.at(250, 0) == 0.0f);

    assert(sensor_stats.active.count == static_cast<std::size_t>(sensor_frame.active_width * sensor_frame.active_height));
    assert(sensor_stats.active.mean > 0.6);
    assert(sensor_stats.active.mean < 0.9);
    assert(sensor_stats.active_row_means.size() == static_cast<std::size_t>(sensor_frame.active_height));
    assert(sensor_stats.active_column_means.size() == static_cast<std::size_t>(sensor_frame.active_width));

    for (std::size_t index = 0; index < sensor_stats.clip_fractions.size(); ++index) {
        assert(sensor_stats.clip_fractions[index] > 0.0);
        assert(sensor_stats.clip_fractions[index] < 0.2);
    }

    std::cout << "test_modern_sensor_frame passed\n";
    return 0;
}
