#include "dj1000/pregeometry_pipeline.hpp"

#include "dj1000/pregeometry_normalize.hpp"
#include "dj1000/source_seed_stage.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

bool almost_equal(float left, float right) {
    return std::fabs(left - right) < 0.0002f;
}

}  // namespace

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

        const auto planes = dj1000::build_pregeometry_pipeline(source, false);
        const auto row10 = static_cast<std::size_t>(10) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
        assert(almost_equal(planes.plane1[row10 + 20], 10.0f));
        assert(almost_equal(planes.plane0[row10 + 20], 0.0f));
        assert(almost_equal(planes.plane2[row10 + 20], 0.0f));
    }

    {
        std::vector<std::uint8_t> source(dj1000::expected_source_seed_input_byte_count(true), 0);
        for (int row = 0; row < dj1000::kPreviewSourceSeedActiveRows; ++row) {
            const auto row_offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kPreviewSourceSeedInputStride);
            const auto value = static_cast<std::uint8_t>(row & 0xFF);
            for (int col = 0; col < dj1000::kPreviewSourceSeedActiveWidth; ++col) {
                source[row_offset + static_cast<std::size_t>(col)] = value;
            }
        }

        const auto planes = dj1000::build_pregeometry_pipeline(source, true);
        const auto row20 = static_cast<std::size_t>(20) * static_cast<std::size_t>(dj1000::kPreviewFloatPlaneStride);
        assert(almost_equal(planes.plane1[row20 + 20], 20.0f));
        assert(almost_equal(planes.plane0[row20 + 20], 0.0f));
        assert(almost_equal(planes.plane2[row20 + 20], 0.0f));
    }

    std::cout << "test_pregeometry_pipeline passed\n";
    return 0;
}
