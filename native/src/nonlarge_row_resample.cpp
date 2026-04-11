#include "dj1000/nonlarge_row_resample.hpp"

#include "dj1000/resample_lut.hpp"

#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

constexpr int kBlendBase = 0x40;
constexpr int kBlendMask = 0x7FE0;
constexpr int kBlendClamp = 0x1FE;
constexpr int kBlendOverflowMask = 0x600;

std::uint8_t decode_scale(std::uint8_t packed_control) {
    const std::uint8_t low_bits = static_cast<std::uint8_t>(packed_control & 0x3F);
    if (low_bits == 0x3F) {
        return kBlendBase;
    }
    return low_bits;
}

int scale_sample(std::uint8_t sample, std::uint8_t scale) {
    return ((static_cast<int>(sample) * static_cast<int>(scale)) & kBlendMask) >> 5;
}

std::uint8_t clamp_and_round(int accum) {
    accum &= 0x7FF;
    if ((accum & kBlendOverflowMask) != 0) {
        accum = kBlendClamp;
    }
    if ((accum & 0x01) != 0) {
        accum += 2;
    }
    return static_cast<std::uint8_t>(accum >> 1);
}

void validate_inputs(
    std::span<const std::uint8_t> source_row,
    int lut_width_parameter,
    int width_limit,
    int output_length
) {
    if (source_row.size() < 2) {
        throw std::runtime_error("non-large row resample needs at least two source bytes");
    }
    if (lut_width_parameter <= 0) {
        throw std::runtime_error("lut_width_parameter must be positive");
    }
    if (width_limit <= 0) {
        throw std::runtime_error("width_limit must be positive");
    }
    if (output_length <= 0) {
        throw std::runtime_error("output_length must be positive");
    }
}

}  // namespace

std::vector<std::uint8_t> resample_nonlarge_row(
    std::span<const std::uint8_t> source_row,
    int lut_width_parameter,
    int width_limit,
    int output_length
) {
    validate_inputs(source_row, lut_width_parameter, width_limit, output_length);

    const auto lut = build_resample_lut(lut_width_parameter, true);
    if (lut.packed_bytes.empty()) {
        throw std::runtime_error("row resample LUT helper returned no data");
    }

    std::vector<std::uint8_t> output;
    output.reserve(static_cast<std::size_t>(output_length));

    std::size_t source_index = 1;
    std::uint8_t previous_sample = source_row[0];
    std::uint8_t current_sample = source_row[1];
    int source_counter = 2;

    std::size_t lut_index = 0;
    int phase_counter = 0;
    int guard_iterations = 0;

    std::uint8_t packed_control = lut.packed_bytes[lut_index];
    while (static_cast<int>(output.size()) < output_length) {
        if (++guard_iterations > 8192) {
            throw std::runtime_error("non-large row resample exceeded its iteration guard");
        }

        const std::uint8_t carry_mode = static_cast<std::uint8_t>(packed_control >> 6);
        const std::uint8_t scale = decode_scale(packed_control);
        const std::uint8_t inverse_scale = static_cast<std::uint8_t>(kBlendBase - scale);

        if (carry_mode != 0x00 && carry_mode != 0x02) {
            const int accum =
                scale_sample(previous_sample, scale) +
                scale_sample(current_sample, inverse_scale);
            output.push_back(clamp_and_round(accum));
            if (static_cast<int>(output.size()) == output_length) {
                break;
            }
        }

        if (source_counter < width_limit) {
            previous_sample = current_sample;
            ++source_index;
            current_sample =
                source_index < source_row.size() ? source_row[source_index] : static_cast<std::uint8_t>(0);
        }
        ++source_counter;

        ++phase_counter;
        if (phase_counter > kNonLargePhaseResetCounter) {
            phase_counter = 0;
            lut_index = 0;
        } else {
            ++lut_index;
            if (lut_index >= lut.packed_bytes.size()) {
                throw std::runtime_error("non-large row resample LUT index ran past packed bytes");
            }
        }
        packed_control = lut.packed_bytes[lut_index];
    }

    if (static_cast<int>(output.size()) != output_length) {
        throw std::runtime_error(
            "non-large row resample produced " + std::to_string(output.size()) +
            " bytes, expected " + std::to_string(output_length)
        );
    }

    return output;
}

}  // namespace dj1000
