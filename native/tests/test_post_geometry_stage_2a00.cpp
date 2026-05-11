#include "dj1000/post_geometry_stage_2a00.hpp"

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
        const int width = 7;
        const int height = 5;
        std::vector<double> plane(width * height, 0.0);
        plane[(2 * width) + 3] = 100.0;
        const auto output = dj1000::build_post_geometry_stage_2a00_output(plane, width, height);
        const std::vector<double> expected{
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 25.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 12.5, 25.0, 12.5, 0.0, 0.0,
            0.0, 0.0, 0.0, 25.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
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
        const auto output = dj1000::build_post_geometry_stage_2a00_output(plane, width, height);
        const std::vector<double> expected{
            19.75, 32.5, 49.5, 66.5, 83.5, 100.5, 113.25,
            35.25, 48.0, 65.0, 82.0, 99.0, 116.0, 128.75,
            66.25, 79.0, 96.0, 113.0, 130.0, 147.0, 159.75,
            97.25, 110.0, 127.0, 144.0, 161.0, 178.0, 190.75,
            112.75, 125.5, 142.5, 159.5, 176.5, 193.5, 206.25,
        };
        for (std::size_t index = 0; index < expected.size(); ++index) {
            if (!nearly_equal(output[index], expected[index])) {
                return 1;
            }
        }
    }

    std::cout << "test_post_geometry_stage_2a00 passed\n";
    return 0;
}
