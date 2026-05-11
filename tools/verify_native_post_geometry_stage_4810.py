#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import struct
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_post_geometry_stage_4810


WIDTH = 6
HEIGHT = 4

CASES = {
    "gradient": {
        "plane0": [float((row * 11) - (col * 7)) for row in range(HEIGHT) for col in range(WIDTH)],
        "plane1": [float((row * 13) + (col * 5)) for row in range(HEIGHT) for col in range(WIDTH)],
        "plane2": [float((row * 17) - (col * 3)) for row in range(HEIGHT) for col in range(WIDTH)],
    },
    "checker": {
        "plane0": [100.0 if ((row + col) % 2 == 0) else -25.0 for row in range(HEIGHT) for col in range(WIDTH)],
        "plane1": [50.0 if ((row + col) % 2 == 0) else 10.0 for row in range(HEIGHT) for col in range(WIDTH)],
        "plane2": [0.0 if ((row + col) % 2 == 0) else 75.0 for row in range(HEIGHT) for col in range(WIDTH)],
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
        description="Compare the native post-geometry stage 0x4810 against the original DLL helper."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_post_geometry_stage_4810"),
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for case_name, case in CASES.items():
        plane0_path = args.scratch / f"{case_name}.input.plane0.f64"
        plane1_path = args.scratch / f"{case_name}.input.plane1.f64"
        plane2_path = args.scratch / f"{case_name}.input.plane2.f64"
        write_doubles(plane0_path, case["plane0"])
        write_doubles(plane1_path, case["plane1"])
        write_doubles(plane2_path, case["plane2"])

        dll_prefix = args.scratch / f"{case_name}.dll"
        dll_outputs = dump_post_geometry_stage_4810(
            plane0_path=plane0_path,
            plane1_path=plane1_path,
            plane2_path=plane2_path,
            output_prefix=dll_prefix,
            width=WIDTH,
            height=HEIGHT,
        )

        native_prefix = args.scratch / f"{case_name}.native"
        subprocess.run(
            [
                str(args.native_binary),
                "post-geometry-stage-4810",
                str(WIDTH),
                str(HEIGHT),
                str(plane0_path),
                str(plane1_path),
                str(plane2_path),
                str(native_prefix),
            ],
            check=True,
        )

        native_outputs = (
            native_prefix.parent / f"{native_prefix.name}.plane0.f64",
            native_prefix.parent / f"{native_prefix.name}.plane1.f64",
            native_prefix.parent / f"{native_prefix.name}.plane2.f64",
        )

        for label, dll_path, native_path in zip(("plane0", "plane1", "plane2"), dll_outputs, native_outputs):
            matched, max_difference = files_match_with_tolerance(dll_path, native_path)
            if not matched:
                print(f"MISMATCH case={case_name} plane={label} max_diff={max_difference:.3e}")
                print(f"  dll:    {dll_path}")
                print(f"  native: {native_path}")
                return 1

        print(f"OK case={case_name}")

    print(f"verified {len(CASES)} post-geometry stage 4810 cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
