#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
from pathlib import Path


EXPECTED_SIZE = 0x20000
RAW_BLOCK_SIZE = 0x1F600
TRAILER_SIZE = 13
EXPECTED_SIGNATURE = bytes([0xC4, 0xB2, 0xE3, 0x22])


def parse_dat(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    trailer = data[RAW_BLOCK_SIZE:RAW_BLOCK_SIZE + TRAILER_SIZE]
    tail = data[RAW_BLOCK_SIZE + TRAILER_SIZE:]

    version_major = trailer[5] if len(trailer) >= 6 else None
    version_minor = trailer[7] if len(trailer) >= 8 else None
    camera_version = (
        f"{version_major}.{version_minor:02d}"
        if version_major is not None and version_minor is not None
        else None
    )

    # PhotoRun.exe uses byte 8 directly and combines bytes 10/11 into a value:
    # value = trailer[11] + 10 * trailer[10]
    mode_flag = trailer[8] if len(trailer) >= 9 else None
    mode_value = trailer[11] + 10 * trailer[10] if len(trailer) >= 12 else None

    return {
        "path": str(path),
        "size": len(data),
        "expected_size": EXPECTED_SIZE,
        "size_matches": len(data) == EXPECTED_SIZE,
        "raw_block_offset": 0,
        "raw_block_size": RAW_BLOCK_SIZE,
        "trailer_offset": RAW_BLOCK_SIZE,
        "trailer_size": len(trailer),
        "trailer_hex": trailer.hex(),
        "signature_hex": trailer[:4].hex(),
        "signature_matches": trailer[:4] == EXPECTED_SIGNATURE,
        "camera_version_bytes": trailer[4:8].hex() if len(trailer) >= 8 else None,
        "camera_version_guess": camera_version,
        "mode_flag_byte": mode_flag,
        "mode_value_guess": mode_value,
        "unused_tail_size": len(tail),
        "unused_tail_all_zero": all(b == 0 for b in tail),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Inspect Mitsubishi DJ-1000 / UMAX PhotoRun .dat files.")
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument(
        "--json",
        action="store_true",
        help="print machine-readable JSON",
    )
    args = parser.parse_args()

    results = [parse_dat(path) for path in args.paths]

    if args.json:
        print(json.dumps(results, indent=2))
        return 0

    for result in results:
        print(result["path"])
        print(f"  size: {result['size']} bytes")
        print(
            f"  raw block: offset 0x{result['raw_block_offset']:x}, "
            f"size 0x{result['raw_block_size']:x}"
        )
        print(
            f"  trailer @ 0x{result['trailer_offset']:x}: "
            f"{result['trailer_hex']}"
        )
        print(
            f"  signature matches: {result['signature_matches']} "
            f"({result['signature_hex']})"
        )
        print(f"  camera version guess: {result['camera_version_guess']}")
        print(f"  mode flag byte: {result['mode_flag_byte']}")
        print(f"  mode value guess: {result['mode_value_guess']}")
        print(
            f"  unused tail: {result['unused_tail_size']} bytes, "
            f"all zero={result['unused_tail_all_zero']}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
