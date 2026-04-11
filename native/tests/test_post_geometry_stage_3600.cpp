#include "dj1000/post_geometry_stage_3600.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

bool nearly_equal(double left, double right, double tolerance = 1.0e-12) {
    return std::abs(left - right) <= tolerance;
}

}  // namespace

int main() {
    {
        const int width = 5;
        const int height = 4;
        std::vector<double> plane(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 100.0);
        const auto output = dj1000::build_post_geometry_stage_3600_output(plane, width, height);
        for (double value : output) {
            if (!nearly_equal(value, 100.0)) {
                std::cerr << "stage 3600 flat-field preservation failed\n";
                return EXIT_FAILURE;
            }
        }
    }

    {
        const int width = 5;
        const int height = 4;
        std::vector<double> plane(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0.0);
        plane[(2 * width) + 2] = 100.0;
        const auto output = dj1000::build_post_geometry_stage_3600_output(plane, width, height);
        const std::vector<double> expected = {
            0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 11.11111111111111, 0.0, 0.0,
            0.0, 5.555555555555555, 66.66666666666666, 5.555555555555555, 0.0,
            0.0, 0.0, 22.22222222222222, 0.0, 0.0,
        };
        for (std::size_t index = 0; index < expected.size(); ++index) {
            if (!nearly_equal(output[index], expected[index])) {
                std::cerr << "stage 3600 interior impulse mismatch at index " << index << '\n';
                return EXIT_FAILURE;
            }
        }
    }

    {
        const int width = 5;
        const int height = 4;
        std::vector<double> plane(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0.0);
        plane[2] = 100.0;
        const auto output = dj1000::build_post_geometry_stage_3600_output(plane, width, height);
        const std::vector<double> expected = {
            0.0, 5.555555555555555, 77.77777777777777, 5.555555555555555, 0.0,
            0.0, 0.0, 11.11111111111111, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0,
        };
        for (std::size_t index = 0; index < expected.size(); ++index) {
            if (!nearly_equal(output[index], expected[index])) {
                std::cerr << "stage 3600 top-edge impulse mismatch at index " << index << '\n';
                return EXIT_FAILURE;
            }
        }
    }

    {
        const int width = 5;
        const int height = 4;
        std::vector<double> plane(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0.0);
        plane[(3 * width) + 2] = 100.0;
        const auto output = dj1000::build_post_geometry_stage_3600_output(plane, width, height);
        const std::vector<double> expected = {
            0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 11.11111111111111, 0.0, 0.0,
            0.0, 5.555555555555555, 66.66666666666666, 5.555555555555555, 0.0,
        };
        for (std::size_t index = 0; index < expected.size(); ++index) {
            if (!nearly_equal(output[index], expected[index])) {
                std::cerr << "stage 3600 bottom-edge impulse mismatch at index " << index << '\n';
                return EXIT_FAILURE;
            }
        }
    }

    std::cout << "test_post_geometry_stage_3600 passed\n";
    return EXIT_SUCCESS;
}
