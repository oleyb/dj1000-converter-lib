#include "dj1000/bmp.hpp"
#include "dj1000/dat_file.hpp"
#include "dj1000/index_view.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <vector>

int main() {
    std::vector<std::uint8_t> raw{std::vector<std::uint8_t>(dj1000::kRawBlockSize)};
    std::iota(raw.begin(), raw.end(), 0);

    const auto out = dj1000::trans_to_index_view(raw);
    assert(out.size() == dj1000::kIndexBytes);

    auto expect = [&](int row, int col, std::uint8_t value) {
        const auto actual = out[static_cast<std::size_t>(row * dj1000::kIndexWidth + col)];
        if (actual != value) {
            std::cerr
                << "unexpected byte at row " << row
                << " col " << col
                << ": expected " << static_cast<int>(value)
                << " got " << static_cast<int>(actual)
                << '\n';
            std::abort();
        }
    };

    expect(0, 0, raw[0x0000]);
    expect(0, 1, raw[0x0001]);
    expect(1, 0, raw[0x0200]);
    expect(1, 1, raw[0x0201]);
    expect(2, 0, raw[0x1000]);
    expect(2, 1, raw[0x1001]);
    expect(63, 126, raw[0x1F3F8]);
    expect(63, 127, raw[0x1F3F9]);

    const auto rgb = dj1000::grayscale_to_rgb(out);
    assert(rgb.size() == out.size() * 3);
    std::cout << "test_index_view passed\n";
    return 0;
}
