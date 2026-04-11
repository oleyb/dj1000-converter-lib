#include "dj1000/converter.hpp"

#include "export_pipeline_cache.hpp"
#include "dj1000/large_export_pipeline.hpp"
#include "dj1000/normal_export_pipeline.hpp"

#include <bit>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace dj1000 {

namespace {

NormalExportOverrides build_normal_overrides(const ConvertOptions& options) {
    NormalExportOverrides overrides;
    overrides.source_gain = options.source_gain;
    overrides.red_balance = options.sliders.red_balance;
    overrides.green_balance = options.sliders.green_balance;
    overrides.blue_balance = options.sliders.blue_balance;
    overrides.contrast = options.sliders.contrast;
    overrides.brightness = options.sliders.brightness;
    overrides.vividness = options.sliders.vividness;
    overrides.sharpness = options.sliders.sharpness;
    return overrides;
}

LargeExportOverrides build_large_overrides(const ConvertOptions& options) {
    LargeExportOverrides overrides;
    overrides.source_gain = options.source_gain;
    overrides.red_balance = options.sliders.red_balance;
    overrides.green_balance = options.sliders.green_balance;
    overrides.blue_balance = options.sliders.blue_balance;
    overrides.contrast = options.sliders.contrast;
    overrides.brightness = options.sliders.brightness;
    overrides.vividness = options.sliders.vividness;
    overrides.sharpness = options.sliders.sharpness;
    return overrides;
}

void populate_debug_state(ConvertDebugState* target, const NormalExportDebugState& source) {
    if (target == nullptr) {
        return;
    }
    target->source_gain = source.source_gain;
    target->sharpness_scalar = source.sharpness_scalar;
    target->stage3060_scalar = source.stage3060_scalar;
    target->stage3060_param0 = source.stage3060_param0;
    target->stage3060_param1 = source.stage3060_param1;
    target->stage3060_threshold = source.stage3060_threshold;
    target->post_rgb_scalar = source.post_rgb_scalar;
}

void populate_debug_state(ConvertDebugState* target, const LargeExportDebugState& source) {
    if (target == nullptr) {
        return;
    }
    target->source_gain = source.source_gain;
    target->sharpness_scalar = source.sharpness_scalar;
    target->stage3060_scalar = source.stage3060_scalar;
    target->stage3060_param0 = source.stage3060_param0;
    target->stage3060_param1 = source.stage3060_param1;
    target->stage3060_threshold = source.stage3060_threshold;
    target->post_rgb_scalar = source.post_rgb_scalar;
}

}  // namespace

enum class SessionPipelineKind {
    Nonlarge,
    Large,
};

struct SessionCacheKey {
    SessionPipelineKind pipeline_kind = SessionPipelineKind::Nonlarge;
    bool has_source_gain = false;
    std::uint64_t source_gain_bits = 0;

    [[nodiscard]] bool operator==(const SessionCacheKey& other) const noexcept = default;
};

struct SessionCacheEntry {
    SessionCacheKey key;
    std::shared_ptr<const CachedPostGeometryStage> stage;
};

struct SessionState {
    explicit SessionState(DatFile dat_value)
        : dat(std::move(dat_value)) {}

    DatFile dat;
    mutable std::mutex cache_mutex;
    mutable std::vector<SessionCacheEntry> cache_entries;
};

namespace {

void validate_plane_sizes(const RgbBytePlanes& planes) {
    if (planes.plane0.size() != planes.plane1.size() || planes.plane0.size() != planes.plane2.size()) {
        throw std::runtime_error("converted image planes must have the same size");
    }
}

[[nodiscard]] SessionPipelineKind pipeline_kind_for_size(ExportSize size) {
    return size == ExportSize::Large ? SessionPipelineKind::Large : SessionPipelineKind::Nonlarge;
}

[[nodiscard]] SessionCacheKey build_session_cache_key(const ConvertOptions& options) {
    SessionCacheKey key;
    key.pipeline_kind = pipeline_kind_for_size(options.size);
    key.has_source_gain = options.source_gain.has_value();
    if (options.source_gain.has_value()) {
        key.source_gain_bits = std::bit_cast<std::uint64_t>(*options.source_gain);
    }
    return key;
}

}  // namespace

std::vector<std::uint8_t> ConvertedImage::interleaved_bgr() const {
    validate_plane_sizes(planes);

    std::vector<std::uint8_t> output;
    output.reserve(planes.plane0.size() * 3);
    for (std::size_t index = 0; index < planes.plane0.size(); ++index) {
        output.push_back(planes.plane0[index]);
        output.push_back(planes.plane1[index]);
        output.push_back(planes.plane2[index]);
    }
    return output;
}

std::vector<std::uint8_t> ConvertedImage::interleaved_rgb() const {
    validate_plane_sizes(planes);

    std::vector<std::uint8_t> output;
    output.reserve(planes.plane0.size() * 3);
    for (std::size_t index = 0; index < planes.plane0.size(); ++index) {
        output.push_back(planes.plane0[index]);
        output.push_back(planes.plane1[index]);
        output.push_back(planes.plane2[index]);
    }
    return output;
}

std::vector<std::uint8_t> ConvertedImage::interleaved_rgba() const {
    validate_plane_sizes(planes);

    std::vector<std::uint8_t> output;
    output.reserve(planes.plane0.size() * 4);
    for (std::size_t index = 0; index < planes.plane0.size(); ++index) {
        output.push_back(planes.plane0[index]);
        output.push_back(planes.plane1[index]);
        output.push_back(planes.plane2[index]);
        output.push_back(0xFF);
    }
    return output;
}

ConvertedImage convert_dat_to_bgr(
    const DatFile& dat,
    const ConvertOptions& options,
    ConvertDebugState* debug_state
) {
    if (options.size == ExportSize::Large) {
        LargeExportDebugState large_debug;
        const auto overrides = build_large_overrides(options);
        auto image = build_default_large_export_bgr_planes(
            dat,
            debug_state != nullptr ? &large_debug : nullptr,
            &overrides
        );
        populate_debug_state(debug_state, large_debug);
        return {
            .width = 504,
            .height = 378,
            .planes = std::move(image),
        };
    }

    NormalExportDebugState normal_debug;
    const auto overrides = build_normal_overrides(options);
    auto image =
        options.size == ExportSize::Small
            ? build_default_small_export_bgr_planes(
                  dat,
                  debug_state != nullptr ? &normal_debug : nullptr,
                  &overrides
              )
            : build_default_normal_export_bgr_planes(
                  dat,
                  debug_state != nullptr ? &normal_debug : nullptr,
                  &overrides
              );
    populate_debug_state(debug_state, normal_debug);
    return {
        .width = 320,
        .height = 240,
        .planes = std::move(image),
    };
}

ConvertedImage convert_dat_bytes_to_bgr(
    std::span<const std::uint8_t> dat_bytes,
    const ConvertOptions& options,
    ConvertDebugState* debug_state
) {
    return convert_dat_to_bgr(make_dat_file(dat_bytes), options, debug_state);
}

namespace {

[[nodiscard]] std::shared_ptr<const CachedPostGeometryStage> find_cached_stage_locked(
    const SessionState& state,
    const SessionCacheKey& key
) {
    for (const auto& entry : state.cache_entries) {
        if (entry.key == key) {
            return entry.stage;
        }
    }
    return nullptr;
}

[[nodiscard]] std::shared_ptr<const CachedPostGeometryStage> build_cached_stage_for_options(
    const SessionState& state,
    const ConvertOptions& options
) {
    const auto source_gain_override = options.source_gain;
    if (pipeline_kind_for_size(options.size) == SessionPipelineKind::Large) {
        return std::make_shared<const CachedPostGeometryStage>(
            build_cached_large_post_geometry_stage(state.dat, source_gain_override)
        );
    }
    return std::make_shared<const CachedPostGeometryStage>(
        build_cached_nonlarge_post_geometry_stage(state.dat, source_gain_override)
    );
}

[[nodiscard]] std::shared_ptr<const CachedPostGeometryStage> get_or_build_cached_stage(
    const SessionState& state,
    const ConvertOptions& options
) {
    const auto key = build_session_cache_key(options);
    {
        std::lock_guard<std::mutex> lock(state.cache_mutex);
        if (const auto cached = find_cached_stage_locked(state, key)) {
            return cached;
        }
    }

    auto built = build_cached_stage_for_options(state, options);
    std::lock_guard<std::mutex> lock(state.cache_mutex);
    if (const auto cached = find_cached_stage_locked(state, key)) {
        return cached;
    }
    state.cache_entries.push_back({key, built});
    return built;
}

}  // namespace

Session Session::open(std::span<const std::uint8_t> dat_bytes) {
    return Session(std::make_shared<SessionState>(make_dat_file(dat_bytes)));
}

Session Session::open(const DatFile& dat) {
    return Session(std::make_shared<SessionState>(dat));
}

Session::Session(std::shared_ptr<SessionState> state)
    : state_(std::move(state)) {}

const DatFile& Session::dat_file() const noexcept {
    return state_->dat;
}

ConvertedImage Session::render(const ConvertOptions& options, ConvertDebugState* debug_state) const {
    const auto cached_stage = get_or_build_cached_stage(*state_, options);

    if (options.size == ExportSize::Large) {
        LargeExportDebugState large_debug;
        const auto overrides = build_large_overrides(options);
        auto image = render_default_large_export_from_cached_stage(
            *cached_stage,
            debug_state != nullptr ? &large_debug : nullptr,
            &overrides
        );
        populate_debug_state(debug_state, large_debug);
        return {
            .width = 504,
            .height = 378,
            .planes = std::move(image),
        };
    }

    NormalExportDebugState normal_debug;
    const auto overrides = build_normal_overrides(options);
    auto image = render_default_nonlarge_export_from_cached_stage(
        *cached_stage,
        options.size == ExportSize::Small,
        debug_state != nullptr ? &normal_debug : nullptr,
        &overrides
    );
    populate_debug_state(debug_state, normal_debug);
    return {
        .width = 320,
        .height = 240,
        .planes = std::move(image),
    };
}

}  // namespace dj1000
