#include "dj1000/post_rgb_stage_40f0.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    {
        std::vector<std::uint8_t> plane0{10, 20, 30};
        std::vector<std::uint8_t> plane1{40, 50, 60};
        std::vector<std::uint8_t> plane2{70, 80, 90};
        dj1000::apply_post_rgb_stage_40f0(plane0, plane1, plane2, 3, 1, 3, 0);
        if (plane0 != std::vector<std::uint8_t>{10, 20, 30} ||
            plane1 != std::vector<std::uint8_t>{40, 50, 60} ||
            plane2 != std::vector<std::uint8_t>{70, 80, 90}) {
            throw std::runtime_error("level 3 should be a no-op");
        }
    }

    {
        std::vector<std::uint8_t> plane0{250, 5};
        std::vector<std::uint8_t> plane1{200, 10};
        std::vector<std::uint8_t> plane2{100, 20};
        dj1000::apply_post_rgb_stage_40f0(plane0, plane1, plane2, 2, 1, 0, 0);
        if (plane0 != std::vector<std::uint8_t>{255, 26} ||
            plane1 != std::vector<std::uint8_t>{221, 31} ||
            plane2 != std::vector<std::uint8_t>{121, 41}) {
            throw std::runtime_error("selector 0 level 0 adjustment mismatch");
        }
    }

    {
        std::vector<std::uint8_t> plane0{250, 5};
        std::vector<std::uint8_t> plane1{200, 10};
        std::vector<std::uint8_t> plane2{100, 20};
        dj1000::apply_post_rgb_stage_40f0(plane0, plane1, plane2, 2, 1, 0, 1);
        if (plane0 != std::vector<std::uint8_t>{229, 0} ||
            plane1 != std::vector<std::uint8_t>{179, 0} ||
            plane2 != std::vector<std::uint8_t>{79, 0}) {
            throw std::runtime_error("selector 1 level 0 adjustment mismatch");
        }
    }

    std::cout << "test_post_rgb_stage_40f0 passed\n";
    return 0;
}
