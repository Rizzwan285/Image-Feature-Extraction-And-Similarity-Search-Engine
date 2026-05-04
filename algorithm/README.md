# Algorithm Layer (C)

This is the C heart of the project. Everything below maps to a topic from the
C programming course, and is laid out so it can be explained in the
6-minute "explain the C code" segment of the presentation.

## Build it

```
make             # produces bin/imgsearch
make clean       # removes bin/
```

The Makefile uses `gcc` with `-Wall -Wextra -O2 -std=c11 -pthread`. It
compiles each `src/*.c` to a `bin/*.o` object file and links them all into
`bin/imgsearch`.

## File layout

```
algorithm/
├── Makefile
├── include/
│   ├── image_io.h       defines the Image struct, load_ppm, free_image
│   ├── img_features.h   defines the FeatureVector struct + cache API
│   ├── similarity.h     declares euclidean_distance
│   └── search.h         declares run_search and SearchResult / SearchStats
└── src/
    ├── image_io.c       PPM (P6 binary) reader
    ├── img_features.c   feature extraction + cache save/load
    ├── similarity.c     L2 distance over feature vectors
    ├── search.c         threaded search core (this is the interesting one)
    └── main.c           CLI entry point, writes results as JSON
```

> **Note**: the header is named `img_features.h`, not `features.h`. There's a
> reason — glibc has its own internal `<features.h>` header that gets pulled
> in by `<stdio.h>`, and a project-local `features.h` shadows it and breaks
> the build with confusing macro errors. The prefix avoids the collision.

## Course concepts demonstrated

**Structures** — three real ones, each used as a record type:
- `Image` (in `image_io.h`) holds width, height, a heap-allocated pixel
  buffer, and the filename.
- `FeatureVector` (in `img_features.h`) is the per-image fingerprint:
  a 512-entry RGB histogram, three channel means, three variances, and a
  4×4 grid of intensity means.
- `SearchResult` (in `search.h`) is what the search returns per match:
  filename, distance, rank.

**Functions** split across multiple translation units, each with a clear
single responsibility — load, extract, compare, search, output.

**Pointers** everywhere: structs are passed by pointer (`const Image *img`),
the pixel buffer is a `unsigned char *`, the dataset feature array is a
`FeatureVector *` we walk by index, and the top-K array is shared between
threads via a pointer in the shared state struct.

**Modular programming + custom header files** — every `.c` file has a
matching `.h` with include guards (`#ifndef IMAGE_IO_H` ... `#endif`).
Headers expose just the public API. Internal helpers in the `.c` files are
marked `static`.

**Makefile** — a real one with object files, dependency rules, a `clean`
target, and proper `CFLAGS` / `LDFLAGS`.

**C threads (pthreads)** — see `search.c`. The main thread spawns
`num_threads` workers with `pthread_create`. Each worker pulls the next
dataset index off a shared counter (protected by `work_mutex`), computes the
distance, and tries to insert the result into the shared sorted top-K array
(protected by `topk_mutex`). The main thread joins all workers with
`pthread_join` and then sorts and reports the final ranks.

**File I/O** — reading PPM image files in `image_io.c` (`fread`, `fscanf`,
header parsing with `fgetc` and `ungetc`); reading and writing the binary
feature cache in `img_features.c` (`fread` / `fwrite` of raw structs);
writing the JSON output in `main.c` (`fprintf`).

**Dynamic memory allocation** — `malloc` for the pixel buffer, the dataset
feature vector array, the thread handles array, and the cache buffer; every
allocation has a matching `free`. Verified with valgrind: 98 allocs, 98
frees, zero leaks.

**Synchronization** — two `pthread_mutex_t` instances. `work_mutex` protects
a simple work-stealing counter; `topk_mutex` protects the shared sorted
top-K array. Both are initialized with `pthread_mutex_init` and destroyed
with `pthread_mutex_destroy` after the workers finish.

## What the algorithm actually does

For each image (query and dataset), `extract_features` builds a 534-float
fingerprint:

1. **RGB histogram** — every pixel votes into one of 8×8×8 = 512 bins
   (the top 3 bits of each channel). Then the histogram is normalized so the
   bin values sum to 1.
2. **Per-channel mean and variance** — six numbers describing the average
   color and how much it varies.
3. **Block-mean grid** — the image is divided into a 4×4 grid; each cell
   holds the mean luminance of the pixels that fall inside it. This captures
   rough spatial layout.

To compare two images, `euclidean_distance` treats both fingerprints as one
long vector and returns the L2 distance. Lower = more similar. The query
image always has distance 0 against itself (it acts as a sanity check).

## Running it standalone

```
./bin/imgsearch <query.ppm> <dataset_dir> <cache_path> <top_k> <num_threads> <output.json>
```

Example output on the terminal:

```
Search complete. dataset=20 threads=4 cache=HIT elapsed=4.21ms
  #1  04_ocean_blue.ppm                         distance=0.000000
  #2  17_gradient_cyan.ppm                      distance=0.917826
  #3  19_gradient_lime.ppm                      distance=0.944646
  ...
Wrote results to out.json
```

The JSON file has the same data plus a `stats` block with timing and cache
information that the FastAPI server passes through to the UI.
