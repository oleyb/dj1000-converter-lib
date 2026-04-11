#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from PIL import Image, ImageChops, ImageStat

from dj1000_convert_original import compile_harness, run_harness


def score(candidate: Path, reference: Path) -> tuple[float, int]:
    with Image.open(candidate).convert("RGB") as cand, Image.open(reference).convert("RGB") as ref:
        diff = ImageChops.difference(cand, ref)
        stat = ImageStat.Stat(diff)
        mean_total = sum(stat.mean)
        max_channel = max(channel_max for _, channel_max in stat.extrema)
        return mean_total, max_channel


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Brute-force a single PhotoRun settings field against a reference BMP."
    )
    parser.add_argument("dat", type=Path)
    parser.add_argument("reference_bmp", type=Path)
    parser.add_argument("--field", type=int, required=True, choices=range(7))
    parser.add_argument("--start", type=int, required=True)
    parser.add_argument("--stop", type=int, required=True)
    parser.add_argument(
        "--size",
        choices=("small", "medium", "large"),
        default="large",
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("search_settings"),
    )
    args = parser.parse_args()

    export_mode_map = {"small": 0, "medium": 2, "large": 5}
    args.scratch.mkdir(parents=True, exist_ok=True)
    compile_harness(force=False)

    best_value: int | None = None
    best_score: tuple[float, int] | None = None
    for value in range(args.start, args.stop + 1):
        candidate = args.scratch / f"{args.reference_bmp.stem}_f{args.field}_{value}.bmp"
        run_harness(
            input_path=args.dat,
            output_path=candidate,
            export_mode=export_mode_map[args.size],
            converter_mode=0,
            settings={args.field: value},
        )
        current_score = score(candidate, args.reference_bmp)
        print(
            f"field={args.field} value={value} "
            f"mean_total={current_score[0]:.4f} max_channel={current_score[1]}"
        )
        if best_score is None or current_score < best_score:
            best_score = current_score
            best_value = value

    print(
        f"best field={args.field} value={best_value} "
        f"mean_total={best_score[0]:.4f} max_channel={best_score[1]}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
