#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace dj1000 {

inline constexpr std::size_t kExpectedDatSize = 0x20000;
inline constexpr std::size_t kRawBlockSize = 0x1F600;
inline constexpr std::size_t kTrailerSize = 13;

struct DatMetadata {
    std::array<std::uint8_t, kTrailerSize> trailer{};
    bool signature_matches = false;
    std::uint8_t mode_flag = 0;
    int mode_value = 0;
};

struct DatFile {
    std::filesystem::path path;
    std::vector<std::uint8_t> bytes;
    DatMetadata metadata;

    [[nodiscard]] std::span<const std::uint8_t> raw_payload() const;
};

[[nodiscard]] DatFile load_dat_file(const std::filesystem::path& path);
[[nodiscard]] DatMetadata parse_dat_metadata(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::string format_hex(std::span<const std::uint8_t> bytes);

}  // namespace dj1000
