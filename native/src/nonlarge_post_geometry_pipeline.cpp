#include "dj1000/nonlarge_post_geometry_pipeline.hpp"

#include "dj1000/nonlarge_source_pipeline.hpp"
#include "dj1000/post_geometry_center_scale.hpp"
#include "dj1000/post_geometry_edge_response.hpp"
#include "dj1000/post_geometry_filter.hpp"
#include "dj1000/post_geometry_rgb_convert.hpp"
#include "dj1000/post_geometry_stage_2a00.hpp"
#include "dj1000/post_geometry_stage_2dd0.hpp"
#include "dj1000/post_geometry_stage_3060.hpp"
#include "dj1000/post_geometry_stage_3600.hpp"
#include "dj1000/post_geometry_stage_3890.hpp"
#include "dj1000/post_geometry_stage_4810.hpp"

namespace dj1000 {

namespace {

constexpr int kNormalPostGeometryWidth = 320;
constexpr int kNormalPostGeometryHeight = 244;
constexpr int kPreviewPostGeometryWidth = 80;
constexpr int kPreviewPostGeometryHeight = 64;
constexpr int kNormalStageHeight = 246;
constexpr int kNormalSharpnessStageParam0 = 8;
constexpr int kNormalSharpnessStage3890Level = 3;
constexpr double kNormalSharpnessScalar = 1.0;
constexpr int kNormalSharpnessThreshold = 25;

std::vector<std::uint8_t> crop_normal_geometry_plane(std::span<const std::uint8_t> plane) {
    constexpr int width = kNormalPostGeometryWidth;
    std::vector<std::uint8_t> output(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(kNormalPostGeometryHeight),
        0
    );
    for (int row = 0; row < kNormalPostGeometryHeight; ++row) {
        const std::size_t source_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        const std::size_t target_offset =
            static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        std::copy_n(
            plane.begin() + static_cast<std::ptrdiff_t>(source_offset),
            width,
            output.begin() + static_cast<std::ptrdiff_t>(target_offset)
        );
    }
    return output;
}

}  // namespace

PostGeometryPlanes build_nonlarge_post_geometry_planes_from_source(
    std::span<const std::uint8_t> source,
    bool preview_mode
) {
    const auto stage = build_nonlarge_stage_planes_from_source(source, preview_mode);
    const int width = preview_mode ? kPreviewPostGeometryWidth : kNormalPostGeometryWidth;
    const int height = preview_mode ? kPreviewPostGeometryHeight : kNormalPostGeometryHeight;
    const auto plane0 = preview_mode ? stage.plane0 : crop_normal_geometry_plane(stage.plane0);
    const auto plane1 = preview_mode ? stage.plane1 : crop_normal_geometry_plane(stage.plane1);
    const auto plane2 = preview_mode ? stage.plane2 : crop_normal_geometry_plane(stage.plane2);

    auto prepared = build_post_geometry_planes(
        plane0,
        plane1,
        plane2,
        width,
        height
    );
    apply_post_geometry_delta_filters(prepared.delta0, prepared.delta2, width, height);
    return prepared;
}

RgbBytePlanes build_nonlarge_rgb_planes_from_source(
    std::span<const std::uint8_t> source,
    bool preview_mode
) {
    auto prepared = build_nonlarge_post_geometry_planes_from_source(source, preview_mode);
    const int width = preview_mode ? kPreviewPostGeometryWidth : kNormalPostGeometryWidth;
    const int height = preview_mode ? kPreviewPostGeometryHeight : kNormalPostGeometryHeight;

    apply_post_geometry_stage_4810(
        prepared.delta0,
        prepared.center,
        prepared.delta2,
        width,
        height
    );
    if (!preview_mode) {
        apply_post_geometry_stage_3600(prepared.center, width, height);
    }
    const auto edge_response = build_post_geometry_edge_response(prepared.center, width, height);
    apply_post_geometry_stage_2a00(prepared.center, width, height);
    apply_post_geometry_center_scale(
        prepared.delta0,
        prepared.center,
        prepared.delta2,
        width,
        height
    );
    apply_post_geometry_stage_2dd0(prepared.delta0, prepared.delta2, width, height);
    if (!preview_mode) {
        apply_post_geometry_stage_3890(
            prepared.delta0,
            prepared.delta2,
            width,
            height,
            kNormalSharpnessStage3890Level
        );
        apply_post_geometry_stage_3060(
            edge_response,
            prepared.center,
            width,
            height,
            kNormalSharpnessStageParam0,
            kNormalSharpnessThreshold,
            kNormalSharpnessScalar,
            kNormalSharpnessThreshold
        );
    }

    return convert_post_geometry_rgb_planes(
        prepared.delta0,
        prepared.center,
        prepared.delta2,
        width,
        height
    );
}

}  // namespace dj1000
