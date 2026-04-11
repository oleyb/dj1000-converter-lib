#include "dj1000/converter.hpp"

#include "dj1000/bmp.hpp"

namespace dj1000 {

void write_bmp(
    const DatFile& dat,
    const std::filesystem::path& output_path,
    const ConvertOptions& options,
    ConvertDebugState* debug_state
) {
    const auto image = convert_dat_to_bgr(dat, options, debug_state);
    const auto pixels = image.interleaved_bgr();
    write_bmp24_bgr(output_path, image.width, image.height, pixels);
}

}  // namespace dj1000
