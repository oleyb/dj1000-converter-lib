#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, dump_post_rgb_stage_3b00


WIDTH = 256
HEIGHT = 1

CASES = {
    "scalar30-level3": {"scalar": 30.0, "level": 3},
    "scalar30-level4": {"scalar": 30.0, "level": 4},
    "scalar0-level3": {"scalar": 0.0, "level": 3},
}


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare the native post-RGB stage 0x3b00 against the original DLL helper."
    )
    parser.add_argument(
        "--native-binary",
        type=Path,
        default=native_binary_default(),
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("native_post_rgb_stage_3b00"),
    )
    args = parser.parse_args()

    if not args.native_binary.exists():
        print(f"native binary not found: {args.native_binary}")
        return 1

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    ramp = bytes(range(256))
    plane0_path = args.scratch / "ramp.plane0.bin"
    plane1_path = args.scratch / "ramp.plane1.bin"
    plane2_path = args.scratch / "ramp.plane2.bin"
    plane0_path.write_bytes(ramp)
    plane1_path.write_bytes(ramp)
    plane2_path.write_bytes(ramp)

    for case_name, case in CASES.items():
        dll_prefix = args.scratch / f"{case_name}.dll"
        dll_plane0, dll_plane1, dll_plane2 = dump_post_rgb_stage_3b00(
            plane0_path=plane0_path,
            plane1_path=plane1_path,
            plane2_path=plane2_path,
            output_prefix=dll_prefix,
            width=WIDTH,
            height=HEIGHT,
            level=case["level"],
            scalar=case["scalar"],
        )

        native_prefix = args.scratch / f"{case_name}.native"
        subprocess.run(
            [
                str(args.native_binary),
                "post-rgb-stage-3b00",
                str(WIDTH),
                str(HEIGHT),
                str(case["level"]),
                str(case["scalar"]),
                str(plane0_path),
                str(plane1_path),
                str(plane2_path),
                str(native_prefix),
            ],
            check=True,
        )

        outputs = [
            (dll_plane0, native_prefix.parent / f"{native_prefix.name}.plane0.bin"),
            (dll_plane1, native_prefix.parent / f"{native_prefix.name}.plane1.bin"),
            (dll_plane2, native_prefix.parent / f"{native_prefix.name}.plane2.bin"),
        ]
        for dll_path, native_path in outputs:
            if dll_path.read_bytes() != native_path.read_bytes():
                print(f"MISMATCH case={case_name}")
                print(f"  dll:    {dll_path}")
                print(f"  native: {native_path}")
                return 1

        print(f"OK case={case_name}")

    print(f"verified {len(CASES)} post-RGB stage 0x3b00 cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
