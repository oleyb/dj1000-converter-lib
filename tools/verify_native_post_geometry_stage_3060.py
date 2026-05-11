#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import struct
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_post_geometry_stage_3060


WIDTH = 5
HEIGHT = 3

CASES = {
    "positive-impulse": {
        "edge_response": [200 if (row == 1 and col == 2) else 0 for row in range(HEIGHT) for col in range(WIDTH)],
        "center": [50.0] * (WIDTH * HEIGHT),
        "stage_param0": 16,
        "stage_param1": 20,
        "scalar": 0.6,
        "threshold": 14,
    },
    "negative-impulse": {
        "edge_response": [-200 if (row == 1 and col == 2) else 0 for row in range(HEIGHT) for col in range(WIDTH)],
        "center": [50.0] * (WIDTH * HEIGHT),
        "stage_param0": 16,
        "stage_param1": 20,
        "scalar": 0.6,
        "threshold": 14,
    },
    "highlight-curve": {
        "edge_response": [200 if (row == 1 and col == 2) else 0 for row in range(HEIGHT) for col in range(WIDTH)],
        "center": [200.0] * (WIDTH * HEIGHT),
        "stage_param0": 16,
        "stage_param1": 20,
        "scalar": 0.6,
        "threshold": 14,
    },
}


def write_doubles(path: Path, values: list[float]) -> None:
    path.write_bytes(struct.pack("<" + "d" * len(values), *values))


def write_int32(path: Path, values: list[int]) -> None:
    path.write_bytes(struct.pack("<" + "i" * len(values), *values))


def read_doubles(path: Path) -> list[float]:
    data = path.read_bytes()
    return list(struct.unpack("<" + "d" * (len(data) // 8), data))


def files_match_with_tolerance(left: Path, right: Path, tolerance: float = 1.0e-12) -> tuple[bool, float]:
    left_values = read_doubles(left)
    right_values = read_doubles(right)
    if len(left_values) != len(right_values):
        return False, math.inf

    max_difference = 0.0
    for left_value, right_value in zip(left_values, right_values):
        difference = abs(left_value - right_value)
        if difference > max_difference:
            max_difference = difference
        if difference > tolerance:
            return False, max_difference
    return True, max_difference


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native post-geometry stage 0x3060 against the original DLL helper."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_post_geometry_stage_3060"),
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for case_name, case in CASES.items():
        edge_path = args.scratch / f"{case_name}.edge.i32"
        center_path = args.scratch / f"{case_name}.center.f64"
        write_int32(edge_path, case["edge_response"])
        write_doubles(center_path, case["center"])

        dll_output_path = args.scratch / f"{case_name}.dll.f64"
        dump_post_geometry_stage_3060(
            plane0_path=edge_path,
            plane1_path=center_path,
            output_path=dll_output_path,
            width=WIDTH,
            height=HEIGHT,
            stage_param0=case["stage_param0"],
            stage_param1=case["stage_param1"],
            scalar=case["scalar"],
            threshold=case["threshold"],
        )

        native_output_path = args.scratch / f"{case_name}.native.f64"
        subprocess.run(
            [
                str(args.native_binary),
                "post-geometry-stage-3060",
                str(WIDTH),
                str(HEIGHT),
                str(edge_path),
                str(center_path),
                str(case["stage_param0"]),
                str(case["stage_param1"]),
                str(case["scalar"]),
                str(case["threshold"]),
                str(native_output_path),
            ],
            check=True,
        )

        matched, max_difference = files_match_with_tolerance(dll_output_path, native_output_path)
        if not matched:
            print(f"MISMATCH case={case_name} max_diff={max_difference:.3e}")
            print(f"  dll:    {dll_output_path}")
            print(f"  native: {native_output_path}")
            return 1

        print(f"OK case={case_name}")

    print(f"verified {len(CASES)} post-geometry stage 0x3060 cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
