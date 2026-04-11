#include "dj1000/converter.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    dj1000::ConvertedImage image;
    image.width = 2;
    image.height = 1;
    image.planes.plane0 = {0x10, 0x20};
    image.planes.plane1 = {0x30, 0x40};
    image.planes.plane2 = {0x50, 0x60};

    const std::vector<std::uint8_t> bgr = image.interleaved_bgr();
    const std::vector<std::uint8_t> rgb = image.interleaved_rgb();
    const std::vector<std::uint8_t> rgba = image.interleaved_rgba();

    assert((bgr == std::vector<std::uint8_t>{0x10, 0x30, 0x50, 0x20, 0x40, 0x60}));
    assert((rgb == std::vector<std::uint8_t>{0x10, 0x30, 0x50, 0x20, 0x40, 0x60}));
    assert((rgba == std::vector<std::uint8_t>{0x10, 0x30, 0x50, 0xFF, 0x20, 0x40, 0x60, 0xFF}));

    std::cout << "test_converter passed\n";
    return 0;
}
