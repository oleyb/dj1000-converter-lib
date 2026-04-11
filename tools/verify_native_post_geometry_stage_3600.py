#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import struct
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_post_geometry_stage_3600


WIDTH = 5
HEIGHT = 4

CASES = {
    "flat": {
        "plane": [100.0] * (WIDTH * HEIGHT),
    },
    "gradient": {
        "plane": [
            float((row * 31) + (col * 17))
            for row in range(HEIGHT)
            for col in range(WIDTH)
        ],
    },
    "center-impulse": {
        "plane": [
            100.0 if (row == 2 and col == 2) else 0.0
            for row in range(HEIGHT)
            for col in range(WIDTH)
        ],
    },
    "top-edge-impulse": {
        "plane": [
            100.0 if (row == 0 and col == 2) else 0.0
            for row in range(HEIGHT)
            for col in range(WIDTH)
        ],
    },
    "corner-impulse": {
        "plane": [
            100.0 if (row == 0 and col == 0) else 0.0
            for row in range(HEIGHT)
            for col in range(WIDTH)
        ],
    },
}


def write_doubles(path: Path, values: list[float]) -> None:
    path.write_bytes(struct.pack("<" + "d" * len(values), *values))


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
        description="Compare the native post-geometry stage 0x3600 against the original DLL helper."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_post_geometry_stage_3600"),
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for case_name, case in CASES.items():
        plane_path = args.scratch / f"{case_name}.input.plane.f64"
        write_doubles(plane_path, case["plane"])

        dll_output_path = args.scratch / f"{case_name}.dll.f64"
        dump_post_geometry_stage_3600(
            plane_path=plane_path,
            output_path=dll_output_path,
            width=WIDTH,
            height=HEIGHT,
        )

        native_output_path = args.scratch / f"{case_name}.native.f64"
        subprocess.run(
            [
                str(args.native_binary),
                "post-geometry-stage-3600",
                str(WIDTH),
                str(HEIGHT),
                str(plane_path),
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

    print(f"verified {len(CASES)} post-geometry stage 0x3600 cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
