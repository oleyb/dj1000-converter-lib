#include "dj1000/post_geometry_stage_3890.hpp"

#include <bit>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool almost_equal(double left, double right, double tolerance = 1.0e-9) {
    return std::fabs(left - right) <= tolerance;
}

bool same_bits(double left, double right) {
    return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right);
}

}  // namespace

int main() {
    {
        auto coefficients = dj1000::get_post_geometry_stage_3890_coefficients(3);
        const double expected_scale = static_cast<double>(0.01f) * 120.0;
        const double expected_plane1_from_plane0 = static_cast<double>(0.01f) * -24.0;
        if (!almost_equal(coefficients.plane0_scale, expected_scale) ||
            !almost_equal(coefficients.plane1_from_plane0, expected_plane1_from_plane0) ||
            !almost_equal(coefficients.plane1_scale, expected_scale)) {
            std::cerr << "unexpected level-3 coefficients\n";
            return 1;
        }
    }

    {
        std::vector<double> plane0{100.0, 0.0, -50.0};
        std::vector<double> plane1{50.0, 25.0, 0.0};
        dj1000::apply_post_geometry_stage_3890(plane0, plane1, 3, 1, 3);

        const double scale = static_cast<double>(0.01f) * 120.0;
        const double plane1_from_plane0 = static_cast<double>(0.01f) * -24.0;
        const std::vector<double> expected_plane0{
            100.0 * scale,
            0.0 * scale,
            -50.0 * scale,
        };
        const std::vector<double> expected_plane1{
            (100.0 * plane1_from_plane0) + (50.0 * scale),
            (0.0 * plane1_from_plane0) + (25.0 * scale),
            (-50.0 * plane1_from_plane0) + (0.0 * scale),
        };
        for (std::size_t index = 0; index < plane0.size(); ++index) {
            if (!almost_equal(plane0[index], expected_plane0[index]) ||
                !almost_equal(plane1[index], expected_plane1[index])) {
                std::cerr << "stage 3890 transform mismatch at index " << index << '\n';
                return 1;
            }
        }
    }

    {
        std::vector<double> plane0{-9.785714285714285};
        std::vector<double> plane1{56.64285714285714};
        dj1000::apply_post_geometry_stage_3890(plane0, plane1, 1, 1, 6);

        constexpr double expected_plane0 = -23.485713760767663;
        constexpr double expected_plane1 = 140.63999685645103;
        if (!same_bits(plane0[0], expected_plane0) || !same_bits(plane1[0], expected_plane1)) {
            std::cerr << "stage 3890 level-6 regression mismatch\n";
            return 1;
        }
    }

    std::cout << "test_post_geometry_stage_3890 passed\n";
    return 0;
}
