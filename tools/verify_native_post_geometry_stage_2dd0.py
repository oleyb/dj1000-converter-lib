#!/usr/bin/env python3

from __future__ import annotations

import argparse
import struct
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_post_geometry_stage_2dd0


CASES = {
    "impulse-center": {
        "width": 15,
        "height": 9,
        "plane0": None,
    },
    "gradient": {
        "width": 7,
        "height": 5,
        "plane0": None,
    },
}


def build_case_values(case_name: str, width: int, height: int) -> list[float]:
    if case_name == "impulse-center":
        values = [0.0] * (width * height)
        values[(4 * width) + 7] = 100.0
        return values
    if case_name == "gradient":
        return [
            float((row * 31) + (col * 17))
            for row in range(height)
            for col in range(width)
        ]
    raise ValueError(case_name)


def write_doubles(path: Path, values: list[float]) -> None:
    path.write_bytes(struct.pack("<" + ("d" * len(values)), *values))


def read_doubles(path: Path) -> list[float]:
    data = path.read_bytes()
    return list(struct.unpack("<" + ("d" * (len(data) // 8)), data))


def files_match_with_tolerance(left: Path, right: Path, tolerance: float = 1.0e-12) -> bool:
    left_values = read_doubles(left)
    right_values = read_doubles(right)
    if len(left_values) != len(right_values):
        return False
    return all(abs(left_value - right_value) <= tolerance for left_value, right_value in zip(left_values, right_values))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native post-geometry stage 0x2dd0 against the original DLL helper."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_post_geometry_stage_2dd0"),
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for case_name, case in CASES.items():
        width = case["width"]
        height = case["height"]
        plane0_values = build_case_values(case_name, width, height)
        plane1_values = [0.0] * (width * height)

        plane0_path = args.scratch / f"{case_name}.plane0.input.f64"
        plane1_path = args.scratch / f"{case_name}.plane1.input.f64"
        write_doubles(plane0_path, plane0_values)
        write_doubles(plane1_path, plane1_values)

        dll_prefix = args.scratch / f"{case_name}.dll"
        dll_plane0, dll_plane1 = dump_post_geometry_stage_2dd0(
            plane0_path=plane0_path,
            plane1_path=plane1_path,
            output_prefix=dll_prefix,
            width=width,
            height=height,
        )

        native_prefix = args.scratch / f"{case_name}.native"
        subprocess.run(
            [
                str(args.native_binary),
                "post-geometry-stage-2dd0",
                str(width),
                str(height),
                str(plane0_path),
                str(plane1_path),
                str(native_prefix),
            ],
            check=True,
        )

        native_plane0 = native_prefix.parent / f"{native_prefix.name}.plane0.f64"
        native_plane1 = native_prefix.parent / f"{native_prefix.name}.plane1.f64"

        if not files_match_with_tolerance(dll_plane0, native_plane0):
            print(f"MISMATCH case={case_name} plane=plane0")
            print(f"  dll:    {dll_plane0}")
            print(f"  native: {native_plane0}")
            return 1

        if not files_match_with_tolerance(dll_plane1, native_plane1):
            print(f"MISMATCH case={case_name} plane=plane1")
            print(f"  dll:    {dll_plane1}")
            print(f"  native: {native_plane1}")
            return 1

        print(f"OK case={case_name}")

    print(f"verified {len(CASES)} post-geometry stage 0x2dd0 cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
