#!/usr/bin/env python3

from __future__ import annotations

import argparse
import filecmp
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_row_resample


CONFIGS = {
    "normal": {
        "lut_width_parameter": 0x101,
        "width_limit": 0x1F9,
        "output_length": 0x144,
        "source_length": 504,
    },
    "preview": {
        "lut_width_parameter": 0x103,
        "width_limit": 0x7C,
        "output_length": 0x50,
        "source_length": 126,
    },
}


def build_row(pattern: str, length: int, impulse_index: int) -> bytes:
    if pattern == "rowcode":
        return bytes((index & 0xFF) for index in range(length))
    if pattern == "gradient":
        return bytes(((index * 5) + 17) & 0xFF for index in range(length))
    if pattern == "impulse":
        out = bytearray(length)
        out[impulse_index] = 0xA0
        return bytes(out)
    raise ValueError(f"unknown pattern: {pattern}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native non-large row helper against the original DLL row resampler."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_nonlarge_row"),
    )
    parser.add_argument(
        "--configs",
        nargs="*",
        choices=tuple(CONFIGS.keys()),
        default=["normal", "preview"],
    )
    parser.add_argument(
        "--patterns",
        nargs="*",
        choices=("rowcode", "gradient", "impulse"),
        default=["rowcode", "gradient", "impulse"],
    )
    parser.add_argument("--impulse-index", type=int, default=60)
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for config_name in args.configs:
        config = CONFIGS[config_name]
        if not (0 <= args.impulse_index < config["source_length"]):
            print(f"impulse index out of range for config={config_name}")
            return 1

        for pattern in args.patterns:
            source_path = args.scratch / f"{config_name}.{pattern}.input.bin"
            source_path.write_bytes(
                build_row(pattern, config["source_length"], args.impulse_index)
            )

            native_output = args.scratch / f"{config_name}.{pattern}.native.bin"
            subprocess.run(
                [
                    str(args.native_binary),
                    "nonlarge-row",
                    str(source_path),
                    str(config["lut_width_parameter"]),
                    str(config["width_limit"]),
                    str(config["output_length"]),
                    str(native_output),
                ],
                check=True,
            )

            dll_output = args.scratch / f"{config_name}.{pattern}.dll.bin"
            dump_row_resample(
                row_input_path=source_path,
                output_path=dll_output,
                lut_width_parameter=config["lut_width_parameter"],
                width_limit=config["width_limit"],
                output_length=config["output_length"],
                reset_counter=0x100,
                mode_flag=0,
                mode_e0=1,
                helper_mode_e4=1,
            )

            if not filecmp.cmp(dll_output, native_output, shallow=False):
                print(f"MISMATCH config={config_name} pattern={pattern}")
                print(f"  dll:    {dll_output}")
                print(f"  native: {native_output}")
                return 1

            print(
                f"OK config={config_name} pattern={pattern} bytes={native_output.stat().st_size}"
            )

    print(
        f"verified {len(args.configs) * len(args.patterns)} non-large row native cases"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
