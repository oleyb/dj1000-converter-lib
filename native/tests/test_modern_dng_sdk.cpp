#include "dj1000/dat_file.hpp"
#include "dj1000/modern_dng.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <unordered_map>
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

std::uint32_t read_u32_le(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

struct ParsedIfdEntry {
    std::uint16_t type = 0;
    std::uint32_t count = 0;
    std::uint32_t value_or_offset = 0;
    std::size_t entry_offset = 0;
};

std::unordered_map<std::uint16_t, ParsedIfdEntry> parse_ifd_entries(
    const std::vector<std::uint8_t>& bytes,
    std::size_t ifd_offset
) {
    const std::uint16_t entry_count = read_u16_le(bytes, ifd_offset);
    std::unordered_map<std::uint16_t, ParsedIfdEntry> entries;
    for (std::uint16_t index = 0; index < entry_count; ++index) {
        const std::size_t entry_offset = ifd_offset + 2 + static_cast<std::size_t>(index) * 12;
        const std::uint16_t tag = read_u16_le(bytes, entry_offset);
        entries[tag] = ParsedIfdEntry{
            .type = read_u16_le(bytes, entry_offset + 2),
            .count = read_u32_le(bytes, entry_offset + 4),
            .value_or_offset = read_u32_le(bytes, entry_offset + 8),
            .entry_offset = entry_offset,
        };
    }
    return entries;
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

    const auto ifd0_entries = parse_ifd_entries(dng, read_u32_le(dng, 4));
    assert(ifd0_entries.contains(254));
    assert(ifd0_entries.contains(256));
    assert(ifd0_entries.contains(257));
    assert(ifd0_entries.contains(259));
    assert(ifd0_entries.contains(262));
    assert(ifd0_entries.contains(273));
    assert(ifd0_entries.contains(277));
    assert(ifd0_entries.contains(279));
    assert(ifd0_entries.contains(330));
    assert(ifd0_entries.at(254).value_or_offset == 1);
    assert(ifd0_entries.at(256).value_or_offset == 128);
    assert(ifd0_entries.at(257).value_or_offset == 96);
    assert((ifd0_entries.at(259).value_or_offset & 0xFFFFU) == 7);
    assert((ifd0_entries.at(262).value_or_offset & 0xFFFFU) == 6);
    assert((ifd0_entries.at(277).value_or_offset & 0xFFFFU) == 3);

    const std::uint32_t preview_offset = ifd0_entries.at(273).value_or_offset;
    assert(preview_offset + 2 < dng.size());
    assert(preview_offset + ifd0_entries.at(279).value_or_offset <= dng.size());
    assert(ifd0_entries.at(279).value_or_offset < 32U * 1024U);
    assert(dng[preview_offset] == 0xFF);
    assert(dng[preview_offset + 1] == 0xD8);

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
