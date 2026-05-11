#include "dj1000/dat_file.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace dj1000 {

namespace {
constexpr std::array<std::uint8_t, 4> kExpectedSignature = {0xC4, 0xB2, 0xE3, 0x22};
}

std::span<const std::uint8_t> DatFile::raw_payload() const {
    return std::span<const std::uint8_t>(bytes.data(), kRawBlockSize);
}

DatMetadata parse_dat_metadata(std::span<const std::uint8_t> bytes) {
    if (bytes.size() != kExpectedDatSize) {
        throw std::runtime_error("DAT file must be exactly 0x20000 bytes");
    }

    DatMetadata metadata;
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(kRawBlockSize), kTrailerSize, metadata.trailer.begin());
    metadata.signature_matches = std::equal(
        kExpectedSignature.begin(),
        kExpectedSignature.end(),
        metadata.trailer.begin()
    );
    metadata.mode_flag = metadata.trailer[8];
    metadata.mode_value = static_cast<int>(metadata.trailer[11]) + (10 * static_cast<int>(metadata.trailer[10]));
    return metadata;
}

DatFile make_dat_file(std::span<const std::uint8_t> bytes, std::filesystem::path path) {
    DatFile dat;
    dat.path = std::move(path);
    dat.bytes.assign(bytes.begin(), bytes.end());
    dat.metadata = parse_dat_metadata(dat.bytes);
    return dat;
}

std::string format_hex(std::span<const std::uint8_t> bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        out << std::setw(2) << static_cast<int>(bytes[index]);
        if (index + 1 != bytes.size()) {
            out << ' ';
        }
    }
    return out.str();
}

}  // namespace dj1000
