#include "dj1000/post_geometry_stage_3060.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool almost_equal(double left, double right, double tolerance = 1.0e-12) {
    return std::fabs(left - right) <= tolerance;
}

}  // namespace

int main() {
    {
        constexpr int width = 5;
        constexpr int height = 3;
        std::vector<std::int32_t> edge_response(width * height, 0);
        std::vector<double> center(width * height, 50.0);
        edge_response[(width * 1) + 2] = 200;

        dj1000::apply_post_geometry_stage_3060(
            edge_response,
            center,
            width,
            height,
            16,
            20,
            0.6,
            14
        );

        for (int index = 0; index < width * height; ++index) {
            const double expected = (index == ((width * 1) + 2)) ? 55.44 : 50.0;
            if (!almost_equal(center[static_cast<std::size_t>(index)], expected)) {
                std::cerr << "positive impulse mismatch at index " << index << '\n';
                return 1;
            }
        }
    }

    {
        constexpr int width = 5;
        constexpr int height = 3;
        std::vector<std::int32_t> edge_response(width * height, 0);
        std::vector<double> center(width * height, 50.0);
        edge_response[(width * 1) + 2] = -200;

        dj1000::apply_post_geometry_stage_3060(
            edge_response,
            center,
            width,
            height,
            16,
            20,
            0.6,
            14
        );

        for (int index = 0; index < width * height; ++index) {
            const double expected = (index == ((width * 1) + 2)) ? 47.696 : 50.0;
            if (!almost_equal(center[static_cast<std::size_t>(index)], expected)) {
                std::cerr << "negative impulse mismatch at index " << index << '\n';
                return 1;
            }
        }
    }

    {
        constexpr int width = 5;
        constexpr int height = 3;
        std::vector<std::int32_t> edge_response(width * height, 0);
        edge_response[(width * 1) + 2] = 200;

        std::vector<double> shadow_center(width * height, 4.0);
        dj1000::apply_post_geometry_stage_3060(
            edge_response,
            shadow_center,
            width,
            height,
            16,
            20,
            0.6,
            14
        );
        if (!almost_equal(shadow_center[(width * 1) + 2], 2.912)) {
            std::cerr << "shadow gating mismatch\n";
            return 1;
        }

        std::vector<double> highlight_center(width * height, 200.0);
        dj1000::apply_post_geometry_stage_3060(
            edge_response,
            highlight_center,
            width,
            height,
            16,
            20,
            0.6,
            14
        );
        if (!almost_equal(highlight_center[(width * 1) + 2], 203.14947368421053, 1.0e-9)) {
            std::cerr << "highlight gating mismatch\n";
            return 1;
        }
    }

    std::cout << "test_post_geometry_stage_3060 passed\n";
    return 0;
}
