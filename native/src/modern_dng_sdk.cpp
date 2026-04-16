#include "dj1000/modern_dng.hpp"

#include "dj1000/modern_raw_frame.hpp"
#include "dj1000/modern_sensor_frame.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#if DJ1000_HAVE_ADOBE_DNG_SDK
#include "dng_camera_profile.h"
#include "dng_exceptions.h"
#include "dng_host.h"
#include "dng_image_writer.h"
#include "dng_matrix.h"
#include "dng_memory_stream.h"
#include "dng_negative.h"
#include "dng_orientation.h"
#include "dng_pixel_buffer.h"
#include "dng_simple_image.h"
#include "dng_tag_values.h"
#include "dng_temperature.h"
#include "dng_color_spec.h"
#endif

namespace dj1000 {

namespace {

constexpr double kAdobeStage1Scale = 257.0;

constexpr std::array<std::array<double, 3>, 4> kLc9997CameraFromRgb = {{
    // Experimental variant: same complementary model, but with red/blue
    // interpretation swapped to match the observed chart behavior.
    {{1.5, 1.0, 0.5}},   // A site
    {{-0.5, 1.0, 0.5}},  // C site
    {{0.5, 1.0, -0.5}},  // D site
    {{0.5, 1.0, 1.5}},   // E site
}};

constexpr std::array<std::array<double, 4>, 3> kLc9997RgbFromCamera = {{
    // Experimental variant: swap red and blue relative to the datasheet
    // labeling to test the chart-driven "red/blue reversed" hypothesis.
    //   R = A - C
    //   G = (C + D) / 2
    //   B = E - D
    {{0.5, -0.5, 0.0, 0.0}},
    {{0.0, 0.5, 0.5, 0.0}},
    {{0.0, 0.0, -0.5, 0.5}},
}};

constexpr std::array<double, 4> kLc9997NeutralCalibrationBias = {{
    // Empirical neutral calibration derived from the photographed Adobe
    // reference chart in MDSC0011.DAT, using the legacy pipeline's gray row
    // as the target after the complementary red/blue swap was locked in.
    // This is intentionally a neutral-only correction, not a broader
    // chart-fit color transform. The current values include a small
    // additional cool shift based on ACR comparison against the legacy
    // chart render.
    1.1712965104085644,
    2.0600000000000001,
    2.1400000000000001,
    0.9300000000000000,
}};

constexpr std::array<double, 4> kLc9997AdobeProfileChannelScale = {{
    0.7200000000000000,
    1.1000000000000001,
    1.1400000000000000,
    0.7400000000000000,
}};

constexpr double kLc9997AdobeTemperatureBiasKelvin = -240.0;
constexpr double kLc9997AdobeTintBias = -122.0;
constexpr float kLc9997ProfileSatRestore = 1.18f;
constexpr float kLc9997ProfileValueRestore = 1.05f;

constexpr std::array<std::array<double, 3>, 3> kXyzD50FromLinearSrgb = {{
    {{0.4360747, 0.3850649, 0.1430804}},
    {{0.2225045, 0.7168786, 0.0606169}},
    {{0.0139322, 0.0971045, 0.7141733}},
}};

constexpr std::array<std::array<double, 3>, 3> kLinearSrgbFromXyzD50 = {{
    {{3.1338561, -1.6168667, -0.4906146}},
    {{-0.9787684, 1.9161415, 0.0334540}},
    {{0.0719453, -0.2289914, 1.4052427}},
}};

#if DJ1000_HAVE_ADOBE_DNG_SDK
template <typename T>
double percentile(std::vector<T> values, double fraction) {
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

std::array<double, 3> reduce_camera_to_rgb(
    double a,
    double c,
    double d,
    double e
) {
    std::array<double, 3> rgb = {0.0, 0.0, 0.0};
    const std::array<double, 4> camera = {a, c, d, e};
    for (std::size_t row = 0; row < 3; ++row) {
        double sum = 0.0;
        for (std::size_t col = 0; col < 4; ++col) {
            sum += kLc9997RgbFromCamera[row][col] * camera[col];
        }
        rgb[row] = std::max(0.0, sum);
    }
    return rgb;
}

std::array<double, 4> estimate_sdk_as_shot_neutral(const ModernCalibratedRawFrame& frame) {
    struct Candidate {
        double luminance;
        double chroma;
        std::array<double, 4> camera;
    };

    const auto accumulate_candidates =
        [](const std::vector<Candidate>& candidates, double low_chroma_fraction, double bright_fraction) {
        std::array<double, 4> result = {0.0, 0.0, 0.0, 0.0};
        if (candidates.empty()) {
            return result;
        }

        std::vector<double> chromas;
        chromas.reserve(candidates.size());
        for (const auto& candidate : candidates) {
            chromas.push_back(candidate.chroma);
        }
        const double chroma_cutoff = percentile(chromas, low_chroma_fraction);

        std::vector<Candidate> low_chroma_candidates;
        low_chroma_candidates.reserve(candidates.size());
        for (const auto& candidate : candidates) {
            if (candidate.chroma <= chroma_cutoff) {
                low_chroma_candidates.push_back(candidate);
            }
        }
        if (low_chroma_candidates.empty()) {
            return result;
        }

        std::vector<double> luminances;
        luminances.reserve(low_chroma_candidates.size());
        for (const auto& candidate : low_chroma_candidates) {
            luminances.push_back(candidate.luminance);
        }
        const double bright_cutoff = percentile(luminances, bright_fraction);

        double weight_sum = 0.0;
        for (const auto& candidate : low_chroma_candidates) {
            if (candidate.luminance < bright_cutoff) {
                continue;
            }
            const double weight = std::max(candidate.luminance, 1.0);
            for (std::size_t channel = 0; channel < 4; ++channel) {
                result[channel] += candidate.camera[channel] * weight;
            }
            weight_sum += weight;
        }

        if (!(weight_sum > 0.0)) {
            for (const auto& candidate : low_chroma_candidates) {
                const double weight = std::max(candidate.luminance, 1.0);
                for (std::size_t channel = 0; channel < 4; ++channel) {
                    result[channel] += candidate.camera[channel] * weight;
                }
                weight_sum += weight;
            }
        }

        if (!(weight_sum > 0.0)) {
            return std::array<double, 4>{0.0, 0.0, 0.0, 0.0};
        }

        for (double& channel : result) {
            channel = std::max(channel / weight_sum, 1.0e-6);
        }
        return result;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(static_cast<std::size_t>(frame.active_width * frame.active_height) / 4);

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

            const auto rgb = reduce_camera_to_rgb(a, c, d, e);
            const double max_channel = std::max({rgb[0], rgb[1], rgb[2]});
            const double min_channel = std::min({rgb[0], rgb[1], rgb[2]});
            const double luminance = rgb[0] * 0.2126 + rgb[1] * 0.7152 + rgb[2] * 0.0722;
            const double chroma = max_channel <= 1.0e-6 ? 1.0 : ((max_channel - min_channel) / max_channel);

            if (!(luminance > 2.0 && luminance < 240.0)) {
                continue;
            }

            candidates.push_back(Candidate{
                luminance,
                chroma,
                {
                    std::max(a, 1.0e-6),
                    std::max(c, 1.0e-6),
                    std::max(d, 1.0e-6),
                    std::max(e, 1.0e-6),
                }
            });
        }
    }

    auto neutral = accumulate_candidates(candidates, 0.08, 0.55);
    const bool neutral_valid = std::all_of(
        neutral.begin(),
        neutral.end(),
        [](double value) { return value > 0.0; }
    );
    if (neutral_valid) {
        return neutral;
    }

    neutral = accumulate_candidates(candidates, 0.18, 0.7);
    const bool bright_valid = std::all_of(
        neutral.begin(),
        neutral.end(),
        [](double value) { return value > 0.0; }
    );
    if (bright_valid) {
        return neutral;
    }

    std::array<double, 4> fallback = {0.0, 0.0, 0.0, 0.0};
    std::size_t fallback_count = 0;
    for (int block_row = 0; block_row < block_height; ++block_row) {
        for (int block_col = 0; block_col < block_width; ++block_col) {
            const int row = frame.active_y + block_row * 2;
            const int col = frame.active_x + block_col * 2;
            fallback[0] += static_cast<double>(frame.at(row, col));
            fallback[1] += static_cast<double>(frame.at(row, col + 1));
            fallback[2] += static_cast<double>(frame.at(row + 1, col));
            fallback[3] += static_cast<double>(frame.at(row + 1, col + 1));
            ++fallback_count;
        }
    }

    if (fallback_count == 0) {
        return {1.0, 1.0, 1.0, 1.0};
    }

    for (double& channel : fallback) {
        channel = std::max(channel / static_cast<double>(fallback_count), 1.0e-6);
    }
    return fallback;
}

std::array<double, 4> apply_sdk_neutral_calibration_bias(const std::array<double, 4>& neutral) {
    std::array<double, 4> corrected = neutral;
    for (std::size_t channel = 0; channel < corrected.size(); ++channel) {
        corrected[channel] = std::max(
            corrected[channel] * kLc9997NeutralCalibrationBias[channel],
            1.0e-6
        );
    }
    return corrected;
}

uint32 pack_lc9997_quad_mosaic_pattern() {
    uint32 pattern = 0;
    constexpr std::array<uint32, 4> tile = {0U, 1U, 2U, 3U};
    for (uint32 row = 0; row < 8; ++row) {
        for (uint32 col = 0; col < 2; ++col) {
            const uint32 index = tile[((row & 1U) << 1U) | col];
            const uint32 shift = ((((row << 1U) & 14U) + (col & 1U)) << 1U);
            pattern |= (index & 3U) << shift;
        }
    }
    return pattern;
}

uint16 scale_to_stage1_sample(double value) {
    return static_cast<uint16>(
        std::clamp(std::lround(std::clamp(value, 0.0, 255.0) * kAdobeStage1Scale), 0L, 65535L)
    );
}

dng_vector make_identity_balance_vector(uint32 channels) {
    dng_vector balance(channels);
    for (uint32 index = 0; index < channels; ++index) {
        balance[index] = 1.0;
    }
    return balance;
}

dng_vector make_camera_neutral_vector(const std::array<double, 4>& values) {
    dng_vector neutral(4);
    const double max_value = std::max(
        {values[0], values[1], values[2], values[3], 1.0e-6}
    );
    for (uint32 index = 0; index < 4; ++index) {
        neutral[index] = std::clamp(values[index] / max_value, 1.0e-6, 1.0);
    }
    return neutral;
}

dng_xy_coord make_corrected_camera_white_xy(
    const dng_negative& negative,
    const dng_camera_profile& profile,
    const std::array<double, 4>& neutral
) {
    dng_color_spec spec(negative, &profile);
    dng_temperature temp_tint(spec.NeutralToXY(make_camera_neutral_vector(neutral)));
    temp_tint.SetTemperature(std::max(2000.0, temp_tint.Temperature() + kLc9997AdobeTemperatureBiasKelvin));
    temp_tint.SetTint(std::clamp(temp_tint.Tint() + kLc9997AdobeTintBias, -150.0, 150.0));
    return temp_tint.Get_xy_coord();
}

dng_matrix make_sdk_forward_matrix1() {
    dng_matrix matrix(3, 4);
    for (uint32 row = 0; row < 3; ++row) {
        for (uint32 col = 0; col < 4; ++col) {
            double sum = 0.0;
            for (uint32 mid = 0; mid < 3; ++mid) {
                sum += kXyzD50FromLinearSrgb[row][mid] * kLc9997RgbFromCamera[mid][col];
            }
            matrix[row][col] = sum;
        }
    }
    dng_camera_profile::NormalizeForwardMatrix(matrix);
    return matrix;
}

dng_matrix make_sdk_color_matrix1() {
    dng_matrix matrix(4, 3);
    for (uint32 row = 0; row < 4; ++row) {
        for (uint32 col = 0; col < 3; ++col) {
            double sum = 0.0;
            for (uint32 mid = 0; mid < 3; ++mid) {
                sum += kLc9997CameraFromRgb[row][mid] * kLinearSrgbFromXyzD50[mid][col];
            }
            matrix[row][col] = sum;
        }
    }
    return matrix;
}

dng_matrix make_sdk_reduction_matrix1() {
    dng_matrix matrix(3, 4);
    for (uint32 row = 0; row < 3; ++row) {
        for (uint32 col = 0; col < 4; ++col) {
            matrix[row][col] = kLc9997RgbFromCamera[row][col];
        }
    }
    return matrix;
}

dng_hue_sat_map make_sdk_saturation_restore_map(float sat_scale, float value_scale) {
    dng_hue_sat_map map;
    constexpr uint32 kHueDivisions = 6;
    constexpr uint32 kSatDivisions = 2;
    map.SetDivisions(kHueDivisions, kSatDivisions, 1);

    for (uint32 hue = 0; hue < kHueDivisions; ++hue) {
        dng_hue_sat_map::HSBModify zero_sat{};
        zero_sat.fHueShift = 0.0f;
        zero_sat.fSatScale = 1.0f;
        zero_sat.fValScale = 1.0f;
        map.SetDelta(hue, 0, 0, zero_sat);

        dng_hue_sat_map::HSBModify colored{};
        colored.fHueShift = 0.0f;
        colored.fSatScale = sat_scale;
        colored.fValScale = value_scale;
        map.SetDelta(hue, 1, 0, colored);
    }

    return map;
}

void apply_sdk_profile_channel_scale(
    dng_matrix& color_matrix,
    dng_matrix& forward_matrix,
    dng_matrix& reduction_matrix
) {
    for (uint32 channel = 0; channel < 4; ++channel) {
        const double scale = kLc9997AdobeProfileChannelScale[channel];
        for (uint32 col = 0; col < 3; ++col) {
            color_matrix[channel][col] *= scale;
        }
        for (uint32 row = 0; row < 3; ++row) {
            forward_matrix[row][channel] /= scale;
            reduction_matrix[row][channel] /= scale;
        }
    }
    dng_camera_profile::NormalizeForwardMatrix(forward_matrix);
}

AutoPtr<dng_simple_image> build_stage1_image(dng_host& host, const ModernRawFrame& frame) {
    AutoPtr<dng_simple_image> image(
        new dng_simple_image(
            dng_rect(0, 0, frame.full_height, frame.full_width),
            1,
            ttShort,
            host.Allocator()
        )
    );

    dng_pixel_buffer buffer;
    image->GetPixelBuffer(buffer);
    for (int row = 0; row < frame.full_height; ++row) {
        for (int col = 0; col < frame.full_width; ++col) {
            *buffer.DirtyPixel_uint16(row, col, 0) =
                scale_to_stage1_sample(static_cast<double>(frame.at(row, col)));
        }
    }

    return AutoPtr<dng_simple_image>(image.Release());
}

AutoPtr<dng_negative> build_sdk_negative(
    dng_host& host,
    const ModernRawFrame& raw_frame,
    const ModernRawCalibration& calibration,
    const ModernCalibratedRawFrame& corrected_frame,
    const ModernSensorFrame& sensor_frame
) {
    if (!sensor_frame.complementary_pattern_locked) {
        throw std::runtime_error("SDK true RAW DNG export currently requires a locked LC9997M complementary pattern");
    }

    AutoPtr<dng_negative> negative(host.Make_dng_negative());
    negative->SetModelName("Mitsubishi DJ-1000 / UMAX PhotoRun");
    negative->SetLocalName("Mitsubishi DJ-1000 / UMAX PhotoRun");
    negative->SetBaseOrientation(dng_orientation());
    negative->SetColorChannels(4);
    negative->SetColorKeys(colorKeyCyan, colorKeyYellow, colorKeyGreen, colorKeyWhite);
    negative->SetQuadMosaic(pack_lc9997_quad_mosaic_pattern());
    negative->SetAnalogBalance(make_identity_balance_vector(4));
    const auto neutral =
        apply_sdk_neutral_calibration_bias(estimate_sdk_as_shot_neutral(corrected_frame));
    negative->SetDefaultCropOrigin(raw_frame.active_x, raw_frame.active_y);
    negative->SetDefaultCropSize(raw_frame.active_width, raw_frame.active_height);
    negative->SetDefaultScale(
        dng_urational(1, 1),
        dng_urational(
            static_cast<uint32>(raw_frame.active_width * 3),
            static_cast<uint32>(raw_frame.active_height * 4)
        )
    );
    negative->SetActiveArea(
        dng_rect(
            raw_frame.active_y,
            raw_frame.active_x,
            raw_frame.active_y + raw_frame.active_height,
            raw_frame.active_x + raw_frame.active_width
        )
    );

    std::vector<dng_rect> masked_areas;
    if (raw_frame.full_width > raw_frame.active_width) {
        masked_areas.emplace_back(
            raw_frame.active_y,
            raw_frame.active_x + raw_frame.active_width,
            raw_frame.full_height,
            raw_frame.full_width
        );
    }
    if (raw_frame.full_height > raw_frame.active_height) {
        masked_areas.emplace_back(
            raw_frame.active_y + raw_frame.active_height,
            raw_frame.active_x,
            raw_frame.full_height,
            raw_frame.full_width
        );
    }
    if (!masked_areas.empty()) {
        negative->SetMaskedAreas(
            static_cast<uint32>(masked_areas.size()),
            masked_areas.data()
        );
    }

    negative->SetQuadBlacks(
        calibration.black_levels[0] * kAdobeStage1Scale,
        calibration.black_levels[1] * kAdobeStage1Scale,
        calibration.black_levels[2] * kAdobeStage1Scale,
        calibration.black_levels[3] * kAdobeStage1Scale
    );

    const double max_black = *std::max_element(
        calibration.black_levels.begin(),
        calibration.black_levels.end()
    );
    const uint32 white_level = static_cast<uint32>(
        std::clamp(
            std::lround(std::clamp(sensor_frame.shared_white_level + max_black, 1.0, 255.0) * kAdobeStage1Scale),
            1L,
            65535L
        )
    );
    negative->SetWhiteLevel(white_level);

    AutoPtr<dng_image> stage1_image(build_stage1_image(host, raw_frame).Release());
    negative->SetStage1Image(stage1_image);

    AutoPtr<dng_camera_profile> profile(new dng_camera_profile());
    profile->SetName("DJ1000 Complementary RAW");
    profile->SetCalibrationIlluminant1(lsD65);
    dng_matrix color_matrix = make_sdk_color_matrix1();
    dng_matrix forward_matrix = make_sdk_forward_matrix1();
    dng_matrix reduction_matrix = make_sdk_reduction_matrix1();
    apply_sdk_profile_channel_scale(color_matrix, forward_matrix, reduction_matrix);
    profile->SetColorMatrix1(color_matrix);
    profile->SetForwardMatrix1(forward_matrix);
    profile->SetReductionMatrix1(reduction_matrix);
    profile->SetHueSatMapEncoding(encoding_Linear);
    profile->SetHueSatDeltas1(
        make_sdk_saturation_restore_map(
            kLc9997ProfileSatRestore,
            kLc9997ProfileValueRestore
        )
    );
    negative->SetCameraWhiteXY(make_corrected_camera_white_xy(*negative, *profile, neutral));
    negative->AddProfile(profile);

    return AutoPtr<dng_negative>(negative.Release());
}
#endif

}  // namespace

bool modern_dng_sdk_available() noexcept {
#if DJ1000_HAVE_ADOBE_DNG_SDK
    return true;
#else
    return false;
#endif
}

std::vector<std::uint8_t> build_modern_dng_sdk_bytes(const DatFile& dat) {
#if !DJ1000_HAVE_ADOBE_DNG_SDK
    (void) dat;
    throw std::runtime_error("Adobe DNG SDK support is not enabled in this build");
#else
    try {
        const auto raw_frame = build_modern_raw_frame(dat);
        const auto calibration = estimate_modern_raw_calibration(raw_frame);
        const auto corrected_frame = build_calibrated_modern_raw_frame(raw_frame, calibration);
        const auto sensor_frame = build_modern_sensor_frame(raw_frame, calibration, corrected_frame);

        dng_host host;
        host.SetSaveLinearDNG(false);
        host.SetKeepOriginalFile(false);

        AutoPtr<dng_negative> negative = build_sdk_negative(
            host,
            raw_frame,
            calibration,
            corrected_frame,
            sensor_frame
        );
        negative->SynchronizeMetadata();

        dng_image_writer writer;
        dng_memory_stream stream(host.Allocator());
        writer.WriteDNG(
            host,
            stream,
            *negative,
            nullptr,
            dngVersion_SaveDefault,
            true
        );

        AutoPtr<dng_memory_block> block(stream.AsMemoryBlock(host.Allocator()));
        const auto* begin = block->Buffer_uint8();
        const auto* end = begin + block->LogicalSize();
        return std::vector<std::uint8_t>(begin, end);
    } catch (const dng_exception& error) {
        throw std::runtime_error(std::string("Adobe DNG SDK export failed: ") + error.what());
    }
#endif
}

void write_modern_dng_sdk(
    const DatFile& dat,
    const std::filesystem::path& output_path
) {
    const std::vector<std::uint8_t> bytes = build_modern_dng_sdk_bytes(dat);
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("could not open Adobe DNG SDK output path: " + output_path.string());
    }
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw std::runtime_error("failed to write Adobe DNG SDK output path: " + output_path.string());
    }
}

}  // namespace dj1000
