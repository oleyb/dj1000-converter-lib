#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
from dataclasses import asdict, dataclass
from pathlib import Path

from PIL import Image, ImageStat

from dj1000_dat_info import parse_dat


PAIR_RE = re.compile(r"(?i)^mdsc(\d{4})([a-z]?)$")


@dataclass
class ImageInfo:
    path: str
    width: int
    height: int
    mode: str
    mean_rgb: list[float] | None
    size_bytes: int


def normalize_key(path: Path) -> tuple[str, str] | None:
    match = PAIR_RE.match(path.stem)
    if match:
        return match.group(1), match.group(2).lower()

    dat_match = re.match(r"(?i)^mdsc(\d{4})$", path.stem)
    if dat_match:
        return dat_match.group(1), ""

    return None


def inspect_image(path: Path) -> ImageInfo:
    with Image.open(path) as image:
        rgb = image.convert("RGB")
        mean = [round(value, 4) for value in ImageStat.Stat(rgb).mean]
        return ImageInfo(
            path=str(path),
            width=rgb.width,
            height=rgb.height,
            mode=rgb.mode,
            mean_rgb=mean,
            size_bytes=path.stat().st_size,
        )


def collect_pairs(directory: Path) -> list[dict[str, object]]:
    dat_files: dict[str, Path] = {}
    images: dict[tuple[str, str, str], Path] = {}

    for path in sorted(directory.iterdir()):
        if path.is_dir():
            continue

        key = normalize_key(path)
        if key is None:
            continue

        number, suffix = key
        lower_suffix = suffix.lower()
        ext = path.suffix.lower()

        if ext == ".dat":
            dat_files[number] = path
        elif ext in {".bmp", ".jpeg", ".jpg"}:
            images[(number, lower_suffix, ext)] = path

    report: list[dict[str, object]] = []
    all_numbers = sorted(set(dat_files) | {number for number, _, _ in images})
    for number in all_numbers:
        entry: dict[str, object] = {"id": number}
        dat_path = dat_files.get(number)
        if dat_path is not None:
            entry["dat"] = parse_dat(dat_path)
        else:
            entry["dat"] = None

        large_bmp = images.get((number, "", ".bmp"))
        small_bmp = images.get((number, "s", ".bmp"))
        jpeg = images.get((number, "", ".jpeg")) or images.get((number, "", ".jpg"))
        small_jpeg = (
            images.get((number, "s", ".jpeg")) or images.get((number, "s", ".jpg"))
        )
        alt_large_jpeg = (
            images.get((number, "l", ".jpeg")) or images.get((number, "l", ".jpg"))
        )

        entry["large_bmp"] = asdict(inspect_image(large_bmp)) if large_bmp else None
        entry["small_bmp"] = asdict(inspect_image(small_bmp)) if small_bmp else None
        entry["jpeg"] = asdict(inspect_image(jpeg)) if jpeg else None
        entry["small_jpeg"] = asdict(inspect_image(small_jpeg)) if small_jpeg else None
        entry["large_jpeg_alt"] = (
            asdict(inspect_image(alt_large_jpeg)) if alt_large_jpeg else None
        )
        report.append(entry)

    return report


def print_human(report: list[dict[str, object]]) -> None:
    valid_dat_count = sum(
        1
        for entry in report
        if entry["dat"] is not None and bool(entry["dat"]["size_matches"])
    )
    print(f"pairs: {len(report)}")
    print(f"valid .dat files: {valid_dat_count}")
    print()

    for entry in report:
        print(f"MDSC{entry['id']}")
        dat = entry["dat"]
        if dat is None:
            print("  dat: missing")
        else:
            print(
                "  dat: "
                f"size={dat['size']} "
                f"signature_ok={dat['signature_matches']} "
                f"trailer={dat['trailer_hex']}"
            )

        for label in ("large_bmp", "small_bmp", "jpeg", "small_jpeg", "large_jpeg_alt"):
            image = entry[label]
            if image is None:
                continue
            mean_rgb = ", ".join(f"{value:.2f}" for value in image["mean_rgb"])
            print(
                f"  {label}: {image['width']}x{image['height']} {image['mode']} "
                f"mean_rgb=[{mean_rgb}]"
            )
        print()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Match DJ-1000 .dat files with reference exports and summarize them."
    )
    parser.add_argument("directory", type=Path)
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args()

    report = collect_pairs(args.directory)
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print_human(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
