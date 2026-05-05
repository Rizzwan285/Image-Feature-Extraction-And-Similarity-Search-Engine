#!/usr/bin/env python3
"""generating sample procedural images for dataset"""
import math
import os
import random
import sys
from PIL import Image, ImageDraw, ImageFilter


def smooth_gradient(w, h, c1, c2, direction="diag"):
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            if direction == "horiz":
                t = x / max(1, w - 1)
            elif direction == "vert":
                t = y / max(1, h - 1)
            else:
                t = (x + y) / max(1, (w + h - 2))
            r = int(c1[0] * (1 - t) + c2[0] * t)
            g = int(c1[1] * (1 - t) + c2[1] * t)
            b = int(c1[2] * (1 - t) + c2[2] * t)
            px[x, y] = (r, g, b)
    return img


def radial_gradient(w, h, inner, outer):
    img = Image.new("RGB", (w, h))
    px = img.load()
    cx, cy = w / 2, h / 2
    max_d = math.hypot(cx, cy)
    for y in range(h):
        for x in range(w):
            d = math.hypot(x - cx, y - cy) / max_d
            d = min(1.0, d)
            r = int(inner[0] * (1 - d) + outer[0] * d)
            g = int(inner[1] * (1 - d) + outer[1] * d)
            b = int(inner[2] * (1 - d) + outer[2] * d)
            px[x, y] = (r, g, b)
    return img


def horizon_scene(w, h, sky_top, sky_bottom, ground, sun=None):
    img = smooth_gradient(w, h, sky_top, sky_bottom, "vert")
    horizon = int(h * 0.65)
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, horizon, w, h], fill=ground)
    if sun:
        sx, sy, sr = sun
        draw.ellipse([sx - sr, sy - sr, sx + sr, sy + sr], fill=(255, 240, 200))
    return img


def stripes(w, h, c1, c2, count=8, vertical=False):
    img = Image.new("RGB", (w, h), c1)
    draw = ImageDraw.Draw(img)
    if vertical:
        sw = w / count
        for i in range(count):
            if i % 2:
                draw.rectangle([int(i * sw), 0, int((i + 1) * sw), h], fill=c2)
    else:
        sh = h / count
        for i in range(count):
            if i % 2:
                draw.rectangle([0, int(i * sh), w, int((i + 1) * sh)], fill=c2)
    return img


def dotted(w, h, bg, dot, count=40):
    img = Image.new("RGB", (w, h), bg)
    draw = ImageDraw.Draw(img)
    rng = random.Random(42)
    for _ in range(count):
        x = rng.randint(0, w)
        y = rng.randint(0, h)
        r = rng.randint(8, 24)
        draw.ellipse([x - r, y - r, x + r, y + r], fill=dot)
    return img


def checker(w, h, c1, c2, n=8):
    img = Image.new("RGB", (w, h), c1)
    draw = ImageDraw.Draw(img)
    sx = w / n
    sy = h / n
    for i in range(n):
        for j in range(n):
            if (i + j) % 2:
                draw.rectangle(
                    [int(j * sx), int(i * sy), int((j + 1) * sx), int((i + 1) * sy)],
                    fill=c2,
                )
    return img


def stylized_landscape_sunset(w, h):
    img = horizon_scene(
        w, h,
        sky_top=(60, 30, 90),
        sky_bottom=(255, 140, 80),
        ground=(40, 20, 60),
        sun=(w * 0.7, h * 0.45, 60),
    )
    return img.filter(ImageFilter.GaussianBlur(radius=1.2))


def stylized_landscape_dawn(w, h):
    img = horizon_scene(
        w, h,
        sky_top=(30, 60, 120),
        sky_bottom=(255, 200, 180),
        ground=(60, 90, 50),
        sun=(w * 0.3, h * 0.55, 50),
    )
    return img.filter(ImageFilter.GaussianBlur(radius=1.2))


def stylized_ocean(w, h):
    img = smooth_gradient(w, h, (135, 206, 235), (25, 90, 140), "vert")
    draw = ImageDraw.Draw(img)
    for y in range(int(h * 0.6), h, 8):
        draw.line([(0, y), (w, y)], fill=(255, 255, 255), width=1)
    return img.filter(ImageFilter.GaussianBlur(radius=0.6))


def stylized_forest(w, h):
    img = smooth_gradient(w, h, (50, 110, 60), (15, 50, 25), "vert")
    draw = ImageDraw.Draw(img)
    rng = random.Random(7)
    for _ in range(35):
        x = rng.randint(0, w)
        y = rng.randint(int(h * 0.3), h)
        radius = rng.randint(20, 50)
        shade = rng.randint(20, 80)
        draw.ellipse(
            [x - radius, y - radius, x + radius, y + radius],
            fill=(shade, shade + 40, shade + 10),
        )
    return img.filter(ImageFilter.GaussianBlur(radius=1.0))


def stylized_desert(w, h):
    img = horizon_scene(
        w, h,
        sky_top=(255, 180, 120),
        sky_bottom=(255, 220, 180),
        ground=(220, 160, 90),
    )
    return img.filter(ImageFilter.GaussianBlur(radius=1.0))


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "data/images"
    os.makedirs(out_dir, exist_ok=True)
    W, H = 480, 360

    catalog = [
        ("01_sunset_warm",     lambda: stylized_landscape_sunset(W, H)),
        ("02_sunset_purple",   lambda: smooth_gradient(W, H, (90, 30, 120), (255, 100, 60))),
        ("03_dawn_soft",       lambda: stylized_landscape_dawn(W, H)),
        ("04_ocean_blue",      lambda: stylized_ocean(W, H)),
        ("05_ocean_deep",      lambda: smooth_gradient(W, H, (10, 30, 80), (5, 15, 40), "vert")),
        ("06_forest_green",    lambda: stylized_forest(W, H)),
        ("07_forest_dark",     lambda: smooth_gradient(W, H, (15, 60, 30), (5, 25, 10), "vert")),
        ("08_desert_warm",     lambda: stylized_desert(W, H)),
        ("09_radial_violet",   lambda: radial_gradient(W, H, (240, 200, 255), (40, 10, 80))),
        ("10_radial_amber",    lambda: radial_gradient(W, H, (255, 230, 150), (120, 50, 10))),
        ("11_stripes_navy",    lambda: stripes(W, H, (10, 20, 60), (240, 240, 250), 8)),
        ("12_stripes_crimson", lambda: stripes(W, H, (140, 20, 30), (250, 240, 220), 6, vertical=True)),
        ("13_dotted_mint",     lambda: dotted(W, H, (200, 240, 220), (40, 120, 100), 60)),
        ("14_dotted_rose",     lambda: dotted(W, H, (250, 220, 230), (200, 60, 100), 60)),
        ("15_checker_mono",    lambda: checker(W, H, (240, 240, 240), (40, 40, 40), 10)),
        ("16_checker_warm",    lambda: checker(W, H, (255, 220, 180), (180, 80, 40), 8)),
        ("17_gradient_cyan",   lambda: smooth_gradient(W, H, (0, 220, 255), (0, 60, 120))),
        ("18_gradient_pink",   lambda: smooth_gradient(W, H, (255, 200, 220), (220, 60, 130))),
        ("19_gradient_lime",   lambda: smooth_gradient(W, H, (240, 255, 200), (60, 140, 30))),
        ("20_gradient_dusk",   lambda: smooth_gradient(W, H, (40, 30, 80), (200, 80, 120))),
    ]

    for name, fn in catalog:
        path = os.path.join(out_dir, f"{name}.png")
        img = fn()
        img.save(path, "PNG")
        print(f"wrote {path}")

    print(f"done. {len(catalog)} images in {out_dir}")


if __name__ == "__main__":
    main()
