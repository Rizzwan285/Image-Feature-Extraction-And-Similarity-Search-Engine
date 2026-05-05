/*acting as command-line entry point*/

#include "search.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*printing usage instructions*/
static void print_usage(const char *prog) {
  fprintf(stderr,
          "Usage: %s <query.ppm> <dataset_dir> <cache_path> <top_k> "
          "<num_threads> <output.json> [metric]\n"
          "  metric: euclidean (default), manhattan, or cosine\n",
          prog);
}

/*writing search results to json manually*/
static int write_results_json(const char *path, const char *query,
                              const SearchResult *results, int count,
                              const SearchStats *stats) {
  FILE *fp = fopen(path, "w");
  if (fp == NULL) {
    fprintf(stderr, "main: cannot open output %s\n", path);
    return 1;
  }

  fprintf(fp, "{\n");

  fprintf(fp, "  \"query\": \"%s\",\n", query);

  fprintf(fp, "  \"stats\": {\n");
  fprintf(fp, "    \"total_dataset_images\": %d,\n",
          stats->total_dataset_images);
  fprintf(fp, "    \"threads_used\": %d,\n", stats->threads_used);
  fprintf(fp, "    \"cache_hit\": %s,\n", stats->cache_hit ? "true" : "false");
  fprintf(fp, "    \"elapsed_ms\": %.3f,\n", stats->elapsed_ms);
  fprintf(fp, "    \"metric\": \"%s\"\n", stats->metric);
  fprintf(fp, "  },\n");

  fprintf(fp, "  \"results\": [\n");
  for (int i = 0; i < count; i++) {
    fprintf(fp,
            "    {\"rank\": %d, \"filename\": \"%s\", \"distance\": %.6f}%s\n",
            results[i].rank, results[i].filename, results[i].distance,
            (i + 1 < count) ? "," : "");
  }
  fprintf(fp, "  ]\n");

  fprintf(fp, "}\n");

  fclose(fp);
  return 0;
}

/*reading cli args and running search*/
int main(int argc, char **argv) {
  if (argc != 7 && argc != 8) {
    print_usage(argv[0]);
    return 1;
  }

  const char *query_path = argv[1];
  const char *dataset_dir = argv[2];
  const char *cache_path = argv[3];
  int top_k = atoi(argv[4]);
  int num_threads = atoi(argv[5]);
  const char *output_json = argv[6];

  const char *metric = (argc == 8) ? argv[7] : NULL;

  SearchResult results[MAX_TOP_K];
  memset(results, 0, sizeof(results));

  SearchStats stats;
  memset(&stats, 0, sizeof(stats));

  int n = run_search(query_path, dataset_dir, cache_path, top_k, num_threads,
                     metric, results, &stats);
  if (n < 0) {
    fprintf(stderr, "search failed (code %d)\n", n);
    return 2;
  }

  if (write_results_json(output_json, query_path, results, n, &stats) != 0) {
    return 3;
  }

  printf("Search complete. dataset=%d threads=%d cache=%s metric=%s "
         "elapsed=%.2fms\n",
         stats.total_dataset_images, stats.threads_used,
         stats.cache_hit ? "HIT" : "MISS", stats.metric, stats.elapsed_ms);

  for (int i = 0; i < n; i++) {
    printf("  #%d  %-40s  distance=%.6f\n", results[i].rank,
           results[i].filename, results[i].distance);
  }

  printf("Wrote results to %s\n", output_json);

  return 0;
}
