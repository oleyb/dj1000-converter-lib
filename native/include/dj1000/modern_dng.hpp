#pragma once

#include "dj1000/converter.hpp"
#include "dj1000/dat_file.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace dj1000 {

[[nodiscard]] bool modern_dng_sdk_available() noexcept;
[[nodiscard]] std::vector<std::uint8_t> build_modern_dng_bytes(const DatFile& dat);
[[nodiscard]] std::vector<std::uint8_t> build_modern_dng_sdk_bytes(const DatFile& dat);
[[nodiscard]] std::vector<std::uint8_t> build_modern_linear_dng_bytes(
    const DatFile& dat,
    ExportSize size = ExportSize::Large
);

void write_modern_dng(
    const DatFile& dat,
    const std::filesystem::path& output_path
);
void write_modern_dng_sdk(
    const DatFile& dat,
    const std::filesystem::path& output_path
);

void write_modern_linear_dng(
    const DatFile& dat,
    const std::filesystem::path& output_path,
    ExportSize size = ExportSize::Large
);

}  // namespace dj1000
