#include "dj1000/geometry_copy.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

void validate_minimum_size(std::span<const std::uint8_t> plane, std::size_t required, const char* label) {
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " plane is too small");
    }
}

}  // namespace

std::vector<std::uint8_t> copy_preview_stage_plane(std::span<const std::uint8_t> plane) {
    const std::size_t required = static_cast<std::size_t>(kPreviewStageWidth * kPreviewStageHeight);
    validate_minimum_size(plane, required, "preview");

    std::vector<std::uint8_t> out(required);
    for (int row = 0; row < kPreviewStageHeight; ++row) {
        const auto src_offset = static_cast<std::size_t>(row) * kPreviewStageWidth;
        const auto dst_offset = src_offset;
        std::copy_n(plane.begin() + static_cast<std::ptrdiff_t>(src_offset), kPreviewStageWidth, out.begin() + static_cast<std::ptrdiff_t>(dst_offset));
    }
    return out;
}

std::vector<std::uint8_t> copy_normal_stage_plane(std::span<const std::uint8_t> plane) {
    const std::size_t required = static_cast<std::size_t>(kGeometrySourceStride) * kNormalStageHeight;
    validate_minimum_size(plane, required, "normal");

    std::vector<std::uint8_t> out(static_cast<std::size_t>(kNormalStageWidth) * kNormalStageHeight);
    for (int row = 0; row < kNormalStageHeight; ++row) {
        const auto src_offset = static_cast<std::size_t>(row) * kGeometrySourceStride + kNormalStageLeftCrop;
        const auto dst_offset = static_cast<std::size_t>(row) * kNormalStageWidth;
        std::copy_n(plane.begin() + static_cast<std::ptrdiff_t>(src_offset), kNormalStageWidth, out.begin() + static_cast<std::ptrdiff_t>(dst_offset));
    }
    return out;
}

std::vector<std::uint8_t> copy_large_stage_plane(std::span<const std::uint8_t> plane) {
    const std::size_t required = static_cast<std::size_t>(kLargeStageWidth) * kLargeStageHeight;
    validate_minimum_size(plane, required, "large");

    std::vector<std::uint8_t> out(required);
    for (int row = 0; row < kLargeStageHeight; ++row) {
        const auto src_offset = static_cast<std::size_t>(row) * kLargeStageWidth;
        const auto dst_offset = src_offset;
        std::copy_n(plane.begin() + static_cast<std::ptrdiff_t>(src_offset), kLargeStageWidth, out.begin() + static_cast<std::ptrdiff_t>(dst_offset));
    }
    return out;
}

}  // namespace dj1000
