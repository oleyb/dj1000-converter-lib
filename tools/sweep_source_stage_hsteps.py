#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_source_stage
from probe_source_stage import MODE_CONFIGS, build_source


def read_floats(path: Path) -> list[float]:
    data = path.read_bytes()
    return list(struct.unpack("<" + "f" * (len(data) // 4), data))


def window(
    values: list[float],
    stride: int,
    row: int,
    split_col: int,
    width: int = 8,
) -> list[tuple[int, float]]:
    start_col = max(0, split_col - 2)
    end_col = min(stride, start_col + width)
    row_offset = row * stride
    return [(col, values[row_offset + col]) for col in range(start_col, end_col)]


def strongest_plane(
    plane_windows: dict[int, list[tuple[int, float]]],
    epsilon: float = 1e-6,
) -> tuple[int | None, float]:
    best_plane: int | None = None
    best_value = 0.0
    for plane_index, entries in plane_windows.items():
        local_best = max((abs(value) for _, value in entries), default=0.0)
        if local_best > best_value + epsilon:
            best_value = local_best
            best_plane = plane_index
    return best_plane, best_value


def strongest_side_plane(
    plane_windows: dict[int, list[tuple[int, float]]],
    epsilon: float = 1e-6,
) -> tuple[int | None, float]:
    side_only = {plane_index: entries for plane_index, entries in plane_windows.items() if plane_index in (0, 2)}
    return strongest_plane(side_only, epsilon)


def format_window(entries: list[tuple[int, float]]) -> str:
    return "[" + ", ".join(f"{col}:{value:.4f}" for col, value in entries) + "]"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Sweep horizontal step positions through the original 0x10005eb0 source stage."
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("source_stage_hstep_sweep"),
    )
    parser.add_argument(
        "--mode",
        choices=tuple(MODE_CONFIGS.keys()),
        default="normal",
    )
    parser.add_argument(
        "--split-cols",
        nargs="*",
        type=int,
        default=[20, 21, 60, 61],
    )
    parser.add_argument("--value-a", type=int, default=64)
    parser.add_argument("--value-b", type=int, default=192)
    parser.add_argument("--row", type=int, default=40)
    parser.add_argument("--prefix", default="hstep_sweep")
    args = parser.parse_args()

    config = MODE_CONFIGS[args.mode]
    if not (0 <= args.value_a <= 255 and 0 <= args.value_b <= 255):
        raise SystemExit("values must be in the 0..255 byte range")
    if not (0 <= args.row < config["output_rows"]):
        raise SystemExit("row is out of range for the selected mode")

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)
    prefix = args.prefix
    if prefix == "hstep_sweep":
        prefix = f"{prefix}.{args.mode}.{args.value_a}.{args.value_b}"

    for split_col in args.split_cols:
        if not (0 <= split_col < config["active_width"]):
            raise SystemExit(f"split column {split_col} is out of range for mode={args.mode}")

        input_path = args.scratch / f"{prefix}.c{split_col}.input.bin"
        input_path.write_bytes(
            build_source(
                pattern="hstep",
                config=config,
                impulse_row=0,
                impulse_col=0,
                value_a=args.value_a,
                value_b=args.value_b,
                split_row=0,
                split_col=split_col,
            )
        )

        outputs = dump_source_stage(
            source_input_path=input_path,
            output_prefix=args.scratch / f"{prefix}.c{split_col}",
            preview_flag=config["preview_flag"],
        )
        plane_windows = {
            plane_index: window(read_floats(path), config["output_stride"], args.row, split_col)
            for plane_index, path in enumerate(outputs)
        }
        strongest, magnitude = strongest_plane(plane_windows)
        side_plane, side_magnitude = strongest_side_plane(plane_windows)
        parity = "even" if split_col % 2 == 0 else "odd"
        strongest_label = "none" if strongest is None or math.isclose(magnitude, 0.0, abs_tol=1e-6) else f"plane{strongest}"
        side_label = "none" if side_plane is None or math.isclose(side_magnitude, 0.0, abs_tol=1e-6) else f"plane{side_plane}"
        direction = "rising" if args.value_b > args.value_a else "falling" if args.value_b < args.value_a else "flat"
        print(
            f"mode={args.mode} split_col={split_col} ({parity}) direction={direction} "
            f"strongest={strongest_label} magnitude={magnitude:.4f} "
            f"active_side={side_label} side_magnitude={side_magnitude:.4f}"
        )
        for plane_index in range(3):
            print(f"  plane{plane_index} {format_window(plane_windows[plane_index])}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
