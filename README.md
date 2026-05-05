# Image Feature Extraction & Similarity Search Engine

A high-performance image similarity search system designed to find visually similar images within a dataset based on a given query image. The core feature extraction and ranking algorithms are implemented in C using multi-threading for optimal performance.

This project was built to demonstrate clean, modular C programming—utilizing structs, pointers, multiple files, custom headers, multi-threading, and mutexes—wrapped within a modern, fully functional full-stack web application.

## Key Features

- **Multi-threaded C Core:** Extracts a 534-float feature vector per image (RGB histograms, per-channel mean and variance, and a 4×4 grid of intensity means) and ranks results concurrently using POSIX threads.
- **Multiple Distance Metrics:** Search using Euclidean (L2), Manhattan (L1), or Cosine distance functions. Metrics are dynamically selected at runtime via C function pointers.
- **Binary Feature Caching:** Extracted features are cached in a binary format (`data/features.cache`) for lightning-fast subsequent searches.
- **FastAPI Bridge:** A Python backend that handles image conversion to PPM using Pillow, orchestrates the C binary subprocess execution, and serves the results.
- **Modern React UI:** Built with Vite, Tailwind CSS, and Framer Motion. It provides a beautiful interface to upload query images, select images from the dataset, and visually compare the impact of different distance metrics.

## Project Architecture

The system is organized into a four-layer structure:

1. **UI Layer (`ui/`)**
   A React application built with Vite that provides a smooth, animated single-page interface. It allows users to upload queries or browse the dataset, and displays the ranked results along with animated similarity score bars.

2. **Communication Layer (`communication/`)**
   A FastAPI Python backend that acts as the bridge. When an image is queried, it converts it to the PPM format, invokes the compiled C binary as a subprocess, parses the JSON output, and returns the enriched data to the frontend.

3. **Algorithm Layer (`algorithm/`)**
   The computational heart of the system, written entirely in C. It parses PPM files, calculates detailed feature vectors, and compares the query against the dataset. The workload is distributed across multiple worker threads, with a mutex safeguarding the shared top-K results array.

4. **Backend Layer (`data/`)**
   A local filesystem layer that stores original images, generated PPMs, and the binary feature cache. The cache prevents redundant feature extraction, drastically reducing search times on subsequent runs.

## Prerequisites

To run this project, ensure you have the following installed:

- `gcc` (A recent version for compiling the C binary)
- `make` (For using the provided Makefile commands)
- Python 3.10 or newer with `pip`
- Node.js 18 or newer with `npm`

## Setup & Execution

### 1. Clone the Repository

```bash
git clone <repository-url>
cd image-similarity-search
```

### 2. Install Dependencies

Install the required Python packages and Node modules:

```bash
make install
```

### 3. Build the Application

Compile the C algorithmic core into the `algorithm/bin/imgsearch` executable:

```bash
make
```

### 4. Generate Sample Data (Optional)

If you do not have your own images, generate the procedural CC0 sample dataset (~20 images) to test the application:

```bash
make sample-data
```
*(If you prefer to use your own images, simply place your PNG/JPG files inside the `data/images/` directory before starting the application).*

### 5. Run the Application

Start both the FastAPI backend and the React frontend simultaneously:

```bash
make run
```

Once running, open **http://localhost:5173** in your web browser. The FastAPI backend will be listening on `http://localhost:8000`.

### Faster Restarts

After your initial setup and data generation, you can quickly restart the application anytime using:

```bash
make run
```

## Running the C Binary Independently

You can execute the C algorithmic core directly from the terminal without the Python or React layers. This is highly useful for testing or benchmarking.

**Usage:**
```bash
./algorithm/bin/imgsearch <query.ppm> <dataset_dir> <cache_path> <top_k> <num_threads> <output.json> [metric]
```

**Distance Metrics:**
The seventh argument (`metric`) is optional. It defaults to Euclidean distance.

| Value | Function Called |
|-------|-----------------|
| `euclidean` (default) | `euclidean_distance` |
| `manhattan` | `manhattan_distance` |
| `cosine` | `cosine_distance` |

**Example Command:**
```bash
./algorithm/bin/imgsearch data/ppm/query.ppm data/ppm data/features.cache 5 4 data/output.json cosine
```
This runs the search using the Cosine distance metric, utilizing 4 threads to find the top 5 matches, and saves the results to `output.json`.

## Troubleshooting

- **`make` fails with "gcc not found":** Ensure you have the build tools installed (`sudo apt install build-essential` on Linux, or Xcode Command Line Tools on macOS).
- **"Could not reach the backend" Error:** The FastAPI server is likely not running. Ensure you used `make run`, or manually start the backend using `make run-api`.
- **No Images in the Dataset Modal:** Ensure you ran `make sample-data` or placed your own images into the `data/images/` directory. Restart the server so it can convert any new images to PPM format.
- **Incorrect Search Matches / Stale Data:** If you have modified the dataset directly, your feature cache might be stale. Run `make clean-cache` to force the system to re-extract features on the next run.
- **Ports Already in Use:** If ports 5173 or 8000 are blocked, terminate the conflicting processes, or modify the ports in `vite.config.js` and the `Makefile`.
