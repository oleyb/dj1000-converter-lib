#include "dj1000/bright_vertical_gate.hpp"

#include "dj1000/pregeometry_normalize.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool almost_equal(float left, float right) {
    return std::fabs(left - right) < 0.0001f;
}

void fill_vertical_step(
    std::vector<float>& plane,
    int stride,
    int active_width,
    int active_rows,
    int split_row,
    float value_a,
    float value_b
) {
    for (int row = 0; row < active_rows; ++row) {
        const auto row_offset = static_cast<std::size_t>(row) * static_cast<std::size_t>(stride);
        const float value = row < split_row ? value_a : value_b;
        for (int col = 0; col < active_width; ++col) {
            plane[row_offset + static_cast<std::size_t>(col)] = value;
        }
    }
}

}  // namespace

int main() {
    {
        std::vector<float> plane(dj1000::expected_pregeometry_float_count(false), 200.0f);
        for (int row = dj1000::kNormalFloatPlaneActiveRows; row < dj1000::kNormalFloatPlaneRows; ++row) {
            const auto row_offset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(dj1000::kNormalFloatPlaneStride);
            for (int col = 0; col < dj1000::kNormalFloatPlaneStride; ++col) {
                plane[row_offset + static_cast<std::size_t>(col)] = 0.0f;
            }
        }

        const auto output = dj1000::apply_bright_vertical_gate(plane, false);
        assert(almost_equal(output[0], 200.0f));
        assert(almost_equal(output[39 * dj1000::kNormalFloatPlaneStride + 20], 200.0f));
        assert(almost_equal(output[244 * dj1000::kNormalFloatPlaneStride], 0.0f));
    }

    {
        std::vector<float> plane(dj1000::expected_pregeometry_float_count(false), 0.0f);
        fill_vertical_step(
            plane,
            dj1000::kNormalFloatPlaneStride,
            dj1000::kNormalFloatPlaneActiveWidth,
            dj1000::kNormalFloatPlaneActiveRows,
            40,
            200.0f,
            255.0f
        );

        const auto output = dj1000::apply_bright_vertical_gate(plane, false);
        assert(almost_equal(output[39 * dj1000::kNormalFloatPlaneStride + 20], 227.5f));
        assert(almost_equal(output[40 * dj1000::kNormalFloatPlaneStride + 20], 255.0f));
    }

    {
        std::vector<float> plane(dj1000::expected_pregeometry_float_count(false), 0.0f);
        fill_vertical_step(
            plane,
            dj1000::kNormalFloatPlaneStride,
            dj1000::kNormalFloatPlaneActiveWidth,
            dj1000::kNormalFloatPlaneActiveRows,
            40,
            100.0f,
            255.0f
        );

        const auto output = dj1000::apply_bright_vertical_gate(plane, false);
        assert(almost_equal(output[39 * dj1000::kNormalFloatPlaneStride + 20], 100.0f));
        assert(almost_equal(output[40 * dj1000::kNormalFloatPlaneStride + 20], 255.0f));
    }

    {
        std::vector<float> plane(dj1000::expected_pregeometry_float_count(true), 0.0f);
        fill_vertical_step(
            plane,
            dj1000::kPreviewFloatPlaneStride,
            dj1000::kPreviewFloatPlaneActiveWidth,
            dj1000::kPreviewFloatPlaneActiveRows,
            40,
            200.0f,
            255.0f
        );

        const auto output = dj1000::apply_bright_vertical_gate(plane, true);
        assert(almost_equal(output[39 * dj1000::kPreviewFloatPlaneStride + 20], 227.5f));
        assert(almost_equal(output[40 * dj1000::kPreviewFloatPlaneStride + 20], 255.0f));
    }

    std::cout << "test_bright_vertical_gate passed\n";
    return 0;
}
