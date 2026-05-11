#include "dj1000/large_vertical_resample.hpp"

#include "dj1000/resample_lut.hpp"

#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

constexpr int kBlendBase = 0x40;
constexpr int kBlendMask = 0x7FE0;
constexpr int kBlendClamp = 0x1FE;
constexpr int kBlendOverflowMask = 0x600;

void validate_source_plane(std::span<const std::uint8_t> plane) {
    const std::size_t required =
        static_cast<std::size_t>(kLargeVerticalSourceRows - 1) * kLargeVerticalSourceStride +
        kLargeVerticalWidth;
    if (plane.size() < required) {
        throw std::runtime_error("large vertical source plane is too small");
    }
}

std::span<const std::uint8_t> source_row(std::span<const std::uint8_t> plane, int row_index) {
    const auto offset = static_cast<std::size_t>(row_index) * kLargeVerticalSourceStride;
    return plane.subspan(offset, kLargeVerticalWidth);
}

std::uint8_t decode_alpha(std::uint8_t packed_control, bool end_of_source) {
    if (end_of_source) {
        return 0;
    }

    const std::uint8_t packed_scale = static_cast<std::uint8_t>(packed_control & 0x3F);
    if (packed_scale == 0x3F) {
        return kBlendBase;
    }
    return packed_scale;
}

std::uint8_t blend_sample(std::uint8_t row_a, std::uint8_t row_b, std::uint8_t alpha) {
    const int beta = kBlendBase - alpha;
    int accum = ((static_cast<int>(row_a) * alpha) & kBlendMask) >> 5;
    accum += ((static_cast<int>(row_b) * beta) & kBlendMask) >> 5;
    accum &= 0x7FF;
    if ((accum & kBlendOverflowMask) != 0) {
        accum = kBlendClamp;
    }
    if ((accum & 0x01) != 0) {
        accum += 2;
    }
    return static_cast<std::uint8_t>(accum >> 1);
}

void write_output_row(
    std::span<const std::uint8_t> plane,
    int row_a_index,
    int row_b_index,
    std::uint8_t alpha,
    std::span<std::uint8_t> out_row
) {
    const auto row_a = source_row(plane, row_a_index);
    const auto row_b = source_row(plane, row_b_index);
    for (int column = 0; column < kLargeVerticalWidth; ++column) {
        out_row[static_cast<std::size_t>(column)] =
            blend_sample(row_a[static_cast<std::size_t>(column)], row_b[static_cast<std::size_t>(column)], alpha);
    }
}

}  // namespace

std::vector<std::uint8_t> resample_large_vertical_plane(std::span<const std::uint8_t> source_plane) {
    validate_source_plane(source_plane);

    const auto lut = build_resample_lut(kLargeVerticalLutWidthParameter, true);
    if (lut.packed_bytes.empty()) {
        throw std::runtime_error("large vertical LUT helper returned no data");
    }

    std::vector<std::uint8_t> output(
        static_cast<std::size_t>(kLargeVerticalWidth) * kLargeVerticalOutputRows
    );

    int row_a_index = 0;
    int row_b_index = 1;
    int next_source_row = 2;
    int phase_counter = 0;
    std::size_t lut_index = 0;
    bool end_of_source = false;

    for (int output_row = 0; output_row < kLargeVerticalOutputRows; ++output_row) {
        const std::uint8_t packed_control = lut.packed_bytes[lut_index];
        const std::uint8_t alpha = decode_alpha(packed_control, end_of_source);
        const auto row_offset = static_cast<std::size_t>(output_row) * kLargeVerticalWidth;
        write_output_row(
            source_plane,
            row_a_index,
            row_b_index,
            alpha,
            std::span<std::uint8_t>(output).subspan(row_offset, kLargeVerticalWidth)
        );

        if (output_row + 1 == kLargeVerticalOutputRows) {
            break;
        }

        const std::uint8_t carry_mode = static_cast<std::uint8_t>(packed_control >> 6);
        if (carry_mode != 0x01) {
            if (next_source_row < kLargeVerticalSourceRows) {
                row_a_index = row_b_index;
                row_b_index = next_source_row;
                ++next_source_row;
            } else {
                end_of_source = true;
            }
        }

        ++phase_counter;
        if (phase_counter > kLargeVerticalLutResetCounter) {
            phase_counter = 0;
            lut_index = 0;
        } else {
            ++lut_index;
            if (lut_index >= lut.packed_bytes.size()) {
                throw std::runtime_error("large vertical LUT index ran past packed bytes");
            }
        }
    }

    return output;
}

}  // namespace dj1000
