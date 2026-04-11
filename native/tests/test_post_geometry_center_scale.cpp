#include "dj1000/post_geometry_center_scale.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool close_enough(double left, double right, double tolerance = 1.0e-12) {
    return std::fabs(left - right) <= tolerance;
}

}  // namespace

int main() {
    {
        std::vector<double> delta0 = {10.0, 5.0, -4.0, 2.0, 7.0};
        std::vector<double> center = {200.0, 127.0, 0.0, 300.0, 10.0};
        std::vector<double> delta2 = {-10.0, 6.0, 4.0, -2.0, 3.0};
        dj1000::apply_post_geometry_center_scale(delta0, center, delta2, 5, 1);

        assert(close_enough(delta0[0], 0.0));
        assert(close_enough(delta0[1], 5.0));
        assert(close_enough(delta0[2], 4.0));
        assert(close_enough(delta0[3], 0.0));
        assert(close_enough(delta0[4], 0.0));

        assert(close_enough(delta2[0], 0.0));
        assert(close_enough(delta2[1], 6.0));
        assert(close_enough(delta2[2], -4.0));
        assert(close_enough(delta2[3], 0.0));
        assert(close_enough(delta2[4], 0.0));

        assert(center == std::vector<double>({200.0, 127.0, 0.0, 44.0, 10.0}));
    }

    {
        std::vector<double> delta0 = {8.0, -8.0};
        std::vector<double> center = {15.0, 130.0};
        std::vector<double> delta2 = {-6.0, 6.0};
        dj1000::apply_post_geometry_center_scale(delta0, center, delta2, 2, 1);

        assert(close_enough(delta0[0], 4.0));
        assert(close_enough(delta2[0], -3.0));
        assert(close_enough(delta0[1], -8.0 * (20.0 / 23.0)));
        assert(close_enough(delta2[1], 6.0 * (20.0 / 23.0)));
        assert(center == std::vector<double>({15.0, 130.0}));
    }

    std::cout << "test_post_geometry_center_scale passed\n";
    return 0;
}
