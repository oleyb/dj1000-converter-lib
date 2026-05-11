#include "dj1000/nonlarge_geometry.hpp"
#include "dj1000/geometry_copy.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint8_t> make_constant_plane(std::size_t size, std::uint8_t value) {
    return std::vector<std::uint8_t>(size, value);
}

}  // namespace

int main() {
    const auto plane0 = make_constant_plane(0x2F400, 60);
    const auto plane1 = make_constant_plane(0x2F400, 90);
    const auto plane2 = make_constant_plane(0x2F400, 120);

    const auto normal_stage = dj1000::build_nonlarge_stage_plane(plane0, plane1, plane2, 0, false);
    assert(normal_stage.size() ==
           static_cast<std::size_t>(dj1000::kNormalStageWidth) * dj1000::kNormalStageHeight);
    for (int row = 0; row < 244; ++row) {
        const auto row_offset = static_cast<std::size_t>(row) * dj1000::kNormalStageWidth;
        assert(std::all_of(
            normal_stage.begin() + static_cast<std::ptrdiff_t>(row_offset),
            normal_stage.begin() + static_cast<std::ptrdiff_t>(row_offset + dj1000::kNormalStageWidth),
            [](std::uint8_t value) { return value == 60; }
        ));
    }
    for (int row = 244; row < dj1000::kNormalStageHeight; ++row) {
        const auto row_offset = static_cast<std::size_t>(row) * dj1000::kNormalStageWidth;
        assert(std::all_of(
            normal_stage.begin() + static_cast<std::ptrdiff_t>(row_offset),
            normal_stage.begin() + static_cast<std::ptrdiff_t>(row_offset + dj1000::kNormalStageWidth),
            [](std::uint8_t value) { return value == 0; }
        ));
    }

    const auto preview_stage = dj1000::build_nonlarge_stage_plane(plane0, plane1, plane2, 1, true);
    assert(preview_stage.size() ==
           static_cast<std::size_t>(dj1000::kPreviewStageWidth) * dj1000::kPreviewStageHeight);
    assert(std::all_of(preview_stage.begin(), preview_stage.end(), [](std::uint8_t value) {
        return value == 90;
    }));

    std::cout << "test_nonlarge_geometry passed\n";
    return 0;
}
