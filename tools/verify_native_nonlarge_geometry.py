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

MODE_CONFIGS = {
    "normal": {
        "preview_flag": 0,
        "export_mode": 2,
        "input_stride": 512,
        "source_rows": 244,
        "output_bytes": 320 * 246,
    },
    "preview": {
        "preview_flag": 1,
        "export_mode": 0,
        "input_stride": 128,
        "source_rows": 64,
        "output_bytes": 80 * 64,
    },
}


def build_plane(pattern: str, channel: int, input_stride: int, source_rows: int, impulse_row: int, impulse_col: int) -> bytes:
    out = bytearray(PLANE_BYTES)
    active_cols = min(ACTIVE_WIDTH, input_stride - 1)
    if pattern == "rowcode":
        for row in range(source_rows):
            row_offset = row * input_stride + 1
            value = (row + channel * 17) & 0xFF
            for col in range(active_cols):
                out[row_offset + col] = value
        return bytes(out)
    if pattern == "gradient":
        for row in range(source_rows):
            row_offset = row * input_stride + 1
            for col in range(active_cols):
                out[row_offset + col] = (row * (11 + channel) + col * (3 + channel * 2)) & 0xFF
        return bytes(out)
    if pattern == "impulse":
        out[impulse_row * input_stride + 1 + impulse_col] = (0x40 + channel * 0x20) & 0xFF
        return bytes(out)
    raise ValueError(f"unknown pattern: {pattern}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native non-large geometry stage against the original DLL-backed geometry stage."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_nonlarge_geometry"),
    )
    parser.add_argument(
        "--modes",
        nargs="*",
        choices=tuple(MODE_CONFIGS.keys()),
        default=["normal", "preview"],
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

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for mode_name in args.modes:
        config = MODE_CONFIGS[mode_name]
        active_cols = min(ACTIVE_WIDTH, config["input_stride"] - 1)
        if not (0 <= args.impulse_row < config["source_rows"] and 0 <= args.impulse_col < active_cols):
            print(f"impulse coordinates are out of range for mode={mode_name}")
            return 1

        for pattern in args.patterns:
            plane_paths: list[Path] = []
            for channel in range(3):
                plane_path = args.scratch / f"{mode_name}.{pattern}.input.plane{channel}.bin"
                plane_path.write_bytes(
                    build_plane(
                        pattern=pattern,
                        channel=channel,
                        input_stride=config["input_stride"],
                        source_rows=config["source_rows"],
                        impulse_row=args.impulse_row,
                        impulse_col=args.impulse_col,
                    )
                )
                plane_paths.append(plane_path)

            dll_prefix = args.scratch / f"{mode_name}.{pattern}.dll"
            dll_outputs = dump_geometry_stage(
                plane0_path=plane_paths[0],
                plane1_path=plane_paths[1],
                plane2_path=plane_paths[2],
                output_prefix=dll_prefix,
                preview_flag=config["preview_flag"],
                export_mode=config["export_mode"],
                geometry_selector=1,
            )

            for channel, dll_path in enumerate(dll_outputs):
                native_path = args.scratch / f"{mode_name}.{pattern}.native.plane{channel}.bin"
                subprocess.run(
                    [
                        str(args.native_binary),
                        "nonlarge-stage-plane",
                        str(config["preview_flag"]),
                        str(channel),
                        str(plane_paths[0]),
                        str(plane_paths[1]),
                        str(plane_paths[2]),
                        str(native_path),
                    ],
                    check=True,
                )

                trimmed_dll = args.scratch / f"{mode_name}.{pattern}.dll.trimmed.plane{channel}.bin"
                trimmed_dll.write_bytes(dll_path.read_bytes()[: config["output_bytes"]])
                if not filecmp.cmp(trimmed_dll, native_path, shallow=False):
                    print(f"MISMATCH mode={mode_name} pattern={pattern} plane={channel}")
                    print(f"  dll:    {trimmed_dll}")
                    print(f"  native: {native_path}")
                    return 1

            print(f"OK mode={mode_name} pattern={pattern} bytes={config['output_bytes']}")

    print(f"verified {len(args.modes) * len(args.patterns)} non-large native geometry cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
