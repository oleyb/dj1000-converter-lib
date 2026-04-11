#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, reference_dataset_default, scratch_dir


DEFAULT_CONTROLS = {
    "red_balance": 100,
    "green_balance": 100,
    "blue_balance": 100,
    "contrast": 3,
    "brightness": 3,
    "vividness": 3,
    "sharpness": 3,
}

CORPUS_CASES = (
    {"dat": "MDSC0001.DAT", "bmp": "mdsc0001.bmp", "mode": "large"},
    {"dat": "MDSC0004.DAT", "bmp": "mdsc0004.bmp", "mode": "large"},
    {"dat": "MDSC0005.DAT", "bmp": "mdsc0005.bmp", "mode": "large"},
    {"dat": "MDSC0006.DAT", "bmp": "mdsc0006.bmp", "mode": "large", "brightness": 6},
    {"dat": "MDSC0007.DAT", "bmp": "mdsc0007.bmp", "mode": "large"},
    {"dat": "MDSC0008.DAT", "bmp": "mdsc0008.bmp", "mode": "large"},
    {"dat": "MDSC0008.DAT", "bmp": "mdsc0008s.bmp", "mode": "small"},
    {"dat": "MDSC0009.DAT", "bmp": "mdsc0009.bmp", "mode": "large"},
    {"dat": "MDSC0009.DAT", "bmp": "mdsc0009s.bmp", "mode": "small"},
    {"dat": "MDSC0010.DAT", "bmp": "mdsc0010.bmp", "mode": "large", "brightness": 6, "vividness": 6},
    {"dat": "MDSC0011.DAT", "bmp": "mdsc0011.bmp", "mode": "large"},
    {"dat": "MDSC0015.DAT", "bmp": "mdsc0015.bmp", "mode": "large"},
    {"dat": "MDSC0016.DAT", "bmp": "mdsc0016.bmp", "mode": "large"},
    {"dat": "MDSC0017.DAT", "bmp": "mdsc0017.bmp", "mode": "large"},
)


def build_command(
    native_binary: Path,
    dat_path: Path,
    output_path: Path,
    mode: str,
    controls: dict[str, int],
) -> list[str]:
    nondefault = any(controls[key] != DEFAULT_CONTROLS[key] for key in DEFAULT_CONTROLS)
    if mode == "large":
        if not nondefault:
            return [str(native_binary), "large-export-bmp", str(dat_path), str(output_path)]
        command = "large-export-bmp-controls"
    elif mode == "small":
        if not nondefault:
            return [str(native_binary), "small-export-bmp", str(dat_path), str(output_path)]
        command = "small-export-bmp-controls"
    else:
        raise RuntimeError(f"unsupported mode: {mode}")

    return [
        str(native_binary),
        command,
        str(dat_path),
        str(output_path),
        str(controls["red_balance"]),
        str(controls["green_balance"]),
        str(controls["blue_balance"]),
        str(controls["contrast"]),
        str(controls["brightness"]),
        str(controls["vividness"]),
        str(controls["sharpness"]),
    ]


def first_mismatch(left: bytes, right: bytes) -> tuple[int, int, int]:
    for index, (left_byte, right_byte) in enumerate(zip(left, right)):
        if left_byte != right_byte:
            return index, left_byte, right_byte
    if len(left) != len(right):
        index = min(len(left), len(right))
        left_byte = left[index] if index < len(left) else -1
        right_byte = right[index] if index < len(right) else -1
        return index, left_byte, right_byte
    raise RuntimeError("no mismatch")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify the native exporter against the known MDSC reference BMP corpus."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--dataset-root",
        type=Path,
        default=reference_dataset_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_reference_corpus"),
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1
    if args.dataset_root is None:
        print("dataset root not provided. Pass --dataset-root or set DJ1000_REFERENCE_DATASET.")
        return 1
    if not args.dataset_root.exists():
        print(f"dataset root not found: {args.dataset_root}")
        return 1

    args.scratch.mkdir(parents=True, exist_ok=True)

    for case in CORPUS_CASES:
        controls = dict(DEFAULT_CONTROLS)
        for key in DEFAULT_CONTROLS:
            if key in case:
                controls[key] = int(case[key])

        dat_path = args.dataset_root / str(case["dat"])
        bmp_path = args.dataset_root / str(case["bmp"])
        output_path = args.scratch / str(case["bmp"])
        command = build_command(
            args.native_binary,
            dat_path,
            output_path,
            str(case["mode"]),
            controls,
        )
        subprocess.run(command, check=True, stdout=subprocess.DEVNULL)

        output_bytes = output_path.read_bytes()
        reference_bytes = bmp_path.read_bytes()
        if output_bytes != reference_bytes:
            index, output_byte, reference_byte = first_mismatch(output_bytes, reference_bytes)
            diff_count = sum(left != right for left, right in zip(output_bytes, reference_bytes))
            diff_count += abs(len(output_bytes) - len(reference_bytes))
            print(
                f"MISMATCH {case['bmp']} mode={case['mode']} first_offset={index} "
                f"native={output_byte} reference={reference_byte} diff_count={diff_count}"
            )
            return 1

        print(f"OK {case['bmp']} mode={case['mode']}")

    print(f"verified {len(CORPUS_CASES)} corpus BMPs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
