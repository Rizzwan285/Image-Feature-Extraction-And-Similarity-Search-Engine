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
        # 5 sunset images
        ("01_sunset_warm",      lambda: stylized_landscape_sunset(W, H)),
        ("02_sunset_purple",    lambda: smooth_gradient(W, H, (90, 30, 120), (255, 100, 60))),
        ("21_sunset_gold",      lambda: horizon_scene(W, H, (90, 35, 90), (255, 180, 90), (50, 25, 55), sun=(int(W * 0.55), int(H * 0.48), 45)).filter(ImageFilter.GaussianBlur(radius=1.0))),
        ("22_sunset_crimson",   lambda: horizon_scene(W, H, (80, 20, 70), (240, 70, 40), (35, 15, 40), sun=(int(W * 0.75), int(H * 0.40), 55)).filter(ImageFilter.GaussianBlur(radius=1.1))),
        ("23_sunset_soft",      lambda: smooth_gradient(W, H, (120, 60, 120), (255, 170, 110), "vert").filter(ImageFilter.GaussianBlur(radius=1.0))),

        # 5 dawn images
        ("03_dawn_soft",        lambda: stylized_landscape_dawn(W, H)),
        ("24_dawn_peach",       lambda: horizon_scene(W, H, (60, 100, 160), (255, 210, 190), (70, 100, 65), sun=(int(W * 0.25), int(H * 0.52), 40)).filter(ImageFilter.GaussianBlur(radius=1.0))),
        ("25_dawn_blue",        lambda: smooth_gradient(W, H, (25, 55, 120), (190, 220, 250), "vert").filter(ImageFilter.GaussianBlur(radius=0.9))),
        ("26_dawn_orange",      lambda: horizon_scene(W, H, (45, 80, 150), (255, 190, 140), (65, 85, 55), sun=(int(W * 0.42), int(H * 0.58), 45)).filter(ImageFilter.GaussianBlur(radius=1.1))),
        ("27_dawn_mist",        lambda: smooth_gradient(W, H, (80, 110, 170), (240, 225, 210), "diag").filter(ImageFilter.GaussianBlur(radius=1.5))),

        # 5 ocean images
        ("04_ocean_blue",       lambda: stylized_ocean(W, H)),
        ("05_ocean_deep",       lambda: smooth_gradient(W, H, (10, 30, 80), (5, 15, 40), "vert")),
        ("28_ocean_calm",       lambda: smooth_gradient(W, H, (150, 220, 245), (30, 110, 170), "vert").filter(ImageFilter.GaussianBlur(radius=0.7))),
        ("29_ocean_teal",       lambda: smooth_gradient(W, H, (80, 210, 210), (5, 80, 120), "vert").filter(ImageFilter.GaussianBlur(radius=0.7))),
        ("30_ocean_night",      lambda: smooth_gradient(W, H, (20, 60, 120), (2, 10, 35), "vert").filter(ImageFilter.GaussianBlur(radius=0.8))),

        # 5 forest images
        ("06_forest_green",     lambda: stylized_forest(W, H)),
        ("07_forest_dark",      lambda: smooth_gradient(W, H, (15, 60, 30), (5, 25, 10), "vert")),
        ("31_forest_moss",      lambda: smooth_gradient(W, H, (80, 140, 70), (20, 70, 25), "vert").filter(ImageFilter.GaussianBlur(radius=0.9))),
        ("32_forest_shadow",    lambda: smooth_gradient(W, H, (35, 90, 45), (5, 35, 12), "diag").filter(ImageFilter.GaussianBlur(radius=1.0))),
        ("33_forest_light",     lambda: smooth_gradient(W, H, (120, 180, 95), (35, 95, 40), "vert").filter(ImageFilter.GaussianBlur(radius=0.9))),

        # 5 desert images
        ("08_desert_warm",      lambda: stylized_desert(W, H)),
        ("34_desert_gold",      lambda: horizon_scene(W, H, (255, 190, 130), (255, 225, 170), (230, 170, 80)).filter(ImageFilter.GaussianBlur(radius=0.9))),
        ("35_desert_pale",      lambda: horizon_scene(W, H, (255, 210, 170), (250, 235, 200), (210, 170, 110)).filter(ImageFilter.GaussianBlur(radius=1.0))),
        ("36_desert_sunset",    lambda: horizon_scene(W, H, (240, 120, 80), (255, 215, 160), (190, 120, 60), sun=(int(W * 0.68), int(H * 0.43), 45)).filter(ImageFilter.GaussianBlur(radius=1.0))),
        ("37_desert_dune",      lambda: smooth_gradient(W, H, (255, 220, 160), (190, 120, 55), "vert").filter(ImageFilter.GaussianBlur(radius=0.9))),

        # 5 radial images
        ("09_radial_violet",    lambda: radial_gradient(W, H, (240, 200, 255), (40, 10, 80))),
        ("10_radial_amber",     lambda: radial_gradient(W, H, (255, 230, 150), (120, 50, 10))),
        ("38_radial_blue",      lambda: radial_gradient(W, H, (210, 235, 255), (20, 45, 120))),
        ("39_radial_green",     lambda: radial_gradient(W, H, (230, 255, 210), (20, 90, 45))),
        ("40_radial_rose",      lambda: radial_gradient(W, H, (255, 220, 235), (140, 30, 80))),

        # 5 stripes images
        ("11_stripes_navy",     lambda: stripes(W, H, (10, 20, 60), (240, 240, 250), 8)),
        ("12_stripes_crimson",  lambda: stripes(W, H, (140, 20, 30), (250, 240, 220), 6, vertical=True)),
        ("41_stripes_green",    lambda: stripes(W, H, (30, 100, 60), (220, 250, 230), 10)),
        ("42_stripes_gold",     lambda: stripes(W, H, (190, 130, 40), (255, 240, 190), 7, vertical=True)),
        ("43_stripes_gray",     lambda: stripes(W, H, (60, 60, 70), (230, 230, 240), 9)),

        # 5 dotted images
        ("13_dotted_mint",      lambda: dotted(W, H, (200, 240, 220), (40, 120, 100), 60)),
        ("14_dotted_rose",      lambda: dotted(W, H, (250, 220, 230), (200, 60, 100), 60)),
        ("44_dotted_blue",      lambda: dotted(W, H, (215, 235, 255), (50, 90, 180), 55)),
        ("45_dotted_yellow",    lambda: dotted(W, H, (255, 245, 200), (210, 150, 40), 55)),
        ("46_dotted_violet",    lambda: dotted(W, H, (235, 220, 250), (110, 60, 170), 55)),

        # 5 checker images
        ("15_checker_mono",     lambda: checker(W, H, (240, 240, 240), (40, 40, 40), 10)),
        ("16_checker_warm",     lambda: checker(W, H, (255, 220, 180), (180, 80, 40), 8)),
        ("47_checker_blue",     lambda: checker(W, H, (220, 235, 255), (40, 90, 170), 8)),
        ("48_checker_green",    lambda: checker(W, H, (220, 250, 220), (40, 130, 70), 9)),
        ("49_checker_purple",   lambda: checker(W, H, (235, 220, 250), (100, 50, 150), 8)),

        # 5 gradient images
        ("17_gradient_cyan",    lambda: smooth_gradient(W, H, (0, 220, 255), (0, 60, 120))),
        ("18_gradient_pink",    lambda: smooth_gradient(W, H, (255, 200, 220), (220, 60, 130))),
        ("19_gradient_lime",    lambda: smooth_gradient(W, H, (240, 255, 200), (60, 140, 30))),
        ("20_gradient_dusk",    lambda: smooth_gradient(W, H, (40, 30, 80), (200, 80, 120))),
        ("50_gradient_fire",    lambda: smooth_gradient(W, H, (255, 230, 100), (180, 20, 20))),
    ]

    for name, fn in catalog:
        path = os.path.join(out_dir, f"{name}.png")
        img = fn()
        img.save(path, "PNG")
        print(f"wrote {path}")

    print(f"done. {len(catalog)} images in {out_dir}")


if __name__ == "__main__":
    main()
