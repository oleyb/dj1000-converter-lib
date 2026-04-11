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
    dump_post_rgb_stage_3f40,
    dump_post_rgb_stage_42a0,
    dump_post_rgb_stage_3b00,
    dump_source_stage,
    trace_large_callsite,
)
from verify_native_nonlarge_source_pipeline import build_geometry_input_plane, quantize_float_plane


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
LARGE_WIDTH = 504
LARGE_HEIGHT = 378
DEFAULT_STAGE3060_PARAM0 = 16
DEFAULT_STAGE3060_PARAM1 = 40
DEFAULT_STAGE3060_THRESHOLD = 20


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


def compute_large_stage_3060_scalar(source_gain: float) -> float:
    if source_gain < 1.5:
        return 0.0
    if source_gain >= 4.0:
        return 30.0
    return (source_gain - 1.5) * 12.0


def compute_large_stage_3060_default_params(
    source_gain: float,
) -> tuple[int, int, float, int]:
    param0 = DEFAULT_STAGE3060_PARAM0
    param1 = DEFAULT_STAGE3060_PARAM1
    scalar = compute_large_stage_3060_scalar(source_gain)
    threshold = DEFAULT_STAGE3060_THRESHOLD
    if source_gain < 1.5:
        scalar = source_gain
        param1 = max(20, trunc_to_int(source_gain * 20.0))
        threshold = 25 - max(0, (param1 - 20) // 2)
    return param0, param1, scalar, threshold


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


def trim_path(source: Path, output: Path, size: int) -> Path:
    output.write_bytes(source.read_bytes()[:size])
    return output


def interleave_bgr_planes(
    plane0_path: Path,
    plane1_path: Path,
    plane2_path: Path,
    output_path: Path,
) -> Path:
    plane0 = plane0_path.read_bytes()
    plane1 = plane1_path.read_bytes()
    plane2 = plane2_path.read_bytes()
    if not (len(plane0) == len(plane1) == len(plane2)):
        raise RuntimeError("plane sizes must match for BGR interleave")
    output = bytearray(len(plane0) * 3)
    dst = 0
    for index in range(len(plane0)):
        output[dst] = plane2[index]
        output[dst + 1] = plane1[index]
        output[dst + 2] = plane0[index]
        dst += 3
    output_path.write_bytes(output)
    return output_path


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
        description="Trace the default large-export sample stage by stage against the original DLL."
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
        default=scratch_dir("default_large_export_trace"),
    )
    parser.add_argument("--reference-bmp", type=Path)
    parser.add_argument("--stage3060-param0", type=int)
    parser.add_argument("--stage3060-param1", type=int)
    parser.add_argument("--stage3060-scalar", type=float)
    parser.add_argument("--stage3060-threshold", type=int)
    parser.add_argument("--source-gain", type=float)
    parser.add_argument("--red-balance", type=int, default=100)
    parser.add_argument("--green-balance", type=int, default=100)
    parser.add_argument("--blue-balance", type=int, default=100)
    parser.add_argument("--contrast", type=int, default=3)
    parser.add_argument("--brightness", type=int, default=3)
    parser.add_argument("--vividness", type=int, default=3)
    parser.add_argument("--sharpness", type=int, default=3)
    parser.add_argument("--probe-3f40", action="store_true")
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
    source = dat[:SOURCE_BYTES]

    source_gain = compute_source_gain(source)
    if args.source_gain is not None:
        source_gain = args.source_gain
    source_with_gain = apply_source_gain(source, source_gain)
    source_path = args.scratch / "source_with_gain.bin"
    source_path.write_bytes(source_with_gain)

    sharpness = args.sharpness
    vividness = args.vividness
    balance_red = args.red_balance
    balance_green = args.green_balance
    balance_blue = args.blue_balance
    contrast = args.contrast
    brightness = args.brightness
    sharpness_scalar = max(1.0, min(1.5, source_gain))
    post_rgb_scalar = compute_large_stage_3060_scalar(source_gain)
    stage3060_param0, stage3060_param1, stage3060_scalar, stage3060_threshold = (
        compute_large_stage_3060_default_params(source_gain)
    )
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
        f"stage3060_threshold={stage3060_threshold} post_rgb_scalar={post_rgb_scalar} "
        f"rgb=({balance_red},{balance_green},{balance_blue}) "
        f"contrast={contrast} brightness={brightness} vividness={vividness} sharpness={sharpness}"
    )

    source_outputs = dump_source_stage(source_path, args.scratch / "dll.source", preview_flag=0)
    normalized_outputs = dump_normalize_stage(
        source_outputs[0], source_outputs[1], source_outputs[2], args.scratch / "dll.normalize", preview_flag=0
    )

    geometry_inputs: list[Path] = []
    for plane_index, normalized_path in enumerate(normalized_outputs):
        quantized = quantize_float_plane(normalized_path)
        geometry_input_path = args.scratch / f"dll.geometry-input.plane{plane_index}.bin"
        geometry_input_path.write_bytes(build_geometry_input_plane(quantized))
        geometry_inputs.append(geometry_input_path)

    geometry_outputs = dump_geometry_stage(
        geometry_inputs[0],
        geometry_inputs[1],
        geometry_inputs[2],
        args.scratch / "dll.geometry",
        preview_flag=0,
        export_mode=5,
        geometry_selector=1,
    )
    trimmed_geometry_outputs = [
        trim_path(
            geometry_outputs[plane_index],
            args.scratch / f"dll.geometry.trimmed.plane{plane_index}.bin",
            LARGE_WIDTH * LARGE_HEIGHT,
        )
        for plane_index in range(3)
    ]

    native_stage_prefix = args.scratch / "native.large"
    run_native(
        args.native_binary,
        ["pregeometry-pipeline", "0", str(source_path), str(args.scratch / "native.pre")],
    )
    run_native(
        args.native_binary,
        [
            "quantize-stage",
            str(args.scratch / "native.pre.plane0.f32"),
            str(args.scratch / "native.pre.plane1.f32"),
            str(args.scratch / "native.pre.plane2.f32"),
            str(args.scratch / "native.quant"),
        ],
    )
    for plane_index in range(3):
        run_native(
            args.native_binary,
            [
                "large-stage-plane",
                str(args.scratch / f"native.quant.plane{plane_index}.bin"),
                str(args.scratch / f"native.large.plane{plane_index}.bin"),
            ],
        )
        compare_path(
            f"geometry.plane{plane_index}",
            trimmed_geometry_outputs[plane_index],
            args.scratch / f"native.large.plane{plane_index}.bin",
        )

    dll_center, dll_delta0, dll_delta2 = dump_post_geometry_prepare(
        trimmed_geometry_outputs[0],
        trimmed_geometry_outputs[1],
        trimmed_geometry_outputs[2],
        args.scratch / "dll.prepare",
        LARGE_WIDTH,
        LARGE_HEIGHT,
    )
    run_native(
        args.native_binary,
        [
            "post-geometry-prepare",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(args.scratch / "native.large.plane0.bin"),
            str(args.scratch / "native.large.plane1.bin"),
            str(args.scratch / "native.large.plane2.bin"),
            str(args.scratch / "native.prepare"),
        ],
    )
    compare_path("prepare.center", dll_center, args.scratch / "native.prepare.center.f64")
    compare_path("prepare.delta0", dll_delta0, args.scratch / "native.prepare.delta0.f64")
    compare_path("prepare.delta2", dll_delta2, args.scratch / "native.prepare.delta2.f64")

    dll_delta0, dll_delta2 = dump_post_geometry_filter(
        dll_delta0, dll_delta2, args.scratch / "dll.filter", LARGE_WIDTH, LARGE_HEIGHT
    )
    run_native(
        args.native_binary,
        [
            "post-geometry-filter",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(args.scratch / "native.prepare.delta0.f64"),
            str(args.scratch / "native.prepare.delta2.f64"),
            str(args.scratch / "native.filter"),
        ],
    )
    compare_path("filter.delta0", dll_delta0, args.scratch / "native.filter.delta0.f64")
    compare_path("filter.delta2", dll_delta2, args.scratch / "native.filter.delta2.f64")

    dll_delta0, dll_center, dll_delta2 = dump_post_geometry_stage_4810(
        dll_delta0, dll_center, dll_delta2, args.scratch / "dll.4810", LARGE_WIDTH, LARGE_HEIGHT
    )
    run_native(
        args.native_binary,
        [
            "post-geometry-stage-4810",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(args.scratch / "native.filter.delta0.f64"),
            str(args.scratch / "native.prepare.center.f64"),
            str(args.scratch / "native.filter.delta2.f64"),
            str(args.scratch / "native.4810"),
        ],
    )
    compare_path("4810.center", dll_center, args.scratch / "native.4810.plane1.f64")
    compare_path("4810.delta0", dll_delta0, args.scratch / "native.4810.plane0.f64")
    compare_path("4810.delta2", dll_delta2, args.scratch / "native.4810.plane2.f64")

    dll_center = dump_post_geometry_stage_3600(dll_center, args.scratch / "dll.3600.center.f64", LARGE_WIDTH, LARGE_HEIGHT)
    run_native(
        args.native_binary,
        [
            "post-geometry-stage-3600",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(args.scratch / "native.4810.plane1.f64"),
            str(args.scratch / "native.3600.center.f64"),
        ],
    )
    compare_path("3600.center", dll_center, args.scratch / "native.3600.center.f64")

    dll_edge = dump_post_geometry_edge_response(dll_center, args.scratch / "dll.edge.i32", LARGE_WIDTH, LARGE_HEIGHT)
    run_native(
        args.native_binary,
        [
            "post-geometry-edge-response",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(args.scratch / "native.3600.center.f64"),
            str(args.scratch / "native.edge.i32"),
        ],
    )
    compare_path("edge-response", dll_edge, args.scratch / "native.edge.i32")

    dll_center = dump_post_geometry_stage_2a00(dll_center, args.scratch / "dll.2a00.center.f64", LARGE_WIDTH, LARGE_HEIGHT)
    run_native(
        args.native_binary,
        [
            "post-geometry-stage-2a00",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(args.scratch / "native.3600.center.f64"),
            str(args.scratch / "native.2a00.center.f64"),
        ],
    )
    compare_path("2a00.center", dll_center, args.scratch / "native.2a00.center.f64")

    dll_delta0, dll_center, dll_delta2 = dump_post_geometry_center_scale(
        dll_delta0, dll_center, dll_delta2, args.scratch / "dll.2c40", LARGE_WIDTH, LARGE_HEIGHT
    )
    run_native(
        args.native_binary,
        [
            "post-geometry-center-scale",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(args.scratch / "native.4810.plane0.f64"),
            str(args.scratch / "native.2a00.center.f64"),
            str(args.scratch / "native.4810.plane2.f64"),
            str(args.scratch / "native.2c40"),
        ],
    )
    compare_path("2c40.delta0", dll_delta0, args.scratch / "native.2c40.delta0.f64")
    compare_path("2c40.center", dll_center, args.scratch / "native.2c40.center.f64")
    compare_path("2c40.delta2", dll_delta2, args.scratch / "native.2c40.delta2.f64")

    dll_delta0, dll_delta2 = dump_post_geometry_stage_2dd0(
        dll_delta0, dll_delta2, args.scratch / "dll.2dd0", LARGE_WIDTH, LARGE_HEIGHT
    )
    run_native(
        args.native_binary,
        [
            "post-geometry-stage-2dd0",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(args.scratch / "native.2c40.delta0.f64"),
            str(args.scratch / "native.2c40.delta2.f64"),
            str(args.scratch / "native.2dd0"),
        ],
    )
    compare_path("2dd0.delta0", dll_delta0, args.scratch / "native.2dd0.plane0.f64")
    compare_path("2dd0.delta2", dll_delta2, args.scratch / "native.2dd0.plane1.f64")

    dll_delta0, dll_delta2 = dump_post_geometry_stage_3890(
        dll_delta0, dll_delta2, args.scratch / "dll.3890", LARGE_WIDTH, LARGE_HEIGHT, vividness
    )
    run_native(
        args.native_binary,
        [
            "post-geometry-stage-3890",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(vividness),
            str(args.scratch / "native.2dd0.plane0.f64"),
            str(args.scratch / "native.2dd0.plane1.f64"),
            str(args.scratch / "native.3890"),
        ],
    )
    compare_path("3890.delta0", dll_delta0, args.scratch / "native.3890.plane0.f64")
    compare_path("3890.delta2", dll_delta2, args.scratch / "native.3890.plane1.f64")

    dll_center = dump_post_geometry_stage_3060(
        dll_edge,
        dll_center,
        args.scratch / "dll.3060.center.f64",
        LARGE_WIDTH,
        LARGE_HEIGHT,
        stage3060_param0,
        stage3060_param1,
        stage3060_scalar,
        stage3060_threshold,
    )
    run_native(
        args.native_binary,
        [
            "post-geometry-stage-3060",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(args.scratch / "native.edge.i32"),
            str(args.scratch / "native.2c40.center.f64"),
            str(stage3060_param0),
            str(stage3060_param1),
            str(stage3060_scalar),
            str(stage3060_threshold),
            str(args.scratch / "native.3060.center.f64"),
        ],
    )
    compare_path("3060.center", dll_center, args.scratch / "native.3060.center.f64")

    dll_delta0, dll_delta2 = dump_post_geometry_dual_scale(
        dll_delta0, dll_delta2, args.scratch / "dll.4450", LARGE_WIDTH, LARGE_HEIGHT, sharpness_scalar
    )
    run_native(
        args.native_binary,
        [
            "post-geometry-dual-scale",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(args.scratch / "native.3890.plane0.f64"),
            str(args.scratch / "native.3890.plane1.f64"),
            str(sharpness_scalar),
            str(args.scratch / "native.4450"),
        ],
    )
    compare_path("4450.delta0", dll_delta0, args.scratch / "native.4450.plane0.f64")
    compare_path("4450.delta2", dll_delta2, args.scratch / "native.4450.plane1.f64")

    dll_plane0, dll_plane1, dll_plane2 = dump_post_geometry_rgb_convert(
        dll_delta0, dll_center, dll_delta2, args.scratch / "dll.rgb", LARGE_WIDTH, LARGE_HEIGHT
    )
    run_native(
        args.native_binary,
        [
            "post-geometry-rgb-convert",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(args.scratch / "native.4450.plane0.f64"),
            str(args.scratch / "native.3060.center.f64"),
            str(args.scratch / "native.4450.plane1.f64"),
            str(args.scratch / "native.rgb"),
        ],
    )
    compare_path("2530.plane0", dll_plane0, args.scratch / "native.rgb.plane0.bin")
    compare_path("2530.plane1", dll_plane1, args.scratch / "native.rgb.plane1.bin")
    compare_path("2530.plane2", dll_plane2, args.scratch / "native.rgb.plane2.bin")

    dll_plane0, dll_plane1, dll_plane2 = dump_post_rgb_stage_42a0(
        dll_plane0,
        dll_plane1,
        dll_plane2,
        args.scratch / "dll.42a0",
        LARGE_WIDTH,
        LARGE_HEIGHT,
        balance_red,
        balance_green,
        balance_blue,
    )
    run_native(
        args.native_binary,
        [
            "post-rgb-stage-42a0",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(balance_red),
            str(balance_green),
            str(balance_blue),
            str(args.scratch / "native.rgb.plane0.bin"),
            str(args.scratch / "native.rgb.plane1.bin"),
            str(args.scratch / "native.rgb.plane2.bin"),
            str(args.scratch / "native.42a0"),
        ],
    )
    compare_path("42a0.plane0", dll_plane0, args.scratch / "native.42a0.plane0.bin")
    compare_path("42a0.plane1", dll_plane1, args.scratch / "native.42a0.plane1.bin")
    compare_path("42a0.plane2", dll_plane2, args.scratch / "native.42a0.plane2.bin")

    dll_plane0, dll_plane1, dll_plane2 = dump_post_rgb_stage_3b00(
        dll_plane0,
        dll_plane1,
        dll_plane2,
        args.scratch / "dll.3b00",
        LARGE_WIDTH,
        LARGE_HEIGHT,
        contrast,
        post_rgb_scalar,
    )
    run_native(
        args.native_binary,
        [
            "post-rgb-stage-3b00",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(contrast),
            str(post_rgb_scalar),
            str(args.scratch / "native.42a0.plane0.bin"),
            str(args.scratch / "native.42a0.plane1.bin"),
            str(args.scratch / "native.42a0.plane2.bin"),
            str(args.scratch / "native.3b00"),
        ],
    )
    compare_path("3b00.plane0", dll_plane0, args.scratch / "native.3b00.plane0.bin")
    compare_path("3b00.plane1", dll_plane1, args.scratch / "native.3b00.plane1.bin")
    compare_path("3b00.plane2", dll_plane2, args.scratch / "native.3b00.plane2.bin")

    dll_plane0, dll_plane1, dll_plane2 = dump_post_rgb_stage_40f0(
        dll_plane0,
        dll_plane1,
        dll_plane2,
        args.scratch / "dll.40f0.contrast",
        LARGE_WIDTH,
        LARGE_HEIGHT,
        contrast,
        0,
    )
    run_native(
        args.native_binary,
        [
            "post-rgb-stage-40f0",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(contrast),
            "0",
            str(args.scratch / "native.3b00.plane0.bin"),
            str(args.scratch / "native.3b00.plane1.bin"),
            str(args.scratch / "native.3b00.plane2.bin"),
            str(args.scratch / "native.40f0.contrast"),
        ],
    )
    compare_path("40f0.contrast.plane0", dll_plane0, args.scratch / "native.40f0.contrast.plane0.bin")
    compare_path("40f0.contrast.plane1", dll_plane1, args.scratch / "native.40f0.contrast.plane1.bin")
    compare_path("40f0.contrast.plane2", dll_plane2, args.scratch / "native.40f0.contrast.plane2.bin")

    dll_plane0, dll_plane1, dll_plane2 = dump_post_rgb_stage_40f0(
        dll_plane0,
        dll_plane1,
        dll_plane2,
        args.scratch / "dll.40f0.brightness",
        LARGE_WIDTH,
        LARGE_HEIGHT,
        brightness,
        1,
    )
    run_native(
        args.native_binary,
        [
            "post-rgb-stage-40f0",
            str(LARGE_WIDTH),
            str(LARGE_HEIGHT),
            str(brightness),
            "1",
            str(args.scratch / "native.40f0.contrast.plane0.bin"),
            str(args.scratch / "native.40f0.contrast.plane1.bin"),
            str(args.scratch / "native.40f0.contrast.plane2.bin"),
            str(args.scratch / "native.40f0.brightness"),
        ],
    )
    compare_path("40f0.brightness.plane0", dll_plane0, args.scratch / "native.40f0.brightness.plane0.bin")
    compare_path("40f0.brightness.plane1", dll_plane1, args.scratch / "native.40f0.brightness.plane1.bin")
    compare_path("40f0.brightness.plane2", dll_plane2, args.scratch / "native.40f0.brightness.plane2.bin")

    if args.probe_3f40:
        dll_plane0, dll_plane1, dll_plane2 = dump_post_rgb_stage_3f40(
            dll_plane0,
            dll_plane1,
            dll_plane2,
            args.scratch / "dll.3f40",
            LARGE_WIDTH,
            LARGE_HEIGHT,
            3,
        )
        interleave_bgr_planes(
            dll_plane0,
            dll_plane1,
            dll_plane2,
            args.scratch / "dll.3f40.bgr.bin",
        )
        print(f"dumped 3f40 probe to {args.scratch / 'dll.3f40.bgr.bin'}")
        if args.reference_bmp is not None:
            reference_pixels = extract_bmp_pixel_bytes(
                args.reference_bmp,
                args.scratch / "reference.pixeldata.bin",
            )
            compare_path("3f40.pixeldata", reference_pixels, args.scratch / "dll.3f40.bgr.bin")

    if args.probe_top_level_callsite:
        trace_output = args.scratch / "dll.top_level.bmp"
        trace_large_callsite(
            args.dat_path,
            trace_output,
            settings={
                0: balance_green,
                1: balance_red,
                2: balance_blue,
                3: contrast,
                4: brightness,
                5: vividness,
                6: sharpness,
            },
        )
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
        if args.reference_bmp is not None:
            reference_pixels = extract_bmp_pixel_bytes(
                args.reference_bmp,
                args.scratch / "reference.pixeldata.bin",
            )
            compare_path(
                "top-level.pixeldata",
                reference_pixels,
                Path(str(trace_output) + ".final.pixeldata.bin"),
            )

    if args.reference_bmp is not None:
        final_command: list[str]
        if (
            args.source_gain is not None or
            args.stage3060_param0 is not None or
            args.stage3060_param1 is not None or
            args.stage3060_scalar is not None or
            args.stage3060_threshold is not None
        ):
            if args.source_gain is not None:
                final_command = [
                    "large-export-bmp-tuned-gain",
                    str(args.dat_path),
                    str(args.scratch / "native.final.bmp"),
                    str(source_gain),
                    str(stage3060_param0),
                    str(stage3060_param1),
                    str(stage3060_scalar),
                    str(stage3060_threshold),
                ]
            else:
                final_command = [
                    "large-export-bmp-tuned",
                    str(args.dat_path),
                    str(args.scratch / "native.final.bmp"),
                    str(stage3060_param0),
                    str(stage3060_param1),
                    str(stage3060_scalar),
                    str(stage3060_threshold),
                ]
        else:
            final_command = [
                "large-export-bmp",
                str(args.dat_path),
                str(args.scratch / "native.final.bmp"),
            ]
        run_native(args.native_binary, final_command)
        compare_path("final.bmp", args.reference_bmp, args.scratch / "native.final.bmp")

    print("default large export trace matched through the final large-export BMP")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
