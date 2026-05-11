#!/usr/bin/env python3

from __future__ import annotations

import argparse
import filecmp
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_resample_lut


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native resample LUT helper against the original DsGraph.dll helper."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_resample_lut"),
    )
    parser.add_argument(
        "--helper-mode-e4",
        type=int,
        default=0,
        choices=(0, 1),
        help="value to write to the DLL's 0x1001a0e4 mode flag before dumping",
    )
    parser.add_argument(
        "widths",
        nargs="*",
        type=int,
        default=[0x7C, 0x101, 0x103, 0x140, 0x1F8],
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    for width in args.widths:
        dll_output = args.scratch / f"dll_{width}.bin"
        native_output = args.scratch / f"native_{width}.bin"

        dump_resample_lut(width, dll_output, helper_mode_e4=args.helper_mode_e4)
        subprocess.run(
            [
                str(args.native_binary),
                "dump-resample-lut",
                str(width),
                str(native_output),
                *(["--preserve-full-scale-marker"] if args.helper_mode_e4 == 1 else []),
            ],
            check=True,
        )

        if not filecmp.cmp(dll_output, native_output, shallow=False):
            print(f"MISMATCH width={width}")
            print(f"  dll:    {dll_output}")
            print(f"  native: {native_output}")
            return 1

        print(
            f"OK width={width} bytes={dll_output.stat().st_size} helper_mode_e4={args.helper_mode_e4}"
        )

    print(f"verified {len(args.widths)} resample LUT widths at helper_mode_e4={args.helper_mode_e4}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
