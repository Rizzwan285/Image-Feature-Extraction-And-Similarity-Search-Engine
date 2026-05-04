# Communication Layer (Python)

A small FastAPI server that bridges the React UI and the C binary.

It does three things:

1. Converts uploaded images (PNG/JPG/etc) to PPM with Pillow, since the C
   binary only reads PPM (P6).
2. Calls `algorithm/bin/imgsearch` via `subprocess` and parses its JSON
   output.
3. Serves the original images back to the UI so they can be displayed in
   the browser.

## Files

- `server.py` — FastAPI app and HTTP endpoints
- `bridge.py` — subprocess wrapper, Pillow conversion, path resolution
- `requirements.txt` — Python dependencies

## Endpoints

| Method | Path                    | What it does                                |
| ------ | ----------------------- | ------------------------------------------- |
| GET    | `/api/health`           | Health check + binary status + dataset size |
| GET    | `/api/dataset`          | List every image in `data/images/`          |
| GET    | `/api/image/{filename}` | Serve an original image file                |
| POST   | `/api/upload`           | Upload an image, save it, convert to PPM    |
| POST   | `/api/search`           | Run a similarity search                     |

## Run it standalone

```
cd communication
python3 -m uvicorn server:app --host 0.0.0.0 --port 8000
```

Then hit `http://localhost:8000/api/health` to verify it's up. The OpenAPI
docs are at `http://localhost:8000/docs`.

## Path handling

`bridge.py` resolves all paths relative to the project root using
`Path(__file__).resolve().parent.parent`, so the C binary can be invoked
correctly regardless of which directory uvicorn is started from.
