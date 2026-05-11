#include "dj1000/dat_file.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace dj1000 {

DatFile load_dat_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open DAT file: " + path.string());
    }

    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    if (!input.good() && !input.eof()) {
        throw std::runtime_error("Failed to read DAT file: " + path.string());
    }
    if (bytes.size() != kExpectedDatSize) {
        throw std::runtime_error("Unexpected DAT size for " + path.string());
    }

    return make_dat_file(bytes, path);
}

}  // namespace dj1000
