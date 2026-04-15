#include "dj1000/dat_file.hpp"
#include "dj1000/modern_raw_frame.hpp"

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
            } else if (row == dj1000::kModernRawActiveHeight) {
                value = bytes[static_cast<std::size_t>(dj1000::kModernRawActiveHeight - 2) *
                                  static_cast<std::size_t>(dj1000::kModernRawFullWidth) +
                              static_cast<std::size_t>(std::min(col, dj1000::kModernRawActiveWidth - 1))];
            } else if (row == dj1000::kModernRawActiveHeight + 1) {
                value = bytes[static_cast<std::size_t>(dj1000::kModernRawActiveHeight - 1) *
                                  static_cast<std::size_t>(dj1000::kModernRawFullWidth) +
                              static_cast<std::size_t>(std::min(col, dj1000::kModernRawActiveWidth - 1))];
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

bool almost_equal(double left, double right) {
    return std::fabs(left - right) < 1.0e-9;
}

}  // namespace

int main() {
    const auto bytes = make_sample_dat();
    const auto dat = dj1000::make_dat_file(bytes);
    const auto frame = dj1000::build_modern_raw_frame(dat);
    const auto stats = dj1000::analyze_modern_raw_frame(frame);
    const auto calibration = dj1000::estimate_modern_raw_calibration(frame);
    const auto corrected = dj1000::build_calibrated_modern_raw_frame(frame, calibration);
    const auto corrected_stats = dj1000::analyze_modern_calibrated_raw_frame(corrected);

    assert(frame.full_width == 512);
    assert(frame.full_height == 251);
    assert(frame.active_width == 504);
    assert(frame.active_height == 244);
    assert(frame.mosaic.size() == static_cast<std::size_t>(frame.full_width * frame.full_height));
    assert(frame.at(0, 0) == 10);
    assert(frame.at(0, 504) == frame.at(0, 502));
    assert(frame.at(0, 505) == 2);
    assert(frame.at(250, 0) == 0);

    assert(stats.active.count == static_cast<std::size_t>(504 * 244));
    assert(stats.right_margin.count == static_cast<std::size_t>(8 * 244));
    assert(stats.bottom_margin.count == static_cast<std::size_t>(512 * 7));
    assert(stats.inactive.count == stats.right_margin.count + stats.bottom_margin.count);

    assert(stats.active.mean > 11.0);
    assert(stats.right_margin.mean > 1.0);
    assert(stats.bottom_margin.mean > 1.0);
    assert(stats.inactive.mean > 0.0);

    assert(stats.full_row_means.size() == static_cast<std::size_t>(frame.full_height));
    assert(stats.active_row_means.size() == static_cast<std::size_t>(frame.active_height));
    assert(stats.full_column_means.size() == static_cast<std::size_t>(frame.full_width));
    assert(stats.active_column_means.size() == static_cast<std::size_t>(frame.active_width));
    assert(stats.right_margin_column_means.size() == static_cast<std::size_t>(frame.full_width - frame.active_width));
    assert(stats.bottom_margin_row_means.size() == static_cast<std::size_t>(frame.full_height - frame.active_height));
    assert(stats.right_margin_column_nonzero_counts.size() == stats.right_margin_column_means.size());
    assert(stats.bottom_margin_row_nonzero_counts.size() == stats.bottom_margin_row_means.size());
    assert(stats.right_margin_column_correlations.size() == stats.right_margin_column_means.size());
    assert(stats.bottom_margin_row_correlations.size() == stats.bottom_margin_row_means.size());
    assert(stats.leading_structured_right_margin_columns == 1);
    assert(stats.leading_structured_bottom_margin_rows == 2);
    assert(stats.dark_reference_right_margin_columns == 1);
    assert(stats.dark_reference_bottom_margin_rows == 2);
    assert(stats.trailing_hard_zero_right_margin_columns == 6);
    assert(stats.trailing_hard_zero_bottom_margin_rows == 3);
    assert(stats.trailing_zero_right_margin_columns == 6);
    assert(stats.trailing_zero_bottom_margin_rows == 3);

    assert(stats.right_margin_column_nonzero_counts[0] == static_cast<std::size_t>(frame.active_height));
    assert(stats.right_margin_column_nonzero_counts[1] == static_cast<std::size_t>(frame.active_height));
    assert(stats.right_margin_column_nonzero_counts[2] == 0);
    assert(stats.bottom_margin_row_nonzero_counts[0] == static_cast<std::size_t>(frame.full_width));
    assert(stats.bottom_margin_row_nonzero_counts[1] == static_cast<std::size_t>(frame.full_width));
    assert(stats.bottom_margin_row_nonzero_counts[2] == static_cast<std::size_t>(frame.full_width));
    assert(stats.bottom_margin_row_nonzero_counts[3] == static_cast<std::size_t>(frame.full_width));
    assert(stats.bottom_margin_row_nonzero_counts[4] == 0);
    assert(stats.right_margin_column_correlations[0] > 0.99);
    assert(stats.bottom_margin_row_correlations[0] > 0.99);
    assert(stats.bottom_margin_row_correlations[1] > 0.99);

    for (const auto& parity : stats.parity_sites) {
        assert(parity.count == static_cast<std::size_t>(252 * 122));
        assert(parity.mean > 11.0);
    }

    assert(calibration.black_levels[0] == 2.0);
    assert(calibration.black_levels[1] == 2.0);
    assert(calibration.black_levels[2] == 2.0);
    assert(calibration.black_levels[3] == 2.0);
    assert(calibration.active_row_offsets.size() == static_cast<std::size_t>(frame.active_height));
    assert(calibration.full_column_offsets.size() == static_cast<std::size_t>(frame.full_width));
    assert(calibration.dark_reference_right_margin_columns == 1);
    assert(calibration.dark_reference_bottom_margin_rows == 2);
    assert(calibration.row_offset_reference_right_margin_columns == 1);
    assert(calibration.column_offset_reference_bottom_margin_rows == 2);
    assert(calibration.black_reference_sample_count == static_cast<std::size_t>(frame.active_height + (2 * frame.full_width)));

    assert(corrected_stats.active.mean > 9.0);
    assert(corrected_stats.parity_sites[0].mean > 9.0);
    assert(corrected_stats.parity_sites[1].mean > 9.0);
    assert(corrected_stats.parity_sites[2].mean > 9.0);
    assert(corrected_stats.parity_sites[3].mean > 9.0);
    assert(almost_equal(corrected.at(0, 0), 8.0f));
    assert(almost_equal(corrected.at(0, 1), 9.0f));

    std::cout << "test_modern_raw_frame passed\n";
    return 0;
}
