#include "dj1000/nonlarge_row_resample.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint8_t> make_constant_row(std::size_t size, std::uint8_t value) {
    return std::vector<std::uint8_t>(size, value);
}

}  // namespace

int main() {
    const auto normal_source = make_constant_row(504, 123);
    const auto normal_output = dj1000::resample_nonlarge_row(
        normal_source,
        dj1000::kNormalRowLutWidthParameter,
        dj1000::kNormalRowWidthLimit,
        dj1000::kNormalRowOutputLength
    );
    assert(normal_output.size() == static_cast<std::size_t>(dj1000::kNormalRowOutputLength));
    assert(std::all_of(normal_output.begin(), normal_output.end(), [](std::uint8_t value) {
        return value == 123;
    }));

    const auto preview_source = make_constant_row(126, 77);
    const auto preview_output = dj1000::resample_nonlarge_row(
        preview_source,
        dj1000::kPreviewRowLutWidthParameter,
        dj1000::kPreviewRowWidthLimit,
        dj1000::kPreviewRowOutputLength
    );
    assert(preview_output.size() == static_cast<std::size_t>(dj1000::kPreviewRowOutputLength));
    assert(std::all_of(preview_output.begin(), preview_output.end(), [](std::uint8_t value) {
        return value == 77;
    }));

    std::cout << "test_nonlarge_row_resample passed\n";
    return 0;
}
