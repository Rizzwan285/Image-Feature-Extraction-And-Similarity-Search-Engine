/*header for threaded search module*/

#ifndef SEARCH_H
#define SEARCH_H

/* We need Image / MAX_FILENAME_LEN from the image_io header */
#include "image_io.h"

/*defining maximum top results to return*/
#define MAX_TOP_K 32

/*defining maximum length for metric name*/
#define MAX_METRIC_NAME_LEN 32

/*defining search result entry structure*/
typedef struct {
  char filename[MAX_FILENAME_LEN];
  float distance;
  int rank;
} SearchResult;

/*defining diagnostic stats structure*/
typedef struct {
  int total_dataset_images;
  int threads_used;
  int cache_hit;
  double elapsed_ms;
  char metric[MAX_METRIC_NAME_LEN];
} SearchStats;

/*running similarity search entry point*/
int run_search(const char *query_ppm, const char *dataset_dir,
               const char *cache_path, int top_k, int num_threads,
               const char *metric, SearchResult *results_out,
               SearchStats *stats_out);

#endif /* SEARCH_H */
