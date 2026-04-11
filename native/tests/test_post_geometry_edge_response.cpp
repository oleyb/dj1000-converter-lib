#include "dj1000/post_geometry_edge_response.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    {
        const std::vector<double> plane{
            1.0, 2.0, 3.0,
            4.0, 5.0, 6.0,
            7.0, 8.0, 9.0,
        };
        const auto output = dj1000::build_post_geometry_edge_response(plane, 3, 3);
        if (output.size() != 9) {
            return 1;
        }
        if (output[4] != 0) {
            return 1;
        }
    }

    {
        const std::vector<double> plane(25, 255.0);
        const auto output = dj1000::build_post_geometry_edge_response(plane, 5, 5);
        for (std::int32_t value : output) {
            if (value != 0) {
                return 1;
            }
        }
    }

    {
        const std::vector<double> plane{
            0.0, 17.0, 34.0, 51.0,
            31.0, 48.0, 65.0, 82.0,
            62.0, 79.0, 96.0, 113.0,
        };
        const auto output = dj1000::build_post_geometry_edge_response(plane, 4, 3);
        const std::vector<std::int32_t> expected{
            -984, -372, -372, 240,
            -612, 0, 0, 612,
            132, 744, 744, 1356,
        };
        if (output != expected) {
            return 1;
        }
    }

    std::cout << "test_post_geometry_edge_response passed\n";
    return 0;
}
