#include "dj1000/dat_file.hpp"
#include "dj1000/modern_dng.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

std::vector<std::uint8_t> make_sample_dat() {
    std::vector<std::uint8_t> bytes(dj1000::kExpectedDatSize, 0);
    for (std::size_t index = 0; index < dj1000::kRawBlockSize; ++index) {
        bytes[index] = static_cast<std::uint8_t>((index * 29U + 11U) & 0xFFU);
    }

    bytes[dj1000::kRawBlockSize + 0] = 0xC4;
    bytes[dj1000::kRawBlockSize + 1] = 0xB2;
    bytes[dj1000::kRawBlockSize + 2] = 0xE3;
    bytes[dj1000::kRawBlockSize + 3] = 0x22;
    bytes[dj1000::kRawBlockSize + 8] = 0xE6;
    bytes[dj1000::kRawBlockSize + 10] = 0x0E;
    bytes[dj1000::kRawBlockSize + 11] = 0x01;
    return bytes;
}

std::uint16_t read_u16_le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]) |
           (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

}  // namespace

int main() {
    assert(dj1000::modern_dng_sdk_available());

    const auto dat = dj1000::make_dat_file(make_sample_dat());
    const std::vector<std::uint8_t> dng = dj1000::build_modern_dng_sdk_bytes(dat);

    assert(dng.size() > 4096);
    assert(dng[0] == 'I');
    assert(dng[1] == 'I');
    assert(read_u16_le(dng, 2) == 42);

    const std::string camera_name = "Mitsubishi DJ-1000 / UMAX PhotoRun";
    const auto camera_it = std::search(
        dng.begin(),
        dng.end(),
        camera_name.begin(),
        camera_name.end()
    );
    assert(camera_it != dng.end());

    const std::array<std::uint8_t, 4> dng_version = {1, 4, 0, 0};
    const auto version_it = std::search(
        dng.begin(),
        dng.end(),
        dng_version.begin(),
        dng_version.end()
    );
    assert(version_it != dng.end());

    std::cout << "test_modern_dng_sdk passed\n";
    return 0;
}
