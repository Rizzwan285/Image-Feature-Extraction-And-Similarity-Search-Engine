"""fastapi server for image similarity search"""

import shutil
from pathlib import Path
from fastapi import FastAPI, File, HTTPException, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse
from pydantic import BaseModel

import bridge

app = FastAPI(title="Image Similarity Search", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)


class SearchRequest(BaseModel):
    query_filename: str
    top_k: int = 6
    num_threads: int = 4
    metric: str = "euclidean"


class CompareRequest(BaseModel):
    """running compare metrics request"""
    query_filename: str
    top_k: int = 6
    num_threads: int = 4


class EvaluateRequest(BaseModel):
    """running evaluation request"""
    top_k: int = 5
    num_threads: int = 4


@app.on_event("startup")
def on_startup() -> None:
    """running startup tasks"""
    bridge.ensure_directories()
    converted = bridge.sync_dataset_to_ppm()

    if converted > 0:
        bridge.invalidate_cache()
        print(f"[startup] converted {converted} new image(s) to PPM, cache invalidated")
    else:
        print("[startup] dataset PPMs already up to date")


@app.get("/api/health")
def health() -> dict:
    """checking server health"""
    return {
        "status": "ok",
        "binary_exists": bridge.ALGORITHM_BIN.exists(),
        "dataset_count": len(bridge.list_dataset()),
    }


@app.get("/api/dataset")
def get_dataset() -> dict:
    """getting dataset images"""
    return {"images": bridge.list_dataset()}


@app.get("/api/image/{filename}")
def get_image(filename: str):
    """serving image file"""
    if "/" in filename or "\\" in filename or ".." in filename:
        raise HTTPException(status_code=400, detail="invalid filename")

    path = bridge.IMAGES_DIR / filename

    if not path.exists() or not path.is_file():
        raise HTTPException(status_code=404, detail="image not found")

    return FileResponse(path)


@app.post("/api/upload")
async def upload_image(file: UploadFile = File(...)) -> dict:
    """uploading and converting image"""
    if not file.filename:
        raise HTTPException(status_code=400, detail="missing filename")

    safe_name = Path(file.filename).name
    target = bridge.IMAGES_DIR / safe_name

    with target.open("wb") as f:
        shutil.copyfileobj(file.file, f)

    ppm_path = bridge.PPM_DIR / (target.stem + ".ppm")
    try:
        bridge.convert_to_ppm(target, ppm_path)
    except Exception as e:
        target.unlink(missing_ok=True)
        raise HTTPException(status_code=400, detail=f"could not read image: {e}")

    bridge.invalidate_cache()

    return {
        "filename": safe_name,
        "url": f"/api/image/{safe_name}",
    }


@app.post("/api/search")
def search(req: SearchRequest) -> JSONResponse:
    """running similarity search"""
    if "/" in req.query_filename or "\\" in req.query_filename or ".." in req.query_filename:
        raise HTTPException(status_code=400, detail="invalid filename")

    stem = Path(req.query_filename).stem
    query_ppm = bridge.PPM_DIR / (stem + ".ppm")

    if not query_ppm.exists():
        original = bridge.IMAGES_DIR / req.query_filename
        if original.exists():
            bridge.convert_to_ppm(original, query_ppm)
        else:
            raise HTTPException(status_code=404, detail="query image not found")

    try:
        result = bridge.run_c_search(
            query_ppm=query_ppm,
            top_k=max(1, min(req.top_k, 20)),
            num_threads=max(1, min(req.num_threads, 16)),
            metric=req.metric,
        )
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

    original_name = bridge.find_original_for_ppm(stem + ".ppm")
    result["query_info"] = {
        "filename": original_name,
        "url": f"/api/image/{original_name}",
    }

    return JSONResponse(result)


@app.post("/api/compare")
def compare(req: CompareRequest) -> JSONResponse:
    """comparing metrics"""
    if "/" in req.query_filename or "\\" in req.query_filename or ".." in req.query_filename:
        raise HTTPException(status_code=400, detail="invalid filename")

    try:
        result = bridge.compare_metrics(
            query_filename=req.query_filename,
            top_k=max(1, min(req.top_k, 20)),
            num_threads=max(1, min(req.num_threads, 16)),
        )
    except FileNotFoundError as e:
        raise HTTPException(status_code=404, detail=str(e))
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

    return JSONResponse(result)


@app.post("/api/evaluate")
def evaluate(req: EvaluateRequest) -> JSONResponse:
    """evaluating metrics recall"""
    try:
        result = bridge.evaluate_recall_at_k(
            top_k=max(1, min(req.top_k, 20)),
            num_threads=max(1, min(req.num_threads, 16)),
        )
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

    return JSONResponse(result)


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("server:app", host="0.0.0.0", port=8000, reload=False)
