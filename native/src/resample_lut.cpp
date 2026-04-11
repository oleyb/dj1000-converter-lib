#include "dj1000/resample_lut.hpp"

#include <stdexcept>

namespace dj1000 {

namespace {

std::uint8_t pack_resample_byte(
    std::uint8_t scale_value,
    std::uint8_t carry_flag,
    bool preserve_full_scale_marker
) {
    std::uint8_t packed_scale = scale_value;
    if (!preserve_full_scale_marker) {
        packed_scale = 0x3F;
    } else if (packed_scale == 0x40) {
        packed_scale = 0x3F;
    }

    const std::uint8_t top_bits = static_cast<std::uint8_t>(carry_flag << 6);
    std::uint8_t packed = static_cast<std::uint8_t>(packed_scale ^ top_bits);
    packed = static_cast<std::uint8_t>(packed & 0x3F);
    packed = static_cast<std::uint8_t>(packed ^ top_bits);
    return packed;
}

void build_narrow_resample_lut(ResampleLut& lut) {
    lut.phase_stride = 0x14;
    lut.base_period = kResampleBasePeriod;

    int accumulator = 0;
    while (static_cast<int>(lut.scale_values.size()) < kResampleBasePeriod) {
        std::uint8_t scale_value = 0x40;
        if (accumulator != 0) {
            const auto scaled = static_cast<std::uint32_t>(accumulator) << 20;
            const auto quotient = scaled / static_cast<std::uint32_t>(lut.width_parameter);
            const auto reduced = quotient >> 14;
            scale_value = static_cast<std::uint8_t>(0x40 - reduced);
        }

        lut.scale_values.push_back(scale_value);
        accumulator += kResampleBasePeriod;

        int wrap_count = 0;
        while (accumulator >= lut.width_parameter) {
            accumulator -= lut.width_parameter;
            ++wrap_count;
        }

        lut.carry_flags.push_back(0x03);
        if (wrap_count > 1) {
            const int extras = wrap_count - 1;
            lut.scale_values.insert(lut.scale_values.end(), extras, 0x00);
            lut.carry_flags.insert(lut.carry_flags.end(), extras, 0x00);
        }
    }
}

void build_wide_resample_lut(ResampleLut& lut) {
    lut.phase_stride = 0x11;
    lut.base_period = kResampleBasePeriod;
    lut.scale_values.reserve(kWideResamplePackedLength);
    lut.carry_flags.reserve(kWideResamplePackedLength);

    int accumulator = 0;
    for (int index = 0; index < kWideResamplePackedLength; ++index) {
        std::uint8_t scale_value = 0x40;
        if (accumulator != 0) {
            const auto scaled = static_cast<std::uint32_t>(accumulator) << 17;
            const auto quotient = scaled / static_cast<std::uint32_t>(lut.width_parameter);
            const auto reduced = quotient >> 11;
            scale_value = static_cast<std::uint8_t>(0x40 - reduced);
        }

        lut.scale_values.push_back(scale_value);
        accumulator += kResampleBasePeriod;

        if (accumulator < lut.width_parameter) {
            lut.carry_flags.push_back(0x01);
        } else {
            lut.carry_flags.push_back(0x00);
            accumulator -= lut.width_parameter;
        }
    }
}

}  // namespace

ResampleLut build_resample_lut(int width_parameter, bool preserve_full_scale_marker) {
    if (width_parameter <= 0) {
        throw std::runtime_error("width_parameter must be positive");
    }

    ResampleLut lut;
    lut.width_parameter = width_parameter;

    if (width_parameter > kResampleBasePeriod) {
        build_wide_resample_lut(lut);
    } else {
        build_narrow_resample_lut(lut);
    }

    if (lut.scale_values.size() != lut.carry_flags.size()) {
        throw std::runtime_error("resample LUT helper generated mismatched table sizes");
    }

    lut.packed_length = static_cast<int>(lut.scale_values.size());
    lut.packed_bytes.reserve(lut.scale_values.size());
    for (std::size_t index = 0; index < lut.scale_values.size(); ++index) {
        lut.packed_bytes.push_back(
            pack_resample_byte(
                lut.scale_values[index],
                lut.carry_flags[index],
                preserve_full_scale_marker
            )
        );
    }

    return lut;
}

}  // namespace dj1000
