#include "dj1000/geometry_copy.hpp"
#include "dj1000/nonlarge_source_pipeline.hpp"
#include "dj1000/nonlarge_geometry.hpp"
#include "dj1000/source_seed_stage.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    {
        std::vector<std::uint8_t> source(dj1000::expected_source_seed_input_byte_count(false), 0);
        for (int row = 0; row < dj1000::kNormalSourceSeedActiveRows; ++row) {
            const auto row_offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kNormalSourceSeedInputStride);
            const auto value = static_cast<std::uint8_t>(row & 0xFF);
            for (int col = 0; col < dj1000::kNormalSourceSeedActiveWidth; ++col) {
                source[row_offset + static_cast<std::size_t>(col)] = value;
            }
        }

        const auto stage = dj1000::build_nonlarge_stage_planes_from_source(source, false);
        assert(stage.plane0.size() ==
               static_cast<std::size_t>(dj1000::kNormalStageWidth) * dj1000::kNormalStageHeight);
        assert(stage.plane1.size() ==
               static_cast<std::size_t>(dj1000::kNormalStageWidth) * dj1000::kNormalStageHeight);
        assert(stage.plane2.size() ==
               static_cast<std::size_t>(dj1000::kNormalStageWidth) * dj1000::kNormalStageHeight);
    }

    {
        std::vector<std::uint8_t> source(dj1000::expected_source_seed_input_byte_count(true), 0);
        for (int row = 0; row < dj1000::kPreviewSourceSeedActiveRows; ++row) {
            const auto row_offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kPreviewSourceSeedInputStride);
            const auto value = static_cast<std::uint8_t>((row * 3) & 0xFF);
            for (int col = 0; col < dj1000::kPreviewSourceSeedActiveWidth; ++col) {
                source[row_offset + static_cast<std::size_t>(col)] = value;
            }
        }

        const auto stage = dj1000::build_nonlarge_stage_planes_from_source(source, true);
        assert(stage.plane0.size() ==
               static_cast<std::size_t>(dj1000::kPreviewStageWidth) * dj1000::kPreviewStageHeight);
        assert(stage.plane1.size() ==
               static_cast<std::size_t>(dj1000::kPreviewStageWidth) * dj1000::kPreviewStageHeight);
        assert(stage.plane2.size() ==
               static_cast<std::size_t>(dj1000::kPreviewStageWidth) * dj1000::kPreviewStageHeight);
    }

    std::cout << "test_nonlarge_source_pipeline passed\n";
    return 0;
}
