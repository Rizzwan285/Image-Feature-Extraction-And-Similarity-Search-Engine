# Image Feature Extraction & Similarity Search Engine
### Project Details Document

---

## What This Project Does

You upload an image to a web interface, and the system finds the most visually
similar images from a pre-loaded dataset. It ranks them by similarity and
shows the top matches with a score for each one.

The actual comparison work happens in C. The web interface is built in React.
A Python server sits in between, passing data back and forth. This is the
four-layer architecture required by the course template.

---

## Why It Works The Way It Does

Comparing two images directly (pixel by pixel) is impractical — two photos of
the same sunset taken a second apart would have completely different pixel values
but look identical to a human eye. Instead, we convert each image into a list
of numbers (called a **feature vector**) that captures its overall character:
what colors are in it, how bright it is, and how the brightness is distributed
spatially. Then we compare those numbers, which is both fast and more meaningful
than raw pixel comparison.

---

## System Architecture

The project has four layers as required by the course template. Each layer
has one job and one job only.

```
Browser (React UI)
      │  HTTP requests (fetch API)
      ▼
Python Server (FastAPI)
      │  subprocess call
      ▼
C Binary (imgsearch)
      │  reads/writes
      ▼
Feature Cache File (data/features.cache)
```

### Layer 1 — UI Layer (React + Vite)

**Files:** `ui/src/`

The browser-side single-page app. It handles:
- Drag-and-drop or click-to-browse image uploads
- A modal grid to pick any dataset image as the query
- A loading state that cycles through descriptive phase labels while C runs
- A results grid with animated score bars and ranked match cards
- A stats row showing elapsed time, thread count, dataset size, and cache status
- An architecture overlay (the "view architecture" button) explaining all four layers
- A keyboard shortcut: press `R` to repeat the last search

The UI never runs any image processing itself. Every action either calls the
Python API or displays data the API returned. React's job here is purely to
make the results look good and feel interactive.

Key technologies:
- **Vite** — fast development server and production build tool
- **React 18** — component-based UI with hooks
- **Tailwind CSS** — utility-class styling, dark glassmorphism theme
- **Framer Motion** — animations (entrance, exit, layout, score bar growth)
- **lucide-react** — icon set

---

### Layer 2 — Communication Layer (Python + FastAPI)

**Files:** `communication/server.py`, `communication/bridge.py`

The Python server is what lets React and C talk to each other. It:
- Accepts image uploads over HTTP and saves them to `data/images/`
- Converts uploaded PNG/JPG files to PPM format using Pillow (because C reads PPM)
- Calls the compiled C binary as a subprocess via `subprocess.run()`
- Reads the JSON file the C binary writes and enriches it with image URLs
- Serves the original images back to the browser so they appear in the UI

The five API endpoints:

| Method | Path | What it does |
|--------|------|--------------|
| GET | `/api/health` | Check the server and binary are alive |
| GET | `/api/dataset` | List all images in `data/images/` |
| GET | `/api/image/{filename}` | Serve an image file to the browser |
| POST | `/api/upload` | Accept and save an uploaded image |
| POST | `/api/search` | Run a similarity search, return ranked results |

Why Python in the middle rather than calling C from JavaScript? Because running
a compiled native binary from a browser is not possible. Python can do it easily
with `subprocess`. Python also handles PPM conversion with Pillow, which is
much simpler than writing a PNG decoder in C.

---

### Layer 3 — Algorithm Layer (C)

**Files:** `algorithm/src/`, `algorithm/include/`

This is the core of the project. Everything computationally interesting happens
here. The compiled binary is called `imgsearch` and is invoked from the command
line like this:

```
./algorithm/bin/imgsearch <query.ppm> <dataset_dir> <cache_path> <top_k> <num_threads> <output.json> [metric]
```

The optional `[metric]` argument is one of `euclidean` (default), `manhattan`,
or `cosine`. Omitted or unknown values fall back to Euclidean.

**How the algorithm works:**

1. **Load the query image** — read the .ppm file into memory using `load_ppm()`.
   PPM (P6 binary) is a trivial format: a small text header followed by raw RGB bytes.
   No external library needed.

2. **Extract features for the query** — call `extract_features()` to compute
   a 534-float fingerprint. Three types of features are computed in one pass
   over all pixels:

   | Feature | Size | What it captures |
   |---------|------|-----------------|
   | RGB histogram | 512 floats | Overall color distribution |
   | Per-channel mean | 3 floats | Average tint of the image |
   | Per-channel variance | 3 floats | How much colors vary |
   | 4×4 block-mean grid | 16 floats | Rough spatial layout |

   The histogram uses the top 3 bits of each channel (giving 8 levels × 8 × 8 = 512 bins)
   and is normalized so all values sum to 1.0.

3. **Load dataset features** — check if `features.cache` exists and matches
   the current dataset. If yes, load it (fast path, ~5ms). If not, load every
   .ppm file in the dataset directory, extract its features, and save a fresh
   cache (slow path, ~30ms for 20 images).

4. **Spawn worker threads** — create N `pthread` worker threads that share a
   `SharedSearchState` struct. Each thread:
   - Locks `work_mutex`, grabs the next dataset index, unlocks
   - Computes `euclidean_distance()` between its dataset entry and the query
   - Locks `topk_mutex`, tries to insert into the shared sorted top-K array, unlocks
   - Repeats until no more images

5. **Join threads** — call `pthread_join()` for each thread and wait for all to finish.

6. **Assign ranks and write JSON** — number the results 1, 2, 3, ... and write
   a JSON file that Python reads.

**The distance formula:** the user picks one of three metrics at runtime;
all of them operate on the concatenated 534-float vectors and return 0 when
two images are identical, with higher values meaning more different.

| Metric | Formula | Selected by |
|--------|---------|-------------|
| Euclidean (L2) | `sqrt( sum (a[i] - b[i])^2 )` | `"euclidean"` (default) |
| Manhattan (L1) | `sum( |a[i] - b[i]| )` | `"manhattan"` |
| Cosine         | `1 - (a · b) / (‖a‖·‖b‖)`     | `"cosine"` |

**How runtime metric selection works (function pointers):**

- `similarity.h` declares a typedef:
  ```c
  typedef float (*DistanceFunction)(const FeatureVector *, const FeatureVector *);
  ```
  A `DistanceFunction` is a pointer to any function with that exact
  signature — `euclidean_distance`, `manhattan_distance`, and
  `cosine_distance` all qualify.
- `pick_distance_function(name)` maps a string ("euclidean" / "manhattan"
  / "cosine") to the matching function pointer. NULL or unknown names
  fall back to Euclidean so old callers keep working.
- `run_search()` calls `pick_distance_function()` once, stores the
  resulting pointer in `SharedSearchState.metric_fn`, and worker threads
  invoke the metric through `state->metric_fn(query, candidate)`. The
  threading code never mentions a specific distance function by name —
  swapping in a new metric is a one-function-plus-one-table-entry change.

This is the C-level demonstration of "runtime algorithm selection": the
user's choice flows React → FastAPI → subprocess argv → `pick_distance_function`
→ a function pointer in shared state → the worker's call site, with no
`if`/`switch` inside the hot loop.

**C Concepts Demonstrated:**

| Concept | Where |
|---------|-------|
| Structures (`struct`) | `Image`, `FeatureVector`, `SearchResult`, `SearchStats`, `SharedSearchState` |
| Functions | One clearly-named function per task, split across 5 source files |
| Pointers | Struct-by-pointer everywhere, pixel buffer as `unsigned char *`, dynamic arrays |
| Function pointers | `DistanceFunction` typedef in `similarity.h`; `SharedSearchState.metric_fn` lets workers call the user-selected metric without naming it |
| Makefile | `algorithm/Makefile` with object files, dependency rules, `clean` target |
| Modular programming | Five `.c` files, each with a matching `.h`, one concern per file |
| Custom header files | Include guards, public API declarations, private helpers `static` in `.c` |
| C threads | `pthread_create`, `pthread_join`, two `pthread_mutex_t` instances |
| File I/O | PPM reading with `fread`/`fscanf`, binary cache with `fwrite`/`fread`, JSON output with `fprintf` |
| Dynamic memory | `malloc` / `realloc` / `calloc` / `free` — verified zero leaks with valgrind |
| Synchronization | `work_mutex` guards the job queue, `topk_mutex` guards the results array |

---

### Layer 4 — Backend Layer (Feature Cache File)

**File:** `data/features.cache`

The simplest possible backend: a binary file. The C program writes feature
vectors to it after the first search and reads from it on subsequent searches.

**File format:**
```
[int32_t count] [FeatureVector 0 (raw bytes)] [FeatureVector 1] ... [FeatureVector n-1]
```

This is just `fwrite()` of the struct directly to disk. Reading it back is
`fread()` of the same struct. No parsing, no libraries.

**Why a file and not a database?** The proposal specified a database is optional
if a file is sufficient. Here it clearly is — we only need to cache one array
of structs. A database would be unnecessary complexity with no benefit.

**Cache invalidation:** Python deletes `features.cache` whenever the dataset
changes (upload or server restart with new images). The C binary then rebuilds
it fresh on the next search.

---

## File Structure

```
image-similarity-search/
│
├── Makefile                    top-level: make / make install / make run
├── README.md                   setup and run instructions
│
├── algorithm/                  C code — the algorithmic core
│   ├── Makefile                compiles all src/*.c into bin/imgsearch
│   ├── README.md               explains the C concepts for the presentation
│   ├── include/
│   │   ├── image_io.h          Image struct, load_ppm, free_image
│   │   ├── img_features.h      FeatureVector struct, extract_features, cache API
│   │   ├── similarity.h        euclidean_distance declaration
│   │   └── search.h            SearchResult, SearchStats, run_search
│   └── src/
│       ├── image_io.c          PPM file reader, heap-allocates pixel buffer
│       ├── img_features.c      feature extraction + binary cache save/load
│       ├── similarity.c        L2 distance over 534-float vectors
│       ├── search.c            threading core — work queue + mutex-protected top-K
│       └── main.c              CLI entry, writes output.json
│
├── communication/              Python bridge between UI and C
│   ├── server.py               FastAPI HTTP server, 5 endpoints
│   ├── bridge.py               path resolution, Pillow conversion, subprocess call
│   ├── requirements.txt        fastapi, uvicorn, pillow, python-multipart
│   └── README.md               API endpoint reference
│
├── ui/                         React frontend
│   ├── index.html              HTML shell, loads Google Fonts
│   ├── vite.config.js          dev server config + /api proxy to port 8000
│   ├── tailwind.config.js      custom colors, fonts, animations
│   ├── src/
│   │   ├── main.jsx            React entry point
│   │   ├── App.jsx             root component, owns all state
│   │   ├── index.css           Tailwind layers + custom CSS (aurora bg, glass util)
│   │   └── components/
│   │       ├── Hero.jsx                title, tagline, tech-stack pill, arch button
│   │       ├── QueryPanel.jsx          upload zone + query-selected card
│   │       ├── DatasetModal.jsx        full-screen image picker grid
│   │       ├── LoadingState.jsx        animated spinner + cycling phase labels
│   │       ├── ResultCard.jsx          one result with rank, score bar, metadata
│   │       ├── ResultsGrid.jsx         stats row + responsive grid of ResultCards
│   │       └── ArchitectureOverlay.jsx four-layer diagram + request flow
│   └── README.md               component tour, keyboard shortcut
│
├── scripts/
│   ├── generate_sample_images.py   procedurally generates 20 CC0 PNG images
│   └── convert_to_ppm.py           converts a folder of images to PPM
│
└── data/
    ├── images/                 20 sample PNG images (committed to repo)
    │   └── SOURCES.md          confirms all images are CC0/procedurally generated
    ├── ppm/                    PPM versions (gitignored, rebuilt on startup)
    └── features.cache          binary feature cache (gitignored, rebuilt on search)
```

---

## How to Run It

```bash
git clone <repo-url>
cd image-similarity-search

make install       # installs Python and Node dependencies
make               # compiles the C binary into algorithm/bin/imgsearch
make sample-data   # generates the 20 sample images and converts them to PPM
make run           # starts FastAPI on :8000 and Vite on :5173
```

Then open **http://localhost:5173** in your browser.

---

## Running the C Binary Directly

You can skip Python and React entirely and talk to the C binary on the command line:

```bash
# First convert an image to PPM (Python one-liner)
python3 -c "from PIL import Image; Image.open('myimage.png').convert('RGB').save('query.ppm')"

# Then run the search
./algorithm/bin/imgsearch query.ppm data/ppm data/features.cache 5 4 output.json

# Read the result
cat output.json
```

Output looks like:
```
Search complete. dataset=20 threads=4 cache=MISS elapsed=18.46ms
  #1  01_sunset_warm.ppm                        distance=0.000000
  #2  20_gradient_dusk.ppm                      distance=0.850290
  #3  03_dawn_soft.ppm                          distance=0.882881
  ...
Wrote results to output.json
```

---

## Verified Test Results

These numbers were measured during development on the actual codebase:

| Test | Result |
|------|--------|
| Clean build (`make clean && make`) | 0 warnings, 0 errors |
| valgrind leak check | 98 allocs, 98 frees, 0 leaks, 0 errors |
| 1-thread search (cold, 20 images) | ~32 ms |
| 4-thread search (cold, 20 images) | ~20 ms |
| 4-thread search (warm/cached) | ~5 ms |
| UI production build | ✓ clean in 5.75s |
| End-to-end HTTP round-trip | ✓ tested with curl |

---

## Presentation Cheat Sheet

**Setup segment (TM1):**
```bash
git clone <repo>
cd image-similarity-search
make install && make
```

**Quick demo script:**
1. Open http://localhost:5173
2. Click "view architecture" — walk through the overlay
3. Click "Pick from dataset" — show the modal grid
4. Click an ocean image to select it as query
5. Click "Search Similar Images" — watch the loading phases
6. Point at the stats bar: elapsed time, thread count, cache miss → hit on repeat
7. Press `R` — instant repeat, cache HIT, much faster

**Things to mention during C code explanation:**
- `image_io.h` and `image_io.c` — the `Image` struct, `load_ppm` reading the PPM header
- `img_features.h` and `img_features.c` — the `FeatureVector` struct, the pixel loop
- `search.h` and `search.c` — `SharedSearchState`, `pthread_create`, `pthread_mutex_t`,
  the work queue pattern (one index, two threads grab work atomically)
- `algorithm/Makefile` — the compilation rules, `-pthread` flag

**Why `img_features.h` not `features.h`?** — glibc has its own internal `<features.h>`
that `<stdio.h>` pulls in. Naming ours the same way would shadow it and cause cryptic
compile errors. Renaming with a prefix is the correct engineering fix, and is
worth mentioning as a real thing you debugged.
