#pragma once

#include "dj1000/dat_file.hpp"
#include "dj1000/post_geometry_rgb_convert.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace dj1000 {

struct SessionState;

enum class ExportSize {
    Small,
    Normal,
    Large,
};

enum class ConversionPipeline {
    Legacy,
    Modern,
};

struct SliderSettings {
    int red_balance = 100;
    int green_balance = 100;
    int blue_balance = 100;
    int contrast = 3;
    int brightness = 3;
    int vividness = 3;
    int sharpness = 3;
};

struct ConvertOptions {
    ExportSize size = ExportSize::Large;
    ConversionPipeline pipeline = ConversionPipeline::Legacy;
    SliderSettings sliders{};
    std::optional<double> source_gain;
};

struct ConvertDebugState {
    double source_gain = 1.0;
    double sharpness_scalar = 1.0;
    double stage3060_scalar = 0.0;
    int stage3060_param0 = 0;
    int stage3060_param1 = 0;
    int stage3060_threshold = 0;
    double post_rgb_scalar = 0.0;
};

struct ConvertedImage {
    int width = 0;
    int height = 0;
    RgbBytePlanes planes;

    [[nodiscard]] std::vector<std::uint8_t> interleaved_bgr() const;
    [[nodiscard]] std::vector<std::uint8_t> interleaved_rgb() const;
    [[nodiscard]] std::vector<std::uint8_t> interleaved_rgba() const;
};

class Session {
public:
    [[nodiscard]] static Session open(std::span<const std::uint8_t> dat_bytes);
    [[nodiscard]] static Session open(const DatFile& dat);

    [[nodiscard]] const DatFile& dat_file() const noexcept;
    [[nodiscard]] ConvertedImage render(
        const ConvertOptions& options = {},
        ConvertDebugState* debug_state = nullptr
    ) const;

private:
    explicit Session(std::shared_ptr<SessionState> state);

    std::shared_ptr<SessionState> state_;
};

[[nodiscard]] ConvertedImage convert_dat_bytes_to_bgr(
    std::span<const std::uint8_t> dat_bytes,
    const ConvertOptions& options = {},
    ConvertDebugState* debug_state = nullptr
);
[[nodiscard]] ConvertedImage convert_dat_to_bgr(
    const DatFile& dat,
    const ConvertOptions& options = {},
    ConvertDebugState* debug_state = nullptr
);

void write_bmp(
    const DatFile& dat,
    const std::filesystem::path& output_path,
    const ConvertOptions& options = {},
    ConvertDebugState* debug_state = nullptr
);

}  // namespace dj1000
