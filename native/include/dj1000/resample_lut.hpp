#pragma once

#include <cstdint>
#include <vector>

namespace dj1000 {

inline constexpr int kResampleBasePeriod = 0x190;
inline constexpr int kWideResamplePackedLength = 0x320;

struct ResampleLut {
    int width_parameter = 0;
    int phase_stride = 0;
    int base_period = 0;
    int packed_length = 0;
    std::vector<std::uint8_t> scale_values;
    std::vector<std::uint8_t> carry_flags;
    std::vector<std::uint8_t> packed_bytes;
};

[[nodiscard]] ResampleLut build_resample_lut(
    int width_parameter,
    bool preserve_full_scale_marker = false
);

}  // namespace dj1000
