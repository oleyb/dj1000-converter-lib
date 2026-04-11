#include "dj1000/nonlarge_geometry.hpp"

#include "dj1000/geometry_copy.hpp"
#include "dj1000/nonlarge_row_resample.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

namespace dj1000 {

namespace {

std::span<const std::uint8_t> source_plane_for_channel(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2,
    int channel_index
) {
    switch (channel_index) {
        case 0:
            return plane0;
        case 1:
            return plane1;
        case 2:
            return plane2;
        default:
            throw std::runtime_error("channel_index must be 0, 1, or 2");
    }
}

void validate_source_plane(
    std::span<const std::uint8_t> plane,
    int stride,
    int row_offset,
    int row_bytes,
    int rows,
    const char* label
) {
    const std::size_t required =
        static_cast<std::size_t>(rows - 1) * static_cast<std::size_t>(stride) +
        static_cast<std::size_t>(row_offset) +
        static_cast<std::size_t>(row_bytes);
    if (plane.size() < required) {
        throw std::runtime_error(std::string(label) + " source plane is too small");
    }
}

std::span<const std::uint8_t> extract_row(
    std::span<const std::uint8_t> plane,
    int row_index,
    int stride,
    int row_offset,
    int row_bytes
) {
    const auto offset =
        static_cast<std::size_t>(row_index) * static_cast<std::size_t>(stride) +
        static_cast<std::size_t>(row_offset);
    return plane.subspan(offset, static_cast<std::size_t>(row_bytes));
}

std::vector<std::uint8_t> build_preview_intermediate_plane(std::span<const std::uint8_t> source_plane) {
    validate_source_plane(
        source_plane,
        kPreviewSourceRowStride,
        kPreviewSourceRowOffset,
        kPreviewSourceRowBytes,
        kPreviewResampledRows,
        "preview"
    );

    std::vector<std::uint8_t> output(
        static_cast<std::size_t>(kPreviewIntermediateStride) * kPreviewIntermediateRows,
        0
    );
    for (int row = 0; row < kPreviewResampledRows; ++row) {
        const auto source_row = extract_row(
            source_plane,
            row,
            kPreviewSourceRowStride,
            kPreviewSourceRowOffset,
            kPreviewSourceRowBytes
        );
        const auto resampled = resample_nonlarge_row(
            source_row,
            kPreviewRowLutWidthParameter,
            kPreviewRowWidthLimit,
            kPreviewRowOutputLength
        );
        const auto destination_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(kPreviewIntermediateStride);
        std::copy(resampled.begin(), resampled.end(), output.begin() + static_cast<std::ptrdiff_t>(destination_offset));
    }
    return output;
}

std::vector<std::uint8_t> build_normal_intermediate_plane(std::span<const std::uint8_t> source_plane) {
    validate_source_plane(
        source_plane,
        kNormalSourceRowStride,
        kNormalSourceRowOffset,
        kNormalSourceRowBytes,
        kNormalResampledRows,
        "normal"
    );

    std::vector<std::uint8_t> output(
        static_cast<std::size_t>(kNormalIntermediateStride) * kNormalIntermediateRows,
        0
    );
    for (int row = 0; row < kNormalResampledRows; ++row) {
        const auto source_row = extract_row(
            source_plane,
            row,
            kNormalSourceRowStride,
            kNormalSourceRowOffset,
            kNormalSourceRowBytes
        );
        const auto resampled = resample_nonlarge_row(
            source_row,
            kNormalRowLutWidthParameter,
            kNormalRowWidthLimit,
            kNormalRowOutputLength
        );
        const auto destination_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(kNormalIntermediateStride);
        std::copy(resampled.begin(), resampled.end(), output.begin() + static_cast<std::ptrdiff_t>(destination_offset));
    }
    return output;
}

}  // namespace

std::vector<std::uint8_t> build_nonlarge_intermediate_plane(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2,
    int channel_index,
    bool preview_mode
) {
    const auto source_plane = source_plane_for_channel(plane0, plane1, plane2, channel_index);
    if (preview_mode) {
        return build_preview_intermediate_plane(source_plane);
    }
    return build_normal_intermediate_plane(source_plane);
}

std::vector<std::uint8_t> build_nonlarge_stage_plane(
    std::span<const std::uint8_t> plane0,
    std::span<const std::uint8_t> plane1,
    std::span<const std::uint8_t> plane2,
    int channel_index,
    bool preview_mode
) {
    const auto intermediate = build_nonlarge_intermediate_plane(
        plane0,
        plane1,
        plane2,
        channel_index,
        preview_mode
    );
    if (preview_mode) {
        return copy_preview_stage_plane(intermediate);
    }
    return copy_normal_stage_plane(intermediate);
}

}  // namespace dj1000
