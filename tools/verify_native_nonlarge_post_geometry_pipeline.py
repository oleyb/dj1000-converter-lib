#!/usr/bin/env python3

from __future__ import annotations

import argparse
import filecmp
import math
import struct
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import (
    compile_harness,
    dump_geometry_stage,
    dump_normalize_stage,
    dump_post_geometry_filter,
    dump_post_geometry_prepare,
    dump_source_stage,
)
from probe_source_stage import MODE_CONFIGS, build_source
from verify_native_nonlarge_source_pipeline import build_geometry_input_plane
from verify_native_source_stage import CASES


NONLARGE_MODE_CONFIGS = {
    "normal": {
        "preview_flag": 0,
        "export_mode": 2,
        "stage_width": 320,
        "stage_height": 246,
    },
    "preview": {
        "preview_flag": 1,
        "export_mode": 0,
        "stage_width": 80,
        "stage_height": 64,
    },
}


def read_floats(path: Path) -> list[float]:
    data = path.read_bytes()
    return list(struct.unpack("<" + "f" * (len(data) // 4), data))


def quantize_stage_sample(value: float) -> int:
    if not math.isfinite(value) or not (value > 0.0):
        return 0
    if value >= 255.0:
        return 255
    return int(math.trunc(value))


def quantize_float_plane(path: Path) -> bytes:
    return bytes(quantize_stage_sample(value) for value in read_floats(path))


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Compare the native non-large source->normalize->quantize->geometry->post-geometry "
            "prepare/filter pipeline against the original DLL-backed helpers."
        )
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_nonlarge_post_geometry_pipeline"),
    )
    parser.add_argument(
        "--modes",
        nargs="*",
        choices=tuple(NONLARGE_MODE_CONFIGS.keys()),
        default=["normal"],
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    verified_cases = 0
    for mode_name in args.modes:
        mode_config = NONLARGE_MODE_CONFIGS[mode_name]
        source_config = MODE_CONFIGS[mode_name]

        for case in CASES:
            input_path = args.scratch / f"{mode_name}.{case['name']}.input.bin"
            input_path.write_bytes(
                build_source(
                    pattern=case["pattern"],
                    config=source_config,
                    impulse_row=case["impulse_row"],
                    impulse_col=case["impulse_col"],
                    value_a=case["value_a"],
                    value_b=case["value_b"],
                    split_row=case["split_row"],
                    split_col=case["split_col"],
                )
            )

            source_prefix = args.scratch / f"{mode_name}.{case['name']}.source"
            source_outputs = dump_source_stage(
                source_input_path=input_path,
                output_prefix=source_prefix,
                preview_flag=mode_config["preview_flag"],
            )

            normalize_prefix = args.scratch / f"{mode_name}.{case['name']}.normalize"
            normalized_outputs = dump_normalize_stage(
                plane0_path=source_outputs[0],
                plane1_path=source_outputs[1],
                plane2_path=source_outputs[2],
                output_prefix=normalize_prefix,
                preview_flag=mode_config["preview_flag"],
            )

            geometry_input_paths: list[Path] = []
            for plane_index, normalized_path in enumerate(normalized_outputs):
                quantized = quantize_float_plane(normalized_path)
                geometry_input_path = (
                    args.scratch / f"{mode_name}.{case['name']}.geometry-input.plane{plane_index}.bin"
                )
                geometry_input_path.write_bytes(build_geometry_input_plane(quantized))
                geometry_input_paths.append(geometry_input_path)

            stage_prefix = args.scratch / f"{mode_name}.{case['name']}.stage"
            stage_outputs = dump_geometry_stage(
                plane0_path=geometry_input_paths[0],
                plane1_path=geometry_input_paths[1],
                plane2_path=geometry_input_paths[2],
                output_prefix=stage_prefix,
                preview_flag=mode_config["preview_flag"],
                export_mode=mode_config["export_mode"],
                geometry_selector=1,
            )

            prepare_prefix = args.scratch / f"{mode_name}.{case['name']}.prepare"
            dll_center_path, dll_delta0_path, dll_delta2_path = dump_post_geometry_prepare(
                plane0_path=stage_outputs[0],
                plane1_path=stage_outputs[1],
                plane2_path=stage_outputs[2],
                output_prefix=prepare_prefix,
                width=mode_config["stage_width"],
                height=mode_config["stage_height"],
            )

            filter_prefix = args.scratch / f"{mode_name}.{case['name']}.filter"
            dll_filtered_delta0_path, dll_filtered_delta2_path = dump_post_geometry_filter(
                delta0_path=dll_delta0_path,
                delta2_path=dll_delta2_path,
                output_prefix=filter_prefix,
                width=mode_config["stage_width"],
                height=mode_config["stage_height"],
            )

            native_prefix = args.scratch / f"{mode_name}.{case['name']}.native"
            subprocess.run(
                [
                    str(args.native_binary),
                    "nonlarge-post-geometry-source",
                    str(mode_config["preview_flag"]),
                    str(input_path),
                    str(native_prefix),
                ],
                check=True,
            )

            native_outputs = (
                native_prefix.parent / f"{native_prefix.name}.center.f64",
                native_prefix.parent / f"{native_prefix.name}.delta0.f64",
                native_prefix.parent / f"{native_prefix.name}.delta2.f64",
            )
            dll_outputs = (
                dll_center_path,
                dll_filtered_delta0_path,
                dll_filtered_delta2_path,
            )

            for label, dll_path, native_path in zip(("center", "delta0", "delta2"), dll_outputs, native_outputs):
                if not filecmp.cmp(dll_path, native_path, shallow=False):
                    print(f"MISMATCH mode={mode_name} case={case['name']} plane={label}")
                    print(f"  dll:    {dll_path}")
                    print(f"  native: {native_path}")
                    return 1

            print(f"OK mode={mode_name} case={case['name']}")
            verified_cases += 1

    print(f"verified {verified_cases} nonlarge post-geometry pipeline cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
