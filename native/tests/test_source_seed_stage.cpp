#include "dj1000/source_seed_stage.hpp"

#include "dj1000/pregeometry_normalize.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

bool almost_equal(float left, float right) {
    return std::fabs(left - right) < 0.0001f;
}

void assert_impulse_hit(const std::vector<float>& plane, int stride, int row, int col, float value) {
    const auto index = static_cast<std::size_t>(row) * static_cast<std::size_t>(stride) +
                       static_cast<std::size_t>(col);
    assert(almost_equal(plane[index], value));
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

        const auto planes = dj1000::build_source_seed_planes(source, false);
        assert(planes.plane0.size() == dj1000::expected_pregeometry_float_count(false));
        assert(planes.plane1.size() == dj1000::expected_pregeometry_float_count(false));
        assert(planes.plane2.size() == dj1000::expected_pregeometry_float_count(false));

        for (int row = 0; row < dj1000::kNormalSourceSeedActiveRows; ++row) {
            const auto row_offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
            for (int col = 0; col < dj1000::kNormalSourceSeedActiveWidth; ++col) {
                assert(almost_equal(
                    planes.plane1[row_offset + static_cast<std::size_t>(col)],
                    static_cast<float>(row)
                ));
            }
        }

        for (int row = dj1000::kNormalSourceSeedActiveRows; row < dj1000::kNormalFloatPlaneRows; ++row) {
            const auto row_offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
            for (int col = 0; col < dj1000::kNormalFloatPlaneStride; ++col) {
                assert(almost_equal(planes.plane1[row_offset + static_cast<std::size_t>(col)], 0.0f));
            }
        }
    }

    {
        std::vector<std::uint8_t> source(dj1000::expected_source_seed_input_byte_count(false), 0);
        source[static_cast<std::size_t>(40) * dj1000::kNormalSourceSeedInputStride + 60] = 0xFF;
        const auto planes = dj1000::build_source_seed_planes(source, false);
        assert_impulse_hit(planes.plane0, dj1000::kNormalFloatPlaneStride, 40, 59, 63.75f);
        assert_impulse_hit(planes.plane0, dj1000::kNormalFloatPlaneStride, 40, 60, 127.5f);
        assert_impulse_hit(planes.plane0, dj1000::kNormalFloatPlaneStride, 40, 61, 63.75f);
        assert_impulse_hit(planes.plane1, dj1000::kNormalFloatPlaneStride, 40, 59, 63.75f);
        assert_impulse_hit(planes.plane1, dj1000::kNormalFloatPlaneStride, 40, 60, 127.5f);
        assert_impulse_hit(planes.plane1, dj1000::kNormalFloatPlaneStride, 40, 61, 63.75f);
        assert_impulse_hit(planes.plane2, dj1000::kNormalFloatPlaneStride, 40, 60, 0.0f);
    }

    {
        std::vector<std::uint8_t> source(dj1000::expected_source_seed_input_byte_count(false), 0);
        source[static_cast<std::size_t>(41) * dj1000::kNormalSourceSeedInputStride + 61] = 0xFF;
        const auto planes = dj1000::build_source_seed_planes(source, false);
        assert_impulse_hit(planes.plane0, dj1000::kNormalFloatPlaneStride, 41, 61, 0.0f);
        assert_impulse_hit(planes.plane1, dj1000::kNormalFloatPlaneStride, 41, 60, 63.75f);
        assert_impulse_hit(planes.plane1, dj1000::kNormalFloatPlaneStride, 41, 61, 127.5f);
        assert_impulse_hit(planes.plane1, dj1000::kNormalFloatPlaneStride, 41, 62, 63.75f);
        assert_impulse_hit(planes.plane2, dj1000::kNormalFloatPlaneStride, 41, 60, 63.75f);
        assert_impulse_hit(planes.plane2, dj1000::kNormalFloatPlaneStride, 41, 61, 127.5f);
        assert_impulse_hit(planes.plane2, dj1000::kNormalFloatPlaneStride, 41, 62, 63.75f);
    }

    {
        std::vector<std::uint8_t> source(dj1000::expected_source_seed_input_byte_count(true), 0);
        source[static_cast<std::size_t>(40) * dj1000::kPreviewSourceSeedInputStride + 60] = 0xFF;
        const auto planes = dj1000::build_source_seed_planes(source, true);
        assert_impulse_hit(planes.plane0, dj1000::kPreviewFloatPlaneStride, 40, 59, 63.75f);
        assert_impulse_hit(planes.plane0, dj1000::kPreviewFloatPlaneStride, 40, 60, 127.5f);
        assert_impulse_hit(planes.plane0, dj1000::kPreviewFloatPlaneStride, 40, 61, 63.75f);
        assert_impulse_hit(planes.plane1, dj1000::kPreviewFloatPlaneStride, 40, 59, 63.75f);
        assert_impulse_hit(planes.plane1, dj1000::kPreviewFloatPlaneStride, 40, 60, 127.5f);
        assert_impulse_hit(planes.plane1, dj1000::kPreviewFloatPlaneStride, 40, 61, 63.75f);
        assert_impulse_hit(planes.plane2, dj1000::kPreviewFloatPlaneStride, 40, 60, 0.0f);
    }

    std::cout << "test_source_seed_stage passed\n";
    return 0;
}
