#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import struct
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_normalize_stage, dump_source_stage
from probe_source_stage import MODE_CONFIGS, build_source
from verify_native_source_stage import CASES


def read_floats(path: Path) -> list[float]:
    data = path.read_bytes()
    return list(struct.unpack("<" + "f" * (len(data) // 4), data))


def files_match_with_subnormal_zero_tolerance(left: Path, right: Path) -> tuple[bool, int]:
    left_bytes = left.read_bytes()
    right_bytes = right.read_bytes()
    if left_bytes == right_bytes:
        return True, 0

    left_values = read_floats(left)
    right_values = read_floats(right)
    if len(left_values) != len(right_values):
        return False, 0

    tolerated = 0
    for left_value, right_value in zip(left_values, right_values):
        if struct.pack("<f", left_value) == struct.pack("<f", right_value):
            continue
        if (
            abs(left_value) < 1.0e-37
            and abs(right_value) < 1.0e-37
            and (left_value == 0.0 or right_value == 0.0)
        ):
            tolerated += 1
            continue
        return False, tolerated

    return True, tolerated


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native source->normalize pipeline against the original DLL."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_pregeometry_pipeline"),
    )
    parser.add_argument(
        "--modes",
        nargs="*",
        choices=tuple(MODE_CONFIGS.keys()),
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
        config = MODE_CONFIGS[mode_name]
        for case in CASES:
            input_path = args.scratch / f"{mode_name}.{case['name']}.input.bin"
            input_path.write_bytes(
                build_source(
                    pattern=case["pattern"],
                    config=config,
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
                preview_flag=config["preview_flag"],
            )

            dll_prefix = args.scratch / f"{mode_name}.{case['name']}.dll"
            dll_outputs = dump_normalize_stage(
                plane0_path=source_outputs[0],
                plane1_path=source_outputs[1],
                plane2_path=source_outputs[2],
                output_prefix=dll_prefix,
                preview_flag=config["preview_flag"],
            )

            native_prefix = args.scratch / f"{mode_name}.{case['name']}.native"
            subprocess.run(
                [
                    str(args.native_binary),
                    "pregeometry-pipeline",
                    str(config["preview_flag"]),
                    str(input_path),
                    str(native_prefix),
                ],
                check=True,
            )

            native_outputs = (
                native_prefix.parent / f"{native_prefix.name}.plane0.f32",
                native_prefix.parent / f"{native_prefix.name}.plane1.f32",
                native_prefix.parent / f"{native_prefix.name}.plane2.f32",
            )

            tolerated_subnormals = 0
            for plane_index, (dll_path, native_path) in enumerate(zip(dll_outputs, native_outputs)):
                matched, tolerated = files_match_with_subnormal_zero_tolerance(dll_path, native_path)
                tolerated_subnormals += tolerated
                if not matched:
                    print(f"MISMATCH mode={mode_name} case={case['name']} plane={plane_index}")
                    print(f"  dll:    {dll_path}")
                    print(f"  native: {native_path}")
                    return 1

            if tolerated_subnormals:
                print(
                    f"OK mode={mode_name} case={case['name']} "
                    f"(tolerated {tolerated_subnormals} subnormal-zero differences)"
                )
            else:
                print(f"OK mode={mode_name} case={case['name']}")
            verified_cases += 1

    print(f"verified {verified_cases} pregeometry-pipeline cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
