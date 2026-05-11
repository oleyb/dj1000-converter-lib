#include "dj1000/post_rgb_stage_42a0.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    {
        std::vector<std::uint8_t> plane0{10, 20, 30};
        std::vector<std::uint8_t> plane1{40, 50, 60};
        std::vector<std::uint8_t> plane2{70, 80, 90};
        dj1000::apply_post_rgb_stage_42a0(plane0, plane1, plane2, 3, 1, 100, 100, 100);
        if (plane0 != std::vector<std::uint8_t>{10, 20, 30} ||
            plane1 != std::vector<std::uint8_t>{40, 50, 60} ||
            plane2 != std::vector<std::uint8_t>{70, 80, 90}) {
            throw std::runtime_error("100% scaling should be a no-op");
        }
    }

    {
        std::vector<std::uint8_t> plane0{200, 255};
        std::vector<std::uint8_t> plane1{10, 20};
        std::vector<std::uint8_t> plane2{100, 200};
        dj1000::apply_post_rgb_stage_42a0(plane0, plane1, plane2, 2, 1, 150, 50, 125);
        if (plane0 != std::vector<std::uint8_t>{255, 255} ||
            plane1 != std::vector<std::uint8_t>{5, 10} ||
            plane2 != std::vector<std::uint8_t>{125, 250}) {
            throw std::runtime_error("scaled post-rgb outputs mismatch");
        }
    }

    std::cout << "test_post_rgb_stage_42a0 passed\n";
    return 0;
}
