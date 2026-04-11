#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

from dj1000_convert_original import compile_harness, dump_geometry_stage
from path_helpers import scratch_dir


PLANE_BYTES = 0x2F400
PLANE_ACTIVE_WIDTH = 504
DEFAULT_INPUT_STRIDE = 504

MODE_DIMENSIONS = {
    (1, 0): (80, 64),
    (0, 2): (320, 246),
    (0, 5): (504, 378),
}


def build_impulse_plane(
    channel: int,
    active_channel: int,
    row: int,
    col: int,
    input_stride: int,
) -> bytes:
    out = bytearray(PLANE_BYTES)
    if channel == active_channel:
        out[row * input_stride + col] = 0xA0
    return bytes(out)


def find_nonzero_coordinate(data: bytes, width: int) -> tuple[int, int] | None:
    positions = [index for index, byte in enumerate(data) if byte != 0]
    if len(positions) != 1:
        return None
    index = positions[0]
    return divmod(index, width)


def format_positions(positions: list[int], width: int) -> str:
    if not positions:
        return ""
    return ";".join(f"{index // width}:{index % width}" for index in positions)


def parse_int_list(value: str) -> list[int]:
    return [int(part.strip(), 0) for part in value.split(",") if part.strip()]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Sweep synthetic impulses through the original 0x100013c0 geometry stage."
    )
    parser.add_argument("--scratch", type=Path, default=scratch_dir("geometry_sweep"))
    parser.add_argument("--preview-flag", type=int, default=0)
    parser.add_argument("--export-mode", type=int, default=2)
    parser.add_argument("--geometry-selector", type=int, default=1)
    parser.add_argument("--channel", type=int, choices=(0, 1, 2), default=0)
    parser.add_argument("--input-stride", type=int, default=DEFAULT_INPUT_STRIDE)
    parser.add_argument("--rows", type=parse_int_list, default=[0, 40, 80, 120, 160, 200, 240, 280, 320, 360])
    parser.add_argument("--cols", type=parse_int_list, default=[0, 60, 120, 180, 240, 300, 360, 420, 480])
    args = parser.parse_args()

    output_width, output_height = MODE_DIMENSIONS[(args.preview_flag, args.export_mode)]
    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)
    if args.input_stride <= 0 or PLANE_BYTES % args.input_stride != 0:
        raise SystemExit("input stride must be a positive divisor of 0x2f400")
    input_rows = PLANE_BYTES // args.input_stride
    input_cols = min(args.input_stride, PLANE_ACTIVE_WIDTH)

    print("input_row,input_col,output_row,output_col,nonzero_count,positions")
    for row in args.rows:
        for col in args.cols:
            if not (0 <= row < input_rows and 0 <= col < input_cols):
                continue

            plane_paths: list[Path] = []
            for channel in range(3):
                plane_path = args.scratch / f"r{row:03d}_c{col:03d}.plane{channel}.bin"
                plane_path.write_bytes(
                    build_impulse_plane(
                        channel=channel,
                        active_channel=args.channel,
                        row=row,
                        col=col,
                        input_stride=args.input_stride,
                    )
                )
                plane_paths.append(plane_path)

            output_prefix = args.scratch / f"r{row:03d}_c{col:03d}"
            outputs = dump_geometry_stage(
                plane0_path=plane_paths[0],
                plane1_path=plane_paths[1],
                plane2_path=plane_paths[2],
                output_prefix=output_prefix,
                preview_flag=args.preview_flag,
                export_mode=args.export_mode,
                geometry_selector=args.geometry_selector,
            )
            output_bytes = outputs[args.channel].read_bytes()
            nonzero_positions = [index for index, byte in enumerate(output_bytes[: output_width * output_height]) if byte != 0]
            coordinate = find_nonzero_coordinate(output_bytes[: output_width * output_height], output_width)
            if coordinate is None:
                print(f"{row},{col},,,{len(nonzero_positions)},{format_positions(nonzero_positions, output_width)}")
            else:
                out_row, out_col = coordinate
                print(f"{row},{col},{out_row},{out_col},{len(nonzero_positions)},{format_positions(nonzero_positions, output_width)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
