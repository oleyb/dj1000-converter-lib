#include "dj1000/quantize_stage.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    assert(dj1000::quantize_stage_sample(-3.5f) == 0);
    assert(dj1000::quantize_stage_sample(-0.0f) == 0);
    assert(dj1000::quantize_stage_sample(0.0f) == 0);
    assert(dj1000::quantize_stage_sample(0.99f) == 0);
    assert(dj1000::quantize_stage_sample(1.0f) == 1);
    assert(dj1000::quantize_stage_sample(1.99f) == 1);
    assert(dj1000::quantize_stage_sample(254.99f) == 254);
    assert(dj1000::quantize_stage_sample(255.0f) == 255);
    assert(dj1000::quantize_stage_sample(300.0f) == 255);

    const std::vector<float> input = {-2.0f, 0.0f, 0.9f, 4.2f, 255.9f};
    const std::vector<std::uint8_t> expected = {0, 0, 0, 4, 255};
    assert(dj1000::quantize_stage_plane(input) == expected);

    std::cout << "test_quantize_stage passed\n";
    return 0;
}
