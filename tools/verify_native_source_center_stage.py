#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_source_stage
from probe_source_stage import MODE_CONFIGS, build_source


CASES = (
    {
        "name": "flat-200",
        "pattern": "flat",
        "value_a": 200,
        "value_b": 200,
        "split_row": 40,
        "split_col": 60,
    },
    {
        "name": "flat-255",
        "pattern": "flat",
        "value_a": 255,
        "value_b": 255,
        "split_row": 40,
        "split_col": 60,
    },
    {
        "name": "rowcode",
        "pattern": "rowcode",
        "value_a": 0,
        "value_b": 0,
        "split_row": 40,
        "split_col": 60,
    },
    {
        "name": "checker-64-192",
        "pattern": "checker",
        "value_a": 64,
        "value_b": 192,
        "split_row": 40,
        "split_col": 60,
    },
    {
        "name": "hstep-64-192",
        "pattern": "hstep",
        "value_a": 64,
        "value_b": 192,
        "split_row": 40,
        "split_col": 60,
    },
    {
        "name": "hstep-192-64",
        "pattern": "hstep",
        "value_a": 192,
        "value_b": 64,
        "split_row": 40,
        "split_col": 60,
    },
    {
        "name": "vstep-200-255",
        "pattern": "vstep",
        "value_a": 200,
        "value_b": 255,
        "split_row": 40,
        "split_col": 60,
    },
    {
        "name": "vstep-100-255",
        "pattern": "vstep",
        "value_a": 100,
        "value_b": 255,
        "split_row": 40,
        "split_col": 60,
    },
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native source-center stage against the original DLL middle-plane output."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_source_center_stage"),
    )
    parser.add_argument(
        "--modes",
        nargs="*",
        choices=tuple(MODE_CONFIGS.keys()),
        default=["normal", "preview"],
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    verified_cases = 0
    for mode_name in args.modes:
        config = MODE_CONFIGS[mode_name]
        for case in CASES:
            input_path = args.scratch / f"{mode_name}.{case['name']}.input.bin"
            input_path.write_bytes(
                build_source(
                    pattern=case["pattern"],
                    config=config,
                    impulse_row=0,
                    impulse_col=0,
                    value_a=case["value_a"],
                    value_b=case["value_b"],
                    split_row=case["split_row"],
                    split_col=case["split_col"],
                )
            )

            dll_prefix = args.scratch / f"{mode_name}.{case['name']}.dll"
            dll_outputs = dump_source_stage(
                source_input_path=input_path,
                output_prefix=dll_prefix,
                preview_flag=config["preview_flag"],
            )

            native_output = args.scratch / f"{mode_name}.{case['name']}.native.center.f32"
            subprocess.run(
                [
                    str(args.native_binary),
                    "source-center-stage",
                    str(config["preview_flag"]),
                    str(input_path),
                    str(native_output),
                ],
                check=True,
            )

            if dll_outputs[1].read_bytes() != native_output.read_bytes():
                print(f"MISMATCH mode={mode_name} case={case['name']} plane=1")
                print(f"  dll:    {dll_outputs[1]}")
                print(f"  native: {native_output}")
                return 1

            print(f"OK mode={mode_name} case={case['name']}")
            verified_cases += 1

    print(f"verified {verified_cases} source-center stage cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
