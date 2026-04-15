#include "dj1000/modern_dng.hpp"

#include "modern_export_pipeline.hpp"
#include "dj1000/modern_sensor_frame.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dj1000 {

namespace {

constexpr std::uint16_t kTiffTypeByte = 1;
constexpr std::uint16_t kTiffTypeAscii = 2;
constexpr std::uint16_t kTiffTypeShort = 3;
constexpr std::uint16_t kTiffTypeLong = 4;
constexpr std::uint16_t kTiffTypeRational = 5;
constexpr std::uint16_t kTiffTypeSRational = 10;

constexpr std::uint16_t kTagNewSubfileType = 254;
constexpr std::uint16_t kTagImageWidth = 256;
constexpr std::uint16_t kTagImageLength = 257;
constexpr std::uint16_t kTagBitsPerSample = 258;
constexpr std::uint16_t kTagCompression = 259;
constexpr std::uint16_t kTagPhotometricInterpretation = 262;
constexpr std::uint16_t kTagMake = 271;
constexpr std::uint16_t kTagModel = 272;
constexpr std::uint16_t kTagStripOffsets = 273;
constexpr std::uint16_t kTagOrientation = 274;
constexpr std::uint16_t kTagSamplesPerPixel = 277;
constexpr std::uint16_t kTagRowsPerStrip = 278;
constexpr std::uint16_t kTagStripByteCounts = 279;
constexpr std::uint16_t kTagPlanarConfiguration = 284;
constexpr std::uint16_t kTagSoftware = 305;
constexpr std::uint16_t kTagCFARepeatPatternDim = 33421;
constexpr std::uint16_t kTagCFAPattern = 33422;
constexpr std::uint16_t kTagDNGVersion = 50706;
constexpr std::uint16_t kTagDNGBackwardVersion = 50707;
constexpr std::uint16_t kTagUniqueCameraModel = 50708;
constexpr std::uint16_t kTagCFAPlaneColor = 50710;
constexpr std::uint16_t kTagCFALayout = 50711;
constexpr std::uint16_t kTagBlackLevelRepeatDim = 50713;
constexpr std::uint16_t kTagBlackLevel = 50714;
constexpr std::uint16_t kTagWhiteLevel = 50717;
constexpr std::uint16_t kTagDefaultScale = 50718;
constexpr std::uint16_t kTagDefaultCropOrigin = 50719;
constexpr std::uint16_t kTagDefaultCropSize = 50720;
constexpr std::uint16_t kTagColorMatrix1 = 50721;
constexpr std::uint16_t kTagReductionMatrix1 = 50725;
constexpr std::uint16_t kTagAnalogBalance = 50727;
constexpr std::uint16_t kTagAsShotNeutral = 50728;
constexpr std::uint16_t kTagCalibrationIlluminant1 = 50778;
constexpr std::uint16_t kTagActiveArea = 50829;
constexpr std::uint16_t kCompressionNone = 1;
constexpr std::uint16_t kPhotometricRgb = 2;
constexpr std::uint16_t kPhotometricCfa = 32803;
constexpr std::uint16_t kOrientationTopLeft = 1;
constexpr std::uint16_t kPlanarConfigurationChunky = 1;
constexpr std::uint16_t kRectangularCfaLayout = 1;
constexpr std::uint16_t kCalibrationIlluminantD65 = 21;

struct IfdEntry {
    std::uint16_t tag = 0;
    std::uint16_t type = 0;
    std::uint32_t count = 0;
    std::vector<std::uint8_t> data;
};

void append_u16_le(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32_le(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void append_i32_le(std::vector<std::uint8_t>& bytes, std::int32_t value) {
    append_u32_le(bytes, static_cast<std::uint32_t>(value));
}

void set_u32_le(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset + 0] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

void align_even(std::vector<std::uint8_t>& bytes) {
    if ((bytes.size() & 1U) != 0U) {
        bytes.push_back(0);
    }
}

std::vector<std::uint8_t> make_ascii_data(const std::string& value) {
    std::vector<std::uint8_t> data(value.begin(), value.end());
    data.push_back(0);
    return data;
}

IfdEntry make_ascii_entry(std::uint16_t tag, const std::string& text) {
    auto data = make_ascii_data(text);
    return IfdEntry{
        .tag = tag,
        .type = kTiffTypeAscii,
        .count = static_cast<std::uint32_t>(data.size()),
        .data = std::move(data),
    };
}

std::vector<std::uint8_t> make_byte_data(std::initializer_list<std::uint8_t> values) {
    return std::vector<std::uint8_t>(values);
}

std::vector<std::uint8_t> make_short_data(std::initializer_list<std::uint16_t> values) {
    std::vector<std::uint8_t> data;
    for (const std::uint16_t value : values) {
        append_u16_le(data, value);
    }
    return data;
}

std::vector<std::uint8_t> make_long_data(std::initializer_list<std::uint32_t> values) {
    std::vector<std::uint8_t> data;
    for (const std::uint32_t value : values) {
        append_u32_le(data, value);
    }
    return data;
}

std::vector<std::uint8_t> make_rational_data(std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> values) {
    std::vector<std::uint8_t> data;
    for (const auto& [numerator, denominator] : values) {
        append_u32_le(data, numerator);
        append_u32_le(data, denominator == 0 ? 1U : denominator);
    }
    return data;
}

std::vector<std::uint8_t> make_srational_data(std::initializer_list<std::pair<std::int32_t, std::int32_t>> values) {
    std::vector<std::uint8_t> data;
    for (const auto& [numerator, denominator] : values) {
        append_i32_le(data, numerator);
        append_i32_le(data, denominator == 0 ? 1 : denominator);
    }
    return data;
}

std::uint8_t cfa_plane_index(CfaColor color) {
    switch (color) {
        case CfaColor::Red:
            return 0;
        case CfaColor::Green:
            return 1;
        case CfaColor::Blue:
            return 2;
        case CfaColor::Unknown:
            return 1;
    }
    return 1;
}

std::uint8_t complementary_plane_index(ComplementarySite site) {
    switch (site) {
        case ComplementarySite::A:
            return 0;
        case ComplementarySite::C:
            return 1;
        case ComplementarySite::D:
            return 2;
        case ComplementarySite::E:
            return 3;
        case ComplementarySite::Unknown:
            return 2;
    }
    return 2;
}

std::vector<std::uint8_t> quantize_calibrated_frame_to_u16(
    std::span<const float> mosaic,
    int full_width,
    int full_height,
    double white_level
) {
    const double safe_white_level = std::max(white_level, 1.0);
    std::vector<std::uint8_t> bytes;
    bytes.reserve(static_cast<std::size_t>(full_width) * static_cast<std::size_t>(full_height) * 2);
    for (const float sample : mosaic) {
        const std::uint16_t value = static_cast<std::uint16_t>(
            std::clamp(
                std::lround(
                    std::clamp(static_cast<double>(sample) / safe_white_level, 0.0, 1.0) * 65535.0
                ),
                0L,
                65535L
            )
        );
        append_u16_le(bytes, value);
    }
    return bytes;
}

double compute_float_percentile(std::vector<float> values, double fraction) {
    if (values.empty()) {
        return 0.0;
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

struct ParityRatioEstimate {
    double ratio = 1.0;
    std::size_t sample_count = 0;
};

ParityRatioEstimate estimate_local_same_color_ratio(
    const ModernCalibratedRawFrame& frame,
    std::size_t target_parity,
    std::size_t reference_parity
) {
    std::vector<float> ratios;
    ratios.reserve(static_cast<std::size_t>(frame.active_width) * static_cast<std::size_t>(frame.active_height) / 8U);

    const int start_row = frame.active_y + 1;
    const int end_row = frame.active_y + frame.active_height - 1;
    const int start_col = frame.active_x + 1;
    const int end_col = frame.active_x + frame.active_width - 1;
    for (int row = start_row; row < end_row; ++row) {
        for (int col = start_col; col < end_col; ++col) {
            const std::size_t parity = static_cast<std::size_t>(((row & 1) << 1) | (col & 1));
            if (parity != target_parity) {
                continue;
            }

            const float actual = frame.at(row, col);
            if (!(actual > 1.0f)) {
                continue;
            }

            const std::array<float, 4> neighbors = {
                frame.at(row - 1, col - 1),
                frame.at(row - 1, col + 1),
                frame.at(row + 1, col - 1),
                frame.at(row + 1, col + 1),
            };
            const std::array<std::size_t, 4> neighbor_parities = {
                static_cast<std::size_t>((((row - 1) & 1) << 1) | ((col - 1) & 1)),
                static_cast<std::size_t>((((row - 1) & 1) << 1) | ((col + 1) & 1)),
                static_cast<std::size_t>((((row + 1) & 1) << 1) | ((col - 1) & 1)),
                static_cast<std::size_t>((((row + 1) & 1) << 1) | ((col + 1) & 1)),
            };
            if (std::any_of(neighbor_parities.begin(), neighbor_parities.end(), [&](std::size_t parity_value) {
                    return parity_value != reference_parity;
                })) {
                continue;
            }

            const float neighbor_min = *std::min_element(neighbors.begin(), neighbors.end());
            const float neighbor_max = *std::max_element(neighbors.begin(), neighbors.end());
            const float spread = neighbor_max - neighbor_min;
            const float predicted = 0.25f * (neighbors[0] + neighbors[1] + neighbors[2] + neighbors[3]);
            if (!(predicted > 1.0f)) {
                continue;
            }

            const float smooth_threshold = std::max(12.0f, predicted * 0.15f);
            if (spread > smooth_threshold) {
                continue;
            }

            const float ratio = predicted / actual;
            if (ratio < 0.5f || ratio > 2.0f) {
                continue;
            }
            ratios.push_back(ratio);
        }
    }

    if (ratios.empty()) {
        return {};
    }
    const std::size_t sample_count = ratios.size();

    return {
        .ratio = compute_float_percentile(std::move(ratios), 0.5),
        .sample_count = sample_count,
    };
}

std::array<double, 4> compute_same_color_parity_gains(
    const ModernCalibratedRawFrame& frame,
    const std::array<CfaColor, 4>& pattern
) {
    std::array<std::vector<float>, 4> parity_samples;
    for (int row = frame.active_y; row < frame.active_y + frame.active_height; ++row) {
        for (int col = frame.active_x; col < frame.active_x + frame.active_width; ++col) {
            const float value = frame.at(row, col);
            if (!(value > 0.0f)) {
                continue;
            }
            const std::size_t parity = static_cast<std::size_t>(((row & 1) << 1) | (col & 1));
            parity_samples[parity].push_back(value);
        }
    }

    std::array<double, 4> robust_levels = {1.0, 1.0, 1.0, 1.0};
    for (std::size_t index = 0; index < parity_samples.size(); ++index) {
        if (!parity_samples[index].empty()) {
            robust_levels[index] = std::max(compute_float_percentile(parity_samples[index], 0.5), 1.0);
        }
    }

    std::array<double, 4> gains = {1.0, 1.0, 1.0, 1.0};
    for (CfaColor color : {CfaColor::Red, CfaColor::Green, CfaColor::Blue}) {
        std::vector<std::size_t> matching;
        for (std::size_t index = 0; index < pattern.size(); ++index) {
            if (pattern[index] == color) {
                matching.push_back(index);
            }
        }
        if (matching.size() <= 1) {
            continue;
        }
        if (matching.size() == 2) {
            const auto forward = estimate_local_same_color_ratio(frame, matching[0], matching[1]);
            const auto backward = estimate_local_same_color_ratio(frame, matching[1], matching[0]);
            if (forward.sample_count >= 256 && backward.sample_count >= 256) {
                const double relative_gain = std::sqrt(
                    std::max(forward.ratio, 1.0e-6) / std::max(backward.ratio, 1.0e-6)
                );
                gains[matching[0]] = std::clamp(relative_gain, 0.85, 1.2);
                gains[matching[1]] = std::clamp(1.0 / std::max(relative_gain, 1.0e-6), 0.85, 1.2);
                continue;
            }
        }

        double target = 0.0;
        for (const std::size_t index : matching) {
            target += robust_levels[index];
        }
        target /= static_cast<double>(matching.size());
        for (const std::size_t index : matching) {
            gains[index] = std::clamp(target / std::max(robust_levels[index], 1.0), 0.85, 1.15);
        }
    }

    return gains;
}

std::vector<float> apply_parity_gains(
    const ModernCalibratedRawFrame& frame,
    const std::array<double, 4>& gains
) {
    std::vector<float> balanced = frame.mosaic;
    for (int row = 0; row < frame.full_height; ++row) {
        for (int col = 0; col < frame.full_width; ++col) {
            const std::size_t parity = static_cast<std::size_t>(((row & 1) << 1) | (col & 1));
            const std::size_t index =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(frame.full_width) +
                static_cast<std::size_t>(col);
            balanced[index] = static_cast<float>(std::max(
                0.0,
                static_cast<double>(balanced[index]) * gains[parity]
            ));
        }
    }
    return balanced;
}

double compute_balanced_white_level(
    const ModernSensorFrame& frame,
    const std::array<double, 4>& parity_gains
) {
    return std::max({
        frame.white_levels[0] * parity_gains[0],
        frame.white_levels[1] * parity_gains[1],
        frame.white_levels[2] * parity_gains[2],
        frame.white_levels[3] * parity_gains[3],
    });
}

std::array<double, 3> estimate_as_shot_neutral(
    std::span<const float> mosaic,
    int full_width,
    int active_x,
    int active_y,
    int active_width,
    int active_height,
    const std::array<CfaColor, 4>& pattern
) {
    std::array<double, 3> sums = {0.0, 0.0, 0.0};
    std::array<std::size_t, 3> counts = {0, 0, 0};

    for (int row = active_y; row < active_y + active_height; ++row) {
        for (int col = active_x; col < active_x + active_width; ++col) {
            const std::size_t parity = static_cast<std::size_t>(((row & 1) << 1) | (col & 1));
            const CfaColor color = pattern[parity];
            if (color == CfaColor::Unknown) {
                continue;
            }
            const std::size_t channel = static_cast<std::size_t>(cfa_plane_index(color));
            const std::size_t index =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(full_width) +
                static_cast<std::size_t>(col);
            sums[channel] += mosaic[index];
            counts[channel] += 1;
        }
    }

    std::array<double, 3> means = {1.0, 1.0, 1.0};
    for (std::size_t index = 0; index < means.size(); ++index) {
        means[index] = counts[index] == 0 ? 1.0 : (sums[index] / static_cast<double>(counts[index]));
    }

    const double green = std::max(means[1], 1.0e-6);
    return {
        std::clamp(means[0] / green, 0.25, 4.0),
        1.0,
        std::clamp(means[2] / green, 0.25, 4.0),
    };
}

std::array<double, 4> lift_rgb_neutral_to_complementary(const std::array<double, 3>& rgb_neutral) {
    const double red = std::clamp(rgb_neutral[0], 0.0, 8.0);
    const double green = std::clamp(rgb_neutral[1], 0.0, 8.0);
    const double blue = std::clamp(rgb_neutral[2], 0.0, 8.0);
    return {
        std::clamp(green + blue, 0.25, 8.0),
        std::clamp(green, 0.25, 8.0),
        std::clamp(green, 0.25, 8.0),
        std::clamp(red + green, 0.25, 8.0),
    };
}

std::array<double, 4> estimate_complementary_as_shot_neutral(const ModernCalibratedRawFrame& frame) {
    double sum_red = 0.0;
    double sum_green = 0.0;
    double sum_blue = 0.0;
    std::size_t count = 0;

    const int block_height = frame.active_height / 2;
    const int block_width = frame.active_width / 2;
    for (int block_row = 0; block_row < block_height; ++block_row) {
        for (int block_col = 0; block_col < block_width; ++block_col) {
            const int row = frame.active_y + block_row * 2;
            const int col = frame.active_x + block_col * 2;
            const double a = static_cast<double>(frame.at(row, col));
            const double c = static_cast<double>(frame.at(row, col + 1));
            const double d = static_cast<double>(frame.at(row + 1, col));
            const double e = static_cast<double>(frame.at(row + 1, col + 1));
            const double red = std::max(0.0, e - d);
            const double green = std::max(1.0e-6, 0.5 * (c + d));
            const double blue = std::max(0.0, a - c);
            sum_red += red;
            sum_green += green;
            sum_blue += blue;
            ++count;
        }
    }

    if (count == 0) {
        return {1.0, 1.0, 1.0, 1.0};
    }

    const double mean_green = std::max(sum_green / static_cast<double>(count), 1.0e-6);
    const std::array<double, 3> rgb_neutral = {
        std::clamp((sum_red / static_cast<double>(count)) / mean_green, 0.25, 4.0),
        1.0,
        std::clamp((sum_blue / static_cast<double>(count)) / mean_green, 0.25, 4.0),
    };
    return lift_rgb_neutral_to_complementary(rgb_neutral);
}

std::vector<IfdEntry> build_dng_entries(
    const ModernSensorFrame& frame,
    const std::array<CfaColor, 4>& pattern,
    const std::array<double, 3>& as_shot_neutral
) {
    constexpr std::int32_t kMatrixDenominator = 1000000;

    std::vector<IfdEntry> entries;
    entries.push_back({kTagNewSubfileType, kTiffTypeLong, 1, make_long_data({0})});
    entries.push_back({kTagImageWidth, kTiffTypeLong, 1, make_long_data({static_cast<std::uint32_t>(frame.full_width)})});
    entries.push_back({kTagImageLength, kTiffTypeLong, 1, make_long_data({static_cast<std::uint32_t>(frame.full_height)})});
    entries.push_back({kTagBitsPerSample, kTiffTypeShort, 1, make_short_data({16})});
    entries.push_back({kTagCompression, kTiffTypeShort, 1, make_short_data({kCompressionNone})});
    entries.push_back({kTagPhotometricInterpretation, kTiffTypeShort, 1, make_short_data({kPhotometricCfa})});
    entries.push_back(make_ascii_entry(kTagMake, "Mitsubishi"));
    entries.push_back(make_ascii_entry(kTagModel, "DJ-1000"));
    entries.push_back({kTagStripOffsets, kTiffTypeLong, 1, make_long_data({0})});
    entries.push_back({kTagOrientation, kTiffTypeShort, 1, make_short_data({kOrientationTopLeft})});
    entries.push_back({kTagSamplesPerPixel, kTiffTypeShort, 1, make_short_data({1})});
    entries.push_back({kTagRowsPerStrip, kTiffTypeLong, 1, make_long_data({static_cast<std::uint32_t>(frame.full_height)})});
    entries.push_back({
        kTagStripByteCounts,
        kTiffTypeLong,
        1,
        make_long_data({static_cast<std::uint32_t>(frame.full_width * frame.full_height * 2)})
    });
    entries.push_back({
        kTagPlanarConfiguration,
        kTiffTypeShort,
        1,
        make_short_data({kPlanarConfigurationChunky})
    });
    entries.push_back(make_ascii_entry(kTagSoftware, "libdj1000 modern DNG"));
    entries.push_back({kTagCFARepeatPatternDim, kTiffTypeShort, 2, make_short_data({2, 2})});
    entries.push_back({
        kTagCFAPattern,
        kTiffTypeByte,
        4,
        make_byte_data({
            cfa_plane_index(pattern[0]),
            cfa_plane_index(pattern[1]),
            cfa_plane_index(pattern[2]),
            cfa_plane_index(pattern[3]),
        })
    });
    entries.push_back({kTagDNGVersion, kTiffTypeByte, 4, make_byte_data({1, 1, 0, 0})});
    entries.push_back({kTagDNGBackwardVersion, kTiffTypeByte, 4, make_byte_data({1, 1, 0, 0})});
    entries.push_back(make_ascii_entry(kTagUniqueCameraModel, "Mitsubishi DJ-1000 / UMAX PhotoRun"));
    entries.push_back({kTagCFAPlaneColor, kTiffTypeByte, 3, make_byte_data({0, 1, 2})});
    entries.push_back({kTagCFALayout, kTiffTypeShort, 1, make_short_data({kRectangularCfaLayout})});
    entries.push_back({kTagBlackLevelRepeatDim, kTiffTypeShort, 2, make_short_data({1, 1})});
    entries.push_back({kTagBlackLevel, kTiffTypeRational, 1, make_rational_data({{0, 1}})});
    entries.push_back({kTagWhiteLevel, kTiffTypeShort, 1, make_short_data({65535})});
    entries.push_back({
        kTagDefaultScale,
        kTiffTypeRational,
        2,
        make_rational_data({
            {1, 1},
            {
                static_cast<std::uint32_t>(frame.active_width * 3),
                static_cast<std::uint32_t>(frame.active_height * 4),
            },
        })
    });
    entries.push_back({
        kTagDefaultCropOrigin,
        kTiffTypeRational,
        2,
        make_rational_data({
            {static_cast<std::uint32_t>(frame.active_x), 1},
            {static_cast<std::uint32_t>(frame.active_y), 1},
        })
    });
    entries.push_back({
        kTagDefaultCropSize,
        kTiffTypeRational,
        2,
        make_rational_data({
            {static_cast<std::uint32_t>(frame.active_width), 1},
            {static_cast<std::uint32_t>(frame.active_height), 1},
        })
    });
    entries.push_back({
        kTagColorMatrix1,
        kTiffTypeSRational,
        9,
        make_srational_data({
            {3240454, kMatrixDenominator},
            {-1537138, kMatrixDenominator},
            {-498531, kMatrixDenominator},
            {-969266, kMatrixDenominator},
            {1876011, kMatrixDenominator},
            {41556, kMatrixDenominator},
            {55643, kMatrixDenominator},
            {-204026, kMatrixDenominator},
            {1057225, kMatrixDenominator},
        })
    });
    entries.push_back({
        kTagAsShotNeutral,
        kTiffTypeRational,
        3,
        make_rational_data({
            {static_cast<std::uint32_t>(std::lround(as_shot_neutral[0] * 1000000.0)), 1000000},
            {static_cast<std::uint32_t>(std::lround(as_shot_neutral[1] * 1000000.0)), 1000000},
            {static_cast<std::uint32_t>(std::lround(as_shot_neutral[2] * 1000000.0)), 1000000},
        })
    });
    entries.push_back({kTagCalibrationIlluminant1, kTiffTypeShort, 1, make_short_data({kCalibrationIlluminantD65})});
    entries.push_back({
        kTagActiveArea,
        kTiffTypeLong,
        4,
        make_long_data({
            static_cast<std::uint32_t>(frame.active_y),
            static_cast<std::uint32_t>(frame.active_x),
            static_cast<std::uint32_t>(frame.active_y + frame.active_height),
            static_cast<std::uint32_t>(frame.active_x + frame.active_width),
        })
    });
    return entries;
}

std::vector<IfdEntry> build_complementary_dng_entries(
    const ModernSensorFrame& frame,
    const std::array<ComplementarySite, 4>& pattern,
    const std::array<double, 4>& as_shot_neutral
) {
    constexpr std::int32_t kMatrixDenominator = 1000000;

    std::vector<IfdEntry> entries;
    entries.push_back({kTagNewSubfileType, kTiffTypeLong, 1, make_long_data({0})});
    entries.push_back({kTagImageWidth, kTiffTypeLong, 1, make_long_data({static_cast<std::uint32_t>(frame.full_width)})});
    entries.push_back({kTagImageLength, kTiffTypeLong, 1, make_long_data({static_cast<std::uint32_t>(frame.full_height)})});
    entries.push_back({kTagBitsPerSample, kTiffTypeShort, 1, make_short_data({16})});
    entries.push_back({kTagCompression, kTiffTypeShort, 1, make_short_data({kCompressionNone})});
    entries.push_back({kTagPhotometricInterpretation, kTiffTypeShort, 1, make_short_data({kPhotometricCfa})});
    entries.push_back(make_ascii_entry(kTagMake, "Mitsubishi"));
    entries.push_back(make_ascii_entry(kTagModel, "DJ-1000"));
    entries.push_back({kTagStripOffsets, kTiffTypeLong, 1, make_long_data({0})});
    entries.push_back({kTagOrientation, kTiffTypeShort, 1, make_short_data({kOrientationTopLeft})});
    entries.push_back({kTagSamplesPerPixel, kTiffTypeShort, 1, make_short_data({1})});
    entries.push_back({kTagRowsPerStrip, kTiffTypeLong, 1, make_long_data({static_cast<std::uint32_t>(frame.full_height)})});
    entries.push_back({
        kTagStripByteCounts,
        kTiffTypeLong,
        1,
        make_long_data({static_cast<std::uint32_t>(frame.full_width * frame.full_height * 2)})
    });
    entries.push_back({
        kTagPlanarConfiguration,
        kTiffTypeShort,
        1,
        make_short_data({kPlanarConfigurationChunky})
    });
    entries.push_back(make_ascii_entry(kTagSoftware, "libdj1000 modern complementary DNG"));
    entries.push_back({kTagCFARepeatPatternDim, kTiffTypeShort, 2, make_short_data({2, 2})});
    entries.push_back({
        kTagCFAPattern,
        kTiffTypeByte,
        4,
        make_byte_data({
            complementary_plane_index(pattern[0]),
            complementary_plane_index(pattern[1]),
            complementary_plane_index(pattern[2]),
            complementary_plane_index(pattern[3]),
        })
    });
    entries.push_back({kTagDNGVersion, kTiffTypeByte, 4, make_byte_data({1, 1, 0, 0})});
    entries.push_back({kTagDNGBackwardVersion, kTiffTypeByte, 4, make_byte_data({1, 1, 0, 0})});
    entries.push_back(make_ascii_entry(kTagUniqueCameraModel, "Mitsubishi DJ-1000 / UMAX PhotoRun Complementary RAW"));
    entries.push_back({kTagCFAPlaneColor, kTiffTypeByte, 4, make_byte_data({3, 5, 1, 6})});
    entries.push_back({kTagCFALayout, kTiffTypeShort, 1, make_short_data({kRectangularCfaLayout})});
    entries.push_back({kTagBlackLevelRepeatDim, kTiffTypeShort, 2, make_short_data({1, 1})});
    entries.push_back({kTagBlackLevel, kTiffTypeRational, 1, make_rational_data({{0, 1}})});
    entries.push_back({kTagWhiteLevel, kTiffTypeShort, 1, make_short_data({65535})});
    entries.push_back({
        kTagDefaultScale,
        kTiffTypeRational,
        2,
        make_rational_data({
            {1, 1},
            {
                static_cast<std::uint32_t>(frame.active_width * 3),
                static_cast<std::uint32_t>(frame.active_height * 4),
            },
        })
    });
    entries.push_back({
        kTagDefaultCropOrigin,
        kTiffTypeRational,
        2,
        make_rational_data({
            {static_cast<std::uint32_t>(frame.active_x), 1},
            {static_cast<std::uint32_t>(frame.active_y), 1},
        })
    });
    entries.push_back({
        kTagDefaultCropSize,
        kTiffTypeRational,
        2,
        make_rational_data({
            {static_cast<std::uint32_t>(frame.active_width), 1},
            {static_cast<std::uint32_t>(frame.active_height), 1},
        })
    });
    entries.push_back({
        kTagColorMatrix1,
        kTiffTypeSRational,
        12,
        make_srational_data({
            {-913623, kMatrixDenominator},
            {1671985, kMatrixDenominator},
            {1098781, kMatrixDenominator},
            {-969266, kMatrixDenominator},
            {1876011, kMatrixDenominator},
            {41556, kMatrixDenominator},
            {-969266, kMatrixDenominator},
            {1876011, kMatrixDenominator},
            {41556, kMatrixDenominator},
            {2271188, kMatrixDenominator},
            {338872, kMatrixDenominator},
            {-456975, kMatrixDenominator},
        })
    });
    entries.push_back({
        kTagReductionMatrix1,
        kTiffTypeSRational,
        12,
        make_srational_data({
            {0, 1},
            {0, 1},
            {-1, 1},
            {1, 1},
            {0, 1},
            {1, 2},
            {1, 2},
            {0, 1},
            {1, 1},
            {-1, 1},
            {0, 1},
            {0, 1},
        })
    });
    entries.push_back({
        kTagAnalogBalance,
        kTiffTypeRational,
        4,
        make_rational_data({
            {1, 1},
            {1, 1},
            {1, 1},
            {1, 1},
        })
    });
    entries.push_back({
        kTagAsShotNeutral,
        kTiffTypeRational,
        4,
        make_rational_data({
            {static_cast<std::uint32_t>(std::lround(as_shot_neutral[0] * 1000000.0)), 1000000},
            {static_cast<std::uint32_t>(std::lround(as_shot_neutral[1] * 1000000.0)), 1000000},
            {static_cast<std::uint32_t>(std::lround(as_shot_neutral[2] * 1000000.0)), 1000000},
            {static_cast<std::uint32_t>(std::lround(as_shot_neutral[3] * 1000000.0)), 1000000},
        })
    });
    entries.push_back({kTagCalibrationIlluminant1, kTiffTypeShort, 1, make_short_data({kCalibrationIlluminantD65})});
    entries.push_back({
        kTagActiveArea,
        kTiffTypeLong,
        4,
        make_long_data({
            static_cast<std::uint32_t>(frame.active_y),
            static_cast<std::uint32_t>(frame.active_x),
            static_cast<std::uint32_t>(frame.active_y + frame.active_height),
            static_cast<std::uint32_t>(frame.active_x + frame.active_width),
        })
    });
    return entries;
}

std::vector<IfdEntry> build_linear_dng_entries(
    int width,
    int height
) {
    std::vector<IfdEntry> entries;
    entries.push_back({kTagNewSubfileType, kTiffTypeLong, 1, make_long_data({0})});
    entries.push_back({kTagImageWidth, kTiffTypeLong, 1, make_long_data({static_cast<std::uint32_t>(width)})});
    entries.push_back({kTagImageLength, kTiffTypeLong, 1, make_long_data({static_cast<std::uint32_t>(height)})});
    entries.push_back({kTagBitsPerSample, kTiffTypeShort, 3, make_short_data({16, 16, 16})});
    entries.push_back({kTagCompression, kTiffTypeShort, 1, make_short_data({kCompressionNone})});
    entries.push_back({kTagPhotometricInterpretation, kTiffTypeShort, 1, make_short_data({kPhotometricRgb})});
    entries.push_back(make_ascii_entry(kTagMake, "Mitsubishi"));
    entries.push_back(make_ascii_entry(kTagModel, "DJ-1000"));
    entries.push_back({kTagStripOffsets, kTiffTypeLong, 1, make_long_data({0})});
    entries.push_back({kTagOrientation, kTiffTypeShort, 1, make_short_data({kOrientationTopLeft})});
    entries.push_back({kTagSamplesPerPixel, kTiffTypeShort, 1, make_short_data({3})});
    entries.push_back({kTagRowsPerStrip, kTiffTypeLong, 1, make_long_data({static_cast<std::uint32_t>(height)})});
    entries.push_back({
        kTagStripByteCounts,
        kTiffTypeLong,
        1,
        make_long_data({static_cast<std::uint32_t>(width * height * 6)})
    });
    entries.push_back({
        kTagPlanarConfiguration,
        kTiffTypeShort,
        1,
        make_short_data({kPlanarConfigurationChunky})
    });
    entries.push_back(make_ascii_entry(kTagSoftware, "libdj1000 modern linear DNG"));
    entries.push_back({kTagDNGVersion, kTiffTypeByte, 4, make_byte_data({1, 1, 0, 0})});
    entries.push_back({kTagDNGBackwardVersion, kTiffTypeByte, 4, make_byte_data({1, 1, 0, 0})});
    entries.push_back(make_ascii_entry(kTagUniqueCameraModel, "Mitsubishi DJ-1000 / UMAX PhotoRun"));
    return entries;
}

std::vector<std::uint8_t> quantize_linear_rgb16(const RenderedModernFloatImage& image) {
    const std::size_t plane_size = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
    if (image.width <= 0 || image.height <= 0 ||
        image.plane0.size() != plane_size ||
        image.plane1.size() != plane_size ||
        image.plane2.size() != plane_size) {
        throw std::runtime_error("modern linear DNG export requires dense RGB planes");
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 6);
    for (std::size_t index = 0; index < image.plane0.size(); ++index) {
        const std::array<float, 3> rgb = {
            image.plane0[index],
            image.plane1[index],
            image.plane2[index],
        };
        for (const float value : rgb) {
            const std::uint16_t wide = static_cast<std::uint16_t>(std::clamp(
                std::lround(std::clamp(static_cast<double>(value), 0.0, 1.0) * 65535.0),
                0L,
                65535L
            ));
            append_u16_le(bytes, wide);
        }
    }
    return bytes;
}

std::vector<std::uint8_t> serialize_tiff_ifd(
    std::vector<IfdEntry> entries,
    const std::vector<std::uint8_t>& image_data
) {
    std::sort(entries.begin(), entries.end(), [](const IfdEntry& left, const IfdEntry& right) {
        return left.tag < right.tag;
    });

    const std::uint16_t entry_count = static_cast<std::uint16_t>(entries.size());
    const std::uint32_t ifd_offset = 8;
    const std::uint32_t ifd_byte_count = 2U + static_cast<std::uint32_t>(entry_count) * 12U + 4U;
    std::uint32_t next_data_offset = ifd_offset + ifd_byte_count;

    std::vector<std::uint8_t> bytes;
    bytes.reserve(static_cast<std::size_t>(next_data_offset) + 1024 + image_data.size());
    bytes.push_back('I');
    bytes.push_back('I');
    append_u16_le(bytes, 42);
    append_u32_le(bytes, ifd_offset);

    append_u16_le(bytes, entry_count);
    std::vector<std::vector<std::uint8_t>> deferred_data;
    deferred_data.reserve(entries.size());

    std::size_t strip_offset_patch = 0;
    for (const IfdEntry& entry : entries) {
        append_u16_le(bytes, entry.tag);
        append_u16_le(bytes, entry.type);
        append_u32_le(bytes, entry.count);

        const std::uint32_t inline_capacity = 4;
        const std::uint32_t value_bytes = static_cast<std::uint32_t>(entry.data.size());
        if (value_bytes <= inline_capacity) {
            if (entry.tag == kTagStripOffsets) {
                strip_offset_patch = bytes.size();
            }
            bytes.insert(bytes.end(), entry.data.begin(), entry.data.end());
            for (std::uint32_t padding = value_bytes; padding < inline_capacity; ++padding) {
                bytes.push_back(0);
            }
        } else {
            align_even(bytes);
            append_u32_le(bytes, next_data_offset);
            std::vector<std::uint8_t> payload = entry.data;
            if ((payload.size() & 1U) != 0U) {
                payload.push_back(0);
            }
            deferred_data.push_back(std::move(payload));
            next_data_offset += static_cast<std::uint32_t>(deferred_data.back().size());
        }
    }
    append_u32_le(bytes, 0);

    for (const auto& payload : deferred_data) {
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    }
    align_even(bytes);

    const std::uint32_t image_offset = static_cast<std::uint32_t>(bytes.size());
    if (strip_offset_patch == 0) {
        throw std::runtime_error("modern DNG writer failed to locate StripOffsets tag");
    }
    set_u32_le(bytes, strip_offset_patch, image_offset);
    bytes.insert(bytes.end(), image_data.begin(), image_data.end());

    return bytes;
}

std::vector<std::uint8_t> build_dng_bytes(
    const ModernSensorFrame& frame,
    const ModernCalibratedRawFrame& corrected_frame
) {
    if (frame.complementary_pattern_locked) {
        const auto pattern = resolve_modern_complementary_pattern(frame);
        const auto as_shot_neutral = estimate_complementary_as_shot_neutral(corrected_frame);
        const double white_level = std::max(frame.shared_white_level, 1.0);
        const std::vector<std::uint8_t> image_data = quantize_calibrated_frame_to_u16(
            corrected_frame.mosaic,
            corrected_frame.full_width,
            corrected_frame.full_height,
            white_level
        );
        auto entries = build_complementary_dng_entries(frame, pattern, as_shot_neutral);
        return serialize_tiff_ifd(std::move(entries), image_data);
    }

    const auto pattern = resolve_modern_cfa_pattern(frame);
    const auto parity_gains = compute_same_color_parity_gains(corrected_frame, pattern);
    const std::vector<float> balanced_mosaic = apply_parity_gains(corrected_frame, parity_gains);
    const auto as_shot_neutral = estimate_as_shot_neutral(
        balanced_mosaic,
        corrected_frame.full_width,
        corrected_frame.active_x,
        corrected_frame.active_y,
        corrected_frame.active_width,
        corrected_frame.active_height,
        pattern
    );
    const double white_level = compute_balanced_white_level(frame, parity_gains);
    const std::vector<std::uint8_t> image_data = quantize_calibrated_frame_to_u16(
        balanced_mosaic,
        corrected_frame.full_width,
        corrected_frame.full_height,
        white_level
    );
    auto entries = build_dng_entries(frame, pattern, as_shot_neutral);
    return serialize_tiff_ifd(std::move(entries), image_data);
}

std::vector<std::uint8_t> build_linear_dng_bytes(
    const DatFile& dat,
    ExportSize size
) {
    const auto cached_stage = build_cached_modern_stage(dat);
    const auto image = render_modern_linear_from_cached_stage(cached_stage, size, SliderSettings{});
    auto entries = build_linear_dng_entries(image.width, image.height);
    const std::vector<std::uint8_t> image_data = quantize_linear_rgb16(image);
    return serialize_tiff_ifd(std::move(entries), image_data);
}

}  // namespace

std::vector<std::uint8_t> build_modern_dng_bytes(const DatFile& dat) {
    const auto raw_frame = build_modern_raw_frame(dat);
    const auto calibration = estimate_modern_raw_calibration(raw_frame);
    const auto corrected_frame = build_calibrated_modern_raw_frame(raw_frame, calibration);
    const auto sensor_frame = build_modern_sensor_frame(raw_frame, calibration, corrected_frame);
    return build_dng_bytes(sensor_frame, corrected_frame);
}

std::vector<std::uint8_t> build_modern_linear_dng_bytes(const DatFile& dat, ExportSize size) {
    return build_linear_dng_bytes(dat, size);
}

void write_modern_dng(const DatFile& dat, const std::filesystem::path& output_path) {
    const std::vector<std::uint8_t> bytes = build_modern_dng_bytes(dat);
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("could not open modern DNG output path: " + output_path.string());
    }
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw std::runtime_error("failed to write modern DNG output path: " + output_path.string());
    }
}

void write_modern_linear_dng(
    const DatFile& dat,
    const std::filesystem::path& output_path,
    ExportSize size
) {
    const std::vector<std::uint8_t> bytes = build_modern_linear_dng_bytes(dat, size);
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("could not open modern linear DNG output path: " + output_path.string());
    }
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw std::runtime_error("failed to write modern linear DNG output path: " + output_path.string());
    }
}

}  // namespace dj1000
