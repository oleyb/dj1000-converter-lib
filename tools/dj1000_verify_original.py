#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir

from dj1000_convert_original import compile_harness, run_harness


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify that the Wine-driven original DLL matches a reference corpus."
    )
    parser.add_argument("directory", type=Path)
    parser.add_argument(
        "--scratch",
        type=Path,
        default=scratch_dir("verification"),
    )
    args = parser.parse_args()

    compile_harness(force=False)
    args.scratch.mkdir(parents=True, exist_ok=True)

    checks: list[tuple[str, Path, Path, int]] = []
    for dat_path in sorted(args.directory.glob("MDSC*.DAT")):
        stem = dat_path.stem.lower()
        number = stem[-4:]
        large_bmp = args.directory / f"mdsc{number}.bmp"
        small_bmp = args.directory / f"mdsc{number}s.bmp"
        if large_bmp.exists():
            checks.append((f"MDSC{number} large", dat_path, large_bmp, 5))
        if small_bmp.exists():
            checks.append((f"MDSC{number} small", dat_path, small_bmp, 0))

    matched = 0
    for label, dat_path, reference_path, export_mode in checks:
        candidate = args.scratch / reference_path.name
        run_harness(
            input_path=dat_path,
            output_path=candidate,
            export_mode=export_mode,
            converter_mode=0,
        )
        candidate_hash = sha256(candidate)
        reference_hash = sha256(reference_path)
        identical = candidate_hash == reference_hash
        status = "OK" if identical else "MISMATCH"
        print(f"{status} {label}: {candidate_hash}")
        if not identical:
            print(f"  candidate: {candidate}")
            print(f"  reference: {reference_path}")
            print(f"  expected:  {reference_hash}")
            return 1
        matched += 1

    print(f"verified {matched} reference BMPs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
