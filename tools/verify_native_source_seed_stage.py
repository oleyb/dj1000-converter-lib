#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_source_stage


MODE_CONFIGS = {
    "normal": {
        "preview_flag": 0,
        "input_stride": 0x200,
        "active_width": 0x1F8,
        "active_rows": 0x0F4,
        "cases": (
            ("impulse-ee", (40, 60)),
            ("impulse-eo", (40, 61)),
            ("impulse-oe", (41, 60)),
            ("impulse-oo", (41, 61)),
        ),
    },
    "preview": {
        "preview_flag": 1,
        "input_stride": 0x80,
        "active_width": 0x80,
        "active_rows": 0x40,
        "cases": (
            ("impulse-ee", (40, 60)),
        ),
    },
}


def build_source(config: dict[str, object], case_name: str, impulse: tuple[int, int] | None) -> bytes:
    source = bytearray(config["input_stride"] * config["active_rows"])
    if case_name.startswith("impulse") and impulse is not None:
        row, col = impulse
        source[row * config["input_stride"] + col] = 0xFF
        return bytes(source)

    raise ValueError(f"unsupported case: {case_name}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native source-stage impulse seed helper against the original DLL on proven impulse cases."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_source_seed_stage"),
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
        for case_name, impulse in config["cases"]:
            input_path = args.scratch / f"{mode_name}.{case_name}.input.bin"
            input_path.write_bytes(build_source(config, case_name, impulse))

            dll_prefix = args.scratch / f"{mode_name}.{case_name}.dll"
            dll_outputs = dump_source_stage(
                source_input_path=input_path,
                output_prefix=dll_prefix,
                preview_flag=config["preview_flag"],
            )

            native_prefix = args.scratch / f"{mode_name}.{case_name}.native"
            subprocess.run(
                [
                    str(args.native_binary),
                    "source-seed-stage",
                    str(config["preview_flag"]),
                    str(input_path),
                    str(native_prefix),
                ],
                check=True,
            )

            native_outputs = (
                native_prefix.parent / f"{native_prefix.name}.plane0.f32",
                native_prefix.parent / f"{native_prefix.name}.plane1.f32",
                native_prefix.parent / f"{native_prefix.name}.plane2.f32",
            )

            for plane_index, (dll_path, native_path) in enumerate(zip(dll_outputs, native_outputs)):
                if dll_path.read_bytes() != native_path.read_bytes():
                    print(f"MISMATCH mode={mode_name} case={case_name} plane={plane_index}")
                    print(f"  dll:    {dll_path}")
                    print(f"  native: {native_path}")
                    return 1

            print(f"OK mode={mode_name} case={case_name}")
            verified_cases += 1

    print(f"verified {verified_cases} source-stage impulse seed cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
