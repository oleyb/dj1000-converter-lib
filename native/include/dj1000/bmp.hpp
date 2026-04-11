#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace dj1000 {

[[nodiscard]] std::vector<std::uint8_t> grayscale_to_rgb(std::span<const std::uint8_t> grayscale);
void write_bmp24(
    const std::filesystem::path& path,
    int width,
    int height,
    std::span<const std::uint8_t> rgb_pixels
);
void write_bmp24_bgr(
    const std::filesystem::path& path,
    int width,
    int height,
    std::span<const std::uint8_t> bgr_pixels
);

}  // namespace dj1000
