#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import struct
import tempfile
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import (
    dump_post_geometry_stage_2a00,
    dump_post_geometry_stage_2dd0,
    dump_post_geometry_stage_3600,
    dump_post_geometry_stage_3890,
    dump_post_geometry_stage_4810,
)


def build_plane(
    width: int,
    height: int,
    pattern: str,
    value: float,
    impulse_row: int,
    impulse_col: int,
) -> list[float]:
    if pattern == "flat":
        return [value] * (width * height)
    if pattern == "gradient":
        return [
            float((row * 31) + (col * 17))
            for row in range(height)
            for col in range(width)
        ]
    if pattern == "checker":
        return [
            value if ((row + col) % 2 == 0) else 0.0
            for row in range(height)
            for col in range(width)
        ]
    if pattern == "impulse":
        plane = [0.0] * (width * height)
        plane[(impulse_row * width) + impulse_col] = value
        return plane
    raise ValueError(f"unsupported pattern: {pattern}")


def build_plane_from_json(width: int, height: int, values_json: str) -> list[float]:
    values = json.loads(values_json)
    if not isinstance(values, list):
        raise ValueError("custom plane JSON must decode to a list")
    if len(values) != (width * height):
        raise ValueError(
            f"custom plane JSON must contain exactly width*height values ({width * height})"
        )
    return [float(value) for value in values]


def write_plane(path: Path, values: list[float]) -> None:
    path.write_bytes(struct.pack("<" + ("d" * len(values)), *values))


def read_plane(path: Path) -> list[float]:
    data = path.read_bytes()
    return list(struct.unpack("<" + ("d" * (len(data) // 8)), data))


def print_plane(label: str, values: list[float], width: int) -> None:
    print(label)
    for row_start in range(0, len(values), width):
        row = values[row_start : row_start + width]
        print("  " + json.dumps(row))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Probe DLL-backed post-geometry stages with small synthetic planes."
    )
    parser.add_argument("--stage", choices=("2a00", "2dd0", "3600", "3890", "4810"), required=True)
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--pattern", choices=("flat", "gradient", "checker", "impulse"), default="impulse")
    parser.add_argument("--value", type=float, default=100.0)
    parser.add_argument("--impulse-row", type=int, default=0)
    parser.add_argument("--impulse-col", type=int, default=0)
    parser.add_argument("--active-plane", type=int, choices=(0, 1, 2), default=0)
    parser.add_argument("--plane0-pattern", choices=("zero", "flat", "gradient", "checker", "impulse"))
    parser.add_argument("--plane1-pattern", choices=("zero", "flat", "gradient", "checker", "impulse"))
    parser.add_argument("--plane2-pattern", choices=("zero", "flat", "gradient", "checker", "impulse"))
    parser.add_argument("--plane0-json")
    parser.add_argument("--plane1-json")
    parser.add_argument("--plane2-json")
    parser.add_argument("--plane0-value", type=float)
    parser.add_argument("--plane1-value", type=float)
    parser.add_argument("--plane2-value", type=float)
    parser.add_argument("--level", type=int, default=3)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(dir=runtime_tmp_dir()) as td:
        scratch = Path(td)
        plane_paths = [
            scratch / "plane0.f64",
            scratch / "plane1.f64",
            scratch / "plane2.f64",
        ]

        plane_patterns = [args.plane0_pattern, args.plane1_pattern, args.plane2_pattern]
        plane_values = [args.plane0_value, args.plane1_value, args.plane2_value]
        plane_json_values = [args.plane0_json, args.plane1_json, args.plane2_json]
        for index, plane_path in enumerate(plane_paths):
            if plane_json_values[index] is not None:
                values = build_plane_from_json(args.width, args.height, plane_json_values[index])
            else:
                plane_pattern = plane_patterns[index]
                if plane_pattern is None:
                    plane_pattern = args.pattern if index == args.active_plane else "zero"
                if plane_pattern == "zero":
                    values = [0.0] * (args.width * args.height)
                else:
                    values = build_plane(
                        width=args.width,
                        height=args.height,
                        pattern=plane_pattern,
                        value=args.value if plane_values[index] is None else plane_values[index],
                        impulse_row=args.impulse_row,
                        impulse_col=args.impulse_col,
                    )
            write_plane(plane_path, values)


        if args.stage == "2a00":
            output_path = scratch / "out.f64"
            dump_post_geometry_stage_2a00(
                plane_path=plane_paths[0],
                output_path=output_path,
                width=args.width,
                height=args.height,
            )
            print_plane("stage 2a00 output", read_plane(output_path), args.width)
            return 0

        if args.stage == "3600":
            output_path = scratch / "out.f64"
            dump_post_geometry_stage_3600(
                plane_path=plane_paths[0],
                output_path=output_path,
                width=args.width,
                height=args.height,
            )
            print_plane("stage 3600 output", read_plane(output_path), args.width)
            return 0

        if args.stage == "2dd0":
            output_prefix = scratch / "out"
            out0, out1 = dump_post_geometry_stage_2dd0(
                plane0_path=plane_paths[0],
                plane1_path=plane_paths[1],
                output_prefix=output_prefix,
                width=args.width,
                height=args.height,
            )
            print_plane("stage 2dd0 plane0", read_plane(out0), args.width)
            print_plane("stage 2dd0 plane1", read_plane(out1), args.width)
            return 0

        if args.stage == "3890":
            output_prefix = scratch / "out"
            out0, out1 = dump_post_geometry_stage_3890(
                plane0_path=plane_paths[0],
                plane1_path=plane_paths[1],
                output_prefix=output_prefix,
                width=args.width,
                height=args.height,
                level=args.level,
            )
            print_plane("stage 3890 plane0", read_plane(out0), args.width)
            print_plane("stage 3890 plane1", read_plane(out1), args.width)
            return 0

        output_prefix = scratch / "out"
        out0, out1, out2 = dump_post_geometry_stage_4810(
            plane0_path=plane_paths[0],
            plane1_path=plane_paths[1],
            plane2_path=plane_paths[2],
            output_prefix=output_prefix,
            width=args.width,
            height=args.height,
        )
        print_plane("stage 4810 plane0", read_plane(out0), args.width)
        print_plane("stage 4810 plane1", read_plane(out1), args.width)
        print_plane("stage 4810 plane2", read_plane(out2), args.width)
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
