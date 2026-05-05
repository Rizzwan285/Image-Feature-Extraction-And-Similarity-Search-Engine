#!/usr/bin/env python3
"""converting png/jpg images to ppm format"""
import os
import sys
from PIL import Image


SUPPORTED = (".png", ".jpg", ".jpeg", ".bmp", ".webp", ".gif")


def main():
    in_dir = sys.argv[1] if len(sys.argv) > 1 else "data/images"
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "data/ppm"
    os.makedirs(out_dir, exist_ok=True)

    converted = 0
    for name in sorted(os.listdir(in_dir)):
        lower = name.lower()
        if not lower.endswith(SUPPORTED):
            continue
        in_path = os.path.join(in_dir, name)
        stem, _ = os.path.splitext(name)
        out_path = os.path.join(out_dir, stem + ".ppm")
        try:
            img = Image.open(in_path).convert("RGB")
            img.save(out_path, format="PPM")
            converted += 1
            print(f"converted {name} -> {stem}.ppm")
        except Exception as e:
            print(f"skipping {name}: {e}", file=sys.stderr)

    print(f"done. {converted} files written to {out_dir}")


if __name__ == "__main__":
    main()
