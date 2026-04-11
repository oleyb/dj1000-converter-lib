#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_manifest(corpus: Path) -> dict[str, object]:
    dat_files = sorted(corpus.glob("*.DAT"))
    bmp_files = {path.name.lower(): path for path in corpus.glob("*.bmp")}

    jobs: list[dict[str, object]] = []
    for dat_path in dat_files:
        if dat_path.stat().st_size == 0:
            continue

        stem = dat_path.stem.lower()
        large_name = f"{stem}.bmp"
        small_name = f"{stem}s.bmp"

        job: dict[str, object] = {
            "dat": dat_path.name,
            "dat_sha256": sha256(dat_path),
            "dat_size": dat_path.stat().st_size,
        }

        for label, file_name in (("large", large_name), ("small", small_name)):
            bmp_path = bmp_files.get(file_name)
            if bmp_path is None:
                continue
            with Image.open(bmp_path) as image:
                job[label] = {
                    "bmp": bmp_path.name,
                    "sha256": sha256(bmp_path),
                    "width": image.width,
                    "height": image.height,
                }
        jobs.append(job)

    return {
        "corpus": corpus.name,
        "jobs": jobs,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a native-rewrite fixture manifest from matched DAT/BMP samples."
    )
    parser.add_argument("corpus", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    manifest = build_manifest(args.corpus.resolve())
    args.output.write_text(json.dumps(manifest, indent=2) + "\n")
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
