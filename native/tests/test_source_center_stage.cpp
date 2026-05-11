#include "dj1000/source_center_stage.hpp"

#include "dj1000/pregeometry_normalize.hpp"
#include "dj1000/source_seed_stage.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

bool almost_equal(float left, float right) {
    return std::fabs(left - right) < 0.0001f;
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

        const auto center = dj1000::build_source_center_plane(source, false);
        for (int row = 0; row < dj1000::kNormalSourceSeedActiveRows; ++row) {
            const auto row_offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
            for (int col = 0; col < dj1000::kNormalSourceSeedActiveWidth; ++col) {
                assert(almost_equal(center[row_offset + static_cast<std::size_t>(col)], static_cast<float>(row)));
            }
        }
    }

    {
        std::vector<std::uint8_t> source(dj1000::expected_source_seed_input_byte_count(false), 0);
        for (int row = 0; row < dj1000::kNormalSourceSeedActiveRows; ++row) {
            const auto row_offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kNormalSourceSeedInputStride);
            for (int col = 0; col < dj1000::kNormalSourceSeedActiveWidth; ++col) {
                source[row_offset + static_cast<std::size_t>(col)] =
                    static_cast<std::uint8_t>(col < 60 ? 64 : 192);
            }
        }

        const auto center = dj1000::build_source_center_plane(source, false);
        const auto row_offset = static_cast<std::size_t>(40) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
        assert(almost_equal(center[row_offset + 59], 70.4f));
        assert(almost_equal(center[row_offset + 60], 178.2857f));
    }

    {
        std::vector<std::uint8_t> source(dj1000::expected_source_seed_input_byte_count(false), 0);
        for (int row = 0; row < dj1000::kNormalSourceSeedActiveRows; ++row) {
            const auto row_offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kNormalSourceSeedInputStride);
            for (int col = 0; col < dj1000::kNormalSourceSeedActiveWidth; ++col) {
                source[row_offset + static_cast<std::size_t>(col)] =
                    static_cast<std::uint8_t>(((row + col) & 1) == 0 ? 64 : 192);
            }
        }

        const auto center = dj1000::build_source_center_plane(source, false);
        const auto row_offset = static_cast<std::size_t>(40) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
        for (int col = 1; col < 11; ++col) {
            assert(almost_equal(center[row_offset + static_cast<std::size_t>(col)], 128.0f));
        }
    }

    {
        std::vector<std::uint8_t> source(dj1000::expected_source_seed_input_byte_count(false), 0);
        for (int row = 0; row < dj1000::kNormalSourceSeedActiveRows; ++row) {
            const auto row_offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kNormalSourceSeedInputStride);
            const std::uint8_t value = static_cast<std::uint8_t>(row < 40 ? 200 : 255);
            for (int col = 0; col < dj1000::kNormalSourceSeedActiveWidth; ++col) {
                source[row_offset + static_cast<std::size_t>(col)] = value;
            }
        }

        const auto center = dj1000::build_source_center_plane(source, false);
        const auto row39 = static_cast<std::size_t>(39) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
        const auto row40 = static_cast<std::size_t>(40) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
        assert(almost_equal(center[row39 + 20], 227.5f));
        assert(almost_equal(center[row40 + 20], 255.0f));
    }

    std::cout << "test_source_center_stage passed\n";
    return 0;
}
