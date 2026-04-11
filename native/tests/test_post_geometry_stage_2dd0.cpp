#include "dj1000/post_geometry_stage_2dd0.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool nearly_equal(double left, double right, double tolerance = 1.0e-12) {
    return std::abs(left - right) <= tolerance;
}

}  // namespace

int main() {
    {
        const int width = 15;
        const int height = 9;
        std::vector<double> plane(width * height, 0.0);
        plane[(4 * width) + 7] = 100.0;
        const auto output = dj1000::build_post_geometry_stage_2dd0_plane_output(plane, width, height);
        const std::vector<double> expected{
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 7.142857142857142, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 7.142857142857142, 14.285714285714285, 14.285714285714285, 14.285714285714285, 14.285714285714285, 14.285714285714285, 7.142857142857142, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 7.142857142857142, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
        };
        if (output.size() != expected.size()) {
            return 1;
        }
        for (std::size_t index = 0; index < expected.size(); ++index) {
            if (!nearly_equal(output[index], expected[index])) {
                return 1;
            }
        }
    }

    {
        const int width = 7;
        const int height = 5;
        std::vector<double> plane;
        plane.reserve(width * height);
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                plane.push_back(static_cast<double>((row * 31) + (col * 17)));
            }
        }
        const auto output = dj1000::build_post_geometry_stage_2dd0_plane_output(plane, width, height);
        const std::vector<double> expected{
            24.071428571428573, 28.928571428571427, 38.642857142857146, 53.214285714285715, 67.78571428571429, 77.5, 82.35714285714286,
            52.857142857142854, 57.714285714285715, 67.42857142857143, 82.0, 96.57142857142857, 106.28571428571429, 111.14285714285714,
            83.85714285714286, 88.71428571428571, 98.42857142857143, 113.0, 127.57142857142857, 137.28571428571428, 142.14285714285714,
            114.85714285714286, 119.71428571428571, 129.42857142857142, 144.0, 158.57142857142858, 168.28571428571428, 173.14285714285714,
            141.42857142857142, 146.28571428571428, 156.0, 170.57142857142858, 185.14285714285714, 194.85714285714286, 199.71428571428572,
        };
        for (std::size_t index = 0; index < expected.size(); ++index) {
            if (!nearly_equal(output[index], expected[index])) {
                return 1;
            }
        }
    }

    std::cout << "test_post_geometry_stage_2dd0 passed\n";
    return 0;
}
