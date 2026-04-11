#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import struct
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_post_geometry_center_scale


WIDTH = 7
HEIGHT = 3

CASES = {
    "clamp-and-invert": {
        "delta0": [
            10.0, 5.0, -4.0, 2.0, 7.0, -3.0, 12.0,
            8.0, -8.0, 6.0, -6.0, 4.0, -4.0, 2.0,
            1.0, -1.0, 3.0, -3.0, 9.0, -9.0, 11.0,
        ],
        "center": [
            200.0, 127.0, 0.0, 300.0, 10.0, 20.0, 255.0,
            15.0, 130.0, 5.0, 140.0, 30.0, 127.0, 19.0,
            18.0, 128.0, 21.0, 126.0, 1.0, 149.0, 151.0,
        ],
        "delta2": [
            -10.0, 6.0, 4.0, -2.0, 3.0, -5.0, 12.0,
            -6.0, 6.0, -8.0, 8.0, -4.0, 4.0, -2.0,
            7.0, -7.0, 5.0, -5.0, 9.0, -9.0, 11.0,
        ],
    },
    "ramp": {
        "delta0": [
            float(row * 17 - col * 9)
            for row in range(HEIGHT)
            for col in range(WIDTH)
        ],
        "center": [
            float((row * 43 + col * 19) % 260 - 2)
            for row in range(HEIGHT)
            for col in range(WIDTH)
        ],
        "delta2": [
            float(col * 13 - row * 7)
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
    left_bytes = left.read_bytes()
    right_bytes = right.read_bytes()
    if left_bytes == right_bytes:
        return True, 0.0

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
        description="Compare the native post-geometry center-scale stage against the original DLL helper."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_post_geometry_center_scale"),
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for case_name, case in CASES.items():
        delta0_path = args.scratch / f"{case_name}.input.delta0.f64"
        center_path = args.scratch / f"{case_name}.input.center.f64"
        delta2_path = args.scratch / f"{case_name}.input.delta2.f64"
        write_doubles(delta0_path, case["delta0"])
        write_doubles(center_path, case["center"])
        write_doubles(delta2_path, case["delta2"])

        dll_prefix = args.scratch / f"{case_name}.dll"
        dll_outputs = dump_post_geometry_center_scale(
            delta0_path=delta0_path,
            center_path=center_path,
            delta2_path=delta2_path,
            output_prefix=dll_prefix,
            width=WIDTH,
            height=HEIGHT,
        )

        native_prefix = args.scratch / f"{case_name}.native"
        subprocess.run(
            [
                str(args.native_binary),
                "post-geometry-center-scale",
                str(WIDTH),
                str(HEIGHT),
                str(delta0_path),
                str(center_path),
                str(delta2_path),
                str(native_prefix),
            ],
            check=True,
        )

        native_outputs = (
            native_prefix.parent / f"{native_prefix.name}.delta0.f64",
            native_prefix.parent / f"{native_prefix.name}.center.f64",
            native_prefix.parent / f"{native_prefix.name}.delta2.f64",
        )

        for label, dll_path, native_path in zip(("delta0", "center", "delta2"), dll_outputs, native_outputs):
            matched, max_difference = files_match_with_tolerance(dll_path, native_path)
            if not matched:
                print(f"MISMATCH case={case_name} plane={label} max_diff={max_difference:.3e}")
                print(f"  dll:    {dll_path}")
                print(f"  native: {native_path}")
                return 1

        print(f"OK case={case_name}")

    print(f"verified {len(CASES)} post-geometry center-scale cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
