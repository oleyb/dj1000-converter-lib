#include "dj1000/source_stage.hpp"

#include "dj1000/pregeometry_normalize.hpp"

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

        const auto planes = dj1000::build_source_stage_planes(source, false);
        const auto row10 = static_cast<std::size_t>(10) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
        assert(almost_equal(planes.plane1[row10 + 20], 10.0f));
        assert(almost_equal(planes.plane0[row10 + 20], 0.0f));
        assert(almost_equal(planes.plane2[row10 + 20], 0.0f));
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

        const auto planes = dj1000::build_source_stage_planes(source, false);
        const auto row40 = static_cast<std::size_t>(40) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
        for (int col = 1; col < 11; ++col) {
            assert(almost_equal(planes.plane1[row40 + static_cast<std::size_t>(col)], 128.0f));
            assert(almost_equal(planes.plane0[row40 + static_cast<std::size_t>(col)], 0.0f));
            assert(almost_equal(planes.plane2[row40 + static_cast<std::size_t>(col)], 0.0f));
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

        const auto planes = dj1000::build_source_stage_planes(source, false);
        const auto row40 = static_cast<std::size_t>(40) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
        assert(almost_equal(planes.plane0[row40 + 59], 6.4f));
        assert(almost_equal(planes.plane0[row40 + 60], 13.7143f));
        assert(almost_equal(planes.plane1[row40 + 59], 70.4f));
        assert(almost_equal(planes.plane1[row40 + 60], 178.2857f));
        assert(almost_equal(planes.plane2[row40 + 59], 0.0f));
        assert(almost_equal(planes.plane2[row40 + 60], 0.0f));
    }

    {
        std::vector<std::uint8_t> source(dj1000::expected_source_seed_input_byte_count(false), 0);
        for (int row = 0; row < dj1000::kNormalSourceSeedActiveRows; ++row) {
            const auto row_offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kNormalSourceSeedInputStride);
            for (int col = 0; col < dj1000::kNormalSourceSeedActiveWidth; ++col) {
                source[row_offset + static_cast<std::size_t>(col)] =
                    static_cast<std::uint8_t>(col < 60 ? 192 : 64);
            }
        }

        const auto planes = dj1000::build_source_stage_planes(source, false);
        const auto row40 = static_cast<std::size_t>(40) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
        assert(almost_equal(planes.plane0[row40 + 59], 0.0f));
        assert(almost_equal(planes.plane0[row40 + 60], 0.0f));
        assert(almost_equal(planes.plane2[row40 + 59], 13.7143f));
        assert(almost_equal(planes.plane2[row40 + 60], 6.4f));
    }

    std::cout << "test_source_stage passed\n";
    return 0;
}
