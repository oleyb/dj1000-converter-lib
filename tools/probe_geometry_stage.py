#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_geometry_stage


PLANE_BYTES = 0x2F400
PLANE_ACTIVE_WIDTH = 504
PLANE_STORAGE_ROWS = 384
DEFAULT_INPUT_STRIDE = 504


def build_gradient_plane(channel: int, input_stride: int) -> bytes:
    out = bytearray(PLANE_BYTES)
    row_count = PLANE_BYTES // input_stride
    for row in range(row_count):
        for col in range(min(PLANE_ACTIVE_WIDTH, input_stride)):
            out[row * input_stride + col] = (row * (11 + channel) + col * (3 + channel * 2)) & 0xFF
    return bytes(out)


def build_impulse_plane(channel: int, row: int, col: int, input_stride: int) -> bytes:
    out = bytearray(PLANE_BYTES)
    index = row * input_stride + col
    out[index] = 0x40 + channel * 0x20
    return bytes(out)


def build_plane(channel: int, pattern: str, impulse_row: int, impulse_col: int, input_stride: int) -> bytes:
    if pattern == "gradient":
        return build_gradient_plane(channel, input_stride)
    if pattern == "impulse":
        return build_impulse_plane(channel, impulse_row, impulse_col, input_stride)
    raise ValueError(f"unknown pattern: {pattern}")


def write_plane(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def summarize_plane(input_path: Path, output_path: Path) -> str:
    before = input_path.read_bytes()
    data = output_path.read_bytes()
    nonzero = sum(1 for byte in data if byte != 0)
    head = data[:16].hex()
    changed = [index for index, (left, right) in enumerate(zip(before, data)) if left != right]
    nonzero_positions = [index for index, byte in enumerate(data) if byte != 0]
    if changed:
        changed_summary = f"changed={len(changed)} first={changed[0]} last={changed[-1]}"
    else:
        changed_summary = "changed=0"
    if nonzero_positions:
        nonzero_summary = f"nonzero_first={nonzero_positions[0]} nonzero_last={nonzero_positions[-1]}"
    else:
        nonzero_summary = "nonzero_first=none nonzero_last=none"
    return (
        f"{output_path.name}: bytes={len(data)} nonzero={nonzero} "
        f"{nonzero_summary} {changed_summary} head={head}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Feed synthetic planes through the original 0x100013c0 geometry stage."
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("geometry_probe"),
    )
    parser.add_argument("--preview-flag", type=int, default=0)
    parser.add_argument("--export-mode", type=int, default=5)
    parser.add_argument("--geometry-selector", type=int, default=1)
    parser.add_argument("--pattern", choices=("gradient", "impulse"), default="gradient")
    parser.add_argument("--impulse-row", type=int, default=0)
    parser.add_argument("--impulse-col", type=int, default=0)
    parser.add_argument("--input-stride", type=int, default=DEFAULT_INPUT_STRIDE)
    parser.add_argument(
        "--prefix",
        type=str,
        default="probe",
        help="prefix for generated input/output files inside the scratch directory",
    )
    args = parser.parse_args()

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)
    input_rows = PLANE_BYTES // args.input_stride
    if args.input_stride <= 0 or PLANE_BYTES % args.input_stride != 0:
        raise SystemExit("input stride must be a positive divisor of 0x2f400")
    if not (0 <= args.impulse_row < input_rows and 0 <= args.impulse_col < min(PLANE_ACTIVE_WIDTH, args.input_stride)):
        raise SystemExit("impulse coordinates are out of range")

    plane_paths = []
    for channel in range(3):
        plane_path = args.scratch / f"{args.prefix}.input.plane{channel}.bin"
        write_plane(
            plane_path,
            build_plane(
                channel=channel,
                pattern=args.pattern,
                impulse_row=args.impulse_row,
                impulse_col=args.impulse_col,
                input_stride=args.input_stride,
            ),
        )
        plane_paths.append(plane_path)

    output_prefix = args.scratch / args.prefix
    outputs = dump_geometry_stage(
        plane0_path=plane_paths[0],
        plane1_path=plane_paths[1],
        plane2_path=plane_paths[2],
        output_prefix=output_prefix,
        preview_flag=args.preview_flag,
        export_mode=args.export_mode,
        geometry_selector=args.geometry_selector,
    )

    for input_path, output_path in zip(plane_paths, outputs):
        print(summarize_plane(input_path, output_path))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
