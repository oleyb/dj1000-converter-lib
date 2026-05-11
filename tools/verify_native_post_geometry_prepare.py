#!/usr/bin/env python3

from __future__ import annotations

import argparse
import filecmp
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_post_geometry_prepare


WIDTH = 9
HEIGHT = 5
SAMPLE_COUNT = WIDTH * HEIGHT

CASES = ("rowcode", "gradient", "impulse")


def build_plane(case: str, channel: int) -> bytes:
    output = bytearray(SAMPLE_COUNT)
    if case == "rowcode":
        for row in range(HEIGHT):
            for col in range(WIDTH):
                output[row * WIDTH + col] = (row * 29 + col * 7 + channel * 41) & 0xFF
        return bytes(output)

    if case == "gradient":
        for row in range(HEIGHT):
            for col in range(WIDTH):
                output[row * WIDTH + col] = (row * (17 + channel * 3) + col * (11 + channel * 5)) & 0xFF
        return bytes(output)

    if case == "impulse":
        output[(HEIGHT // 2) * WIDTH + (WIDTH // 2)] = (0x50 + channel * 0x30) & 0xFF
        if channel == 1:
            output[1] = 0xE0
        if channel == 2:
            output[-2] = 0x20
        return bytes(output)

    raise ValueError(f"unknown case: {case}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native post-geometry prepare stage against the original DLL helper."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_post_geometry_prepare"),
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for case in CASES:
        plane_paths: list[Path] = []
        for channel in range(3):
            path = args.scratch / f"{case}.input.plane{channel}.bin"
            path.write_bytes(build_plane(case, channel))
            plane_paths.append(path)

        dll_prefix = args.scratch / f"{case}.dll"
        dll_center_path, dll_delta0_path, dll_delta2_path = dump_post_geometry_prepare(
            plane0_path=plane_paths[0],
            plane1_path=plane_paths[1],
            plane2_path=plane_paths[2],
            output_prefix=dll_prefix,
            width=WIDTH,
            height=HEIGHT,
        )

        native_prefix = args.scratch / f"{case}.native"
        subprocess.run(
            [
                str(args.native_binary),
                "post-geometry-prepare",
                str(WIDTH),
                str(HEIGHT),
                str(plane_paths[0]),
                str(plane_paths[1]),
                str(plane_paths[2]),
                str(native_prefix),
            ],
            check=True,
        )

        native_outputs = (
            native_prefix.parent / f"{native_prefix.name}.center.f64",
            native_prefix.parent / f"{native_prefix.name}.delta0.f64",
            native_prefix.parent / f"{native_prefix.name}.delta2.f64",
        )
        dll_outputs = (dll_center_path, dll_delta0_path, dll_delta2_path)

        for label, dll_path, native_path in zip(("center", "delta0", "delta2"), dll_outputs, native_outputs):
            if not filecmp.cmp(dll_path, native_path, shallow=False):
                print(f"MISMATCH case={case} plane={label}")
                print(f"  dll:    {dll_path}")
                print(f"  native: {native_path}")
                return 1

        print(f"OK case={case}")

    print(f"verified {len(CASES)} post-geometry prepare cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
