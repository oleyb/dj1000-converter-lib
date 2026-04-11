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
    dump_source_stage,
)
from probe_source_stage import MODE_CONFIGS, build_source
from verify_native_source_stage import CASES


GEOMETRY_INPUT_BYTES = 0x2F400

NONLARGE_MODE_CONFIGS = {
    "normal": {
        "preview_flag": 0,
        "export_mode": 2,
        "compact_plane_bytes": 0x200 * 0x0F6,
        "stage_output_bytes": 320 * 246,
    },
    "preview": {
        "preview_flag": 1,
        "export_mode": 0,
        "compact_plane_bytes": 0x80 * 0x40,
        "stage_output_bytes": 80 * 64,
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


def build_geometry_input_plane(compact_plane: bytes) -> bytes:
    output = bytearray(GEOMETRY_INPUT_BYTES)
    output[: len(compact_plane)] = compact_plane
    return bytes(output)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Compare the native normal/preview source->normalize->quantize->geometry "
            "pipeline against the original DLL-backed helpers."
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
        default=scratch_dir("native_nonlarge_source_pipeline"),
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
            expected_quantized_paths: list[Path] = []
            for plane_index, normalized_path in enumerate(normalized_outputs):
                quantized = quantize_float_plane(normalized_path)
                expected_quantized_path = (
                    args.scratch / f"{mode_name}.{case['name']}.expected.quantized.plane{plane_index}.bin"
                )
                expected_quantized_path.write_bytes(quantized)
                expected_quantized_paths.append(expected_quantized_path)

                geometry_input_path = (
                    args.scratch / f"{mode_name}.{case['name']}.geometry-input.plane{plane_index}.bin"
                )
                geometry_input_path.write_bytes(build_geometry_input_plane(quantized))
                geometry_input_paths.append(geometry_input_path)

            dll_prefix = args.scratch / f"{mode_name}.{case['name']}.dll"
            dll_outputs = dump_geometry_stage(
                plane0_path=geometry_input_paths[0],
                plane1_path=geometry_input_paths[1],
                plane2_path=geometry_input_paths[2],
                output_prefix=dll_prefix,
                preview_flag=mode_config["preview_flag"],
                export_mode=mode_config["export_mode"],
                geometry_selector=1,
            )

            native_prefix = args.scratch / f"{mode_name}.{case['name']}.native"
            subprocess.run(
                [
                    str(args.native_binary),
                    "nonlarge-source-stage",
                    str(mode_config["preview_flag"]),
                    str(input_path),
                    str(native_prefix),
                ],
                check=True,
            )

            for plane_index in range(3):
                native_quantized_path = (
                    native_prefix.parent /
                    f"{native_prefix.name}.quantized.plane{plane_index}.bin"
                )
                if not filecmp.cmp(expected_quantized_paths[plane_index], native_quantized_path, shallow=False):
                    print(f"MISMATCH mode={mode_name} case={case['name']} quantized_plane={plane_index}")
                    print(f"  expected: {expected_quantized_paths[plane_index]}")
                    print(f"  native:   {native_quantized_path}")
                    return 1

                trimmed_dll_path = (
                    args.scratch / f"{mode_name}.{case['name']}.dll.trimmed.plane{plane_index}.bin"
                )
                trimmed_dll_path.write_bytes(
                    dll_outputs[plane_index].read_bytes()[: mode_config["stage_output_bytes"]]
                )

                native_stage_path = (
                    native_prefix.parent /
                    f"{native_prefix.name}.stage.plane{plane_index}.bin"
                )
                if not filecmp.cmp(trimmed_dll_path, native_stage_path, shallow=False):
                    print(f"MISMATCH mode={mode_name} case={case['name']} stage_plane={plane_index}")
                    print(f"  dll:    {trimmed_dll_path}")
                    print(f"  native: {native_stage_path}")
                    return 1

            print(f"OK mode={mode_name} case={case['name']}")
            verified_cases += 1

    print(f"verified {verified_cases} nonlarge source-pipeline cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
