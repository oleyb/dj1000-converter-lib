#!/usr/bin/env python3

from __future__ import annotations

import argparse
import struct
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_normalize_stage


MODE_CONFIGS = {
    "normal": {
        "preview_flag": 0,
        "stride": 0x200,
        "rows": 0x0F6,
        "active_width": 0x1F8,
        "active_rows": 0x0F4,
    },
    "preview": {
        "preview_flag": 1,
        "stride": 0x80,
        "rows": 0x40,
        "active_width": 0x80,
        "active_rows": 0x40,
    },
}


def plane_count(config: dict[str, int]) -> int:
    return config["stride"] * config["rows"]


def write_float_plane(path: Path, values: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(struct.pack("<" + "f" * len(values), *values))


def build_planes(mode_name: str, pattern: str) -> tuple[list[float], list[float], list[float]]:
    config = MODE_CONFIGS[mode_name]
    count = plane_count(config)
    plane0 = [0.0] * count
    plane1 = [0.0] * count
    plane2 = [0.0] * count

    for row in range(config["rows"]):
        row_offset = row * config["stride"]
        for col in range(config["stride"]):
            index = row_offset + col
            plane0[index] = 60.0
            plane1[index] = 120.0
            plane2[index] = 240.0

    if pattern == "balanced":
        return plane0, plane1, plane2

    if pattern == "masked":
        for row in range(min(8, config["active_rows"])):
            for col in range(min(8, config["active_width"])):
                index = row * config["stride"] + col
                plane0[index] = 30.0
                plane1[index] = -10.0 if (row + col) % 2 == 0 else 300.0
                plane2[index] = 15.0
        return plane0, plane1, plane2

    if pattern == "zero-fallback":
        for row in range(config["rows"]):
            row_offset = row * config["stride"]
            for col in range(config["stride"]):
                index = row_offset + col
                plane0[index] = 0.0
                plane1[index] = 80.0
                plane2[index] = 160.0
        return plane0, plane1, plane2

    if pattern == "clamp":
        for row in range(config["rows"]):
            row_offset = row * config["stride"]
            for col in range(config["stride"]):
                index = row_offset + col
                plane0[index] = 100.0
                plane1[index] = 200.0
                plane2[index] = 50.0
        plane0[0] = 200.0
        plane1[0] = 400.0
        plane2[0] = -20.0
        plane0[1] = -10.0
        plane1[1] = 200.0
        plane2[1] = 150.0
        return plane0, plane1, plane2

    raise ValueError(f"unknown pattern: {pattern}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native 0x10001e00 pre-geometry normalize stage against the original DLL."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_pregeometry_normalize"),
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
        choices=("balanced", "masked", "zero-fallback", "clamp"),
        default=["balanced", "masked", "zero-fallback", "clamp"],
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for mode_name in args.modes:
        config = MODE_CONFIGS[mode_name]
        for pattern in args.patterns:
            plane_paths: list[Path] = []
            for channel, values in enumerate(build_planes(mode_name, pattern)):
                plane_path = args.scratch / f"{mode_name}.{pattern}.input.plane{channel}.f32"
                write_float_plane(plane_path, values)
                plane_paths.append(plane_path)

            dll_prefix = args.scratch / f"{mode_name}.{pattern}.dll"
            dll_outputs = dump_normalize_stage(
                plane0_path=plane_paths[0],
                plane1_path=plane_paths[1],
                plane2_path=plane_paths[2],
                output_prefix=dll_prefix,
                preview_flag=config["preview_flag"],
            )

            native_prefix = args.scratch / f"{mode_name}.{pattern}.native"
            subprocess.run(
                [
                    str(args.native_binary),
                    "normalize-stage",
                    str(config["preview_flag"]),
                    str(plane_paths[0]),
                    str(plane_paths[1]),
                    str(plane_paths[2]),
                    str(native_prefix),
                ],
                check=True,
            )

            native_outputs = (
                native_prefix.parent / f"{native_prefix.name}.plane0.f32",
                native_prefix.parent / f"{native_prefix.name}.plane1.f32",
                native_prefix.parent / f"{native_prefix.name}.plane2.f32",
            )

            for channel, (dll_path, native_path) in enumerate(zip(dll_outputs, native_outputs)):
                if dll_path.read_bytes() != native_path.read_bytes():
                    print(f"MISMATCH mode={mode_name} pattern={pattern} plane={channel}")
                    print(f"  dll:    {dll_path}")
                    print(f"  native: {native_path}")
                    return 1

            print(f"OK mode={mode_name} pattern={pattern}")

    print(f"verified {len(args.modes) * len(args.patterns)} pre-geometry normalize cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
