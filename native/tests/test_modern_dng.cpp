#include "dj1000/dat_file.hpp"
#include "dj1000/modern_dng.hpp"
#include "dj1000/modern_raw_frame.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace {

std::vector<std::uint8_t> make_sample_dat() {
    std::vector<std::uint8_t> bytes(dj1000::kExpectedDatSize, 0);
    for (std::size_t index = 0; index < dj1000::kRawBlockSize; ++index) {
        bytes[index] = static_cast<std::uint8_t>((index * 17U + 23U) & 0xFFU);
    }

    bytes[dj1000::kRawBlockSize + 0] = 0xC4;
    bytes[dj1000::kRawBlockSize + 1] = 0xB2;
    bytes[dj1000::kRawBlockSize + 2] = 0xE3;
    bytes[dj1000::kRawBlockSize + 3] = 0x22;
    bytes[dj1000::kRawBlockSize + 8] = 0;
    bytes[dj1000::kRawBlockSize + 10] = 0;
    bytes[dj1000::kRawBlockSize + 11] = 1;
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

}  // namespace

int main() {
    const auto dat = dj1000::make_dat_file(make_sample_dat());
    const std::vector<std::uint8_t> dng = dj1000::build_modern_dng_bytes(dat);
    const std::vector<std::uint8_t> linear_dng =
        dj1000::build_modern_linear_dng_bytes(dat, dj1000::ExportSize::Large);

    assert(dng.size() > static_cast<std::size_t>(dj1000::kModernRawFullWidth * dj1000::kModernRawFullHeight * 2));
    assert(dng[0] == 'I');
    assert(dng[1] == 'I');
    assert(read_u16_le(dng, 2) == 42);
    assert(read_u32_le(dng, 4) == 8);

    const std::size_t ifd0_offset = read_u32_le(dng, 4);
    const std::uint16_t ifd0_entry_count = read_u16_le(dng, ifd0_offset);
    assert(ifd0_entry_count >= 5);

    std::unordered_map<std::uint16_t, ParsedIfdEntry> ifd0_entries;
    for (std::uint16_t index = 0; index < ifd0_entry_count; ++index) {
        const std::size_t entry_offset = ifd0_offset + 2 + static_cast<std::size_t>(index) * 12;
        const std::uint16_t tag = read_u16_le(dng, entry_offset);
        ifd0_entries[tag] = ParsedIfdEntry{
            .type = read_u16_le(dng, entry_offset + 2),
            .count = read_u32_le(dng, entry_offset + 4),
            .value_or_offset = read_u32_le(dng, entry_offset + 8),
            .entry_offset = entry_offset,
        };
    }

    assert(ifd0_entries.contains(50706));
    assert(ifd0_entries.size() >= 20);

    std::unordered_map<std::uint16_t, ParsedIfdEntry> entries;
    for (std::uint16_t index = 0; index < ifd0_entry_count; ++index) {
        const std::size_t entry_offset = ifd0_offset + 2 + static_cast<std::size_t>(index) * 12;
        const std::uint16_t tag = read_u16_le(dng, entry_offset);
        entries[tag] = ParsedIfdEntry{
            .type = read_u16_le(dng, entry_offset + 2),
            .count = read_u32_le(dng, entry_offset + 4),
            .value_or_offset = read_u32_le(dng, entry_offset + 8),
            .entry_offset = entry_offset,
        };
    }

    assert(entries.contains(254));
    assert(entries.contains(256));
    assert(entries.contains(257));
    assert(entries.contains(262));
    assert(entries.contains(273));
    assert(entries.contains(284));
    assert(entries.contains(279));
    assert(entries.contains(33421));
    assert(entries.contains(33422));
    assert(entries.contains(50710));
    assert(entries.contains(50718));
    assert(entries.contains(50721));
    assert(entries.contains(50725));
    assert(entries.contains(50727));
    assert(entries.contains(50728));
    assert(entries.contains(50829));

    assert((entries[254].value_or_offset) == 0);
    assert(entries[256].value_or_offset == dj1000::kModernRawFullWidth);
    assert(entries[257].value_or_offset == dj1000::kModernRawFullHeight);
    assert((entries[262].value_or_offset & 0xFFFFU) == 32803);
    assert((entries[258].value_or_offset & 0xFFFFU) == 16);
    assert((entries[277].value_or_offset & 0xFFFFU) == 1);
    assert((entries[284].value_or_offset & 0xFFFFU) == 1);
    assert(entries[279].value_or_offset == static_cast<std::uint32_t>(
        dj1000::kModernRawFullWidth * dj1000::kModernRawFullHeight * 2
    ));

    const std::size_t dng_version_offset =
        entries[50706].count <= 4 ? entries[50706].entry_offset + 8 : entries[50706].value_or_offset;
    assert(dng[dng_version_offset + 0] == 1);
    assert(dng[dng_version_offset + 1] == 1);
    assert(dng[dng_version_offset + 2] == 0);
    assert(dng[dng_version_offset + 3] == 0);

    const std::size_t cfa_pattern_offset =
        entries[33422].count <= 4 ? entries[33422].entry_offset + 8 : entries[33422].value_or_offset;
    assert(dng[cfa_pattern_offset + 0] == 0);
    assert(dng[cfa_pattern_offset + 1] == 1);
    assert(dng[cfa_pattern_offset + 2] == 2);
    assert(dng[cfa_pattern_offset + 3] == 3);

    const std::size_t cfa_plane_color_offset =
        entries[50710].count <= 4 ? entries[50710].entry_offset + 8 : entries[50710].value_or_offset;
    assert(dng[cfa_plane_color_offset + 0] == 3);
    assert(dng[cfa_plane_color_offset + 1] == 5);
    assert(dng[cfa_plane_color_offset + 2] == 1);
    assert(dng[cfa_plane_color_offset + 3] == 6);

    assert(entries[50721].count == 12);
    assert(entries[50725].count == 12);
    assert(entries[50727].count == 4);
    assert(entries[50728].count == 4);

    const std::size_t active_area_offset = entries[50829].value_or_offset;
    assert(read_u32_le(dng, active_area_offset + 0) == 0);
    assert(read_u32_le(dng, active_area_offset + 4) == 0);
    assert(read_u32_le(dng, active_area_offset + 8) == dj1000::kModernRawActiveHeight);
    assert(read_u32_le(dng, active_area_offset + 12) == dj1000::kModernRawActiveWidth);

    const std::size_t default_scale_offset = entries[50718].value_or_offset;
    assert(read_u32_le(dng, default_scale_offset + 0) == 1);
    assert(read_u32_le(dng, default_scale_offset + 4) == 1);
    assert(read_u32_le(dng, default_scale_offset + 8) == static_cast<std::uint32_t>(dj1000::kModernRawActiveWidth * 3));
    assert(read_u32_le(dng, default_scale_offset + 12) == static_cast<std::uint32_t>(dj1000::kModernRawActiveHeight * 4));

    const std::size_t default_crop_origin_offset = entries[50719].value_or_offset;
    assert(read_u32_le(dng, default_crop_origin_offset + 0) == 0);
    assert(read_u32_le(dng, default_crop_origin_offset + 4) == 1);
    assert(read_u32_le(dng, default_crop_origin_offset + 8) == 0);
    assert(read_u32_le(dng, default_crop_origin_offset + 12) == 1);

    const std::size_t default_crop_size_offset = entries[50720].value_or_offset;
    assert(read_u32_le(dng, default_crop_size_offset + 0) == dj1000::kModernRawActiveWidth);
    assert(read_u32_le(dng, default_crop_size_offset + 4) == 1);
    assert(read_u32_le(dng, default_crop_size_offset + 8) == dj1000::kModernRawActiveHeight);
    assert(read_u32_le(dng, default_crop_size_offset + 12) == 1);

    const std::uint32_t strip_offset = entries[273].value_or_offset;
    assert(strip_offset < dng.size());
    assert(strip_offset + entries[279].value_or_offset <= dng.size());
    const std::uint16_t first_sample = read_u16_le(dng, strip_offset);
    assert(first_sample > 0);

    const std::size_t linear_ifd0_offset = read_u32_le(linear_dng, 4);
    const std::uint16_t linear_ifd0_entry_count = read_u16_le(linear_dng, linear_ifd0_offset);
    assert(linear_ifd0_entry_count >= 10);

    std::unordered_map<std::uint16_t, ParsedIfdEntry> linear_entries;
    for (std::uint16_t index = 0; index < linear_ifd0_entry_count; ++index) {
        const std::size_t entry_offset = linear_ifd0_offset + 2 + static_cast<std::size_t>(index) * 12;
        const std::uint16_t tag = read_u16_le(linear_dng, entry_offset);
        linear_entries[tag] = ParsedIfdEntry{
            .type = read_u16_le(linear_dng, entry_offset + 2),
            .count = read_u32_le(linear_dng, entry_offset + 4),
            .value_or_offset = read_u32_le(linear_dng, entry_offset + 8),
            .entry_offset = entry_offset,
        };
    }

    assert(linear_entries.contains(256));
    assert(linear_entries.contains(257));
    assert(linear_entries.contains(258));
    assert(linear_entries.contains(262));
    assert(linear_entries.contains(277));
    assert(linear_entries.contains(279));
    assert(linear_entries.contains(284));
    assert(linear_entries.contains(50706));
    assert(linear_entries[256].value_or_offset == 504);
    assert(linear_entries[257].value_or_offset == 378);
    assert((linear_entries[262].value_or_offset & 0xFFFFU) == 2);
    assert((linear_entries[277].value_or_offset & 0xFFFFU) == 3);
    assert((linear_entries[284].value_or_offset & 0xFFFFU) == 1);
    assert(linear_entries[279].value_or_offset == static_cast<std::uint32_t>(504 * 378 * 6));

    const std::size_t linear_bits_offset = linear_entries[258].value_or_offset;
    assert(read_u16_le(linear_dng, linear_bits_offset + 0) == 16);
    assert(read_u16_le(linear_dng, linear_bits_offset + 2) == 16);
    assert(read_u16_le(linear_dng, linear_bits_offset + 4) == 16);

    const std::size_t linear_strip_offset = linear_entries[273].value_or_offset;
    assert(linear_strip_offset < linear_dng.size());
    assert(linear_strip_offset + linear_entries[279].value_or_offset <= linear_dng.size());
    const std::uint16_t first_linear_sample = read_u16_le(linear_dng, linear_strip_offset);
    assert(first_linear_sample > 0);

    std::cout << "test_modern_dng passed\n";
    return 0;
}
