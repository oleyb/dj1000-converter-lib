#include "dj1000/index_view.hpp"

#include "dj1000/dat_file.hpp"

#include <stdexcept>

namespace dj1000 {

std::vector<std::uint8_t> trans_to_index_view(std::span<const std::uint8_t> raw_block) {
    if (raw_block.size() < kRawBlockSize) {
        throw std::runtime_error("raw block too small for TransToIndexView");
    }

    std::vector<std::uint8_t> out(kIndexBytes, 0);
    std::size_t src_block_offset = 0;
    int dst_row = 0;

    for (int block = 0; block < 0x20; ++block) {
        const std::size_t src_a = src_block_offset;
        const std::size_t src_b = src_block_offset + 0x200;
        const std::size_t dst_a = static_cast<std::size_t>(dst_row) * kIndexWidth;
        const std::size_t dst_b = static_cast<std::size_t>(dst_row + 1) * kIndexWidth;

        for (int group = 0; group < 0x40; ++group) {
            const std::size_t src_pair = static_cast<std::size_t>(group) * 8;
            const std::size_t dst_pair = static_cast<std::size_t>(group) * 2;
            out[dst_a + dst_pair] = raw_block[src_a + src_pair];
            out[dst_a + dst_pair + 1] = raw_block[src_a + src_pair + 1];
            out[dst_b + dst_pair] = raw_block[src_b + src_pair];
            out[dst_b + dst_pair + 1] = raw_block[src_b + src_pair + 1];
        }

        src_block_offset += 0x1000;
        dst_row += 2;
    }

    return out;
}

}  // namespace dj1000
