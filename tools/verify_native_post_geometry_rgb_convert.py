#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import struct
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_post_geometry_rgb_convert


WIDTH = 6
HEIGHT = 3

CASES = {
    "mixed": {
        "plane0": [
            200.0, 20.0, -20.0, 0.0, 100.0, 50.0,
            10.0, 30.0, 60.0, 90.0, 120.0, 150.0,
            180.0, 210.0, 240.0, 15.0, 45.0, 75.0,
        ],
        "plane1": [
            20.0, 100.0, 40.0, 80.0, -10.0, 5.0,
            15.0, 25.0, 35.0, 45.0, 55.0, 65.0,
            75.0, 85.0, 95.0, 105.0, 115.0, 125.0,
        ],
        "plane2": [
            10.0, -10.0, 20.0, 30.0, 40.0, 50.0,
            60.0, 70.0, 80.0, 90.0, 100.0, 110.0,
            120.0, 130.0, 140.0, 150.0, 160.0, 170.0,
        ],
    },
    "clamp": {
        "plane0": [400.0] * (WIDTH * HEIGHT),
        "plane1": [50.0] * (WIDTH * HEIGHT),
        "plane2": [-200.0] * (WIDTH * HEIGHT),
    },
}


def write_doubles(path: Path, values: list[float]) -> None:
    path.write_bytes(struct.pack("<" + "d" * len(values), *values))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native post-geometry RGB convert stage against the original DLL helper."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_post_geometry_rgb_convert"),
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
        dll_outputs = dump_post_geometry_rgb_convert(
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
                "post-geometry-rgb-convert",
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
            native_prefix.parent / f"{native_prefix.name}.plane0.bin",
            native_prefix.parent / f"{native_prefix.name}.plane1.bin",
            native_prefix.parent / f"{native_prefix.name}.plane2.bin",
        )

        for label, dll_path, native_path in zip(("plane0", "plane1", "plane2"), dll_outputs, native_outputs):
            if dll_path.read_bytes() != native_path.read_bytes():
                print(f"MISMATCH case={case_name} plane={label}")
                print(f"  dll:    {dll_path}")
                print(f"  native: {native_path}")
                return 1

        print(f"OK case={case_name}")

    print(f"verified {len(CASES)} post-geometry RGB convert cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
