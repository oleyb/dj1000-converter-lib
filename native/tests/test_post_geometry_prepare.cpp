#include "dj1000/post_geometry_prepare.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    const std::vector<std::uint8_t> plane0 = {10, 40, 100};
    const std::vector<std::uint8_t> plane1 = {20, 30, 90};
    const std::vector<std::uint8_t> plane2 = {5, 35, 120};

    const auto prepared = dj1000::build_post_geometry_planes(plane0, plane1, plane2, 3, 1);
    assert(prepared.center == std::vector<double>({20.0, 30.0, 90.0}));
    assert(prepared.delta0 == std::vector<double>({-10.0, 10.0, 10.0}));
    assert(prepared.delta2 == std::vector<double>({-15.0, 5.0, 30.0}));

    std::cout << "test_post_geometry_prepare passed\n";
    return 0;
}
