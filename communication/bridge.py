"""
bridge.py
---------
This file is the go-between for the Python server and the C binary.

It handles three things:
  1. Knowing where everything lives (the C binary, the image folders, the cache)
  2. Converting uploaded images from PNG/JPG to PPM format (which C can read)
  3. Calling the C binary as a subprocess and reading the JSON it writes back

Think of it as a translator: React speaks HTTP, C speaks file paths and PPM
images. This file speaks both.
"""

# json lets us parse the JSON file that the C binary writes
import json

# os isn't directly used here but pathlib (below) covers our needs
import os

# subprocess is how Python runs another program — in our case, the C binary
import subprocess

# Path makes file paths much easier to work with than raw strings
from pathlib import Path

# Type hints — just for readability, Python doesn't enforce them at runtime
from typing import Any, Dict

# Pillow (PIL) is the image library we use to convert PNG/JPG → PPM
from PIL import Image


# ---------------------------------------------------------------------------
# Path constants — we resolve everything relative to the project root so
# this file works no matter which directory you start the server from.
# ---------------------------------------------------------------------------

# __file__ is this script's own path. .resolve() makes it absolute.
# .parent.parent goes up two levels (communication/ → project root)
PROJECT_ROOT = Path(__file__).resolve().parent.parent

# The compiled C binary — this is what `make` produces
ALGORITHM_BIN = PROJECT_ROOT / "algorithm" / "bin" / "imgsearch"

# The data directory and its subdirectories
DATA_DIR = PROJECT_ROOT / "data"
IMAGES_DIR = DATA_DIR / "images"    # original PNG/JPG images live here
PPM_DIR = DATA_DIR / "ppm"          # converted PPM files live here
CACHE_PATH = DATA_DIR / "features.cache"    # C binary reads/writes this
OUTPUT_JSON = DATA_DIR / "output.json"      # C binary writes results here


def ensure_directories() -> None:
    """
    Creates the data/images and data/ppm directories if they don't exist yet.
    Called on server startup and before any file operations.
    """
    # parents=True means it also creates any missing parent directories
    # exist_ok=True means it doesn't error if the directory already exists
    IMAGES_DIR.mkdir(parents=True, exist_ok=True)
    PPM_DIR.mkdir(parents=True, exist_ok=True)


def convert_to_ppm(source_path: Path, dest_path: Path) -> None:
    """
    Opens any image file Pillow can read (PNG, JPG, BMP, WebP, etc.)
    and saves it as a PPM P6 binary file that the C binary can load.

    Why PPM? Because it's the simplest possible image format — just a small
    text header followed by raw RGB bytes. Reading it in C requires no
    external libraries whatsoever.

    source_path — the original image file
    dest_path   — where to write the converted .ppm file
    """
    # .convert("RGB") ensures we always get 3 channels (no alpha, no grayscale)
    img = Image.open(source_path).convert("RGB")
    img.save(dest_path, format="PPM")  # Pillow knows how to write PPM


def sync_dataset_to_ppm() -> int:
    """
    Looks through data/images/ and makes sure every image has a matching
    .ppm file in data/ppm/. If a PPM is missing or older than its source
    image, we recreate it.

    This runs on server startup so the C binary always has PPMs ready to read.

    Returns how many files were (re)converted.
    """
    ensure_directories()
    converted = 0

    # Walk every file in the images directory
    for img_path in IMAGES_DIR.iterdir():
        if not img_path.is_file():
            continue    # skip directories

        # Only process image file types we recognize
        ext = img_path.suffix.lower()
        if ext not in (".png", ".jpg", ".jpeg", ".bmp", ".webp", ".gif"):
            continue

        # Figure out where the matching PPM should be
        # e.g. data/images/sunset.png → data/ppm/sunset.ppm
        ppm_path = PPM_DIR / (img_path.stem + ".ppm")

        # Convert if the PPM doesn't exist yet, or if the source image is newer
        # (st_mtime is the file's last-modified timestamp in seconds)
        if not ppm_path.exists() or ppm_path.stat().st_mtime < img_path.stat().st_mtime:
            convert_to_ppm(img_path, ppm_path)
            converted += 1

    return converted


def invalidate_cache() -> None:
    """
    Deletes the feature cache file so the next search re-extracts features
    from scratch. We call this whenever the dataset changes (new image uploaded
    or existing image replaced) because the cached numbers are no longer valid.
    """
    if CACHE_PATH.exists():
        CACHE_PATH.unlink()  # unlink = delete the file


def list_dataset() -> list:
    """
    Returns a list of dicts describing every image in data/images/.
    Each dict has:
      filename — the file's name including extension, e.g. "01_sunset.png"
      stem     — the name without extension, e.g. "01_sunset"
      url      — the API URL the UI can use to display this image

    The UI calls /api/dataset on startup to populate the dataset grid.
    """
    ensure_directories()
    items = []

    # sorted() gives us a consistent order (alphabetical by path)
    for img_path in sorted(IMAGES_DIR.iterdir()):
        if not img_path.is_file():
            continue
        # Only include actual image files, not the SOURCES.md or other files
        if img_path.suffix.lower() not in (".png", ".jpg", ".jpeg", ".bmp", ".webp"):
            continue
        items.append({
            "filename": img_path.name,      # full name with extension
            "stem": img_path.stem,          # name without extension
            "url": f"/api/image/{img_path.name}",   # URL the UI will call
        })

    return items


def find_original_for_ppm(ppm_filename: str) -> str:
    """
    The C binary works entirely with .ppm files, so its output contains
    .ppm filenames. But the UI needs to display the original PNG/JPG files.
    This function maps between them.

    Given a PPM filename like "04_ocean_blue.ppm", it looks in data/images/
    for a file with the same stem and any image extension, and returns its name.
    Falls back to the PPM name itself if no match is found.

    ppm_filename — e.g. "04_ocean_blue.ppm"
    Returns       — e.g. "04_ocean_blue.png"
    """
    stem = Path(ppm_filename).stem  # strip the .ppm extension

    for img_path in IMAGES_DIR.iterdir():
        if img_path.stem == stem:
            return img_path.name    # found a matching original file

    # Nothing matched — return the PPM name as a fallback
    return ppm_filename


def run_c_search(query_ppm: Path, top_k: int = 6, num_threads: int = 4) -> Dict[str, Any]:
    """
    Runs the similarity search by calling the compiled C binary as a subprocess.

    Steps:
      1. Check the binary exists (give a helpful error if not)
      2. Build the command-line arguments
      3. Run the binary and wait for it to finish
      4. Read the JSON file the binary wrote
      5. Enrich each result with the original image filename and a URL
      6. Add a "similarity" score (0..1) that the UI uses for the score bar

    query_ppm   — path to the query image in PPM format
    top_k       — how many results to request from the C binary
    num_threads — how many threads the C binary should use

    Returns the parsed JSON result dict, enriched with UI-friendly fields.
    """
    # Friendly error if someone forgot to run `make`
    if not ALGORITHM_BIN.exists():
        raise FileNotFoundError(
            f"C binary not found at {ALGORITHM_BIN}. "
            f"Run `make` in the algorithm directory first."
        )

    # Build the exact command line we'll pass to the OS.
    # This is equivalent to typing something like:
    #   ./algorithm/bin/imgsearch query.ppm data/ppm data/features.cache 6 4 data/output.json
    cmd = [
        str(ALGORITHM_BIN),     # the binary to run
        str(query_ppm),         # argument 1: query PPM path
        str(PPM_DIR),           # argument 2: dataset directory
        str(CACHE_PATH),        # argument 3: cache file path
        str(top_k),             # argument 4: how many results to return
        str(num_threads),       # argument 5: how many threads to use
        str(OUTPUT_JSON),       # argument 6: where to write the JSON output
    ]

    # subprocess.run runs the command and waits for it to finish.
    # capture_output=True captures stdout/stderr so we can check them.
    # timeout=60 means we give up if it takes longer than 60 seconds.
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

    # A non-zero return code means the C binary reported an error
    if proc.returncode != 0:
        raise RuntimeError(
            f"imgsearch failed (code {proc.returncode}):\n{proc.stderr}"
        )

    # Read and parse the JSON file that the C binary just wrote
    with open(OUTPUT_JSON, "r", encoding="utf-8") as f:
        result = json.load(f)

    # Enrich each result entry with UI-friendly data.
    # The C binary only knows about .ppm filenames; the UI needs the original
    # image names and HTTP URLs it can fetch images from.
    for entry in result.get("results", []):
        # Map the .ppm name back to the original PNG/JPG name
        original_name = find_original_for_ppm(entry["filename"])
        entry["original_filename"] = original_name

        # Build the URL the UI will use to load this image
        entry["url"] = f"/api/image/{original_name}"

        # Convert the raw distance into a 0..1 similarity score for the score bar.
        # We use a soft mapping: distance=0 → similarity=1.0 (identical),
        # distance=2 → similarity=0.0 (very different). Values beyond 2 clamp at 0.
        d = entry.get("distance", 0.0)
        entry["similarity"] = max(0.0, 1.0 - min(1.0, d / 2.0))

    return result
