#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import struct
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, scratch_dir

from dj1000_convert_original import (
    compile_harness,
    dump_geometry_stage,
    dump_normalize_stage,
    dump_post_geometry_center_scale,
    dump_post_geometry_dual_scale,
    dump_post_geometry_edge_response,
    dump_post_geometry_filter,
    dump_post_geometry_prepare,
    dump_post_geometry_rgb_convert,
    dump_post_geometry_stage_2a00,
    dump_post_geometry_stage_2dd0,
    dump_post_geometry_stage_3060,
    dump_post_geometry_stage_3600,
    dump_post_geometry_stage_3890,
    dump_post_geometry_stage_4810,
    dump_post_rgb_stage_40f0,
    dump_post_rgb_stage_42a0,
    dump_post_rgb_stage_3b00,
    dump_source_stage,
    trace_large_callsite,
)
from verify_native_nonlarge_source_pipeline import build_geometry_input_plane, quantize_float_plane


RAW_BLOCK_SIZE = 0x1F600
TRAILER_SIZE = 13
SOURCE_BYTES = 0x1E800
SOURCE_GAIN_REGION_OFFSET = 0x8494
SOURCE_GAIN_REGION_WIDTH = 0x0EC
SOURCE_GAIN_REGION_ROWS = 0x077
SOURCE_GAIN_REGION_STRIDE = 0x200
SOURCE_GAIN_WRITABLE_WIDTH = 0x1FA
SOURCE_GAIN_WRITABLE_ROWS = 0x0F4
SOURCE_GAIN_TARGET = 18
SOURCE_GAIN_SCALE = 0.01 * 255.0
SOURCE_GAIN_MAX = 5.0
STAGE_PARAM1_BASE = [20, 20, 20, 20, 22, 24, 27]
STAGE_THRESHOLD_BASE = [0, 35, 30, 25, 20, 15, 10]
TRAILER_SCALAR = {
    54: 16.09,
    61: 15.19,
    64: 14.35,
    67: 13.55,
    72: 12.80,
    79: 12.09,
    83: 11.42,
    87: 10.79,
    90: 10.19,
    92: 9.62,
    95: 9.09,
    97: 8.58,
    102: 8.11,
    110: 7.66,
    114: 7.23,
    117: 6.83,
    120: 6.45,
    123: 6.09,
    128: 5.75,
    132: 5.44,
    136: 5.13,
    140: 4.85,
    144: 4.58,
    149: 4.33,
    154: 4.09,
    166: 3.86,
    179: 3.64,
    192: 3.44,
    205: 3.25,
    218: 3.07,
    230: 2.90,
}


def trunc_to_int(value: float) -> int:
    if not math.isfinite(value):
        return 0
    return int(math.trunc(value))


def compute_source_gain(source: bytes) -> float:
    region_sum = 0
    for row in range(SOURCE_GAIN_REGION_ROWS):
        row_offset = SOURCE_GAIN_REGION_OFFSET + (row * SOURCE_GAIN_REGION_STRIDE)
        region_sum += sum(source[row_offset:row_offset + SOURCE_GAIN_REGION_WIDTH])
    average = region_sum // (SOURCE_GAIN_REGION_WIDTH * SOURCE_GAIN_REGION_ROWS)
    target_level = SOURCE_GAIN_TARGET * SOURCE_GAIN_SCALE
    if average >= target_level:
        return 1.0
    if not (average > 0.0):
        return SOURCE_GAIN_MAX
    return min(SOURCE_GAIN_MAX, target_level / average)


def apply_source_gain(source: bytes, gain: float) -> bytes:
    output = bytearray(source)
    for row in range(SOURCE_GAIN_WRITABLE_ROWS):
        row_offset = row * 0x200
        for col in range(SOURCE_GAIN_WRITABLE_WIDTH):
            index = row_offset + col
            output[index] = max(0, min(255, trunc_to_int(output[index] * gain)))
    return bytes(output)


def compute_trailer_scalar(trailer_value: int) -> float:
    raw = TRAILER_SCALAR.get(trailer_value, 16.0)
    return min(2.0, raw * 0.125)


def compare_path(label: str, left: Path, right: Path) -> None:
    left_bytes = left.read_bytes()
    right_bytes = right.read_bytes()
    if left.suffix == ".f64" and right.suffix == ".f64":
        if len(left_bytes) != len(right_bytes):
            raise RuntimeError(f"mismatch at {label}\n  dll:    {left}\n  native: {right}")
        left_values = struct.unpack("<" + "d" * (len(left_bytes) // 8), left_bytes)
        right_values = struct.unpack("<" + "d" * (len(right_bytes) // 8), right_bytes)
        max_difference = max(abs(left_value - right_value) for left_value, right_value in zip(left_values, right_values))
        if max_difference > 1.0e-12:
            raise RuntimeError(
                f"mismatch at {label} max_diff={max_difference}\n  dll:    {left}\n  native: {right}"
            )
    elif left_bytes != right_bytes:
        raise RuntimeError(f"mismatch at {label}\n  dll:    {left}\n  native: {right}")
    print(f"OK {label}")


def run_native(native_binary: Path, args: list[str]) -> None:
    subprocess.run([str(native_binary), *args], check=True)


def crop_geometry_plane(source_path: Path, output_path: Path, sample_count: int) -> Path:
    data = source_path.read_bytes()
    cropped = data[:sample_count]
    output_path.write_bytes(cropped)
    return output_path


def derive_small_reference_bmp(dat_path: Path) -> Path:
    return dat_path.parent / f"{dat_path.stem.lower()}s.bmp"


def extract_bmp_pixel_bytes(bmp_path: Path, output_path: Path) -> Path:
    bmp = bmp_path.read_bytes()
    if bmp[:2] != b"BM":
        raise RuntimeError(f"not a BMP: {bmp_path}")
    pixel_offset = struct.unpack_from("<I", bmp, 10)[0]
    dib_header_size = struct.unpack_from("<I", bmp, 14)[0]
    width = struct.unpack_from("<i", bmp, 18)[0]
    height = struct.unpack_from("<i", bmp, 22)[0]
    bpp = struct.unpack_from("<H", bmp, 28)[0]
    if dib_header_size != 40 or bpp != 24:
        raise RuntimeError(f"unexpected BMP layout in {bmp_path}")
    stride = ((abs(width) * 3) + 3) & ~3
    pixel_bytes = stride * abs(height)
    output_path.write_bytes(bmp[pixel_offset:pixel_offset + pixel_bytes])
    return output_path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Trace the default normal-export sample stage by stage against the original DLL."
    )
    parser.add_argument("dat_path", type=Path)
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("default_normal_export_trace"),
    )
    parser.add_argument("--reference-bmp", type=Path)
    parser.add_argument("--crop-top", type=int, default=3)
    parser.add_argument("--stage3060-param0", type=int)
    parser.add_argument("--stage3060-param1", type=int)
    parser.add_argument("--stage3060-scalar", type=float)
    parser.add_argument("--stage3060-threshold", type=int)
    parser.add_argument("--probe-top-level-callsite", action="store_true")
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    dat = args.dat_path.read_bytes()
    if len(dat) != 0x20000:
        raise RuntimeError("expected a 0x20000-byte DAT")
    trailer = dat[RAW_BLOCK_SIZE:RAW_BLOCK_SIZE + TRAILER_SIZE]
    source = dat[:SOURCE_BYTES]

    source_gain = compute_source_gain(source)
    source_with_gain = apply_source_gain(source, source_gain)
    source_path = args.scratch / "source_with_gain.bin"
    source_path.write_bytes(source_with_gain)

    sharpness = 3
    vividness = 3
    sharpness_scalar = max(1.0, min(1.5, source_gain))
    stage3060_scalar = 0.0 if source_gain < 1.5 else (30.0 if source_gain >= 4.0 else (source_gain - 1.5) * 12.0)
    trailer_scalar = compute_trailer_scalar(trailer[9])
    stage3060_param0 = 16
    stage3060_param1 = trunc_to_int(STAGE_PARAM1_BASE[sharpness] * sharpness_scalar)
    if trailer_scalar < 1.0:
        stage3060_param1 += 10
    stage3060_threshold = STAGE_THRESHOLD_BASE[sharpness] - trunc_to_int((1.0 - sharpness_scalar) * 10.0)
    if args.stage3060_param0 is not None:
        stage3060_param0 = args.stage3060_param0
    if args.stage3060_param1 is not None:
        stage3060_param1 = args.stage3060_param1
    if args.stage3060_scalar is not None:
        stage3060_scalar = args.stage3060_scalar
    if args.stage3060_threshold is not None:
        stage3060_threshold = args.stage3060_threshold

    print(
        f"source_gain={source_gain} sharpness_scalar={sharpness_scalar} "
        f"stage3060_param0={stage3060_param0} "
        f"stage3060_scalar={stage3060_scalar} stage3060_param1={stage3060_param1} "
        f"stage3060_threshold={stage3060_threshold}"
    )

    source_prefix = args.scratch / "dll.source"
    source_outputs = dump_source_stage(source_path, source_prefix, preview_flag=0)
    normalize_prefix = args.scratch / "dll.normalize"
    normalized_outputs = dump_normalize_stage(
        source_outputs[0], source_outputs[1], source_outputs[2], normalize_prefix, preview_flag=0
    )

    geometry_inputs: list[Path] = []
    for plane_index, normalized_path in enumerate(normalized_outputs):
        quantized = quantize_float_plane(normalized_path)
        geometry_input_path = args.scratch / f"dll.geometry-input.plane{plane_index}.bin"
        geometry_input_path.write_bytes(build_geometry_input_plane(quantized))
        geometry_inputs.append(geometry_input_path)

    geometry_prefix = args.scratch / "dll.geometry"
    geometry_outputs = dump_geometry_stage(
        geometry_inputs[0],
        geometry_inputs[1],
        geometry_inputs[2],
        geometry_prefix,
        preview_flag=0,
        export_mode=2,
        geometry_selector=1,
    )

    geometry_244_outputs: list[Path] = []
    for plane_index, geometry_output in enumerate(geometry_outputs):
        geometry_244_outputs.append(
            crop_geometry_plane(
                geometry_output,
                args.scratch / f"dll.geometry244.plane{plane_index}.bin",
                320 * 244,
            )
        )

    prepare_prefix = args.scratch / "dll.prepare"
    dll_center, dll_delta0, dll_delta2 = dump_post_geometry_prepare(
        geometry_244_outputs[0], geometry_244_outputs[1], geometry_244_outputs[2], prepare_prefix, 320, 244
    )
    filter_prefix = args.scratch / "dll.filter"
    dll_delta0, dll_delta2 = dump_post_geometry_filter(dll_delta0, dll_delta2, filter_prefix, 320, 244)

    native_prepare_prefix = args.scratch / "native.prepare"
    run_native(
        args.native_binary,
        ["nonlarge-post-geometry-source", "0", str(source_path), str(native_prepare_prefix)],
    )
    compare_path("prepare.center", dll_center, args.scratch / "native.prepare.center.f64")
    compare_path("prepare.delta0", dll_delta0, args.scratch / "native.prepare.delta0.f64")
    compare_path("prepare.delta2", dll_delta2, args.scratch / "native.prepare.delta2.f64")

    dll_4810_prefix = args.scratch / "dll.4810"
    dll_delta0, dll_center, dll_delta2 = dump_post_geometry_stage_4810(
        dll_delta0, dll_center, dll_delta2, dll_4810_prefix, 320, 244
    )
    native_4810_prefix = args.scratch / "native.4810"
    run_native(
        args.native_binary,
        [
            "post-geometry-stage-4810",
            "320",
            "244",
            str(args.scratch / "native.prepare.delta0.f64"),
            str(args.scratch / "native.prepare.center.f64"),
            str(args.scratch / "native.prepare.delta2.f64"),
            str(native_4810_prefix),
        ],
    )
    compare_path("4810.center", dll_center, args.scratch / "native.4810.plane1.f64")
    compare_path("4810.delta0", dll_delta0, args.scratch / "native.4810.plane0.f64")
    compare_path("4810.delta2", dll_delta2, args.scratch / "native.4810.plane2.f64")

    dll_3600_center = dump_post_geometry_stage_3600(dll_center, args.scratch / "dll.3600.center.f64", 320, 244)
    native_3600_center = args.scratch / "native.3600.center.f64"
    run_native(
        args.native_binary,
        ["post-geometry-stage-3600", "320", "244", str(args.scratch / "native.4810.plane1.f64"), str(native_3600_center)],
    )
    compare_path("3600.center", dll_3600_center, native_3600_center)
    dll_center = dll_3600_center

    dll_edge = dump_post_geometry_edge_response(dll_center, args.scratch / "dll.edge.i32", 320, 244)
    native_edge = args.scratch / "native.edge.i32"
    run_native(
        args.native_binary,
        ["post-geometry-edge-response", "320", "244", str(native_3600_center), str(native_edge)],
    )
    compare_path("edge-response", dll_edge, native_edge)

    dll_center = dump_post_geometry_stage_2a00(dll_center, args.scratch / "dll.2a00.center.f64", 320, 244)
    native_2a00_center = args.scratch / "native.2a00.center.f64"
    run_native(
        args.native_binary,
        ["post-geometry-stage-2a00", "320", "244", str(native_3600_center), str(native_2a00_center)],
    )
    compare_path("2a00.center", dll_center, native_2a00_center)

    dll_2c40_prefix = args.scratch / "dll.2c40"
    dll_delta0, dll_center, dll_delta2 = dump_post_geometry_center_scale(
        dll_delta0, dll_center, dll_delta2, dll_2c40_prefix, 320, 244
    )
    native_2c40_prefix = args.scratch / "native.2c40"
    run_native(
        args.native_binary,
        [
            "post-geometry-center-scale",
            "320",
            "244",
            str(args.scratch / "native.4810.plane0.f64"),
            str(native_2a00_center),
            str(args.scratch / "native.4810.plane2.f64"),
            str(native_2c40_prefix),
        ],
    )
    compare_path("2c40.delta0", dll_delta0, args.scratch / "native.2c40.delta0.f64")
    compare_path("2c40.center", dll_center, args.scratch / "native.2c40.center.f64")
    compare_path("2c40.delta2", dll_delta2, args.scratch / "native.2c40.delta2.f64")

    dll_2dd0_prefix = args.scratch / "dll.2dd0"
    dll_delta0, dll_delta2 = dump_post_geometry_stage_2dd0(dll_delta0, dll_delta2, dll_2dd0_prefix, 320, 244)
    native_2dd0_prefix = args.scratch / "native.2dd0"
    run_native(
        args.native_binary,
        [
            "post-geometry-stage-2dd0",
            "320",
            "244",
            str(args.scratch / "native.2c40.delta0.f64"),
            str(args.scratch / "native.2c40.delta2.f64"),
            str(native_2dd0_prefix),
        ],
    )
    compare_path("2dd0.delta0", dll_delta0, args.scratch / "native.2dd0.plane0.f64")
    compare_path("2dd0.delta2", dll_delta2, args.scratch / "native.2dd0.plane1.f64")

    dll_3890_prefix = args.scratch / "dll.3890"
    dll_delta0, dll_delta2 = dump_post_geometry_stage_3890(dll_delta0, dll_delta2, dll_3890_prefix, 320, 244, vividness)
    native_3890_prefix = args.scratch / "native.3890"
    run_native(
        args.native_binary,
        [
            "post-geometry-stage-3890",
            "320",
            "244",
            str(vividness),
            str(args.scratch / "native.2dd0.plane0.f64"),
            str(args.scratch / "native.2dd0.plane1.f64"),
            str(native_3890_prefix),
        ],
    )
    compare_path("3890.delta0", dll_delta0, args.scratch / "native.3890.plane0.f64")
    compare_path("3890.delta2", dll_delta2, args.scratch / "native.3890.plane1.f64")

    dll_3060_center = dump_post_geometry_stage_3060(
        dll_edge,
        dll_center,
        args.scratch / "dll.3060.center.f64",
        320,
        244,
        stage3060_param0,
        stage3060_param1,
        stage3060_scalar,
        stage3060_threshold,
    )
    native_3060_center = args.scratch / "native.3060.center.f64"
    run_native(
        args.native_binary,
        [
            "post-geometry-stage-3060",
            "320",
            "244",
            str(native_edge),
            str(args.scratch / "native.2c40.center.f64"),
            str(stage3060_param0),
            str(stage3060_param1),
            str(stage3060_scalar),
            str(stage3060_threshold),
            str(native_3060_center),
        ],
    )
    compare_path("3060.center", dll_3060_center, native_3060_center)
    dll_center = dll_3060_center

    dll_4450_prefix = args.scratch / "dll.4450"
    dll_delta0, dll_delta2 = dump_post_geometry_dual_scale(
        dll_delta0,
        dll_delta2,
        dll_4450_prefix,
        320,
        244,
        sharpness_scalar,
    )
    native_4450_prefix = args.scratch / "native.4450"
    run_native(
        args.native_binary,
        [
            "post-geometry-dual-scale",
            "320",
            "244",
            str(args.scratch / "native.3890.plane0.f64"),
            str(args.scratch / "native.3890.plane1.f64"),
            str(sharpness_scalar),
            str(native_4450_prefix),
        ],
    )
    compare_path("4450.delta0", dll_delta0, args.scratch / "native.4450.plane0.f64")
    compare_path("4450.delta2", dll_delta2, args.scratch / "native.4450.plane1.f64")

    dll_rgb_prefix = args.scratch / "dll.rgb"
    dll_plane0, dll_plane1, dll_plane2 = dump_post_geometry_rgb_convert(
        dll_delta0, dll_center, dll_delta2, dll_rgb_prefix, 320, 244
    )
    native_rgb_prefix = args.scratch / "native.rgb"
    run_native(
        args.native_binary,
        [
            "post-geometry-rgb-convert",
            "320",
            "244",
            str(args.scratch / "native.4450.plane0.f64"),
            str(native_3060_center),
            str(args.scratch / "native.4450.plane1.f64"),
            str(native_rgb_prefix),
        ],
    )
    compare_path("2530.plane0", dll_plane0, args.scratch / "native.rgb.plane0.bin")
    compare_path("2530.plane1", dll_plane1, args.scratch / "native.rgb.plane1.bin")
    compare_path("2530.plane2", dll_plane2, args.scratch / "native.rgb.plane2.bin")

    dll_42a0_prefix = args.scratch / "dll.42a0"
    dll_plane0, dll_plane1, dll_plane2 = dump_post_rgb_stage_42a0(
        dll_plane0,
        dll_plane1,
        dll_plane2,
        dll_42a0_prefix,
        320,
        244,
        100,
        100,
        100,
    )
    native_42a0_prefix = args.scratch / "native.42a0"
    run_native(
        args.native_binary,
        [
            "post-rgb-stage-42a0",
            "320",
            "244",
            "100",
            "100",
            "100",
            str(args.scratch / "native.rgb.plane0.bin"),
            str(args.scratch / "native.rgb.plane1.bin"),
            str(args.scratch / "native.rgb.plane2.bin"),
            str(native_42a0_prefix),
        ],
    )
    compare_path("42a0.plane0", dll_plane0, args.scratch / "native.42a0.plane0.bin")
    compare_path("42a0.plane1", dll_plane1, args.scratch / "native.42a0.plane1.bin")
    compare_path("42a0.plane2", dll_plane2, args.scratch / "native.42a0.plane2.bin")

    dll_3b00_prefix = args.scratch / "dll.3b00"
    dll_plane0, dll_plane1, dll_plane2 = dump_post_rgb_stage_3b00(
        dll_plane0, dll_plane1, dll_plane2, dll_3b00_prefix, 320, 244, 3, stage3060_scalar
    )
    native_3b00_prefix = args.scratch / "native.3b00"
    run_native(
        args.native_binary,
        [
            "post-rgb-stage-3b00",
            "320",
            "244",
            "3",
            str(stage3060_scalar),
            str(args.scratch / "native.42a0.plane0.bin"),
            str(args.scratch / "native.42a0.plane1.bin"),
            str(args.scratch / "native.42a0.plane2.bin"),
            str(native_3b00_prefix),
        ],
    )
    compare_path("3b00.plane0", dll_plane0, args.scratch / "native.3b00.plane0.bin")
    compare_path("3b00.plane1", dll_plane1, args.scratch / "native.3b00.plane1.bin")
    compare_path("3b00.plane2", dll_plane2, args.scratch / "native.3b00.plane2.bin")

    dll_40f0_contrast_prefix = args.scratch / "dll.40f0.contrast"
    dll_plane0, dll_plane1, dll_plane2 = dump_post_rgb_stage_40f0(
        dll_plane0,
        dll_plane1,
        dll_plane2,
        dll_40f0_contrast_prefix,
        320,
        244,
        3,
        0,
    )
    native_40f0_contrast_prefix = args.scratch / "native.40f0.contrast"
    run_native(
        args.native_binary,
        [
            "post-rgb-stage-40f0",
            "320",
            "244",
            "3",
            "0",
            str(args.scratch / "native.3b00.plane0.bin"),
            str(args.scratch / "native.3b00.plane1.bin"),
            str(args.scratch / "native.3b00.plane2.bin"),
            str(native_40f0_contrast_prefix),
        ],
    )
    compare_path("40f0.contrast.plane0", dll_plane0, args.scratch / "native.40f0.contrast.plane0.bin")
    compare_path("40f0.contrast.plane1", dll_plane1, args.scratch / "native.40f0.contrast.plane1.bin")
    compare_path("40f0.contrast.plane2", dll_plane2, args.scratch / "native.40f0.contrast.plane2.bin")

    dll_40f0_brightness_prefix = args.scratch / "dll.40f0.brightness"
    dll_plane0, dll_plane1, dll_plane2 = dump_post_rgb_stage_40f0(
        dll_plane0,
        dll_plane1,
        dll_plane2,
        dll_40f0_brightness_prefix,
        320,
        244,
        3,
        1,
    )
    native_40f0_brightness_prefix = args.scratch / "native.40f0.brightness"
    run_native(
        args.native_binary,
        [
            "post-rgb-stage-40f0",
            "320",
            "244",
            "3",
            "1",
            str(args.scratch / "native.40f0.contrast.plane0.bin"),
            str(args.scratch / "native.40f0.contrast.plane1.bin"),
            str(args.scratch / "native.40f0.contrast.plane2.bin"),
            str(native_40f0_brightness_prefix),
        ],
    )
    compare_path("40f0.brightness.plane0", dll_plane0, args.scratch / "native.40f0.brightness.plane0.bin")
    compare_path("40f0.brightness.plane1", dll_plane1, args.scratch / "native.40f0.brightness.plane1.bin")
    compare_path("40f0.brightness.plane2", dll_plane2, args.scratch / "native.40f0.brightness.plane2.bin")

    reference_bmp = args.reference_bmp if args.reference_bmp is not None else derive_small_reference_bmp(args.dat_path)

    if args.probe_top_level_callsite:
        trace_output = args.scratch / "dll.top_level.bmp"
        trace_large_callsite(args.dat_path, trace_output, export_mode=2, converter_mode=0)
        compare_path(
            "top-level.42a0.plane0",
            Path(str(trace_output) + ".42a0.call1.post.plane0.bin"),
            args.scratch / "native.42a0.plane0.bin",
        )
        compare_path(
            "top-level.42a0.plane1",
            Path(str(trace_output) + ".42a0.call1.post.plane1.bin"),
            args.scratch / "native.42a0.plane1.bin",
        )
        compare_path(
            "top-level.42a0.plane2",
            Path(str(trace_output) + ".42a0.call1.post.plane2.bin"),
            args.scratch / "native.42a0.plane2.bin",
        )
        compare_path(
            "top-level.3b00.plane0",
            Path(str(trace_output) + ".3b00.call1.post.plane0.bin"),
            args.scratch / "native.3b00.plane0.bin",
        )
        compare_path(
            "top-level.3b00.plane1",
            Path(str(trace_output) + ".3b00.call1.post.plane1.bin"),
            args.scratch / "native.3b00.plane1.bin",
        )
        compare_path(
            "top-level.3b00.plane2",
            Path(str(trace_output) + ".3b00.call1.post.plane2.bin"),
            args.scratch / "native.3b00.plane2.bin",
        )
        compare_path(
            "top-level.40f0.selector0.plane0",
            Path(str(trace_output) + ".40f0.call1.selector0.post.plane0.bin"),
            args.scratch / "native.40f0.contrast.plane0.bin",
        )
        compare_path(
            "top-level.40f0.selector0.plane1",
            Path(str(trace_output) + ".40f0.call1.selector0.post.plane1.bin"),
            args.scratch / "native.40f0.contrast.plane1.bin",
        )
        compare_path(
            "top-level.40f0.selector0.plane2",
            Path(str(trace_output) + ".40f0.call1.selector0.post.plane2.bin"),
            args.scratch / "native.40f0.contrast.plane2.bin",
        )
        compare_path(
            "top-level.40f0.selector1.plane0",
            Path(str(trace_output) + ".40f0.call2.selector1.post.plane0.bin"),
            args.scratch / "native.40f0.brightness.plane0.bin",
        )
        compare_path(
            "top-level.40f0.selector1.plane1",
            Path(str(trace_output) + ".40f0.call2.selector1.post.plane1.bin"),
            args.scratch / "native.40f0.brightness.plane1.bin",
        )
        compare_path(
            "top-level.40f0.selector1.plane2",
            Path(str(trace_output) + ".40f0.call2.selector1.post.plane2.bin"),
            args.scratch / "native.40f0.brightness.plane2.bin",
        )
        if reference_bmp.exists():
            reference_pixels = extract_bmp_pixel_bytes(
                reference_bmp,
                args.scratch / "reference.pixeldata.bin",
            )
            compare_path(
                "top-level.pixeldata",
                reference_pixels,
                Path(str(trace_output) + ".final.pixeldata.bin"),
            )

    if reference_bmp.exists():
        native_final_bmp = args.scratch / "native.final.bmp"
        final_command = ["normal-export-bmp", str(args.dat_path), str(native_final_bmp)]
        using_tuned_export = (
            args.crop_top != 3 or
            args.stage3060_param0 is not None or
            args.stage3060_param1 is not None or
            args.stage3060_scalar is not None or
            args.stage3060_threshold is not None
        )
        if using_tuned_export:
            final_command = [
                "normal-export-bmp-tuned",
                str(args.dat_path),
                str(native_final_bmp),
                str(args.crop_top),
                str(stage3060_param0),
                str(stage3060_param1),
                str(stage3060_scalar),
                str(stage3060_threshold),
            ]
        run_native(args.native_binary, final_command)
        compare_path("final.bmp", reference_bmp, native_final_bmp)

    print("default normal export trace matched through the final normal-export BMP")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
