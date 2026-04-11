#include "dj1000/post_geometry_dual_scale.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool nearly_equal(double left, double right) {
    return std::fabs(left - right) < 1e-9;
}

}  // namespace

int main() {
    {
        std::vector<double> plane0{10.0, -20.0, 200.0};
        std::vector<double> plane1{-10.0, 20.0, -200.0};
        dj1000::apply_post_geometry_dual_scale(plane0, plane1, 3, 1, 1.25);
        if (!nearly_equal(plane0[0], 7.5) || !nearly_equal(plane0[1], -15.0) ||
            !nearly_equal(plane0[2], 150.0)) {
            return 1;
        }
        if (!nearly_equal(plane1[0], -7.5) || !nearly_equal(plane1[1], 15.0) ||
            !nearly_equal(plane1[2], -150.0)) {
            return 1;
        }
    }

    {
        std::vector<double> plane0{300.0};
        std::vector<double> plane1{-300.0};
        dj1000::apply_post_geometry_dual_scale(plane0, plane1, 1, 1, 0.5);
        if (!nearly_equal(plane0[0], 255.0) || !nearly_equal(plane1[0], -255.0)) {
            return 1;
        }
    }

    std::cout << "test_post_geometry_dual_scale passed\n";
    return 0;
}
