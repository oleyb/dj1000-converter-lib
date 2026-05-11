#!/usr/bin/env python3

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_source_stage


MODE_CONFIGS = {
    "normal": {
        "preview_flag": 0,
        "input_stride": 0x200,
        "active_width": 0x1F8,
        "active_rows": 0x0F4,
        "output_stride": 0x200,
        "output_rows": 0x0F6,
    },
    "preview": {
        "preview_flag": 1,
        "input_stride": 0x80,
        "active_width": 0x80,
        "active_rows": 0x40,
        "output_stride": 0x80,
        "output_rows": 0x40,
    },
}


def build_source(
    pattern: str,
    config: dict[str, int],
    impulse_row: int,
    impulse_col: int,
    value_a: int,
    value_b: int,
    split_row: int,
    split_col: int,
) -> bytes:
    source = bytearray(config["input_stride"] * config["active_rows"])
    if pattern == "rowcode":
        for row in range(config["active_rows"]):
            row_offset = row * config["input_stride"]
            value = row & 0xFF
            for col in range(config["active_width"]):
                source[row_offset + col] = value
        return bytes(source)

    if pattern == "gradient":
        for row in range(config["active_rows"]):
            row_offset = row * config["input_stride"]
            for col in range(config["active_width"]):
                source[row_offset + col] = (row * 11 + col * 3) & 0xFF
        return bytes(source)

    if pattern == "columncode":
        for row in range(config["active_rows"]):
            row_offset = row * config["input_stride"]
            for col in range(config["active_width"]):
                source[row_offset + col] = col & 0xFF
        return bytes(source)

    if pattern == "flat":
        for row in range(config["active_rows"]):
            row_offset = row * config["input_stride"]
            for col in range(config["active_width"]):
                source[row_offset + col] = value_a & 0xFF
        return bytes(source)

    if pattern == "hstep":
        for row in range(config["active_rows"]):
            row_offset = row * config["input_stride"]
            for col in range(config["active_width"]):
                source[row_offset + col] = (value_a if col < split_col else value_b) & 0xFF
        return bytes(source)

    if pattern == "vstep":
        for row in range(config["active_rows"]):
            row_offset = row * config["input_stride"]
            row_value = value_a if row < split_row else value_b
            for col in range(config["active_width"]):
                source[row_offset + col] = row_value & 0xFF
        return bytes(source)

    if pattern == "checker":
        for row in range(config["active_rows"]):
            row_offset = row * config["input_stride"]
            for col in range(config["active_width"]):
                source[row_offset + col] = (value_a if (row + col) % 2 == 0 else value_b) & 0xFF
        return bytes(source)

    if pattern == "impulse":
        source[impulse_row * config["input_stride"] + impulse_col] = 0xFF
        return bytes(source)

    raise ValueError(f"unknown pattern: {pattern}")


def read_floats(path: Path) -> list[float]:
    data = path.read_bytes()
    return list(struct.unpack("<" + "f" * (len(data) // 4), data))


def row_samples(values: list[float], stride: int, row: int, cols: int = 12) -> list[float]:
    start = row * stride
    return [round(values[start + col], 4) for col in range(cols)]


def window_samples(values: list[float], stride: int, row: int, start_col: int, width: int = 12) -> list[float]:
    row_start = row * stride
    return [
        round(values[row_start + col], 4)
        for col in range(max(0, start_col), min(stride, start_col + width))
    ]


def impulse_summary(values: list[float], stride: int, rows: int) -> str:
    hits: list[str] = []
    for row in range(rows):
        row_offset = row * stride
        for col in range(stride):
            value = values[row_offset + col]
            if not math.isclose(value, 0.0, abs_tol=1e-6):
                hits.append(f"{row}:{col}={value:.4f}")
                if len(hits) >= 12:
                    return ", ".join(hits)
    if not hits:
        return "none"
    return ", ".join(hits)


def summarize_plane(
    path: Path,
    config: dict[str, int],
    pattern: str,
    split_row: int,
    split_col: int,
) -> str:
    values = read_floats(path)
    nonzero = sum(1 for value in values if not math.isclose(value, 0.0, abs_tol=1e-6))
    minimum = min(values)
    maximum = max(values)
    if pattern == "impulse":
        detail = impulse_summary(values, config["output_stride"], config["output_rows"])
        return (
            f"{path.name}: floats={len(values)} nonzero={nonzero} "
            f"min={minimum:.4f} max={maximum:.4f} hits={detail}"
        )

    if pattern in {"hstep", "checker"}:
        row = min(split_row, config["output_rows"] - 1)
        left_col = max(0, split_col - 6)
        detail = window_samples(values, config["output_stride"], row, left_col, 12)
        return (
            f"{path.name}: floats={len(values)} nonzero={nonzero} "
            f"min={minimum:.4f} max={maximum:.4f} "
            f"row{row}@col{left_col}={detail}"
        )

    if pattern == "vstep":
        top_row = max(0, split_row - 1)
        bottom_row = min(config["output_rows"] - 1, split_row)
        row_top = row_samples(values, config["output_stride"], top_row)
        row_bottom = row_samples(values, config["output_stride"], bottom_row)
        return (
            f"{path.name}: floats={len(values)} nonzero={nonzero} "
            f"min={minimum:.4f} max={maximum:.4f} "
            f"row{top_row}={row_top} row{bottom_row}={row_bottom}"
        )

    row0 = row_samples(values, config["output_stride"], 0)
    row1 = row_samples(values, config["output_stride"], 1)
    mid_row = row_samples(values, config["output_stride"], min(10, config["output_rows"] - 1))
    return (
        f"{path.name}: floats={len(values)} nonzero={nonzero} "
        f"min={minimum:.4f} max={maximum:.4f} "
        f"row0={row0} row1={row1} row10={mid_row}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Feed synthetic raw rows into the original 0x10005eb0 source stage."
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("source_stage_probe"),
    )
    parser.add_argument(
        "--mode",
        choices=tuple(MODE_CONFIGS.keys()),
        default="normal",
    )
    parser.add_argument(
        "--pattern",
        choices=("rowcode", "gradient", "columncode", "flat", "hstep", "vstep", "checker", "impulse"),
        default="rowcode",
    )
    parser.add_argument("--impulse-row", type=int, default=40)
    parser.add_argument("--impulse-col", type=int, default=60)
    parser.add_argument("--value-a", type=int, default=64)
    parser.add_argument("--value-b", type=int, default=192)
    parser.add_argument("--split-row", type=int, default=40)
    parser.add_argument("--split-col", type=int, default=60)
    parser.add_argument("--prefix", default="source_probe")
    args = parser.parse_args()

    config = MODE_CONFIGS[args.mode]
    if not (0 <= args.impulse_row < config["active_rows"] and 0 <= args.impulse_col < config["active_width"]):
        raise SystemExit("impulse coordinates are out of range")
    if not (0 <= args.split_row < config["active_rows"] and 0 <= args.split_col < config["active_width"]):
        raise SystemExit("split coordinates are out of range")
    if not (0 <= args.value_a <= 255 and 0 <= args.value_b <= 255):
        raise SystemExit("values must be in the 0..255 byte range")

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    source_path = args.scratch / f"{args.prefix}.input.bin"
    source_path.write_bytes(
        build_source(
            args.pattern,
            config,
            args.impulse_row,
            args.impulse_col,
            args.value_a,
            args.value_b,
            args.split_row,
            args.split_col,
        )
    )

    outputs = dump_source_stage(
        source_input_path=source_path,
        output_prefix=args.scratch / args.prefix,
        preview_flag=config["preview_flag"],
    )

    for output in outputs:
        print(summarize_plane(output, config, args.pattern, args.split_row, args.split_col))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
