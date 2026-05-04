"""
server.py
---------
The FastAPI HTTP server that sits between the React UI and the C binary.

React can't call the C binary directly (it runs in a browser, C is native code).
So the UI makes HTTP requests to this server, and this server does the actual
work: saving uploaded files, converting images, calling bridge.py which calls C,
and sending results back as JSON.

Endpoints:
  GET  /api/health             -- quick check that the server and C binary are alive
  GET  /api/dataset            -- list all images in the dataset (UI uses this on load)
  GET  /api/image/{filename}   -- serve an image file so the UI can display it
  POST /api/upload             -- accept an uploaded image, save it, convert to PPM
  POST /api/search             -- run a similarity search and return ranked results
"""

# shutil has copyfileobj which efficiently copies an uploaded file to disk
import shutil

# Path for file path operations
from pathlib import Path

# FastAPI building blocks:
#   FastAPI      — the web framework itself
#   File         — marks a parameter as a file upload field
#   HTTPException — returns an HTTP error response with a status code
#   UploadFile   — the type of an uploaded file in FastAPI
from fastapi import FastAPI, File, HTTPException, UploadFile

# Middleware for Cross-Origin Resource Sharing — lets the Vite dev server
# (which runs on a different port) make requests to this API
from fastapi.middleware.cors import CORSMiddleware

# Response types:
#   FileResponse — streams a file back as the HTTP response body
#   JSONResponse — returns a JSON object as the response body
from fastapi.responses import FileResponse, JSONResponse

# BaseModel is the base class for Pydantic models.
# FastAPI uses these to validate and parse JSON request bodies automatically.
from pydantic import BaseModel

# Our own bridge module handles the actual file-wrangling and C invocation
import bridge


# Create the FastAPI application instance
app = FastAPI(title="Image Similarity Search", version="1.0.0")

# Add CORS middleware so the React UI (running on localhost:5173 during development)
# can make requests to this API (running on localhost:8000).
# Without this, the browser would block all cross-origin requests.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],        # allow requests from any origin
    allow_credentials=False,    # we don't use cookies or auth headers
    allow_methods=["*"],        # allow GET, POST, etc.
    allow_headers=["*"],        # allow any request headers
)


# Pydantic model for the POST /api/search request body.
# FastAPI automatically validates incoming JSON against this class —
# if query_filename is missing or top_k isn't an integer, it returns a 422 error.
class SearchRequest(BaseModel):
    query_filename: str     # the image to use as the query
    top_k: int = 6          # how many results to return (default 6)
    num_threads: int = 4    # how many C threads to use (default 4)


@app.on_event("startup")
def on_startup() -> None:
    """
    Runs once when the server starts up, before it begins accepting requests.

    We use this to make sure all the data directories exist and that every
    image in data/images/ has a matching PPM in data/ppm/. If there are new
    images that haven't been converted yet, we do it now. We also invalidate
    the feature cache if anything changed, so the first search doesn't use
    stale data.
    """
    bridge.ensure_directories()     # create data/images/ and data/ppm/ if missing
    converted = bridge.sync_dataset_to_ppm()    # convert any new images to PPM

    if converted > 0:
        # Something changed in the dataset — the cached feature vectors are now
        # outdated, so we delete the cache. The C binary will rebuild it on the
        # next search run.
        bridge.invalidate_cache()
        print(f"[startup] converted {converted} new image(s) to PPM, cache invalidated")
    else:
        print("[startup] dataset PPMs already up to date")


@app.get("/api/health")
def health() -> dict:
    """
    Simple health check endpoint.
    The UI doesn't use this directly, but it's handy for debugging:
    curl http://localhost:8000/api/health
    """
    return {
        "status": "ok",
        "binary_exists": bridge.ALGORITHM_BIN.exists(),    # did `make` run?
        "dataset_count": len(bridge.list_dataset()),        # how many images do we have?
    }


@app.get("/api/dataset")
def get_dataset() -> dict:
    """
    Returns metadata for every image in data/images/.
    The React UI calls this on startup to populate the dataset grid
    and the "pick from dataset" modal.
    """
    return {"images": bridge.list_dataset()}


@app.get("/api/image/{filename}")
def get_image(filename: str):
    """
    Serves an image file from data/images/ back to the browser.
    The UI uses these URLs in <img> tags to display thumbnails and results.

    We validate the filename to prevent path traversal attacks
    (e.g. someone requesting filename="../../etc/passwd").
    """
    # Reject any filename that looks like it's trying to escape the images directory
    if "/" in filename or "\\" in filename or ".." in filename:
        raise HTTPException(status_code=400, detail="invalid filename")

    path = bridge.IMAGES_DIR / filename

    # Make sure the file actually exists
    if not path.exists() or not path.is_file():
        raise HTTPException(status_code=404, detail="image not found")

    # FileResponse streams the file with the correct Content-Type header
    return FileResponse(path)


@app.post("/api/upload")
async def upload_image(file: UploadFile = File(...)) -> dict:
    """
    Accepts an image uploaded from the browser, saves it to data/images/,
    and immediately converts it to PPM so it's ready for searching.

    We also invalidate the feature cache because the dataset just changed —
    the C binary needs to re-extract features to include the new image.

    The function is async because file uploads involve I/O waiting.

    Returns the saved filename and the URL the UI can use to display it.
    """
    # Reject uploads with no filename
    if not file.filename:
        raise HTTPException(status_code=400, detail="missing filename")

    # Path(file.filename).name strips any directory components from the filename.
    # Without this, a crafty client could upload with filename="../../evil.sh".
    safe_name = Path(file.filename).name
    target = bridge.IMAGES_DIR / safe_name

    # Write the uploaded bytes to disk.
    # copyfileobj reads from the upload stream and writes to our file in chunks,
    # which is memory-efficient for large files.
    with target.open("wb") as f:
        shutil.copyfileobj(file.file, f)

    # Convert the just-saved image to PPM right now so the next search doesn't
    # have to wait for this conversion step
    ppm_path = bridge.PPM_DIR / (target.stem + ".ppm")
    try:
        bridge.convert_to_ppm(target, ppm_path)
    except Exception as e:
        # If Pillow couldn't read it, it's probably not a valid image.
        # Delete the file we just saved and tell the client what went wrong.
        target.unlink(missing_ok=True)  # missing_ok=True won't error if already gone
        raise HTTPException(status_code=400, detail=f"could not read image: {e}")

    # Dataset changed — force the C binary to re-extract features on next search
    bridge.invalidate_cache()

    return {
        "filename": safe_name,
        "url": f"/api/image/{safe_name}",   # the URL the UI can use to display it
    }


@app.post("/api/search")
def search(req: SearchRequest) -> JSONResponse:
    """
    Runs a similarity search for the given query image.

    FastAPI automatically parses the JSON body and validates it against
    the SearchRequest model. If anything is wrong (missing fields, wrong types),
    it returns a 422 error before this function even runs.

    Steps:
      1. Validate the filename (path traversal check)
      2. Make sure the PPM version of the query exists (convert if needed)
      3. Call bridge.run_c_search() which invokes the C binary
      4. Attach a query_info block so the UI knows what was searched
      5. Return the JSON result

    Returns the full result as JSON including results array and stats.
    """
    # Path traversal check — same reason as in get_image above
    if "/" in req.query_filename or "\\" in req.query_filename or ".." in req.query_filename:
        raise HTTPException(status_code=400, detail="invalid filename")

    # Derive the PPM filename from the original image name
    # e.g. "04_ocean_blue.png" → data/ppm/04_ocean_blue.ppm
    stem = Path(req.query_filename).stem
    query_ppm = bridge.PPM_DIR / (stem + ".ppm")

    if not query_ppm.exists():
        # PPM doesn't exist — maybe the user uploaded an image but conversion
        # was skipped for some reason. Try converting it now.
        original = bridge.IMAGES_DIR / req.query_filename
        if original.exists():
            bridge.convert_to_ppm(original, query_ppm)
        else:
            raise HTTPException(status_code=404, detail="query image not found")

    try:
        # Call the C binary via subprocess and get the parsed + enriched result
        result = bridge.run_c_search(
            query_ppm=query_ppm,
            top_k=max(1, min(req.top_k, 20)),           # clamp to 1..20
            num_threads=max(1, min(req.num_threads, 16)),  # clamp to 1..16
        )
    except Exception as e:
        # Something went wrong running the C binary
        raise HTTPException(status_code=500, detail=str(e))

    # Add a query_info block so the UI can display the query image alongside results.
    # The C binary only records the PPM path; we map it back to the original name here.
    original_name = bridge.find_original_for_ppm(stem + ".ppm")
    result["query_info"] = {
        "filename": original_name,
        "url": f"/api/image/{original_name}",   # URL the UI can use to display it
    }

    # JSONResponse lets us return the dict as an HTTP 200 JSON response
    return JSONResponse(result)


# Allow running the server directly with `python3 server.py` during development
# (though `make run` uses uvicorn directly, which is preferred)
if __name__ == "__main__":
    import uvicorn
    uvicorn.run("server:app", host="0.0.0.0", port=8000, reload=False)
