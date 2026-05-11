#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

from dj1000_dat_info import EXPECTED_SIZE, RAW_BLOCK_SIZE, parse_dat


INDEX_WIDTH = 128
INDEX_HEIGHT = 64
INDEX_BYTES = INDEX_WIDTH * INDEX_HEIGHT


def trans_to_index_view(raw_block: bytes) -> bytes:
    """Exact byte reorder performed by DsGraph.dll::TransToIndexView."""
    if len(raw_block) < RAW_BLOCK_SIZE:
        raise ValueError(
            f"expected at least 0x{RAW_BLOCK_SIZE:x} bytes of raw payload, "
            f"got 0x{len(raw_block):x}"
        )

    out = bytearray(INDEX_BYTES)
    src_block_offset = 0
    dst_row = 0

    for _ in range(0x20):
        src_a = src_block_offset
        src_b = src_block_offset + 0x200
        dst_a = dst_row * INDEX_WIDTH
        dst_b = (dst_row + 1) * INDEX_WIDTH

        for group in range(0x40):
            src_pair = group * 8
            dst_pair = group * 2
            out[dst_a + dst_pair] = raw_block[src_a + src_pair]
            out[dst_a + dst_pair + 1] = raw_block[src_a + src_pair + 1]
            out[dst_b + dst_pair] = raw_block[src_b + src_pair]
            out[dst_b + dst_pair + 1] = raw_block[src_b + src_pair + 1]

        src_block_offset += 0x1000
        dst_row += 2

    return bytes(out)


def save_image(index_view: bytes, path: Path) -> None:
    image = Image.frombytes("L", (INDEX_WIDTH, INDEX_HEIGHT), index_view)
    image.save(path)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Recreate the exact preview/index byte reorder used by "
            "DsGraph.dll::TransToIndexView."
        )
    )
    parser.add_argument("dat_path", type=Path)
    parser.add_argument(
        "--raw-out",
        type=Path,
        help="write the 0x2000-byte reordered preview buffer",
    )
    parser.add_argument(
        "--image-out",
        type=Path,
        help="write the reordered preview as a grayscale image",
    )
    args = parser.parse_args()

    info = parse_dat(args.dat_path)
    size = int(info["size"])
    if size != EXPECTED_SIZE:
        raise SystemExit(
            f"{args.dat_path}: expected 0x{EXPECTED_SIZE:x} bytes, got 0x{size:x}"
        )
    if not bool(info["signature_matches"]):
        raise SystemExit(
            f"{args.dat_path}: trailer signature mismatch ({info['signature_hex']})"
        )

    data = args.dat_path.read_bytes()
    index_view = trans_to_index_view(data[:RAW_BLOCK_SIZE])

    if args.raw_out is not None:
        args.raw_out.write_bytes(index_view)
    if args.image_out is not None:
        save_image(index_view, args.image_out)

    print(args.dat_path)
    print(f"  raw payload: 0x{RAW_BLOCK_SIZE:x} bytes")
    print(f"  index view: {INDEX_WIDTH}x{INDEX_HEIGHT} ({INDEX_BYTES} bytes)")
    print(f"  trailer: {info['trailer_hex']}")
    if args.raw_out is not None:
        print(f"  raw output: {args.raw_out}")
    if args.image_out is not None:
        print(f"  image output: {args.image_out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
