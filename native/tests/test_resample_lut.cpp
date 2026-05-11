#include "dj1000/resample_lut.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

void expect_byte(const dj1000::ResampleLut& lut, int index, std::uint8_t value) {
    const auto actual = lut.packed_bytes.at(static_cast<std::size_t>(index));
    if (actual != value) {
        std::cerr
            << "unexpected byte at index " << index
            << ": expected " << static_cast<int>(value)
            << " got " << static_cast<int>(actual)
            << '\n';
        std::abort();
    }
}

}  // namespace

int main() {
    const auto narrow = dj1000::build_resample_lut(124);
    assert(narrow.phase_stride == 0x14);
    assert(narrow.base_period == dj1000::kResampleBasePeriod);
    assert(narrow.packed_length == 400);
    expect_byte(narrow, 0, 0xFF);
    expect_byte(narrow, 1, 0x3F);
    expect_byte(narrow, 2, 0x3F);
    expect_byte(narrow, 3, 0xFF);
    expect_byte(narrow, 15, 0x3F);
    expect_byte(narrow, 399, 0x3F);

    const auto normal = dj1000::build_resample_lut(0x101);
    assert(normal.phase_stride == 0x14);
    assert(normal.packed_length == 400);
    expect_byte(normal, 0, 0xFF);
    expect_byte(normal, 1, 0xFF);
    expect_byte(normal, 2, 0x3F);
    expect_byte(normal, 5, 0x3F);
    expect_byte(normal, 63, 0xFF);
    expect_byte(normal, 399, 0x3F);

    const auto wide = dj1000::build_resample_lut(0x1F8);
    assert(wide.phase_stride == 0x11);
    assert(wide.base_period == dj1000::kResampleBasePeriod);
    assert(wide.packed_length == dj1000::kWideResamplePackedLength);
    expect_byte(wide, 0, 0x7F);
    expect_byte(wide, 1, 0x3F);
    expect_byte(wide, 4, 0x7F);
    expect_byte(wide, 63, 0x7F);
    expect_byte(wide, 127, 0x3F);
    expect_byte(wide, 799, 0x7F);

    std::cout << "test_resample_lut passed\n";
    return 0;
}
