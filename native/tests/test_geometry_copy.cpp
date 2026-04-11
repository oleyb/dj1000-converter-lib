#include "dj1000/geometry_copy.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint8_t> make_patterned_plane(int width, int height) {
    std::vector<std::uint8_t> plane(static_cast<std::size_t>(width) * height);
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            plane[static_cast<std::size_t>(row) * width + col] =
                static_cast<std::uint8_t>((row * 17 + col * 3) & 0xFF);
        }
    }
    return plane;
}

}  // namespace

int main() {
    const auto preview_source = make_patterned_plane(dj1000::kPreviewStageWidth, dj1000::kPreviewStageHeight);
    const auto preview = dj1000::copy_preview_stage_plane(preview_source);
    assert(preview.size() == preview_source.size());
    assert(preview.front() == preview_source.front());
    assert(preview.back() == preview_source.back());

    const auto normal_source = make_patterned_plane(dj1000::kGeometrySourceStride, dj1000::kNormalStageHeight);
    const auto normal = dj1000::copy_normal_stage_plane(normal_source);
    assert(normal.size() == static_cast<std::size_t>(dj1000::kNormalStageWidth) * dj1000::kNormalStageHeight);
    for (int row = 0; row < dj1000::kNormalStageHeight; ++row) {
        const auto src_offset = static_cast<std::size_t>(row) * dj1000::kGeometrySourceStride + dj1000::kNormalStageLeftCrop;
        const auto dst_offset = static_cast<std::size_t>(row) * dj1000::kNormalStageWidth;
        assert(normal[dst_offset] == normal_source[src_offset]);
        assert(normal[dst_offset + dj1000::kNormalStageWidth - 1] ==
               normal_source[src_offset + dj1000::kNormalStageWidth - 1]);
    }

    const auto large_source = make_patterned_plane(dj1000::kLargeStageWidth, dj1000::kGeometrySourceHeight);
    const auto large = dj1000::copy_large_stage_plane(large_source);
    assert(large.size() == static_cast<std::size_t>(dj1000::kLargeStageWidth) * dj1000::kLargeStageHeight);
    assert(large.front() == large_source.front());
    const auto last_large_index = large.size() - 1;
    assert(large[last_large_index] == large_source[last_large_index]);

    std::cout << "test_geometry_copy passed\n";
    return 0;
}
