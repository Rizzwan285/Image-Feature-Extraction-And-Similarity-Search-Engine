/*
 * main.c
 * ------
 * The command-line entry point for the imgsearch binary.
 *
 * This file is intentionally thin — it just reads the command-line arguments,
 * calls run_search() which does all the real work, then writes the results
 * to a JSON file and prints a summary to the terminal.
 *
 * The Python server (server.py) invokes this binary as a subprocess and
 * reads the JSON file it produces. That's the whole "communication layer"
 * in action: Python speaks HTTP to React, and subprocess to C.
 *
 * Usage:
 *   ./imgsearch <query.ppm> <dataset_dir> <cache_path> <top_k> <num_threads> <output.json> [metric]
 *
 * The seventh argument (metric) is optional. Accepted values:
 *   euclidean   (default if omitted)
 *   manhattan
 *   cosine
 *
 * Older callers that pass only six arguments still work — the metric
 * silently defaults to euclidean.
 */

/* Bring in the SearchResult and SearchStats types, plus run_search() and MAX_TOP_K */
#include "search.h"

/* Standard library headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * print_usage — prints a one-liner explaining how to invoke the program.
 * Called when the user passes the wrong number of arguments.
 *
 * prog — argv[0], the name the binary was invoked with
 */
static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <query.ppm> <dataset_dir> <cache_path> <top_k> <num_threads> <output.json> [metric]\n"
        "  metric: euclidean (default), manhattan, or cosine\n",
        prog);
}

/*
 * write_results_json — writes the search results to a JSON file by hand.
 *
 * We don't use any JSON library here — we just build the output with
 * fprintf calls. This is intentional: it keeps the code dependency-free
 * and shows that JSON is really just a text format.
 *
 * The Python server reads this file immediately after the binary exits.
 *
 * path    — where to write the file
 * query   — the query image path (goes in the "query" field)
 * results — array of SearchResult structs
 * count   — how many results are in the array
 * stats   — timing and diagnostic info to include in the output
 *
 * Returns 0 on success, 1 if the file couldn't be opened.
 */
static int write_results_json(const char *path,
                              const char *query,
                              const SearchResult *results,
                              int count,
                              const SearchStats *stats) {
    /* Open the output file for writing — creates it if it doesn't exist */
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "main: cannot open output %s\n", path);
        return 1;
    }

    /* Opening brace of the top-level JSON object */
    fprintf(fp, "{\n");

    /* The query field — the path to the query image */
    fprintf(fp, "  \"query\": \"%s\",\n", query);

    /* The stats block — Python forwards these to the UI for the stats bar */
    fprintf(fp, "  \"stats\": {\n");
    fprintf(fp, "    \"total_dataset_images\": %d,\n", stats->total_dataset_images);
    fprintf(fp, "    \"threads_used\": %d,\n", stats->threads_used);
    fprintf(fp, "    \"cache_hit\": %s,\n", stats->cache_hit ? "true" : "false");
    fprintf(fp, "    \"elapsed_ms\": %.3f,\n", stats->elapsed_ms);
    /* The metric the search actually used — useful so the UI can label it */
    fprintf(fp, "    \"metric\": \"%s\"\n", stats->metric);
    fprintf(fp, "  },\n");

    /* The results array — one object per match */
    fprintf(fp, "  \"results\": [\n");
    for (int i = 0; i < count; i++) {
        fprintf(fp,
                /* Each result: rank, filename, and the raw distance value */
                "    {\"rank\": %d, \"filename\": \"%s\", \"distance\": %.6f}%s\n",
                results[i].rank,
                results[i].filename,
                results[i].distance,
                (i + 1 < count) ? "," : "");   /* no trailing comma on the last entry */
    }
    fprintf(fp, "  ]\n");

    /* Closing brace */
    fprintf(fp, "}\n");

    fclose(fp);
    return 0;   /* success */
}

/*
 * main — program entry point.
 *
 * Reads the six required command-line arguments (plus an optional 7th for the
 * distance metric), calls run_search(), and writes the results JSON. Also
 * prints a human-readable summary to stdout so whoever runs the binary on
 * the command line can see what happened.
 *
 * argc — number of arguments (7 for old-style invocation, 8 with metric)
 * argv — the argument strings
 */
int main(int argc, char **argv) {
    /* We accept either 6 or 7 positional args after the program name.
     * The 7th (metric) is optional — older callers still work unchanged. */
    if (argc != 7 && argc != 8) {
        print_usage(argv[0]);
        return 1;   /* non-zero exit code signals failure to the shell / Python */
    }

    /* Read each argument — atoi converts a string like "4" to integer 4 */
    const char *query_path = argv[1];       /* e.g. data/ppm/ocean.ppm */
    const char *dataset_dir = argv[2];      /* e.g. data/ppm */
    const char *cache_path = argv[3];       /* e.g. data/features.cache */
    int top_k = atoi(argv[4]);              /* how many matches to return, e.g. 6 */
    int num_threads = atoi(argv[5]);        /* how many threads to use, e.g. 4 */
    const char *output_json = argv[6];      /* e.g. data/output.json */

    /* Optional 7th argument: the distance metric name. NULL means "use the
     * default" — run_search/pick_distance_function will fall back to
     * euclidean for NULL or any unrecognized string. */
    const char *metric = (argc == 8) ? argv[7] : NULL;

    /* Allocate space for the results on the stack — MAX_TOP_K is only 32 entries */
    SearchResult results[MAX_TOP_K];
    memset(results, 0, sizeof(results));    /* zero it out to avoid garbage values */

    /* Same for stats */
    SearchStats stats;
    memset(&stats, 0, sizeof(stats));

    /* Run the actual search — this is where all the interesting work happens.
     * See search.c for the threading implementation. */
    int n = run_search(query_path, dataset_dir, cache_path,
                       top_k, num_threads, metric, results, &stats);
    if (n < 0) {
        /* A negative return value means something went seriously wrong */
        fprintf(stderr, "search failed (code %d)\n", n);
        return 2;
    }

    /* Write the JSON file that the Python server will pick up */
    if (write_results_json(output_json, query_path, results, n, &stats) != 0) {
        return 3;   /* couldn't write the output file */
    }

    /* Print a friendly summary to the terminal — handy when running manually */
    printf("Search complete. dataset=%d threads=%d cache=%s metric=%s elapsed=%.2fms\n",
           stats.total_dataset_images,
           stats.threads_used,
           stats.cache_hit ? "HIT" : "MISS",   /* HIT = we used the cache */
           stats.metric,
           stats.elapsed_ms);

    /* Print each result on its own line */
    for (int i = 0; i < n; i++) {
        printf("  #%d  %-40s  distance=%.6f\n",
               results[i].rank, results[i].filename, results[i].distance);
    }

    printf("Wrote results to %s\n", output_json);

    return 0;   /* zero = success in the shell and for Python's returncode check */
}
