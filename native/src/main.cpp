#include "dj1000/bmp.hpp"
#include "dj1000/bright_vertical_gate.hpp"
#include "dj1000/dat_file.hpp"
#include "dj1000/index_view.hpp"
#include "dj1000/large_export_pipeline.hpp"
#include "dj1000/large_vertical_resample.hpp"
#include "dj1000/nonlarge_geometry.hpp"
#include "dj1000/nonlarge_post_geometry_pipeline.hpp"
#include "dj1000/nonlarge_source_pipeline.hpp"
#include "dj1000/normal_export_pipeline.hpp"
#include "dj1000/nonlarge_row_resample.hpp"
#include "dj1000/post_geometry_center_scale.hpp"
#include "dj1000/post_geometry_dual_scale.hpp"
#include "dj1000/post_geometry_edge_response.hpp"
#include "dj1000/post_geometry_filter.hpp"
#include "dj1000/post_geometry_prepare.hpp"
#include "dj1000/post_geometry_rgb_convert.hpp"
#include "dj1000/post_geometry_stage_3060.hpp"
#include "dj1000/post_geometry_stage_3600.hpp"
#include "dj1000/post_geometry_stage_3890.hpp"
#include "dj1000/post_geometry_stage_2a00.hpp"
#include "dj1000/post_geometry_stage_2dd0.hpp"
#include "dj1000/post_geometry_stage_4810.hpp"
#include "dj1000/post_rgb_stage_40f0.hpp"
#include "dj1000/post_rgb_stage_42a0.hpp"
#include "dj1000/post_rgb_stage_3b00.hpp"
#include "dj1000/pregeometry_pipeline.hpp"
#include "dj1000/pregeometry_normalize.hpp"
#include "dj1000/quantize_stage.hpp"
#include "dj1000/resample_lut.hpp"
#include "dj1000/source_center_stage.hpp"
#include "dj1000/source_stage.hpp"
#include "dj1000/source_seed_stage.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kCliName = "dj1000";

void print_usage() {
    std::cerr
        << "Usage:\n"
        << "  " << kCliName << " info INPUT.DAT\n"
        << "  " << kCliName << " index-bmp INPUT.DAT OUTPUT.bmp\n"
        << "  " << kCliName << " large-export-bmp INPUT.DAT OUTPUT.bmp\n"
        << "  " << kCliName << " large-export-bmp-controls INPUT.DAT OUTPUT.bmp RED GREEN BLUE CONTRAST BRIGHTNESS VIVIDNESS SHARPNESS\n"
        << "  " << kCliName << " large-export-bmp-tuned INPUT.DAT OUTPUT.bmp PARAM0 PARAM1 SCALAR THRESHOLD\n"
        << "  " << kCliName << " large-export-bmp-tuned-gain INPUT.DAT OUTPUT.bmp SOURCE_GAIN PARAM0 PARAM1 SCALAR THRESHOLD\n"
        << "  " << kCliName << " small-export-bmp INPUT.DAT OUTPUT.bmp\n"
        << "  " << kCliName << " small-export-bmp-controls INPUT.DAT OUTPUT.bmp RED GREEN BLUE CONTRAST BRIGHTNESS VIVIDNESS SHARPNESS\n"
        << "  " << kCliName << " small-export-bmp-tuned INPUT.DAT OUTPUT.bmp CROP_TOP PARAM0 PARAM1 SCALAR THRESHOLD\n"
        << "  " << kCliName << " small-export-bmp-tuned-gain INPUT.DAT OUTPUT.bmp SOURCE_GAIN CROP_TOP PARAM0 PARAM1 SCALAR THRESHOLD\n"
        << "  " << kCliName << " normal-export-bmp INPUT.DAT OUTPUT.bmp\n"
        << "  " << kCliName << " normal-export-bmp-controls INPUT.DAT OUTPUT.bmp RED GREEN BLUE CONTRAST BRIGHTNESS VIVIDNESS SHARPNESS\n"
        << "  " << kCliName << " normal-export-bmp-tuned INPUT.DAT OUTPUT.bmp CROP_TOP PARAM0 PARAM1 SCALAR THRESHOLD\n"
        << "  " << kCliName << " normal-export-bmp-tuned-gain INPUT.DAT OUTPUT.bmp SOURCE_GAIN CROP_TOP PARAM0 PARAM1 SCALAR THRESHOLD\n"
        << "  " << kCliName << " dump-resample-lut WIDTH OUTPUT.bin [--preserve-full-scale-marker]\n"
        << "  " << kCliName << " nonlarge-row INPUT.bin LUT_WIDTH WIDTH_LIMIT OUTPUT_LENGTH OUTPUT.bin\n"
        << "  " << kCliName << " nonlarge-stage-plane PREVIEW_FLAG CHANNEL PLANE0.bin PLANE1.bin PLANE2.bin OUTPUT.bin\n"
        << "  " << kCliName << " source-seed-stage PREVIEW_FLAG INPUT.bin OUTPUT_PREFIX\n"
        << "  " << kCliName << " source-center-stage PREVIEW_FLAG INPUT.bin OUTPUT.f32\n"
        << "  " << kCliName << " source-stage PREVIEW_FLAG INPUT.bin OUTPUT_PREFIX\n"
        << "  " << kCliName << " pregeometry-pipeline PREVIEW_FLAG INPUT.bin OUTPUT_PREFIX\n"
        << "  " << kCliName << " quantize-stage PLANE0.f32 PLANE1.f32 PLANE2.f32 OUTPUT_PREFIX\n"
        << "  " << kCliName << " nonlarge-source-stage PREVIEW_FLAG INPUT.bin OUTPUT_PREFIX\n"
        << "  " << kCliName << " nonlarge-post-geometry-source PREVIEW_FLAG INPUT.bin OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-geometry-prepare WIDTH HEIGHT PLANE0.bin PLANE1.bin PLANE2.bin OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-geometry-center-scale WIDTH HEIGHT DELTA0.f64 CENTER.f64 DELTA2.f64 OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-geometry-dual-scale WIDTH HEIGHT PLANE0.f64 PLANE1.f64 SCALAR OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-geometry-edge-response WIDTH HEIGHT INPUT.f64 OUTPUT.bin\n"
        << "  " << kCliName << " post-geometry-filter WIDTH HEIGHT DELTA0.f64 DELTA2.f64 OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-geometry-rgb-convert WIDTH HEIGHT PLANE0.f64 PLANE1.f64 PLANE2.f64 OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-geometry-stage-3060 WIDTH HEIGHT EDGE.i32 CENTER.f64 STAGE_PARAM0 STAGE_PARAM1 SCALAR THRESHOLD OUTPUT.f64\n"
        << "  " << kCliName << " post-geometry-stage-3600 WIDTH HEIGHT INPUT.f64 OUTPUT.f64\n"
        << "  " << kCliName << " post-geometry-stage-3890 WIDTH HEIGHT LEVEL PLANE0.f64 PLANE1.f64 OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-geometry-stage-2a00 WIDTH HEIGHT INPUT.f64 OUTPUT.f64\n"
        << "  " << kCliName << " post-geometry-stage-2dd0 WIDTH HEIGHT PLANE0.f64 PLANE1.f64 OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-geometry-stage-4810 WIDTH HEIGHT PLANE0.f64 PLANE1.f64 PLANE2.f64 OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-geometry-stage-4810-horizontal WIDTH HEIGHT PLANE0.f64 PLANE1.f64 PLANE2.f64 OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-geometry-stage-4810-vertical WIDTH HEIGHT PLANE0.f64 PLANE1.f64 PLANE2.f64 OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-rgb-stage-40f0 WIDTH HEIGHT LEVEL SELECTOR PLANE0.bin PLANE1.bin PLANE2.bin OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-rgb-stage-42a0 WIDTH HEIGHT SCALE0 SCALE1 SCALE2 PLANE0.bin PLANE1.bin PLANE2.bin OUTPUT_PREFIX\n"
        << "  " << kCliName << " post-rgb-stage-3b00 WIDTH HEIGHT LEVEL SCALAR PLANE0.bin PLANE1.bin PLANE2.bin OUTPUT_PREFIX\n"
        << "  " << kCliName << " bright-vertical-gate PREVIEW_FLAG INPUT.f32 OUTPUT.f32\n"
        << "  " << kCliName << " normalize-stage PREVIEW_FLAG PLANE0.f32 PLANE1.f32 PLANE2.f32 OUTPUT_PREFIX\n"
        << "  " << kCliName << " large-stage-plane INPUT.bin OUTPUT.bin\n";
}

std::vector<std::uint8_t> read_uint8_binary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open input path: " + path.string());
    }
    const std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    if (!input.good() && !input.eof()) {
        throw std::runtime_error("Failed to read input path: " + path.string());
    }
    return bytes;
}

std::vector<float> read_float32_binary(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> bytes = read_uint8_binary(path);
    if (bytes.size() % sizeof(float) != 0) {
        throw std::runtime_error("Float input size is not a multiple of 4 bytes: " + path.string());
    }

    std::vector<float> output(bytes.size() / sizeof(float));
    std::memcpy(output.data(), bytes.data(), bytes.size());
    return output;
}

std::vector<double> read_float64_binary(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> bytes = read_uint8_binary(path);
    if (bytes.size() % sizeof(double) != 0) {
        throw std::runtime_error("Double input size is not a multiple of 8 bytes: " + path.string());
    }

    std::vector<double> output(bytes.size() / sizeof(double));
    std::memcpy(output.data(), bytes.data(), bytes.size());
    return output;
}

std::vector<std::int32_t> read_int32_binary(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> bytes = read_uint8_binary(path);
    if (bytes.size() % sizeof(std::int32_t) != 0) {
        throw std::runtime_error("Int32 input size is not a multiple of 4 bytes: " + path.string());
    }

    std::vector<std::int32_t> output(bytes.size() / sizeof(std::int32_t));
    std::memcpy(output.data(), bytes.data(), bytes.size());
    return output;
}

void write_float32_binary(const std::filesystem::path& path, std::span<const float> values) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not open output path: " + path.string());
    }
    output.write(
        reinterpret_cast<const char*>(values.data()),
        static_cast<std::streamsize>(values.size_bytes())
    );
    if (!output) {
        throw std::runtime_error("Failed to write output path: " + path.string());
    }
}

void write_float64_binary(const std::filesystem::path& path, std::span<const double> values) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not open output path: " + path.string());
    }
    output.write(
        reinterpret_cast<const char*>(values.data()),
        static_cast<std::streamsize>(values.size_bytes())
    );
    if (!output) {
        throw std::runtime_error("Failed to write output path: " + path.string());
    }
}

void write_uint8_binary(const std::filesystem::path& path, std::span<const std::uint8_t> values) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not open output path: " + path.string());
    }
    output.write(
        reinterpret_cast<const char*>(values.data()),
        static_cast<std::streamsize>(values.size())
    );
    if (!output) {
        throw std::runtime_error("Failed to write output path: " + path.string());
    }
}

void write_int32_binary(const std::filesystem::path& path, std::span<const std::int32_t> values) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not open output path: " + path.string());
    }
    output.write(
        reinterpret_cast<const char*>(values.data()),
        static_cast<std::streamsize>(values.size_bytes())
    );
    if (!output) {
        throw std::runtime_error("Failed to write output path: " + path.string());
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            print_usage();
            return 1;
        }

        const std::string command = argv[1];

        if (command == "info") {
            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            std::cout << "path: " << dat.path << '\n';
            std::cout << "size: 0x" << std::hex << dat.bytes.size() << std::dec << '\n';
            std::cout << "signature_matches: " << (dat.metadata.signature_matches ? "true" : "false") << '\n';
            std::cout << "trailer: " << dj1000::format_hex(dat.metadata.trailer) << '\n';
            std::cout << "mode_flag: " << static_cast<int>(dat.metadata.mode_flag) << '\n';
            std::cout << "mode_value: " << dat.metadata.mode_value << '\n';
            return 0;
        }

        if (command == "index-bmp") {
            if (argc < 4) {
                print_usage();
                return 1;
            }
            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            const auto index_view = dj1000::trans_to_index_view(dat.raw_payload());
            const auto rgb = dj1000::grayscale_to_rgb(index_view);
            dj1000::write_bmp24(output_path, dj1000::kIndexWidth, dj1000::kIndexHeight, rgb);
            return 0;
        }

        if (command == "normal-export-bmp") {
            if (argc < 4) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::NormalExportDebugState debug_state;
            dj1000::write_default_normal_export_bmp(dat, output_path, &debug_state);
            std::cout
                << "wrote default normal export "
                << output_path
                << " source_gain=" << debug_state.source_gain
                << " sharpness_scalar=" << debug_state.sharpness_scalar
                << " stage3060_param0=" << debug_state.stage3060_param0
                << " stage3060_param1=" << debug_state.stage3060_param1
                << " stage3060_scalar=" << debug_state.stage3060_scalar
                << " stage3060_threshold=" << debug_state.stage3060_threshold
                << " post_rgb_scalar=" << debug_state.post_rgb_scalar
                << '\n';
            return 0;
        }

        if (command == "normal-export-bmp-controls") {
            if (argc < 11) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::NormalExportOverrides overrides;
            overrides.red_balance = std::stoi(argv[4]);
            overrides.green_balance = std::stoi(argv[5]);
            overrides.blue_balance = std::stoi(argv[6]);
            overrides.contrast = std::stoi(argv[7]);
            overrides.brightness = std::stoi(argv[8]);
            overrides.vividness = std::stoi(argv[9]);
            overrides.sharpness = std::stoi(argv[10]);

            dj1000::NormalExportDebugState debug_state;
            dj1000::write_default_normal_export_bmp(dat, output_path, &debug_state, &overrides);
            std::cout << "wrote control normal export " << output_path << '\n';
            return 0;
        }

        if (command == "small-export-bmp") {
            if (argc < 4) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::NormalExportDebugState debug_state;
            dj1000::write_default_small_export_bmp(dat, output_path, &debug_state);
            std::cout
                << "wrote default small export "
                << output_path
                << " source_gain=" << debug_state.source_gain
                << " sharpness_scalar=" << debug_state.sharpness_scalar
                << " stage3060_param0=" << debug_state.stage3060_param0
                << " stage3060_param1=" << debug_state.stage3060_param1
                << " stage3060_scalar=" << debug_state.stage3060_scalar
                << " stage3060_threshold=" << debug_state.stage3060_threshold
                << " post_rgb_scalar=" << debug_state.post_rgb_scalar
                << '\n';
            return 0;
        }

        if (command == "small-export-bmp-controls") {
            if (argc < 11) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::NormalExportOverrides overrides;
            overrides.red_balance = std::stoi(argv[4]);
            overrides.green_balance = std::stoi(argv[5]);
            overrides.blue_balance = std::stoi(argv[6]);
            overrides.contrast = std::stoi(argv[7]);
            overrides.brightness = std::stoi(argv[8]);
            overrides.vividness = std::stoi(argv[9]);
            overrides.sharpness = std::stoi(argv[10]);

            dj1000::NormalExportDebugState debug_state;
            dj1000::write_default_small_export_bmp(dat, output_path, &debug_state, &overrides);
            std::cout << "wrote control small export " << output_path << '\n';
            return 0;
        }

        if (command == "large-export-bmp") {
            if (argc < 4) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::LargeExportDebugState debug_state;
            dj1000::write_default_large_export_bmp(dat, output_path, &debug_state);
            std::cout
                << "wrote default large export "
                << output_path
                << " source_gain=" << debug_state.source_gain
                << " sharpness_scalar=" << debug_state.sharpness_scalar
                << " stage3060_param0=" << debug_state.stage3060_param0
                << " stage3060_param1=" << debug_state.stage3060_param1
                << " stage3060_scalar=" << debug_state.stage3060_scalar
                << " stage3060_threshold=" << debug_state.stage3060_threshold
                << " post_rgb_scalar=" << debug_state.post_rgb_scalar
                << '\n';
            return 0;
        }

        if (command == "large-export-bmp-controls") {
            if (argc < 11) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::LargeExportOverrides overrides;
            overrides.red_balance = std::stoi(argv[4]);
            overrides.green_balance = std::stoi(argv[5]);
            overrides.blue_balance = std::stoi(argv[6]);
            overrides.contrast = std::stoi(argv[7]);
            overrides.brightness = std::stoi(argv[8]);
            overrides.vividness = std::stoi(argv[9]);
            overrides.sharpness = std::stoi(argv[10]);

            dj1000::LargeExportDebugState debug_state;
            dj1000::write_default_large_export_bmp(dat, output_path, &debug_state, &overrides);
            std::cout << "wrote control large export " << output_path << '\n';
            return 0;
        }

        if (command == "large-export-bmp-tuned") {
            if (argc < 8) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::LargeExportOverrides overrides;
            overrides.stage3060_param0 = std::stoi(argv[4]);
            overrides.stage3060_param1 = std::stoi(argv[5]);
            overrides.stage3060_scalar = std::stod(argv[6]);
            overrides.stage3060_threshold = std::stoi(argv[7]);

            dj1000::LargeExportDebugState debug_state;
            dj1000::write_default_large_export_bmp(dat, output_path, &debug_state, &overrides);
            std::cout
                << "wrote tuned large export "
                << output_path
                << " stage3060_param0=" << *overrides.stage3060_param0
                << " stage3060_param1=" << *overrides.stage3060_param1
                << " stage3060_scalar=" << *overrides.stage3060_scalar
                << " stage3060_threshold=" << *overrides.stage3060_threshold
                << '\n';
            return 0;
        }

        if (command == "large-export-bmp-tuned-gain") {
            if (argc < 9) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::LargeExportOverrides overrides;
            overrides.source_gain = std::stod(argv[4]);
            overrides.stage3060_param0 = std::stoi(argv[5]);
            overrides.stage3060_param1 = std::stoi(argv[6]);
            overrides.stage3060_scalar = std::stod(argv[7]);
            overrides.stage3060_threshold = std::stoi(argv[8]);

            dj1000::LargeExportDebugState debug_state;
            dj1000::write_default_large_export_bmp(dat, output_path, &debug_state, &overrides);
            std::cout
                << "wrote tuned-gain large export "
                << output_path
                << " source_gain=" << *overrides.source_gain
                << " stage3060_param0=" << *overrides.stage3060_param0
                << " stage3060_param1=" << *overrides.stage3060_param1
                << " stage3060_scalar=" << *overrides.stage3060_scalar
                << " stage3060_threshold=" << *overrides.stage3060_threshold
                << '\n';
            return 0;
        }

        if (command == "normal-export-bmp-tuned") {
            if (argc < 9) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::NormalExportOverrides overrides;
            overrides.crop_top_rows = std::stoi(argv[4]);
            overrides.stage3060_param0 = std::stoi(argv[5]);
            overrides.stage3060_param1 = std::stoi(argv[6]);
            overrides.stage3060_scalar = std::stod(argv[7]);
            overrides.stage3060_threshold = std::stoi(argv[8]);

            dj1000::NormalExportDebugState debug_state;
            dj1000::write_default_normal_export_bmp(dat, output_path, &debug_state, &overrides);
            std::cout
                << "wrote tuned normal export "
                << output_path
                << " crop_top=" << *overrides.crop_top_rows
                << " stage3060_param0=" << *overrides.stage3060_param0
                << " stage3060_param1=" << *overrides.stage3060_param1
                << " stage3060_scalar=" << *overrides.stage3060_scalar
                << " stage3060_threshold=" << *overrides.stage3060_threshold
                << '\n';
            return 0;
        }

        if (command == "small-export-bmp-tuned") {
            if (argc < 9) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::NormalExportOverrides overrides;
            overrides.crop_top_rows = std::stoi(argv[4]);
            overrides.stage3060_param0 = std::stoi(argv[5]);
            overrides.stage3060_param1 = std::stoi(argv[6]);
            overrides.stage3060_scalar = std::stod(argv[7]);
            overrides.stage3060_threshold = std::stoi(argv[8]);

            dj1000::NormalExportDebugState debug_state;
            dj1000::write_default_small_export_bmp(dat, output_path, &debug_state, &overrides);
            std::cout
                << "wrote tuned small export "
                << output_path
                << " crop_top=" << *overrides.crop_top_rows
                << " stage3060_param0=" << *overrides.stage3060_param0
                << " stage3060_param1=" << *overrides.stage3060_param1
                << " stage3060_scalar=" << *overrides.stage3060_scalar
                << " stage3060_threshold=" << *overrides.stage3060_threshold
                << '\n';
            return 0;
        }

        if (command == "normal-export-bmp-tuned-gain") {
            if (argc < 10) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::NormalExportOverrides overrides;
            overrides.source_gain = std::stod(argv[4]);
            overrides.crop_top_rows = std::stoi(argv[5]);
            overrides.stage3060_param0 = std::stoi(argv[6]);
            overrides.stage3060_param1 = std::stoi(argv[7]);
            overrides.stage3060_scalar = std::stod(argv[8]);
            overrides.stage3060_threshold = std::stoi(argv[9]);

            dj1000::NormalExportDebugState debug_state;
            dj1000::write_default_normal_export_bmp(dat, output_path, &debug_state, &overrides);
            std::cout
                << "wrote tuned-gain normal export "
                << output_path
                << " source_gain=" << *overrides.source_gain
                << " crop_top=" << *overrides.crop_top_rows
                << " stage3060_param0=" << *overrides.stage3060_param0
                << " stage3060_param1=" << *overrides.stage3060_param1
                << " stage3060_scalar=" << *overrides.stage3060_scalar
                << " stage3060_threshold=" << *overrides.stage3060_threshold
                << '\n';
            return 0;
        }

        if (command == "small-export-bmp-tuned-gain") {
            if (argc < 10) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const auto dat = dj1000::load_dat_file(input_path);
            const std::filesystem::path output_path = argv[3];
            dj1000::NormalExportOverrides overrides;
            overrides.source_gain = std::stod(argv[4]);
            overrides.crop_top_rows = std::stoi(argv[5]);
            overrides.stage3060_param0 = std::stoi(argv[6]);
            overrides.stage3060_param1 = std::stoi(argv[7]);
            overrides.stage3060_scalar = std::stod(argv[8]);
            overrides.stage3060_threshold = std::stoi(argv[9]);

            dj1000::NormalExportDebugState debug_state;
            dj1000::write_default_small_export_bmp(dat, output_path, &debug_state, &overrides);
            std::cout
                << "wrote tuned-gain small export "
                << output_path
                << " source_gain=" << *overrides.source_gain
                << " crop_top=" << *overrides.crop_top_rows
                << " stage3060_param0=" << *overrides.stage3060_param0
                << " stage3060_param1=" << *overrides.stage3060_param1
                << " stage3060_scalar=" << *overrides.stage3060_scalar
                << " stage3060_threshold=" << *overrides.stage3060_threshold
                << '\n';
            return 0;
        }

        if (command == "post-rgb-stage-3b00") {
            if (argc < 10) {
                print_usage();
                return 1;
            }

            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const int level = std::stoi(argv[4]);
            const double scalar = std::stod(argv[5]);
            auto plane0 = read_uint8_binary(argv[6]);
            auto plane1 = read_uint8_binary(argv[7]);
            auto plane2 = read_uint8_binary(argv[8]);
            const std::filesystem::path output_prefix = argv[9];

            dj1000::apply_post_rgb_stage_3b00(plane0, plane1, plane2, width, height, level, scalar);
            write_uint8_binary(output_prefix.string() + ".plane0.bin", plane0);
            write_uint8_binary(output_prefix.string() + ".plane1.bin", plane1);
            write_uint8_binary(output_prefix.string() + ".plane2.bin", plane2);
            return 0;
        }

        if (command == "post-rgb-stage-40f0") {
            if (argc < 10) {
                print_usage();
                return 1;
            }

            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const int level = std::stoi(argv[4]);
            const int selector = std::stoi(argv[5]);
            auto plane0 = read_uint8_binary(argv[6]);
            auto plane1 = read_uint8_binary(argv[7]);
            auto plane2 = read_uint8_binary(argv[8]);
            const std::filesystem::path output_prefix = argv[9];

            dj1000::apply_post_rgb_stage_40f0(
                plane0,
                plane1,
                plane2,
                width,
                height,
                level,
                selector
            );
            write_uint8_binary(output_prefix.string() + ".plane0.bin", plane0);
            write_uint8_binary(output_prefix.string() + ".plane1.bin", plane1);
            write_uint8_binary(output_prefix.string() + ".plane2.bin", plane2);
            return 0;
        }

        if (command == "post-rgb-stage-42a0") {
            if (argc < 11) {
                print_usage();
                return 1;
            }

            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const int scale0 = std::stoi(argv[4]);
            const int scale1 = std::stoi(argv[5]);
            const int scale2 = std::stoi(argv[6]);
            auto plane0 = read_uint8_binary(argv[7]);
            auto plane1 = read_uint8_binary(argv[8]);
            auto plane2 = read_uint8_binary(argv[9]);
            const std::filesystem::path output_prefix = argv[10];

            dj1000::apply_post_rgb_stage_42a0(
                plane0,
                plane1,
                plane2,
                width,
                height,
                scale0,
                scale1,
                scale2
            );
            write_uint8_binary(output_prefix.string() + ".plane0.bin", plane0);
            write_uint8_binary(output_prefix.string() + ".plane1.bin", plane1);
            write_uint8_binary(output_prefix.string() + ".plane2.bin", plane2);
            return 0;
        }

        if (command == "dump-resample-lut") {
            if (argc < 4) {
                print_usage();
                return 1;
            }
            const int width_parameter = std::stoi(argv[2]);
            const std::filesystem::path output_path = argv[3];
            bool preserve_full_scale_marker = false;
            if (argc >= 5) {
                const std::string flag = argv[4];
                if (flag != "--preserve-full-scale-marker") {
                    throw std::runtime_error("Unknown dump-resample-lut flag: " + flag);
                }
                preserve_full_scale_marker = true;
            }
            const auto lut = dj1000::build_resample_lut(width_parameter, preserve_full_scale_marker);

            std::ofstream output(output_path, std::ios::binary);
            if (!output) {
                throw std::runtime_error("Could not open output path: " + output_path.string());
            }
            output.write(
                reinterpret_cast<const char*>(lut.packed_bytes.data()),
                static_cast<std::streamsize>(lut.packed_bytes.size())
            );
            if (!output) {
                throw std::runtime_error("Failed to write output path: " + output_path.string());
            }
            std::cout
                << "dumped resample_lut width=" << width_parameter
                << " length=" << lut.packed_length
                << " preserve_full_scale_marker=" << (preserve_full_scale_marker ? "true" : "false")
                << " -> " << output_path << '\n';
            return 0;
        }

        if (command == "large-stage-plane") {
            if (argc < 4) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const std::filesystem::path output_path = argv[3];

            std::ifstream input(input_path, std::ios::binary);
            if (!input) {
                throw std::runtime_error("Could not open input path: " + input_path.string());
            }
            const std::vector<std::uint8_t> source(
                (std::istreambuf_iterator<char>(input)),
                std::istreambuf_iterator<char>()
            );
            if (!input.good() && !input.eof()) {
                throw std::runtime_error("Failed to read input path: " + input_path.string());
            }

            const auto output = dj1000::resample_large_vertical_plane(source);
            std::ofstream output_file(output_path, std::ios::binary);
            if (!output_file) {
                throw std::runtime_error("Could not open output path: " + output_path.string());
            }
            output_file.write(
                reinterpret_cast<const char*>(output.data()),
                static_cast<std::streamsize>(output.size())
            );
            if (!output_file) {
                throw std::runtime_error("Failed to write output path: " + output_path.string());
            }

            std::cout
                << "dumped large_stage_plane bytes=" << output.size()
                << " -> " << output_path << '\n';
            return 0;
        }

        if (command == "post-geometry-prepare") {
            if (argc < 8) {
                print_usage();
                return 1;
            }

            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const auto plane0 = read_uint8_binary(argv[4]);
            const auto plane1 = read_uint8_binary(argv[5]);
            const auto plane2 = read_uint8_binary(argv[6]);
            const std::filesystem::path output_prefix = argv[7];

            const auto prepared = dj1000::build_post_geometry_planes(
                plane0,
                plane1,
                plane2,
                width,
                height
            );
            write_float64_binary(output_prefix.string() + ".center.f64", prepared.center);
            write_float64_binary(output_prefix.string() + ".delta0.f64", prepared.delta0);
            write_float64_binary(output_prefix.string() + ".delta2.f64", prepared.delta2);

            std::cout
                << "dumped post_geometry_prepare width=" << width
                << " height=" << height
                << " -> " << output_prefix << ".{center,delta0,delta2}.f64\n";
            return 0;
        }

        if (command == "post-geometry-filter") {
            if (argc < 7) {
                print_usage();
                return 1;
            }

            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            auto delta0 = read_float64_binary(argv[4]);
            auto delta2 = read_float64_binary(argv[5]);
            const std::filesystem::path output_prefix = argv[6];

            dj1000::apply_post_geometry_delta_filters(delta0, delta2, width, height);
            write_float64_binary(output_prefix.string() + ".delta0.f64", delta0);
            write_float64_binary(output_prefix.string() + ".delta2.f64", delta2);

            std::cout
                << "dumped post_geometry_filter width=" << width
                << " height=" << height
                << " -> " << output_prefix << ".{delta0,delta2}.f64\n";
            return 0;
        }

        if (command == "post-geometry-center-scale") {
            if (argc < 8) {
                print_usage();
                return 1;
            }

            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            auto delta0 = read_float64_binary(argv[4]);
            auto center = read_float64_binary(argv[5]);
            auto delta2 = read_float64_binary(argv[6]);
            const std::filesystem::path output_prefix = argv[7];

            dj1000::apply_post_geometry_center_scale(delta0, center, delta2, width, height);
            write_float64_binary(output_prefix.string() + ".delta0.f64", delta0);
            write_float64_binary(output_prefix.string() + ".center.f64", center);
            write_float64_binary(output_prefix.string() + ".delta2.f64", delta2);

            std::cout
                << "dumped post_geometry_center_scale width=" << width
                << " height=" << height
                << " -> " << output_prefix << ".{delta0,center,delta2}.f64\n";
            return 0;
        }

        if (command == "post-geometry-dual-scale") {
            if (argc < 8) {
                print_usage();
                return 1;
            }

            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            auto plane0 = read_float64_binary(argv[4]);
            auto plane1 = read_float64_binary(argv[5]);
            const double scalar = std::stod(argv[6]);
            const std::filesystem::path output_prefix = argv[7];

            dj1000::apply_post_geometry_dual_scale(plane0, plane1, width, height, scalar);
            write_float64_binary(output_prefix.string() + ".plane0.f64", plane0);
            write_float64_binary(output_prefix.string() + ".plane1.f64", plane1);

            std::cout
                << "dumped post_geometry_dual_scale width=" << width
                << " height=" << height
                << " scalar=" << scalar
                << " -> " << output_prefix << ".{plane0,plane1}.f64\n";
            return 0;
        }

        if (command == "post-geometry-edge-response") {
            if (argc < 6) {
                print_usage();
                return 1;
            }

            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const auto plane = read_float64_binary(argv[4]);
            const std::filesystem::path output_path = argv[5];

            const auto output = dj1000::build_post_geometry_edge_response(plane, width, height);
            write_int32_binary(output_path, output);

            std::cout
                << "dumped post_geometry_edge_response width=" << width
                << " height=" << height
                << " -> " << output_path << '\n';
            return 0;
        }

        if (command == "post-geometry-rgb-convert") {
            if (argc < 8) {
                print_usage();
                return 1;
            }

            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const auto plane0 = read_float64_binary(argv[4]);
            const auto plane1 = read_float64_binary(argv[5]);
            const auto plane2 = read_float64_binary(argv[6]);
            const std::filesystem::path output_prefix = argv[7];

            const auto output = dj1000::convert_post_geometry_rgb_planes(
                plane0,
                plane1,
                plane2,
                width,
                height
            );
            write_uint8_binary(output_prefix.string() + ".plane0.bin", output.plane0);
            write_uint8_binary(output_prefix.string() + ".plane1.bin", output.plane1);
            write_uint8_binary(output_prefix.string() + ".plane2.bin", output.plane2);

            std::cout
                << "dumped post_geometry_rgb_convert width=" << width
                << " height=" << height
                << " -> " << output_prefix << ".plane{0,1,2}.bin\n";
            return 0;
        }

        if (command == "post-geometry-stage-3060") {
            if (argc < 11) {
                print_usage();
                return 1;
            }

            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const auto edge_response = read_int32_binary(argv[4]);
            auto center = read_float64_binary(argv[5]);
            const int stage_param0 = std::stoi(argv[6]);
            const int stage_param1 = std::stoi(argv[7]);
            const double scalar = std::stod(argv[8]);
            const int threshold = std::stoi(argv[9]);
            const std::filesystem::path output_path = argv[10];

            dj1000::apply_post_geometry_stage_3060(
                edge_response,
                center,
                width,
                height,
                stage_param0,
                stage_param1,
                scalar,
                threshold
            );
            write_float64_binary(output_path, center);

            std::cout
                << "dumped post_geometry_stage_3060 width=" << width
                << " height=" << height
                << " -> " << output_path << '\n';
            return 0;
        }

        if (command == "post-geometry-stage-3890") {
            if (argc < 8) {
                print_usage();
                return 1;
            }

            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const int level = std::stoi(argv[4]);
            auto plane0 = read_float64_binary(argv[5]);
            auto plane1 = read_float64_binary(argv[6]);
            const std::filesystem::path output_prefix = argv[7];

            dj1000::apply_post_geometry_stage_3890(plane0, plane1, width, height, level);
            write_float64_binary(output_prefix.string() + ".plane0.f64", plane0);
            write_float64_binary(output_prefix.string() + ".plane1.f64", plane1);

            std::cout
                << "dumped post_geometry_stage_3890 width=" << width
                << " height=" << height
                << " level=" << level
                << " -> " << output_prefix << ".{plane0,plane1}.f64\n";
            return 0;
        }

        if (command == "post-geometry-stage-3600") {
            if (argc < 6) {
                print_usage();
                return 1;
            }
            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const std::filesystem::path input_path = argv[4];
            const std::filesystem::path output_path = argv[5];
            auto plane = read_float64_binary(input_path);
            dj1000::apply_post_geometry_stage_3600(plane, width, height);
            write_float64_binary(output_path, plane);
            std::cout
                << "dumped post_geometry_stage_3600 width=" << width
                << " height=" << height
                << " -> " << output_path << '\n';
            return 0;
        }

        if (command == "nonlarge-row") {
            if (argc < 7) {
                print_usage();
                return 1;
            }

            const std::filesystem::path input_path = argv[2];
            const int lut_width_parameter = std::stoi(argv[3]);
            const int width_limit = std::stoi(argv[4]);
            const int output_length = std::stoi(argv[5]);
            const std::filesystem::path output_path = argv[6];

            std::ifstream input(input_path, std::ios::binary);
            if (!input) {
                throw std::runtime_error("Could not open input path: " + input_path.string());
            }
            const std::vector<std::uint8_t> source(
                (std::istreambuf_iterator<char>(input)),
                std::istreambuf_iterator<char>()
            );
            if (!input.good() && !input.eof()) {
                throw std::runtime_error("Failed to read input path: " + input_path.string());
            }

            const auto output = dj1000::resample_nonlarge_row(
                source,
                lut_width_parameter,
                width_limit,
                output_length
            );
            std::ofstream output_file(output_path, std::ios::binary);
            if (!output_file) {
                throw std::runtime_error("Could not open output path: " + output_path.string());
            }
            output_file.write(
                reinterpret_cast<const char*>(output.data()),
                static_cast<std::streamsize>(output.size())
            );
            if (!output_file) {
                throw std::runtime_error("Failed to write output path: " + output_path.string());
            }

            std::cout
                << "dumped nonlarge_row bytes=" << output.size()
                << " lut_width=" << lut_width_parameter
                << " width_limit=" << width_limit
                << " -> " << output_path << '\n';
            return 0;
        }

        if (command == "nonlarge-stage-plane") {
            if (argc < 8) {
                print_usage();
                return 1;
            }

            const bool preview_flag = std::stoi(argv[2]) != 0;
            const int channel_index = std::stoi(argv[3]);
            const std::filesystem::path plane0_path = argv[4];
            const std::filesystem::path plane1_path = argv[5];
            const std::filesystem::path plane2_path = argv[6];
            const std::filesystem::path output_path = argv[7];

            auto read_binary = [](const std::filesystem::path& path) {
                std::ifstream input(path, std::ios::binary);
                if (!input) {
                    throw std::runtime_error("Could not open input path: " + path.string());
                }
                std::vector<std::uint8_t> bytes(
                    (std::istreambuf_iterator<char>(input)),
                    std::istreambuf_iterator<char>()
                );
                if (!input.good() && !input.eof()) {
                    throw std::runtime_error("Failed to read input path: " + path.string());
                }
                return bytes;
            };

            const auto plane0 = read_binary(plane0_path);
            const auto plane1 = read_binary(plane1_path);
            const auto plane2 = read_binary(plane2_path);

            const auto output = dj1000::build_nonlarge_stage_plane(
                plane0,
                plane1,
                plane2,
                channel_index,
                preview_flag
            );
            std::ofstream output_file(output_path, std::ios::binary);
            if (!output_file) {
                throw std::runtime_error("Could not open output path: " + output_path.string());
            }
            output_file.write(
                reinterpret_cast<const char*>(output.data()),
                static_cast<std::streamsize>(output.size())
            );
            if (!output_file) {
                throw std::runtime_error("Failed to write output path: " + output_path.string());
            }

            std::cout
                << "dumped nonlarge_stage_plane bytes=" << output.size()
                << " preview=" << (preview_flag ? 1 : 0)
                << " channel=" << channel_index
                << " -> " << output_path << '\n';
            return 0;
        }

        if (command == "normalize-stage") {
            if (argc < 7) {
                print_usage();
                return 1;
            }

            const bool preview_flag = std::stoi(argv[2]) != 0;
            const std::filesystem::path plane0_path = argv[3];
            const std::filesystem::path plane1_path = argv[4];
            const std::filesystem::path plane2_path = argv[5];
            const std::filesystem::path output_prefix = argv[6];

            auto plane0 = read_float32_binary(plane0_path);
            auto plane1 = read_float32_binary(plane1_path);
            auto plane2 = read_float32_binary(plane2_path);

            dj1000::normalize_pregeometry_planes(plane0, plane1, plane2, preview_flag);

            write_float32_binary(output_prefix.string() + ".plane0.f32", plane0);
            write_float32_binary(output_prefix.string() + ".plane1.f32", plane1);
            write_float32_binary(output_prefix.string() + ".plane2.f32", plane2);

            std::cout
                << "dumped normalize_stage preview=" << (preview_flag ? 1 : 0)
                << " floats=" << plane0.size()
                << " -> " << output_prefix << ".plane{0,1,2}.f32\n";
            return 0;
        }

        if (command == "source-seed-stage") {
            if (argc < 5) {
                print_usage();
                return 1;
            }

            const bool preview_flag = std::stoi(argv[2]) != 0;
            const std::filesystem::path input_path = argv[3];
            const std::filesystem::path output_prefix = argv[4];

            std::ifstream input(input_path, std::ios::binary);
            if (!input) {
                throw std::runtime_error("Could not open input path: " + input_path.string());
            }
            const std::vector<std::uint8_t> source(
                (std::istreambuf_iterator<char>(input)),
                std::istreambuf_iterator<char>()
            );
            if (!input.good() && !input.eof()) {
                throw std::runtime_error("Failed to read input path: " + input_path.string());
            }

            const auto planes = dj1000::build_source_seed_planes(source, preview_flag);
            write_float32_binary(output_prefix.string() + ".plane0.f32", planes.plane0);
            write_float32_binary(output_prefix.string() + ".plane1.f32", planes.plane1);
            write_float32_binary(output_prefix.string() + ".plane2.f32", planes.plane2);

            std::cout
                << "dumped source_seed_stage preview=" << (preview_flag ? 1 : 0)
                << " floats=" << planes.plane0.size()
                << " -> " << output_prefix << ".plane{0,1,2}.f32\n";
            return 0;
        }

        if (command == "source-center-stage") {
            if (argc < 5) {
                print_usage();
                return 1;
            }

            const bool preview_flag = std::stoi(argv[2]) != 0;
            const std::filesystem::path input_path = argv[3];
            const std::filesystem::path output_path = argv[4];

            std::ifstream input(input_path, std::ios::binary);
            if (!input) {
                throw std::runtime_error("Could not open input path: " + input_path.string());
            }
            const std::vector<std::uint8_t> source(
                (std::istreambuf_iterator<char>(input)),
                std::istreambuf_iterator<char>()
            );
            if (!input.good() && !input.eof()) {
                throw std::runtime_error("Failed to read input path: " + input_path.string());
            }

            const auto center = dj1000::build_source_center_plane(source, preview_flag);
            write_float32_binary(output_path, center);

            std::cout
                << "dumped source_center_stage preview=" << (preview_flag ? 1 : 0)
                << " floats=" << center.size()
                << " -> " << output_path << '\n';
            return 0;
        }

        if (command == "source-stage") {
            if (argc < 5) {
                print_usage();
                return 1;
            }

            const bool preview_flag = std::stoi(argv[2]) != 0;
            const std::filesystem::path input_path = argv[3];
            const std::filesystem::path output_prefix = argv[4];

            std::ifstream input(input_path, std::ios::binary);
            if (!input) {
                throw std::runtime_error("Could not open input path: " + input_path.string());
            }
            const std::vector<std::uint8_t> source(
                (std::istreambuf_iterator<char>(input)),
                std::istreambuf_iterator<char>()
            );
            if (!input.good() && !input.eof()) {
                throw std::runtime_error("Failed to read input path: " + input_path.string());
            }

            const auto planes = dj1000::build_source_stage_planes(source, preview_flag);
            write_float32_binary(output_prefix.string() + ".plane0.f32", planes.plane0);
            write_float32_binary(output_prefix.string() + ".plane1.f32", planes.plane1);
            write_float32_binary(output_prefix.string() + ".plane2.f32", planes.plane2);

            std::cout
                << "dumped source_stage preview=" << (preview_flag ? 1 : 0)
                << " floats=" << planes.plane0.size()
                << " -> " << output_prefix << ".plane{0,1,2}.f32\n";
            return 0;
        }

        if (command == "pregeometry-pipeline") {
            if (argc < 5) {
                print_usage();
                return 1;
            }

            const bool preview_flag = std::stoi(argv[2]) != 0;
            const std::filesystem::path input_path = argv[3];
            const std::filesystem::path output_prefix = argv[4];

            std::ifstream input(input_path, std::ios::binary);
            if (!input) {
                throw std::runtime_error("Could not open input path: " + input_path.string());
            }
            const std::vector<std::uint8_t> source(
                (std::istreambuf_iterator<char>(input)),
                std::istreambuf_iterator<char>()
            );
            if (!input.good() && !input.eof()) {
                throw std::runtime_error("Failed to read input path: " + input_path.string());
            }

            const auto planes = dj1000::build_pregeometry_pipeline(source, preview_flag);
            write_float32_binary(output_prefix.string() + ".plane0.f32", planes.plane0);
            write_float32_binary(output_prefix.string() + ".plane1.f32", planes.plane1);
            write_float32_binary(output_prefix.string() + ".plane2.f32", planes.plane2);

            std::cout
                << "dumped pregeometry_pipeline preview=" << (preview_flag ? 1 : 0)
                << " floats=" << planes.plane0.size()
                << " -> " << output_prefix << ".plane{0,1,2}.f32\n";
            return 0;
        }

        if (command == "quantize-stage") {
            if (argc < 6) {
                print_usage();
                return 1;
            }

            const auto plane0 = read_float32_binary(argv[2]);
            const auto plane1 = read_float32_binary(argv[3]);
            const auto plane2 = read_float32_binary(argv[4]);
            const std::filesystem::path output_prefix = argv[5];

            const auto quantized = dj1000::quantize_pregeometry_planes(plane0, plane1, plane2);
            write_uint8_binary(output_prefix.string() + ".plane0.bin", quantized.plane0);
            write_uint8_binary(output_prefix.string() + ".plane1.bin", quantized.plane1);
            write_uint8_binary(output_prefix.string() + ".plane2.bin", quantized.plane2);

            std::cout
                << "dumped quantize_stage bytes=" << quantized.plane0.size()
                << " -> " << output_prefix << ".plane{0,1,2}.bin\n";
            return 0;
        }

        if (command == "nonlarge-source-stage") {
            if (argc < 5) {
                print_usage();
                return 1;
            }

            const bool preview_flag = std::stoi(argv[2]) != 0;
            const std::filesystem::path input_path = argv[3];
            const std::filesystem::path output_prefix = argv[4];

            std::ifstream input(input_path, std::ios::binary);
            if (!input) {
                throw std::runtime_error("Could not open input path: " + input_path.string());
            }
            const std::vector<std::uint8_t> source(
                (std::istreambuf_iterator<char>(input)),
                std::istreambuf_iterator<char>()
            );
            if (!input.good() && !input.eof()) {
                throw std::runtime_error("Failed to read input path: " + input_path.string());
            }

            const auto quantized = dj1000::build_nonlarge_quantized_planes_from_source(source, preview_flag);
            write_uint8_binary(output_prefix.string() + ".quantized.plane0.bin", quantized.plane0);
            write_uint8_binary(output_prefix.string() + ".quantized.plane1.bin", quantized.plane1);
            write_uint8_binary(output_prefix.string() + ".quantized.plane2.bin", quantized.plane2);

            const auto stage = dj1000::build_nonlarge_stage_planes_from_source(source, preview_flag);
            write_uint8_binary(output_prefix.string() + ".stage.plane0.bin", stage.plane0);
            write_uint8_binary(output_prefix.string() + ".stage.plane1.bin", stage.plane1);
            write_uint8_binary(output_prefix.string() + ".stage.plane2.bin", stage.plane2);

            std::cout
                << "dumped nonlarge_source_stage preview=" << (preview_flag ? 1 : 0)
                << " -> " << output_prefix
                << ".{quantized,stage}.plane{0,1,2}.bin\n";
            return 0;
        }

        if (command == "nonlarge-post-geometry-source") {
            if (argc < 5) {
                print_usage();
                return 1;
            }

            const bool preview_flag = std::stoi(argv[2]) != 0;
            const std::filesystem::path input_path = argv[3];
            const std::filesystem::path output_prefix = argv[4];

            const auto source = read_uint8_binary(input_path);
            const auto prepared = dj1000::build_nonlarge_post_geometry_planes_from_source(
                source,
                preview_flag
            );
            write_float64_binary(output_prefix.string() + ".center.f64", prepared.center);
            write_float64_binary(output_prefix.string() + ".delta0.f64", prepared.delta0);
            write_float64_binary(output_prefix.string() + ".delta2.f64", prepared.delta2);

            std::cout
                << "dumped nonlarge_post_geometry_source preview=" << (preview_flag ? 1 : 0)
                << " doubles=" << prepared.center.size()
                << " -> " << output_prefix << ".{center,delta0,delta2}.f64\n";
            return 0;
        }

        if (command == "nonlarge-rgb-source") {
            if (argc < 5) {
                print_usage();
                return 1;
            }

            const bool preview_flag = std::stoi(argv[2]) != 0;
            const auto source = read_uint8_binary(argv[3]);
            const std::filesystem::path output_prefix = argv[4];

            const auto output = dj1000::build_nonlarge_rgb_planes_from_source(source, preview_flag);
            write_uint8_binary(output_prefix.string() + ".plane0.bin", output.plane0);
            write_uint8_binary(output_prefix.string() + ".plane1.bin", output.plane1);
            write_uint8_binary(output_prefix.string() + ".plane2.bin", output.plane2);

            std::cout
                << "dumped nonlarge_rgb_source preview=" << (preview_flag ? 1 : 0)
                << " -> " << output_prefix << ".plane{0,1,2}.bin\n";
            return 0;
        }

        if (command == "post-geometry-stage-2a00") {
            if (argc < 6) {
                print_usage();
                return 1;
            }
            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const std::filesystem::path input_path = argv[4];
            const std::filesystem::path output_path = argv[5];
            auto plane = read_float64_binary(input_path);
            dj1000::apply_post_geometry_stage_2a00(plane, width, height);
            write_float64_binary(output_path, plane);
            std::cout
                << "dumped post_geometry_stage_2a00 width=" << width
                << " height=" << height
                << " -> " << output_path << '\n';
            return 0;
        }

        if (command == "post-geometry-stage-2dd0") {
            if (argc < 7) {
                print_usage();
                return 1;
            }
            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            const std::filesystem::path plane0_path = argv[4];
            const std::filesystem::path plane1_path = argv[5];
            const std::filesystem::path output_prefix = argv[6];
            auto plane0 = read_float64_binary(plane0_path);
            auto plane1 = read_float64_binary(plane1_path);
            dj1000::apply_post_geometry_stage_2dd0(plane0, plane1, width, height);
            write_float64_binary(output_prefix.string() + ".plane0.f64", plane0);
            write_float64_binary(output_prefix.string() + ".plane1.f64", plane1);
            std::cout
                << "dumped post_geometry_stage_2dd0 width=" << width
                << " height=" << height
                << " -> " << output_prefix << ".{plane0,plane1}.f64\n";
            return 0;
        }

        if (command == "post-geometry-stage-4810" ||
            command == "post-geometry-stage-4810-horizontal" ||
            command == "post-geometry-stage-4810-vertical") {
            if (argc < 8) {
                print_usage();
                return 1;
            }
            const int width = std::stoi(argv[2]);
            const int height = std::stoi(argv[3]);
            auto plane0 = read_float64_binary(argv[4]);
            auto plane1 = read_float64_binary(argv[5]);
            auto plane2 = read_float64_binary(argv[6]);
            const std::filesystem::path output_prefix = argv[7];

            if (command == "post-geometry-stage-4810-horizontal") {
                dj1000::apply_post_geometry_stage_4810_horizontal(plane0, plane1, plane2, width, height);
            } else if (command == "post-geometry-stage-4810-vertical") {
                dj1000::apply_post_geometry_stage_4810_vertical(plane0, plane1, plane2, width, height);
            } else {
                dj1000::apply_post_geometry_stage_4810(plane0, plane1, plane2, width, height);
            }
            write_float64_binary(output_prefix.string() + ".plane0.f64", plane0);
            write_float64_binary(output_prefix.string() + ".plane1.f64", plane1);
            write_float64_binary(output_prefix.string() + ".plane2.f64", plane2);
            std::cout
                << "dumped " << command << " width=" << width
                << " height=" << height
                << " -> " << output_prefix << ".{plane0,plane1,plane2}.f64\n";
            return 0;
        }

        if (command == "bright-vertical-gate") {
            if (argc < 5) {
                print_usage();
                return 1;
            }

            const bool preview_flag = std::stoi(argv[2]) != 0;
            const std::filesystem::path input_path = argv[3];
            const std::filesystem::path output_path = argv[4];

            const auto plane = read_float32_binary(input_path);
            const auto output = dj1000::apply_bright_vertical_gate(plane, preview_flag);
            write_float32_binary(output_path, output);

            std::cout
                << "dumped bright_vertical_gate preview=" << (preview_flag ? 1 : 0)
                << " floats=" << output.size()
                << " -> " << output_path << '\n';
            return 0;
        }

        print_usage();
        return 1;
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << '\n';
        return 1;
    }
}
