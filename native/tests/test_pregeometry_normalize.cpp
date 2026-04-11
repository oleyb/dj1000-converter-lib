#include "dj1000/pregeometry_normalize.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool almost_equal(float left, float right) {
    return std::fabs(left - right) < 0.0001f;
}

}  // namespace

int main() {
    {
        const auto count = dj1000::expected_pregeometry_float_count(false);
        std::vector<float> plane0(count, 60.0f);
        std::vector<float> plane1(count, 120.0f);
        std::vector<float> plane2(count, 240.0f);

        dj1000::normalize_pregeometry_planes(plane0, plane1, plane2, false);

        assert(almost_equal(plane0.front(), 120.0f));
        assert(almost_equal(plane1.front(), 120.0f));
        assert(almost_equal(plane2.front(), 120.0f));
        assert(almost_equal(plane0.back(), 120.0f));
        assert(almost_equal(plane1.back(), 120.0f));
        assert(almost_equal(plane2.back(), 120.0f));
    }

    {
        const auto count = dj1000::expected_pregeometry_float_count(true);
        std::vector<float> plane0(count, 120.0f);
        std::vector<float> plane1(count, 120.0f);
        std::vector<float> plane2(count, 120.0f);

        plane0[0] = -10.0f;
        plane1[0] = 300.0f;
        plane2[0] = -5.0f;

        dj1000::normalize_pregeometry_planes(plane0, plane1, plane2, true);

        assert(almost_equal(plane0[0], 0.0f));
        assert(almost_equal(plane1[0], 255.0f));
        assert(almost_equal(plane2[0], 0.0f));
        assert(almost_equal(plane0[1], 120.0f));
        assert(almost_equal(plane1[1], 120.0f));
        assert(almost_equal(plane2[1], 120.0f));
    }

    std::cout << "test_pregeometry_normalize passed\n";
    return 0;
}
