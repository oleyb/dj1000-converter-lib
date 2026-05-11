#!/usr/bin/env python3

from __future__ import annotations

import argparse
import filecmp
import struct
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_post_geometry_stage_2a00


WIDTH = 7
HEIGHT = 5

CASES = {
    "impulse": [
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 100.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
    ],
    "gradient": [
        float((row * 31) + (col * 17))
        for row in range(HEIGHT)
        for col in range(WIDTH)
    ],
}


def write_doubles(path: Path, values: list[float]) -> None:
    path.write_bytes(struct.pack("<" + ("d" * len(values)), *values))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native post-geometry stage 0x2a00 against the original DLL helper."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_post_geometry_stage_2a00"),
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for case_name, values in CASES.items():
        input_path = args.scratch / f"{case_name}.input.f64"
        write_doubles(input_path, values)

        dll_output = args.scratch / f"{case_name}.dll.f64"
        dump_post_geometry_stage_2a00(
            plane_path=input_path,
            output_path=dll_output,
            width=WIDTH,
            height=HEIGHT,
        )

        native_output = args.scratch / f"{case_name}.native.f64"
        subprocess.run(
            [
                str(args.native_binary),
                "post-geometry-stage-2a00",
                str(WIDTH),
                str(HEIGHT),
                str(input_path),
                str(native_output),
            ],
            check=True,
        )

        if not filecmp.cmp(dll_output, native_output, shallow=False):
            print(f"MISMATCH case={case_name}")
            print(f"  dll:    {dll_output}")
            print(f"  native: {native_output}")
            return 1

        print(f"OK case={case_name}")

    print(f"verified {len(CASES)} post-geometry stage 0x2a00 cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
