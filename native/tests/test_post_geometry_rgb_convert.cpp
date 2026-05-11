#include "dj1000/post_geometry_rgb_convert.hpp"

#include <iostream>
#include <vector>

int main() {
    {
        const std::vector<double> plane0{200.0};
        const std::vector<double> plane1{20.0};
        const std::vector<double> plane2{10.0};
        const auto output = dj1000::convert_post_geometry_rgb_planes(plane0, plane1, plane2, 1, 1);
        if (output.plane0[0] != 220) {
            return 1;
        }
        if (output.plane2[0] != 30) {
            return 1;
        }
        if (output.plane1[0] != 0) {
            return 1;
        }
    }

    {
        const std::vector<double> plane0{-100.0};
        const std::vector<double> plane1{10.0};
        const std::vector<double> plane2{300.0};
        const auto output = dj1000::convert_post_geometry_rgb_planes(plane0, plane1, plane2, 1, 1);
        if (output.plane0[0] != 0) {
            return 1;
        }
        if (output.plane2[0] != 255) {
            return 1;
        }
    }

    std::cout << "test_post_geometry_rgb_convert passed\n";
    return 0;
}
