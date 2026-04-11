#!/usr/bin/env python3

from __future__ import annotations

import argparse
import filecmp
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_geometry_stage


PLANE_BYTES = 0x2F400
ACTIVE_WIDTH = 504
INPUT_STRIDE = 512
SOURCE_ROWS = 244
OUTPUT_ROWS = 378
ACTIVE_OUTPUT_BYTES = ACTIVE_WIDTH * OUTPUT_ROWS


def build_rowcode_plane(channel: int) -> bytes:
    out = bytearray(PLANE_BYTES)
    for row in range(SOURCE_ROWS):
        row_offset = row * INPUT_STRIDE
        value = (row + channel * 17) & 0xFF
        for col in range(ACTIVE_WIDTH):
            out[row_offset + col] = value
    return bytes(out)


def build_gradient_plane(channel: int) -> bytes:
    out = bytearray(PLANE_BYTES)
    for row in range(SOURCE_ROWS):
        row_offset = row * INPUT_STRIDE
        for col in range(ACTIVE_WIDTH):
            out[row_offset + col] = (row * (11 + channel) + col * (3 + channel * 2)) & 0xFF
    return bytes(out)


def build_impulse_plane(channel: int, row: int, col: int) -> bytes:
    out = bytearray(PLANE_BYTES)
    out[row * INPUT_STRIDE + col] = (0x40 + channel * 0x20) & 0xFF
    return bytes(out)


def build_plane(pattern: str, channel: int, impulse_row: int, impulse_col: int) -> bytes:
    if pattern == "rowcode":
        return build_rowcode_plane(channel)
    if pattern == "gradient":
        return build_gradient_plane(channel)
    if pattern == "impulse":
        return build_impulse_plane(channel, impulse_row, impulse_col)
    raise ValueError(f"unknown pattern: {pattern}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native large-path vertical helper against the DLL-backed geometry stage."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_large_vertical"),
    )
    parser.add_argument(
        "--patterns",
        nargs="*",
        choices=("rowcode", "gradient", "impulse"),
        default=["rowcode", "gradient", "impulse"],
    )
    parser.add_argument("--impulse-row", type=int, default=40)
    parser.add_argument("--impulse-col", type=int, default=60)
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1
    if not (0 <= args.impulse_row < SOURCE_ROWS and 0 <= args.impulse_col < ACTIVE_WIDTH):
        print("impulse coordinates are out of range")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for pattern in args.patterns:
        plane_paths: list[Path] = []
        native_paths: list[Path] = []
        for channel in range(3):
            input_path = args.scratch / f"{pattern}.input.plane{channel}.bin"
            input_path.write_bytes(build_plane(pattern, channel, args.impulse_row, args.impulse_col))
            plane_paths.append(input_path)

            native_path = args.scratch / f"{pattern}.native.plane{channel}.bin"
            subprocess.run(
                [str(args.native_binary), "large-stage-plane", str(input_path), str(native_path)],
                check=True,
            )
            native_paths.append(native_path)

        dll_prefix = args.scratch / f"{pattern}.dll"
        dll_outputs = dump_geometry_stage(
            plane0_path=plane_paths[0],
            plane1_path=plane_paths[1],
            plane2_path=plane_paths[2],
            output_prefix=dll_prefix,
            preview_flag=0,
            export_mode=5,
            geometry_selector=1,
        )

        for channel, (dll_path, native_path) in enumerate(zip(dll_outputs, native_paths)):
            trimmed_dll = args.scratch / f"{pattern}.dll.trimmed.plane{channel}.bin"
            trimmed_dll.write_bytes(dll_path.read_bytes()[:ACTIVE_OUTPUT_BYTES])
            if not filecmp.cmp(trimmed_dll, native_path, shallow=False):
                print(f"MISMATCH pattern={pattern} plane={channel}")
                print(f"  dll:    {trimmed_dll}")
                print(f"  native: {native_path}")
                return 1

        print(f"OK pattern={pattern} bytes={ACTIVE_OUTPUT_BYTES}")

    print(f"verified {len(args.patterns)} large-path native cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
