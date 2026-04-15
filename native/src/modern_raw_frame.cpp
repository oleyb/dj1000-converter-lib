#include "dj1000/modern_raw_frame.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace dj1000 {

namespace {

RawRegionStats accumulate_region(
    const ModernRawFrame& frame,
    int start_x,
    int start_y,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        return {};
    }

    RawRegionStats stats;
    stats.minimum = std::numeric_limits<std::uint16_t>::max();

    double sum = 0.0;
    for (int row = start_y; row < start_y + height; ++row) {
        for (int col = start_x; col < start_x + width; ++col) {
            const std::uint16_t value = frame.at(row, col);
            stats.minimum = std::min(stats.minimum, value);
            stats.maximum = std::max(stats.maximum, value);
            sum += static_cast<double>(value);
            stats.count += 1;
        }
    }

    stats.mean = stats.count == 0 ? 0.0 : (sum / static_cast<double>(stats.count));
    if (stats.count == 0) {
        stats.minimum = 0;
    }
    return stats;
}

RawRegionStats accumulate_parity_site(const ModernRawFrame& frame, int parity_row, int parity_col) {
    RawRegionStats stats;
    stats.minimum = std::numeric_limits<std::uint16_t>::max();

    double sum = 0.0;
    for (int row = frame.active_y + parity_row; row < frame.active_y + frame.active_height; row += 2) {
        for (int col = frame.active_x + parity_col; col < frame.active_x + frame.active_width; col += 2) {
            const std::uint16_t value = frame.at(row, col);
            stats.minimum = std::min(stats.minimum, value);
            stats.maximum = std::max(stats.maximum, value);
            sum += static_cast<double>(value);
            stats.count += 1;
        }
    }

    stats.mean = stats.count == 0 ? 0.0 : (sum / static_cast<double>(stats.count));
    if (stats.count == 0) {
        stats.minimum = 0;
    }
    return stats;
}

LinearRegionStats accumulate_linear_region(
    const ModernCalibratedRawFrame& frame,
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
    const ModernCalibratedRawFrame& frame,
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

std::vector<double> compute_row_means(
    const ModernRawFrame& frame,
    int start_x,
    int width,
    int start_y,
    int height
) {
    std::vector<double> means(static_cast<std::size_t>(height), 0.0);
    if (width <= 0 || height <= 0) {
        return means;
    }

    for (int row = 0; row < height; ++row) {
        double sum = 0.0;
        for (int col = 0; col < width; ++col) {
            sum += static_cast<double>(frame.at(start_y + row, start_x + col));
        }
        means[static_cast<std::size_t>(row)] = sum / static_cast<double>(width);
    }

    return means;
}

std::vector<double> compute_column_means(
    const ModernRawFrame& frame,
    int start_x,
    int width,
    int start_y,
    int height
) {
    std::vector<double> means(static_cast<std::size_t>(width), 0.0);
    if (width <= 0 || height <= 0) {
        return means;
    }

    for (int col = 0; col < width; ++col) {
        double sum = 0.0;
        for (int row = 0; row < height; ++row) {
            sum += static_cast<double>(frame.at(start_y + row, start_x + col));
        }
        means[static_cast<std::size_t>(col)] = sum / static_cast<double>(height);
    }

    return means;
}

std::vector<std::size_t> compute_column_nonzero_counts(
    const ModernRawFrame& frame,
    int start_x,
    int width,
    int start_y,
    int height
) {
    std::vector<std::size_t> counts(static_cast<std::size_t>(width), 0);
    if (width <= 0 || height <= 0) {
        return counts;
    }

    for (int col = 0; col < width; ++col) {
        std::size_t count = 0;
        for (int row = 0; row < height; ++row) {
            if (frame.at(start_y + row, start_x + col) != 0) {
                count += 1;
            }
        }
        counts[static_cast<std::size_t>(col)] = count;
    }

    return counts;
}

std::vector<std::size_t> compute_row_nonzero_counts(
    const ModernRawFrame& frame,
    int start_x,
    int width,
    int start_y,
    int height
) {
    std::vector<std::size_t> counts(static_cast<std::size_t>(height), 0);
    if (width <= 0 || height <= 0) {
        return counts;
    }

    for (int row = 0; row < height; ++row) {
        std::size_t count = 0;
        for (int col = 0; col < width; ++col) {
            if (frame.at(start_y + row, start_x + col) != 0) {
                count += 1;
            }
        }
        counts[static_cast<std::size_t>(row)] = count;
    }

    return counts;
}

std::vector<double> compute_active_row_means(const ModernCalibratedRawFrame& frame) {
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

std::vector<double> compute_active_column_means(const ModernCalibratedRawFrame& frame) {
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

double compute_correlation(std::span<const std::uint16_t> left, std::span<const std::uint16_t> right) {
    if (left.size() != right.size() || left.empty()) {
        return 0.0;
    }

    const auto accumulate_mean = [](std::span<const std::uint16_t> values) {
        double sum = 0.0;
        for (const std::uint16_t value : values) {
            sum += static_cast<double>(value);
        }
        return sum / static_cast<double>(values.size());
    };

    const double left_mean = accumulate_mean(left);
    const double right_mean = accumulate_mean(right);

    double numerator = 0.0;
    double left_energy = 0.0;
    double right_energy = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const double left_centered = static_cast<double>(left[index]) - left_mean;
        const double right_centered = static_cast<double>(right[index]) - right_mean;
        numerator += left_centered * right_centered;
        left_energy += left_centered * left_centered;
        right_energy += right_centered * right_centered;
    }

    if (left_energy <= 0.0 || right_energy <= 0.0) {
        return 0.0;
    }
    return numerator / std::sqrt(left_energy * right_energy);
}

double compute_median(std::vector<std::uint16_t> values) {
    if (values.empty()) {
        return 0.0;
    }
    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle), values.end());
    if ((values.size() & 1U) != 0U) {
        return static_cast<double>(values[middle]);
    }
    const std::uint16_t upper = values[middle];
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle - 1), values.end());
    const std::uint16_t lower = values[middle - 1];
    return (static_cast<double>(lower) + static_cast<double>(upper)) * 0.5;
}

void center_series(std::vector<double>& values) {
    if (values.empty()) {
        return;
    }
    double sum = 0.0;
    for (const double value : values) {
        sum += value;
    }
    const double mean = sum / static_cast<double>(values.size());
    for (double& value : values) {
        value -= mean;
    }
}

int count_trailing_near_zero(std::span<const double> values, double epsilon) {
    int count = 0;
    for (std::size_t index = values.size(); index > 0; --index) {
        if (std::fabs(values[index - 1]) > epsilon) {
            break;
        }
        count += 1;
    }
    return count;
}

int count_trailing_hard_zero(std::span<const std::size_t> values) {
    int count = 0;
    for (std::size_t index = values.size(); index > 0; --index) {
        if (values[index - 1] != 0) {
            break;
        }
        count += 1;
    }
    return count;
}

std::vector<double> compute_right_margin_column_correlations(const ModernRawFrame& frame) {
    const int margin_width = frame.full_width - (frame.active_x + frame.active_width);
    std::vector<double> correlations(static_cast<std::size_t>(margin_width), 0.0);
    if (margin_width <= 0 || frame.active_height <= 1 || frame.active_width < 2) {
        return correlations;
    }

    std::vector<std::uint16_t> margin_samples(static_cast<std::size_t>(frame.active_height));
    std::vector<std::uint16_t> active_samples(static_cast<std::size_t>(frame.active_height));

    for (int offset = 0; offset < margin_width; ++offset) {
        const int absolute_col = frame.active_x + frame.active_width + offset;
        const int reference_col = frame.active_x + frame.active_width - 2 + (absolute_col & 1);
        for (int row = 0; row < frame.active_height; ++row) {
            const int absolute_row = frame.active_y + row;
            margin_samples[static_cast<std::size_t>(row)] = frame.at(absolute_row, absolute_col);
            active_samples[static_cast<std::size_t>(row)] = frame.at(absolute_row, reference_col);
        }
        correlations[static_cast<std::size_t>(offset)] = compute_correlation(margin_samples, active_samples);
    }

    return correlations;
}

std::vector<double> compute_bottom_margin_row_correlations(const ModernRawFrame& frame) {
    const int margin_height = frame.full_height - (frame.active_y + frame.active_height);
    std::vector<double> correlations(static_cast<std::size_t>(margin_height), 0.0);
    if (margin_height <= 0 || frame.active_width <= 1 || frame.active_height < 2) {
        return correlations;
    }

    std::vector<std::uint16_t> margin_samples(static_cast<std::size_t>(frame.active_width));
    std::vector<std::uint16_t> active_samples(static_cast<std::size_t>(frame.active_width));

    for (int offset = 0; offset < margin_height; ++offset) {
        const int absolute_row = frame.active_y + frame.active_height + offset;
        const int reference_row = frame.active_y + frame.active_height - 2 + (absolute_row & 1);
        for (int col = 0; col < frame.active_width; ++col) {
            const int absolute_col = frame.active_x + col;
            margin_samples[static_cast<std::size_t>(col)] = frame.at(absolute_row, absolute_col);
            active_samples[static_cast<std::size_t>(col)] = frame.at(reference_row, absolute_col);
        }
        correlations[static_cast<std::size_t>(offset)] = compute_correlation(margin_samples, active_samples);
    }

    return correlations;
}

int detect_leading_structured_lines(
    std::span<const double> means,
    std::span<const std::size_t> nonzero_counts,
    std::span<const double> correlations,
    double full_nonzero_threshold
) {
    int count = 0;
    for (std::size_t index = 0; index < means.size(); ++index) {
        if (index >= nonzero_counts.size() || index >= correlations.size()) {
            break;
        }
        const double nonzero_fraction = full_nonzero_threshold <= 0.0
                                            ? 0.0
                                            : (static_cast<double>(nonzero_counts[index]) / full_nonzero_threshold);
        if (nonzero_fraction < 0.75 || correlations[index] < 0.7) {
            break;
        }
        count += 1;
    }
    return count;
}

int count_dark_reference_lines(
    std::span<const double> means,
    std::span<const std::size_t> nonzero_counts,
    int start_index,
    int end_index,
    double line_length
) {
    (void)nonzero_counts;
    (void)line_length;
    int count = 0;
    for (int index = start_index; index < end_index; ++index) {
        const double mean = means[static_cast<std::size_t>(index)];
        if (mean <= 4.0) {
            count += 1;
        }
    }
    return count;
}

std::vector<int> collect_supported_dark_reference_lines(
    std::span<const double> means,
    std::span<const std::size_t> nonzero_counts,
    int start_index,
    int line_count,
    double line_length
) {
    std::vector<int> indices;
    for (int index = start_index; index < start_index + line_count; ++index) {
        const double mean = means[static_cast<std::size_t>(index)];
        const double nonzero_fraction =
            line_length <= 0.0 ? 0.0 : (static_cast<double>(nonzero_counts[static_cast<std::size_t>(index)]) / line_length);
        if (mean <= 4.0 && nonzero_fraction >= 0.25) {
            indices.push_back(index);
        }
    }
    return indices;
}

}  // namespace

std::uint16_t ModernRawFrame::at(int row, int col) const {
    if (row < 0 || row >= full_height || col < 0 || col >= full_width) {
        throw std::runtime_error("modern raw frame sample is out of bounds");
    }
    const std::size_t index =
        static_cast<std::size_t>(row) * static_cast<std::size_t>(full_width) +
        static_cast<std::size_t>(col);
    return mosaic[index];
}

ModernRawFrame build_modern_raw_frame(const DatFile& dat) {
    if (dat.raw_payload().size() != kRawBlockSize) {
        throw std::runtime_error("modern raw frame expects a full DAT raw payload");
    }

    ModernRawFrame frame;
    frame.mosaic.reserve(dat.raw_payload().size());
    for (const std::uint8_t value : dat.raw_payload()) {
        frame.mosaic.push_back(static_cast<std::uint16_t>(value));
    }

    const std::size_t expected_count =
        static_cast<std::size_t>(frame.full_width) * static_cast<std::size_t>(frame.full_height);
    if (frame.mosaic.size() != expected_count) {
        throw std::runtime_error("modern raw frame geometry does not match the DAT payload size");
    }

    return frame;
}

ModernRawFrameStats analyze_modern_raw_frame(const ModernRawFrame& frame) {
    if (frame.active_x < 0 || frame.active_y < 0 || frame.active_width < 0 || frame.active_height < 0) {
        throw std::runtime_error("modern raw frame active area must be non-negative");
    }
    if (frame.active_x + frame.active_width > frame.full_width ||
        frame.active_y + frame.active_height > frame.full_height) {
        throw std::runtime_error("modern raw frame active area must fit inside the full frame");
    }

    ModernRawFrameStats stats;
    stats.active = accumulate_region(frame, frame.active_x, frame.active_y, frame.active_width, frame.active_height);
    stats.right_margin = accumulate_region(
        frame,
        frame.active_x + frame.active_width,
        frame.active_y,
        frame.full_width - (frame.active_x + frame.active_width),
        frame.active_height
    );
    stats.bottom_margin = accumulate_region(
        frame,
        0,
        frame.active_y + frame.active_height,
        frame.full_width,
        frame.full_height - (frame.active_y + frame.active_height)
    );

    const double inactive_sum =
        (stats.right_margin.mean * static_cast<double>(stats.right_margin.count)) +
        (stats.bottom_margin.mean * static_cast<double>(stats.bottom_margin.count));
    stats.inactive.count = stats.right_margin.count + stats.bottom_margin.count;
    if (stats.inactive.count != 0) {
        stats.inactive.minimum = stats.right_margin.count == 0
                                     ? stats.bottom_margin.minimum
                                     : (stats.bottom_margin.count == 0
                                            ? stats.right_margin.minimum
                                            : std::min(stats.right_margin.minimum, stats.bottom_margin.minimum));
        stats.inactive.maximum = std::max(stats.right_margin.maximum, stats.bottom_margin.maximum);
        stats.inactive.mean = inactive_sum / static_cast<double>(stats.inactive.count);
    }

    stats.parity_sites[0] = accumulate_parity_site(frame, 0, 0);
    stats.parity_sites[1] = accumulate_parity_site(frame, 0, 1);
    stats.parity_sites[2] = accumulate_parity_site(frame, 1, 0);
    stats.parity_sites[3] = accumulate_parity_site(frame, 1, 1);

    stats.full_row_means = compute_row_means(frame, 0, frame.full_width, 0, frame.full_height);
    stats.active_row_means = compute_row_means(
        frame,
        frame.active_x,
        frame.active_width,
        frame.active_y,
        frame.active_height
    );
    stats.full_column_means = compute_column_means(frame, 0, frame.full_width, 0, frame.full_height);
    stats.active_column_means = compute_column_means(
        frame,
        frame.active_x,
        frame.active_width,
        frame.active_y,
        frame.active_height
    );
    stats.right_margin_column_means = compute_column_means(
        frame,
        frame.active_x + frame.active_width,
        frame.full_width - (frame.active_x + frame.active_width),
        frame.active_y,
        frame.active_height
    );
    stats.right_margin_column_nonzero_counts = compute_column_nonzero_counts(
        frame,
        frame.active_x + frame.active_width,
        frame.full_width - (frame.active_x + frame.active_width),
        frame.active_y,
        frame.active_height
    );
    stats.right_margin_column_correlations = compute_right_margin_column_correlations(frame);
    stats.bottom_margin_row_means = compute_row_means(
        frame,
        0,
        frame.full_width,
        frame.active_y + frame.active_height,
        frame.full_height - (frame.active_y + frame.active_height)
    );
    stats.bottom_margin_row_nonzero_counts = compute_row_nonzero_counts(
        frame,
        0,
        frame.full_width,
        frame.active_y + frame.active_height,
        frame.full_height - (frame.active_y + frame.active_height)
    );
    stats.bottom_margin_row_correlations = compute_bottom_margin_row_correlations(frame);
    stats.leading_structured_right_margin_columns = detect_leading_structured_lines(
        stats.right_margin_column_means,
        stats.right_margin_column_nonzero_counts,
        stats.right_margin_column_correlations,
        static_cast<double>(frame.active_height)
    );
    stats.leading_structured_bottom_margin_rows = detect_leading_structured_lines(
        stats.bottom_margin_row_means,
        stats.bottom_margin_row_nonzero_counts,
        stats.bottom_margin_row_correlations,
        static_cast<double>(frame.full_width)
    );
    stats.trailing_hard_zero_right_margin_columns = count_trailing_hard_zero(stats.right_margin_column_nonzero_counts);
    stats.trailing_hard_zero_bottom_margin_rows = count_trailing_hard_zero(stats.bottom_margin_row_nonzero_counts);
    const int right_dark_end =
        static_cast<int>(stats.right_margin_column_means.size()) - stats.trailing_hard_zero_right_margin_columns;
    const int bottom_dark_end =
        static_cast<int>(stats.bottom_margin_row_means.size()) - stats.trailing_hard_zero_bottom_margin_rows;
    stats.dark_reference_right_margin_columns = count_dark_reference_lines(
        stats.right_margin_column_means,
        stats.right_margin_column_nonzero_counts,
        stats.leading_structured_right_margin_columns,
        std::max(stats.leading_structured_right_margin_columns, right_dark_end),
        static_cast<double>(frame.active_height)
    );
    stats.dark_reference_bottom_margin_rows = count_dark_reference_lines(
        stats.bottom_margin_row_means,
        stats.bottom_margin_row_nonzero_counts,
        stats.leading_structured_bottom_margin_rows,
        std::max(stats.leading_structured_bottom_margin_rows, bottom_dark_end),
        static_cast<double>(frame.full_width)
    );
    stats.trailing_zero_right_margin_columns = count_trailing_near_zero(stats.right_margin_column_means, 0.125);
    stats.trailing_zero_bottom_margin_rows = count_trailing_near_zero(stats.bottom_margin_row_means, 0.125);

    return stats;
}

ModernRawCalibration estimate_modern_raw_calibration(const ModernRawFrame& frame) {
    const ModernRawFrameStats stats = analyze_modern_raw_frame(frame);
    ModernRawCalibration calibration;
    calibration.active_row_offsets.assign(static_cast<std::size_t>(frame.active_height), 0.0);
    calibration.full_column_offsets.assign(static_cast<std::size_t>(frame.full_width), 0.0);

    calibration.dark_reference_right_margin_columns = stats.dark_reference_right_margin_columns;
    calibration.dark_reference_bottom_margin_rows = stats.dark_reference_bottom_margin_rows;

    std::array<std::vector<std::uint16_t>, 4> black_reference_samples;

    const int right_reference_start = stats.leading_structured_right_margin_columns;
    const int right_reference_end =
        right_reference_start + stats.dark_reference_right_margin_columns;
    for (int row = frame.active_y; row < frame.active_y + frame.active_height; ++row) {
        for (int offset = right_reference_start; offset < right_reference_end; ++offset) {
            const int col = frame.active_x + frame.active_width + offset;
            const std::size_t parity = static_cast<std::size_t>(parity_index(row, col));
            black_reference_samples[parity].push_back(frame.at(row, col));
            calibration.black_reference_sample_count += 1;
        }
    }

    const int bottom_reference_start = stats.leading_structured_bottom_margin_rows;
    const int bottom_reference_end =
        bottom_reference_start + stats.dark_reference_bottom_margin_rows;
    const std::vector<int> right_offset_reference_offsets = collect_supported_dark_reference_lines(
        stats.right_margin_column_means,
        stats.right_margin_column_nonzero_counts,
        right_reference_start,
        stats.dark_reference_right_margin_columns,
        static_cast<double>(frame.active_height)
    );
    const std::vector<int> bottom_offset_reference_offsets = collect_supported_dark_reference_lines(
        stats.bottom_margin_row_means,
        stats.bottom_margin_row_nonzero_counts,
        bottom_reference_start,
        stats.dark_reference_bottom_margin_rows,
        static_cast<double>(frame.full_width)
    );
    calibration.row_offset_reference_right_margin_columns =
        static_cast<int>(right_offset_reference_offsets.size());
    calibration.column_offset_reference_bottom_margin_rows =
        static_cast<int>(bottom_offset_reference_offsets.size());
    for (int offset = bottom_reference_start; offset < bottom_reference_end; ++offset) {
        const int row = frame.active_y + frame.active_height + offset;
        for (int col = 0; col < frame.full_width; ++col) {
            const std::size_t parity = static_cast<std::size_t>(parity_index(row, col));
            black_reference_samples[parity].push_back(frame.at(row, col));
            calibration.black_reference_sample_count += 1;
        }
    }

    for (std::size_t index = 0; index < calibration.black_levels.size(); ++index) {
        calibration.black_levels[index] = compute_median(std::move(black_reference_samples[index]));
    }

    if (!right_offset_reference_offsets.empty()) {
        for (int row = 0; row < frame.active_height; ++row) {
            double residual_sum = 0.0;
            std::size_t count = 0;
            const int absolute_row = frame.active_y + row;
            for (const int offset : right_offset_reference_offsets) {
                const int col = frame.active_x + frame.active_width + offset;
                const std::size_t parity = static_cast<std::size_t>(parity_index(absolute_row, col));
                residual_sum += static_cast<double>(frame.at(absolute_row, col)) - calibration.black_levels[parity];
                count += 1;
            }
            calibration.active_row_offsets[static_cast<std::size_t>(row)] =
                count == 0 ? 0.0 : (residual_sum / static_cast<double>(count));
        }
        center_series(calibration.active_row_offsets);
    }

    if (!bottom_offset_reference_offsets.empty()) {
        for (int col = 0; col < frame.full_width; ++col) {
            double residual_sum = 0.0;
            std::size_t count = 0;
            for (const int offset : bottom_offset_reference_offsets) {
                const int row = frame.active_y + frame.active_height + offset;
                const std::size_t parity = static_cast<std::size_t>(parity_index(row, col));
                residual_sum += static_cast<double>(frame.at(row, col)) - calibration.black_levels[parity];
                count += 1;
            }
            calibration.full_column_offsets[static_cast<std::size_t>(col)] =
                count == 0 ? 0.0 : (residual_sum / static_cast<double>(count));
        }
        center_series(calibration.full_column_offsets);
    }

    return calibration;
}

float ModernCalibratedRawFrame::at(int row, int col) const {
    if (row < 0 || row >= full_height || col < 0 || col >= full_width) {
        throw std::runtime_error("modern calibrated raw frame sample is out of bounds");
    }
    const std::size_t index =
        static_cast<std::size_t>(row) * static_cast<std::size_t>(full_width) +
        static_cast<std::size_t>(col);
    return mosaic[index];
}

ModernCalibratedRawFrame build_calibrated_modern_raw_frame(
    const ModernRawFrame& frame,
    const ModernRawCalibration& calibration
) {
    if (calibration.active_row_offsets.size() != static_cast<std::size_t>(frame.active_height)) {
        throw std::runtime_error("modern calibration row offsets do not match active height");
    }
    if (calibration.full_column_offsets.size() != static_cast<std::size_t>(frame.full_width)) {
        throw std::runtime_error("modern calibration column offsets do not match full width");
    }

    ModernCalibratedRawFrame corrected;
    corrected.full_width = frame.full_width;
    corrected.full_height = frame.full_height;
    corrected.active_x = frame.active_x;
    corrected.active_y = frame.active_y;
    corrected.active_width = frame.active_width;
    corrected.active_height = frame.active_height;
    corrected.cfa_repeat_x = frame.cfa_repeat_x;
    corrected.cfa_repeat_y = frame.cfa_repeat_y;
    corrected.cfa_pattern = frame.cfa_pattern;
    corrected.complementary_pattern = frame.complementary_pattern;
    corrected.complementary_pattern_locked = frame.complementary_pattern_locked;
    corrected.black_levels = calibration.black_levels;
    corrected.mosaic.resize(frame.mosaic.size(), 0.0f);

    for (int row = 0; row < frame.full_height; ++row) {
        const double row_offset =
            row < frame.active_y + frame.active_height
                ? calibration.active_row_offsets[static_cast<std::size_t>(row - frame.active_y)]
                : 0.0;
        for (int col = 0; col < frame.full_width; ++col) {
            const std::size_t parity = static_cast<std::size_t>(parity_index(row, col));
            double value = static_cast<double>(frame.at(row, col));
            value -= calibration.black_levels[parity];
            value -= calibration.full_column_offsets[static_cast<std::size_t>(col)];
            value -= row_offset;
            const std::size_t index =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(frame.full_width) +
                static_cast<std::size_t>(col);
            corrected.mosaic[index] = static_cast<float>(std::max(0.0, value));
        }
    }

    return corrected;
}

ModernCalibratedRawFrameStats analyze_modern_calibrated_raw_frame(const ModernCalibratedRawFrame& frame) {
    ModernCalibratedRawFrameStats stats;
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
    return stats;
}

}  // namespace dj1000
