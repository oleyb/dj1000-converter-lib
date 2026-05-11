#include "dj1000/post_rgb_stage_3b00.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    {
        const auto lut = dj1000::build_post_rgb_stage_3b00_lut(30.0);
        if (lut.size() != 256) {
            std::cerr << "unexpected LUT size\n";
            return 1;
        }
        if (lut[0] != 30 || lut[50] != 150 || lut[100] != 204 || lut[127] != 229 || lut[255] != 255) {
            std::cerr << "tone LUT breakpoint mismatch\n";
            return 1;
        }
    }

    {
        std::vector<std::uint8_t> plane0{0, 50, 100, 127, 200, 255};
        std::vector<std::uint8_t> plane1 = plane0;
        std::vector<std::uint8_t> plane2 = plane0;

        dj1000::apply_post_rgb_stage_3b00(plane0, plane1, plane2, 6, 1, 3, 30.0);

        const std::vector<std::uint8_t> expected{30, 150, 204, 229, 244, 255};
        if (plane0 != expected || plane1 != expected || plane2 != expected) {
            std::cerr << "default level stage output mismatch\n";
            return 1;
        }
    }

    {
        std::vector<std::uint8_t> plane0{10, 20, 40, 50, 60, 80, 100, 127, 150};
        std::vector<std::uint8_t> plane1 = plane0;
        std::vector<std::uint8_t> plane2 = plane0;

        dj1000::apply_post_rgb_stage_3b00(plane0, plane1, plane2, 9, 1, 6, 30.0);

        const std::vector<std::uint8_t> expected{56, 85, 140, 157, 170, 195, 217, 233, 238};
        if (plane0 != expected || plane1 != expected || plane2 != expected) {
            std::cerr << "level-6 stage output mismatch\n";
            return 1;
        }
    }

    std::cout << "test_post_rgb_stage_3b00 passed\n";
    return 0;
}
