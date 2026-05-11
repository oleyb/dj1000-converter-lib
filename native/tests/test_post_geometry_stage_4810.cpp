#include "dj1000/post_geometry_stage_4810.hpp"

#include <iostream>
#include <vector>

int main() {
    std::vector<double> plane0{1.0, 2.0, 3.0, 4.0};
    std::vector<double> plane1{5.0, 6.0, 7.0, 8.0};
    std::vector<double> plane2{9.0, 10.0, 11.0, 12.0};

    const auto expected0 = plane0;
    const auto expected1 = plane1;
    const auto expected2 = plane2;

    dj1000::apply_post_geometry_stage_4810(plane0, plane1, plane2, 2, 2);

    if (plane0 != expected0 || plane1 != expected1 || plane2 != expected2) {
        std::cerr << "stage 4810 modified an identity test case\n";
        return 1;
    }

    std::cout << "test_post_geometry_stage_4810 passed\n";
    return 0;
}
