# Image Similarity Search Engine

A small image similarity search system. You upload a query image and it finds
the most visually similar images from a dataset. The actual feature extraction
and ranking are done in C, with parallel threads doing the comparison work.

This is a C Programming course project. The point of it is to demonstrate
clean modular C — structs, pointers, multiple files, custom headers, threads
and a mutex — wrapped in a real-looking app so you can see it work end-to-end.

## What you need installed

- `gcc` (any recent version)
- `make`
- Python 3.10 or newer + `pip`
- Node.js 18 or newer + `npm`

That's it. No databases, no Docker, no external image libraries.

## Run it after cloning

```
git clone <repo-url>
cd image-similarity-search

make install      # installs python and node packages
make              # compiles the C binary into algorithm/bin/imgsearch
make sample-data  # generates the demo dataset (~20 small images)
make run          # starts the FastAPI server and the Vite UI together
```

Then open **http://localhost:5173** in your browser. The backend runs at
`http://localhost:8000` and the UI proxies `/api/*` calls to it.

If you already have your own images in `data/images/`, you can skip
`make sample-data`.

### Faster restart

Once you've run `make sample-data` once, you don't need to run it again. To
restart the app in a new terminal session, just do:

```
make run
```

## How it works

The project follows a four-layer structure as required by the course template.

**UI layer** (`ui/`) is a React app built with Vite. It handles uploads,
shows the dataset grid, and renders the ranked results with animated score
bars. It does not run any of the actual algorithm.

**Communication layer** (`communication/`) is a small FastAPI server. When
you upload an image it converts it to PPM with Pillow, then calls the C
binary as a subprocess and reads the JSON it writes. It also serves the
original images back to the UI.

**Algorithm layer** (`algorithm/`) is pure C. It reads PPM images, extracts
a 534-float feature vector for each one (an RGB histogram plus per-channel
mean and variance plus a 4×4 grid of intensity means), and ranks the dataset
against the query using a configurable distance metric (Euclidean, Manhattan,
or Cosine). Multiple worker threads do the extraction and comparison in
parallel, and a mutex protects the shared top-K results array.

The metric is selected at runtime via a **C function pointer**. `similarity.h`
declares a `DistanceFunction` typedef — a pointer to any function that takes
two `FeatureVector *` and returns a `float`. `pick_distance_function()` maps
the user's choice ("euclidean" / "manhattan" / "cosine") to the matching
function, and the search core stores that pointer in its shared thread state.
Worker threads call the metric through the pointer, so the threading code
never names a specific distance function and a new metric can be plugged in
by adding one function and one entry in the lookup table.

**Backend layer** is just a binary cache file at `data/features.cache`. The
C program writes feature vectors to it after the first run and reads from it
on later runs, which is why the second search you do is much faster than the
first.

## Project structure

```
image-similarity-search/
├── algorithm/          # the C code
│   ├── include/        # custom header files
│   ├── src/            # .c source files (one module per concern)
│   ├── Makefile
│   └── bin/            # compiled binary lives here (gitignored)
├── communication/      # python bridge
│   ├── server.py       # FastAPI endpoints
│   ├── bridge.py       # subprocess wrapper + Pillow conversion
│   └── requirements.txt
├── ui/                 # react + vite frontend
│   └── src/components/ # Hero, QueryPanel, ResultsGrid, etc.
├── scripts/            # helpers for generating and converting sample data
├── data/
│   ├── images/         # original images (PNG/JPG)
│   ├── ppm/            # PPM versions for the C binary (gitignored)
│   └── features.cache  # binary feature cache (gitignored)
└── Makefile            # top-level: install, build, run
```

## Using the C binary on its own

You can run the C program directly without the Python or React layers — useful
for testing or for showing the professor what the algorithm produces on its own.

```
./algorithm/bin/imgsearch <query.ppm> <dataset_dir> <cache_path> <top_k> <num_threads> <output.json> [metric]
```

The seventh argument (`metric`) is optional and selects which distance
function pointer the search uses:

| Value | Function called |
|-------|-----------------|
| `euclidean` (default) | `euclidean_distance` |
| `manhattan` | `manhattan_distance` |
| `cosine` | `cosine_distance` |

If the argument is omitted (six-argument invocation) or unrecognized, the
binary falls back to Euclidean — old call-sites keep working unchanged.

For example:

```
./algorithm/bin/imgsearch data/ppm/04_ocean_blue.ppm data/ppm data/features.cache 5 4 data/output.json cosine
```

It prints a summary to the terminal (including which metric ran) and writes
the full result as JSON.

## Common problems

**`make` fails with "gcc not found"** — install build-essential:
`sudo apt install build-essential` on Ubuntu, or Xcode command line tools on
macOS.

**The UI shows a "Could not reach the backend" error** — the FastAPI server
isn't running on port 8000. `make run` starts both the API and the UI together;
if you're running them separately, start the API first with `make run-api`.

**No images appear in the dataset modal** — you haven't run `make sample-data`
yet, or your `data/images/` folder is empty. Drop some PNGs in there and
restart the API (it converts them to PPM on startup).

**Search returns the wrong matches** — the feature cache might be stale if you
swapped images around. Run `make clean-cache` and try again.

**Port 5173 or 8000 is already in use** — kill whatever is using it, or change
the port in `vite.config.js` and the relevant `make run-*` target.

## Notes for the presentation

When the professor runs `make` on a fresh checkout, they'll get the C binary
built. They can then run `make sample-data` and `make run` to see the full
demo. The sample images shipped in `data/images/` are procedurally generated
inside `scripts/generate_sample_images.py`, so they're CC0 / fully owned and
safe to commit and ship.

The `R` keyboard shortcut re-runs the last search without clicking the button —
handy when demoing different threading parameters or showing the cache hit
speedup.

The "view architecture" pill in the hero opens an overlay that lays out all
four layers, the technologies in each, and the request flow. Useful during the
architecture-explanation segment.
