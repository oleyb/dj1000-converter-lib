#include "dj1000/post_geometry_prepare.hpp"

#include <stdexcept>
#include <string>

namespace dj1000 {

PostGeometryPlanes build_post_geometry_planes(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2,
    int width,
    int height
) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("post geometry dimensions must be positive");
    }

    const std::size_t sample_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (plane0.size() < sample_count) {
        throw std::runtime_error("plane0 is too small for post geometry prepare");
    }
    if (plane1.size() < sample_count) {
        throw std::runtime_error("plane1 is too small for post geometry prepare");
    }
    if (plane2.size() < sample_count) {
        throw std::runtime_error("plane2 is too small for post geometry prepare");
    }

    PostGeometryPlanes output{
        .center = std::vector<double>(sample_count, 0.0),
        .delta0 = std::vector<double>(sample_count, 0.0),
        .delta2 = std::vector<double>(sample_count, 0.0),
    };

    for (std::size_t index = 0; index < sample_count; ++index) {
        const double center = static_cast<double>(plane1[index]);
        output.center[index] = center;
        output.delta0[index] = static_cast<double>(plane0[index]) - center;
        output.delta2[index] = static_cast<double>(plane2[index]) - center;
    }

    return output;
}

}  // namespace dj1000
