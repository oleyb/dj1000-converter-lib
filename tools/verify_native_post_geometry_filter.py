#!/usr/bin/env python3

from __future__ import annotations

import argparse
import filecmp
import struct
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_post_geometry_filter


WIDTH = 7
HEIGHT = 4
SAMPLE_COUNT = WIDTH * HEIGHT

CASES = {
    "left-bright": {
        "delta0": [
            200.0, 100.0, 100.0, 30.0, 30.0, 220.0, 10.0,
            210.0, 100.0, 100.0, 50.0, 50.0, 230.0, 60.0,
            159.0, 100.0, 100.0, 70.0, 70.0, 100.0, 100.0,
            205.0, 205.0, 100.0, 100.0, 100.0, 205.0, 205.0,
        ],
        "delta2": [
            100.0, 100.0, 220.0, 40.0, 40.0, 100.0, 210.0,
            120.0, 120.0, 230.0, 60.0, 60.0, 120.0, 215.0,
            100.0, 100.0, 159.0, 80.0, 80.0, 100.0, 100.0,
            205.0, 100.0, 100.0, 100.0, 205.0, 100.0, 100.0,
        ],
    },
    "alternating": {
        "delta0": [
            180.0 if (row + col) % 2 == 0 else 20.0
            for row in range(HEIGHT)
            for col in range(WIDTH)
        ],
        "delta2": [
            20.0 if (row + col) % 2 == 0 else 180.0
            for row in range(HEIGHT)
            for col in range(WIDTH)
        ],
    },
    "ramp": {
        "delta0": [
            float((row * 41 + col * 13) % 256)
            for row in range(HEIGHT)
            for col in range(WIDTH)
        ],
        "delta2": [
            float((220 - row * 29 - col * 17) % 256)
            for row in range(HEIGHT)
            for col in range(WIDTH)
        ],
    },
}


def write_doubles(path: Path, values: list[float]) -> None:
    path.write_bytes(struct.pack("<" + "d" * len(values), *values))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native post-geometry delta filter against the original DLL helper."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_post_geometry_filter"),
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for case_name, case in CASES.items():
        delta0_path = args.scratch / f"{case_name}.input.delta0.f64"
        delta2_path = args.scratch / f"{case_name}.input.delta2.f64"
        write_doubles(delta0_path, case["delta0"])
        write_doubles(delta2_path, case["delta2"])

        dll_prefix = args.scratch / f"{case_name}.dll"
        dll_delta0_path, dll_delta2_path = dump_post_geometry_filter(
            delta0_path=delta0_path,
            delta2_path=delta2_path,
            output_prefix=dll_prefix,
            width=WIDTH,
            height=HEIGHT,
        )

        native_prefix = args.scratch / f"{case_name}.native"
        subprocess.run(
            [
                str(args.native_binary),
                "post-geometry-filter",
                str(WIDTH),
                str(HEIGHT),
                str(delta0_path),
                str(delta2_path),
                str(native_prefix),
            ],
            check=True,
        )

        native_outputs = (
            native_prefix.parent / f"{native_prefix.name}.delta0.f64",
            native_prefix.parent / f"{native_prefix.name}.delta2.f64",
        )
        dll_outputs = (dll_delta0_path, dll_delta2_path)

        for label, dll_path, native_path in zip(("delta0", "delta2"), dll_outputs, native_outputs):
            if not filecmp.cmp(dll_path, native_path, shallow=False):
                print(f"MISMATCH case={case_name} plane={label}")
                print(f"  dll:    {dll_path}")
                print(f"  native: {native_path}")
                return 1

        print(f"OK case={case_name}")

    print(f"verified {len(CASES)} post-geometry filter cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
