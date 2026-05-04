/*
 * search.h
 * --------
 * Header for the threaded search module.
 *
 * This is where threads enter the picture. The idea is simple: we have a
 * query image and a directory full of dataset images. We want to find which
 * dataset images are most similar to the query. To do that faster, we split
 * the work across multiple threads — each thread handles a portion of the
 * dataset, computes distances, and contributes to a shared sorted top-K list.
 *
 * The two structs below carry the output and the timing diagnostics back
 * to the caller (main.c), which then writes them as JSON.
 */

#ifndef SEARCH_H
#define SEARCH_H

/* We need Image / MAX_FILENAME_LEN from the image_io header */
#include "image_io.h"

/* The maximum number of top results we can ever return.
 * Callers ask for top_k results and top_k must be ≤ this. */
#define MAX_TOP_K 32

/*
 * SearchResult — one entry in the ranked results list.
 *
 * filename — the name of the matching dataset image (.ppm filename)
 * distance — how far this image's feature vector is from the query's
 *            (0.0 = identical, higher = more different)
 * rank     — 1-based position in the sorted results (1 = closest match)
 */
typedef struct {
    char filename[MAX_FILENAME_LEN];
    float distance;
    int rank;
} SearchResult;

/*
 * SearchStats — diagnostic numbers from the last search run.
 * Passed back to main.c so it can write them into the JSON output
 * and display them in the UI's stats bar.
 *
 * total_dataset_images — how many PPM files were found in the dataset directory
 * threads_used         — how many worker threads were actually started
 * cache_hit            — 1 if we loaded features from the cache file,
 *                        0 if we had to extract from scratch
 * elapsed_ms           — total wall-clock time the search took, in milliseconds
 */
typedef struct {
    int total_dataset_images;
    int threads_used;
    int cache_hit;
    double elapsed_ms;
} SearchStats;

/*
 * run_search — the main entry point for doing a similarity search.
 *
 * query_ppm   — path to the query image (.ppm format)
 * dataset_dir — directory containing all the dataset .ppm images
 * cache_path  — path to the binary feature cache file (read if it exists,
 *               written if it doesn't)
 * top_k       — how many of the closest matches to return; must be 1..MAX_TOP_K
 * num_threads — how many worker threads to spawn; will be clamped to 1..16
 * results_out — caller provides this array of size top_k; we fill it in
 * stats_out   — pointer to a SearchStats to fill in, or NULL if you don't care
 *
 * Returns the actual number of results written into results_out (could be
 * less than top_k if the dataset has fewer images), or a negative number
 * on error.
 */
int run_search(const char *query_ppm,
               const char *dataset_dir,
               const char *cache_path,
               int top_k,
               int num_threads,
               SearchResult *results_out,
               SearchStats *stats_out);

#endif /* SEARCH_H */
