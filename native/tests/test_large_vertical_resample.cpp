#include "dj1000/large_vertical_resample.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint8_t> make_row_coded_plane() {
    const std::size_t plane_bytes =
        static_cast<std::size_t>(dj1000::kLargeVerticalSourceRows - 1) *
            dj1000::kLargeVerticalSourceStride +
        dj1000::kLargeVerticalWidth;
    std::vector<std::uint8_t> plane(plane_bytes, 0);
    for (int row = 0; row < dj1000::kLargeVerticalSourceRows; ++row) {
        const auto row_offset = static_cast<std::size_t>(row) * dj1000::kLargeVerticalSourceStride;
        for (int col = 0; col < dj1000::kLargeVerticalWidth; ++col) {
            plane[row_offset + static_cast<std::size_t>(col)] = static_cast<std::uint8_t>(row);
        }
    }
    return plane;
}

std::uint8_t output_row_value(const std::vector<std::uint8_t>& plane, int row, int col = 0) {
    const auto offset =
        static_cast<std::size_t>(row) * dj1000::kLargeVerticalWidth + static_cast<std::size_t>(col);
    return plane[offset];
}

}  // namespace

int main() {
    const auto source = make_row_coded_plane();
    const auto output = dj1000::resample_large_vertical_plane(source);

    assert(output.size() ==
           static_cast<std::size_t>(dj1000::kLargeVerticalWidth) * dj1000::kLargeVerticalOutputRows);

    assert(output_row_value(output, 0) == 0);
    assert(output_row_value(output, 1) == 1);
    assert(output_row_value(output, 2) == 1);
    assert(output_row_value(output, 3) == 2);
    assert(output_row_value(output, 61) == 39);
    assert(output_row_value(output, 62) == 40);
    assert(output_row_value(output, 63) == 40);
    assert(output_row_value(output, 122) == 79);
    assert(output_row_value(output, 123) == 79);
    assert(output_row_value(output, 124) == 80);
    assert(output_row_value(output, 125) == 80);
    assert(output_row_value(output, 185) == 119);
    assert(output_row_value(output, 186) == 120);
    assert(output_row_value(output, 187) == 121);
    assert(output_row_value(output, 247) == 159);
    assert(output_row_value(output, 248) == 160);
    assert(output_row_value(output, 249) == 161);
    assert(output_row_value(output, 309) == 199);
    assert(output_row_value(output, 310) == 200);
    assert(output_row_value(output, 311) == 200);
    assert(output_row_value(output, 312) == 201);
    assert(output_row_value(output, 371) == 239);
    assert(output_row_value(output, 372) == 240);
    assert(output_row_value(output, 373) == 240);
    assert(output_row_value(output, 377) == 243);

    std::cout << "test_large_vertical_resample passed\n";
    return 0;
}
