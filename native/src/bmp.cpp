#include "dj1000/bmp.hpp"

#include <array>
#include <fstream>
#include <stdexcept>

namespace dj1000 {

namespace {

void write_le16(std::ostream& out, std::uint16_t value) {
    const std::array<char, 2> bytes = {
        static_cast<char>(value & 0xFF),
        static_cast<char>((value >> 8) & 0xFF),
    };
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void write_le32(std::ostream& out, std::uint32_t value) {
    const std::array<char, 4> bytes = {
        static_cast<char>(value & 0xFF),
        static_cast<char>((value >> 8) & 0xFF),
        static_cast<char>((value >> 16) & 0xFF),
        static_cast<char>((value >> 24) & 0xFF),
    };
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

}  // namespace

std::vector<std::uint8_t> grayscale_to_rgb(std::span<const std::uint8_t> grayscale) {
    std::vector<std::uint8_t> rgb;
    rgb.reserve(grayscale.size() * 3);
    for (std::uint8_t value : grayscale) {
        rgb.push_back(value);
        rgb.push_back(value);
        rgb.push_back(value);
    }
    return rgb;
}

void write_bmp24(
    const std::filesystem::path& path,
    int width,
    int height,
    std::span<const std::uint8_t> rgb_pixels
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("BMP dimensions must be positive");
    }
    if (rgb_pixels.size() != static_cast<std::size_t>(width * height * 3)) {
        throw std::runtime_error("RGB buffer size does not match BMP dimensions");
    }

    const std::size_t row_bytes = static_cast<std::size_t>(width) * 3;
    const std::size_t stride = (row_bytes + 3) & ~std::size_t(3);
    const std::size_t image_size = stride * static_cast<std::size_t>(height);
    const std::uint32_t file_size = static_cast<std::uint32_t>(14 + 40 + image_size);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Could not create BMP: " + path.string());
    }

    out.put('B');
    out.put('M');
    write_le32(out, file_size);
    write_le16(out, 0);
    write_le16(out, 0);
    write_le32(out, 54);

    write_le32(out, 40);
    write_le32(out, static_cast<std::uint32_t>(width));
    write_le32(out, static_cast<std::uint32_t>(height));
    write_le16(out, 1);
    write_le16(out, 24);
    write_le32(out, 0);
    write_le32(out, 0);
    write_le32(out, 0);
    write_le32(out, 0);
    write_le32(out, 0);
    write_le32(out, 0);

    const std::array<char, 3> zero_pad = {0, 0, 0};
    for (int row = height - 1; row >= 0; --row) {
        const std::size_t src_row = static_cast<std::size_t>(row) * row_bytes;
        for (int col = 0; col < width; ++col) {
            const std::size_t src = src_row + (static_cast<std::size_t>(col) * 3);
            const char bgr[3] = {
                static_cast<char>(rgb_pixels[src + 2]),
                static_cast<char>(rgb_pixels[src + 1]),
                static_cast<char>(rgb_pixels[src]),
            };
            out.write(bgr, 3);
        }
        const std::size_t padding = stride - row_bytes;
        if (padding > 0) {
            out.write(zero_pad.data(), static_cast<std::streamsize>(padding));
        }
    }
}

void write_bmp24_bgr(
    const std::filesystem::path& path,
    int width,
    int height,
    std::span<const std::uint8_t> bgr_pixels
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("BMP dimensions must be positive");
    }
    if (bgr_pixels.size() != static_cast<std::size_t>(width * height * 3)) {
        throw std::runtime_error("BGR buffer size does not match BMP dimensions");
    }

    const std::size_t row_bytes = static_cast<std::size_t>(width) * 3;
    const std::size_t stride = (row_bytes + 3) & ~std::size_t(3);
    const std::size_t image_size = stride * static_cast<std::size_t>(height);
    const std::uint32_t file_size = static_cast<std::uint32_t>(14 + 40 + image_size);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Could not create BMP: " + path.string());
    }

    out.put('B');
    out.put('M');
    write_le32(out, file_size);
    write_le16(out, 0);
    write_le16(out, 0);
    write_le32(out, 54);

    write_le32(out, 40);
    write_le32(out, static_cast<std::uint32_t>(width));
    write_le32(out, static_cast<std::uint32_t>(height));
    write_le16(out, 1);
    write_le16(out, 24);
    write_le32(out, 0);
    write_le32(out, 0);
    write_le32(out, 0);
    write_le32(out, 0);
    write_le32(out, 0);
    write_le32(out, 0);

    const std::array<char, 3> zero_pad = {0, 0, 0};
    for (int row = height - 1; row >= 0; --row) {
        const std::size_t src_row = static_cast<std::size_t>(row) * row_bytes;
        out.write(
            reinterpret_cast<const char*>(bgr_pixels.data() + static_cast<std::ptrdiff_t>(src_row)),
            static_cast<std::streamsize>(row_bytes)
        );
        const std::size_t padding = stride - row_bytes;
        if (padding > 0) {
            out.write(zero_pad.data(), static_cast<std::streamsize>(padding));
        }
    }
}

}  // namespace dj1000
