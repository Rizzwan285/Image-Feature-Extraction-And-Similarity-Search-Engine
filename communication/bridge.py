"""bridging python server and c binary"""

import json
import os
import subprocess
from pathlib import Path
from typing import Any, Dict
from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parent.parent
ALGORITHM_BIN = PROJECT_ROOT / "algorithm" / "bin" / "imgsearch"
DATA_DIR = PROJECT_ROOT / "data"
IMAGES_DIR = DATA_DIR / "images"
PPM_DIR = DATA_DIR / "ppm"
CACHE_PATH = DATA_DIR / "features.cache"
OUTPUT_JSON = DATA_DIR / "output.json"


def ensure_directories() -> None:
    """ensuring data directories exist"""
    IMAGES_DIR.mkdir(parents=True, exist_ok=True)
    PPM_DIR.mkdir(parents=True, exist_ok=True)


def convert_to_ppm(source_path: Path, dest_path: Path) -> None:
    """converting image to ppm format"""
    img = Image.open(source_path).convert("RGB")
    img.save(dest_path, format="PPM")


def sync_dataset_to_ppm() -> int:
    """synchronizing dataset to ppm format"""
    ensure_directories()
    converted = 0

    for img_path in IMAGES_DIR.iterdir():
        if not img_path.is_file():
            continue

        ext = img_path.suffix.lower()
        if ext not in (".png", ".jpg", ".jpeg", ".bmp", ".webp", ".gif"):
            continue

        ppm_path = PPM_DIR / (img_path.stem + ".ppm")

        if not ppm_path.exists() or ppm_path.stat().st_mtime < img_path.stat().st_mtime:
            convert_to_ppm(img_path, ppm_path)
            converted += 1

    return converted


def invalidate_cache() -> None:
    """invalidating feature cache"""
    if CACHE_PATH.exists():
        CACHE_PATH.unlink()


def list_dataset() -> list:
    """listing dataset images"""
    ensure_directories()
    items = []

    for img_path in sorted(IMAGES_DIR.iterdir()):
        if not img_path.is_file():
            continue
        if img_path.suffix.lower() not in (".png", ".jpg", ".jpeg", ".bmp", ".webp"):
            continue
        items.append({
            "filename": img_path.name,
            "stem": img_path.stem,
            "url": f"/api/image/{img_path.name}",
        })

    return items


def find_original_for_ppm(ppm_filename: str) -> str:
    """finding original file for ppm file"""
    stem = Path(ppm_filename).stem

    for img_path in IMAGES_DIR.iterdir():
        if img_path.stem == stem:
            return img_path.name

    return ppm_filename


ALLOWED_METRICS = ("euclidean", "manhattan", "cosine")


def category_for(filename: str) -> str:
    """deriving category label from filename"""
    stem = Path(filename).stem
    for token in stem.split("_"):
        if token and not token.isdigit():
            return token.lower()
    return "uncategorized"


def evaluate_recall_at_k(top_k: int = 5, num_threads: int = 4) -> Dict[str, Any]:
    """evaluating recall@k across dataset"""
    images = list_dataset()
    if not images:
        return {"k": top_k, "num_queries": 0, "categories": {}, "metrics": {}}

    category_counts: Dict[str, int] = {}
    for img in images:
        c = category_for(img["filename"])
        category_counts[c] = category_counts.get(c, 0) + 1

    request_k = top_k + 1

    metrics_report: Dict[str, Dict[str, float]] = {}

    for metric_name in ALLOWED_METRICS:
        hits = 0
        total = 0
        elapsed_total = 0.0

        for img in images:
            query_category = category_for(img["filename"])
            stem = Path(img["filename"]).stem
            query_ppm = PPM_DIR / (stem + ".ppm")

            if not query_ppm.exists():
                src = IMAGES_DIR / img["filename"]
                if src.exists():
                    convert_to_ppm(src, query_ppm)
                else:
                    continue

            search_result = run_c_search(
                query_ppm=query_ppm,
                top_k=request_k,
                num_threads=num_threads,
                metric=metric_name,
            )

            elapsed_total += search_result.get("stats", {}).get("elapsed_ms", 0.0)

            results = search_result.get("results", [])
            neighbors = [r for r in results if r.get("filename") != stem + ".ppm"]
            neighbors = neighbors[:top_k]

            for r in neighbors:
                if category_for(r.get("filename", "")) == query_category:
                    hits += 1
                total += 1

        recall = (hits / total) if total > 0 else 0.0
        mean_elapsed = (elapsed_total / len(images)) if images else 0.0

        metrics_report[metric_name] = {
            "recall_at_k": round(recall, 4),
            "mean_elapsed_ms": round(mean_elapsed, 3),
        }

    return {
        "k": top_k,
        "num_queries": len(images),
        "categories": category_counts,
        "metrics": metrics_report,
    }


def compare_metrics(
    query_filename: str,
    top_k: int = 6,
    num_threads: int = 4,
) -> Dict[str, Any]:
    """comparing distance metrics for query"""
    stem = Path(query_filename).stem
    query_ppm = PPM_DIR / (stem + ".ppm")

    if not query_ppm.exists():
        src = IMAGES_DIR / query_filename
        if src.exists():
            convert_to_ppm(src, query_ppm)
        else:
            raise FileNotFoundError(f"query PPM not found for {query_filename}")

    per_metric: Dict[str, Any] = {}
    for metric_name in ALLOWED_METRICS:
        per_metric[metric_name] = run_c_search(
            query_ppm=query_ppm,
            top_k=top_k,
            num_threads=num_threads,
            metric=metric_name,
        )

    original_name = find_original_for_ppm(stem + ".ppm")
    return {
        "query_info": {
            "filename": original_name,
            "url": f"/api/image/{original_name}",
        },
        "metrics": per_metric,
    }


def run_c_search(
    query_ppm: Path,
    top_k: int = 6,
    num_threads: int = 4,
    metric: str = "euclidean",
) -> Dict[str, Any]:
    """running similarity search via c binary"""
    if not ALGORITHM_BIN.exists():
        raise FileNotFoundError(
            f"C binary not found at {ALGORITHM_BIN}. "
            f"Run `make` in the algorithm directory first."
        )

    metric_name = (metric or "euclidean").strip().lower()
    if metric_name not in ALLOWED_METRICS:
        metric_name = "euclidean"
    cmd = [
        str(ALGORITHM_BIN),
        str(query_ppm),
        str(PPM_DIR),
        str(CACHE_PATH),
        str(top_k),
        str(num_threads),
        str(OUTPUT_JSON),
        metric_name,
    ]

    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

    if proc.returncode != 0:
        raise RuntimeError(
            f"imgsearch failed (code {proc.returncode}):\n{proc.stderr}"
        )

    with open(OUTPUT_JSON, "r", encoding="utf-8") as f:
        result = json.load(f)

    for entry in result.get("results", []):
        original_name = find_original_for_ppm(entry["filename"])
        entry["original_filename"] = original_name

        entry["url"] = f"/api/image/{original_name}"

        d = entry.get("distance", 0.0)
        entry["similarity"] = max(0.0, 1.0 - min(1.0, d / 2.0))

    return result
