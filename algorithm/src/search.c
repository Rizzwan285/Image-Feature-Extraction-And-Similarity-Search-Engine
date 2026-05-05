/*implementing multithreaded similarity search*/
#define _POSIX_C_SOURCE 200809L

#include "search.h"
#include "img_features.h"
#include "similarity.h"

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

/*defining shared state for worker threads*/
typedef struct {
  const FeatureVector *query_fv;
  const FeatureVector *dataset_fvs;
  int dataset_count;

  DistanceFunction metric_fn;

  int next_index;
  pthread_mutex_t work_mutex;

  SearchResult *topk;
  int topk_size;
  int topk_filled;
  pthread_mutex_t topk_mutex;
} SharedSearchState;

/*inserting result into top-k array*/
static void try_insert_topk(SharedSearchState *state, const char *filename,
                            float distance) {
  if (state->topk_filled == state->topk_size &&
      distance >= state->topk[state->topk_size - 1].distance) {
    return;
  }

  int pos = state->topk_filled;
  for (int i = 0; i < state->topk_filled; i++) {
    if (distance < state->topk[i].distance) {
      pos = i;
      break;
    }
  }

  int last = (state->topk_filled < state->topk_size) ? state->topk_filled
                                                     : state->topk_size - 1;

  for (int i = last; i > pos; i--) {
    state->topk[i] = state->topk[i - 1];
  }

  size_t flen = strlen(filename);
  if (flen >= MAX_FILENAME_LEN)
    flen = MAX_FILENAME_LEN - 1;
  memcpy(state->topk[pos].filename, filename, flen);
  state->topk[pos].filename[flen] = '\0';
  state->topk[pos].distance = distance;
  state->topk[pos].rank = 0;

  if (state->topk_filled < state->topk_size) {
    state->topk_filled++;
  }
}

/*running worker thread loop*/
static void *worker(void *arg) {
  SharedSearchState *state = (SharedSearchState *)arg;

  while (1) {
    pthread_mutex_lock(&state->work_mutex);
    int idx = state->next_index;
    if (idx >= state->dataset_count) {
      pthread_mutex_unlock(&state->work_mutex);
      break;
    }
    state->next_index++;
    pthread_mutex_unlock(&state->work_mutex);

    const FeatureVector *fv = &state->dataset_fvs[idx];
    float dist = state->metric_fn(state->query_fv, fv);

    pthread_mutex_lock(&state->topk_mutex);
    try_insert_topk(state, fv->filename, dist);
    pthread_mutex_unlock(&state->topk_mutex);
  }

  return NULL;
}

/*checking for ppm extension*/
static int has_ppm_extension(const char *name) {
  size_t n = strlen(name);
  if (n < 4)
    return 0;
  return (strcmp(name + n - 4, ".ppm") == 0);
}

/*listing ppm files in directory*/
static int list_ppm_files(const char *dir, char ***out_names) {
  DIR *d = opendir(dir);
  if (d == NULL)
    return -1;

  int capacity = 32;
  int count = 0;
  char **names = (char **)malloc(sizeof(char *) * capacity);
  if (names == NULL) {
    closedir(d);
    return -1;
  }

  struct dirent *entry;
  while ((entry = readdir(d)) != NULL) {
    if (!has_ppm_extension(entry->d_name))
      continue;

    if (count == capacity) {
      capacity *= 2;
      char **bigger = (char **)realloc(names, sizeof(char *) * capacity);
      if (bigger == NULL) {
        for (int i = 0; i < count; i++)
          free(names[i]);
        free(names);
        closedir(d);
        return -1;
      }
      names = bigger;
    }

    names[count] = strdup(entry->d_name);
    if (names[count] == NULL) {
      for (int i = 0; i < count; i++)
        free(names[i]);
      free(names);
      closedir(d);
      return -1;
    }
    count++;
  }

  closedir(d);
  *out_names = names;
  return count;
}

/*building or loading dataset features*/
static int build_or_load_dataset_features(const char *dataset_dir,
                                          const char *cache_path,
                                          char **filenames, int filename_count,
                                          FeatureVector **out_fvs,
                                          int *cache_hit_out) {
  *cache_hit_out = 0;

  FeatureVector *cached = NULL;
  int cached_count = 0;
  if (load_features_from_cache(cache_path, &cached, &cached_count) == 0 &&
      cached_count == filename_count) {
    int all_present = 1;
    for (int i = 0; i < filename_count && all_present; i++) {
      int found = 0;
      for (int j = 0; j < cached_count; j++) {
        if (strcmp(cached[j].filename, filenames[i]) == 0) {
          found = 1;
          break;
        }
      }
      if (!found)
        all_present = 0;
    }

    if (all_present) {
      *out_fvs = cached;
      *cache_hit_out = 1;
      return cached_count;
    }

    free(cached);
    cached = NULL;
  } else if (cached != NULL) {
    free(cached);
  }

  FeatureVector *fvs =
      (FeatureVector *)malloc(sizeof(FeatureVector) * (size_t)filename_count);
  if (fvs == NULL)
    return -1;

  for (int i = 0; i < filename_count; i++) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dataset_dir, filenames[i]);

    Image img;
    memset(&img, 0, sizeof(img));
    if (load_ppm(path, &img) != 0) {
      fprintf(stderr, "warning: skipping unreadable %s\n", path);
      memset(&fvs[i], 0, sizeof(fvs[i]));
      strncpy(fvs[i].filename, filenames[i], MAX_FILENAME_LEN - 1);
      continue;
    }

    extract_features(&img, &fvs[i]);
    free_image(&img);
  }

  save_features_to_cache(cache_path, fvs, filename_count);

  *out_fvs = fvs;
  return filename_count;
}

/*getting current time in ms*/
static double now_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

/*running the search process*/
int run_search(const char *query_ppm, const char *dataset_dir,
               const char *cache_path, int top_k, int num_threads,
               const char *metric, SearchResult *results_out,
               SearchStats *stats_out) {
  if (top_k <= 0)
    top_k = 5;
  if (top_k > MAX_TOP_K)
    top_k = MAX_TOP_K;
  if (num_threads <= 0)
    num_threads = 1;
  if (num_threads > 16)
    num_threads = 16;

  DistanceFunction metric_fn = pick_distance_function(metric);

  const char *metric_used = "euclidean";
  if (metric_fn == manhattan_distance)
    metric_used = "manhattan";
  else if (metric_fn == cosine_distance)
    metric_used = "cosine";

  double t_start = now_ms();

  Image query_img;
  memset(&query_img, 0, sizeof(query_img));
  if (load_ppm(query_ppm, &query_img) != 0) {
    fprintf(stderr, "run_search: failed to load query %s\n", query_ppm);
    return -1;
  }
  FeatureVector query_fv;
  extract_features(&query_img, &query_fv);
  free_image(&query_img);

  char **filenames = NULL;
  int filename_count = list_ppm_files(dataset_dir, &filenames);
  if (filename_count <= 0) {
    fprintf(stderr, "run_search: no PPM images in %s\n", dataset_dir);
    return -2;
  }

  FeatureVector *fvs = NULL;
  int cache_hit = 0;
  int n = build_or_load_dataset_features(dataset_dir, cache_path, filenames,
                                         filename_count, &fvs, &cache_hit);
  if (n <= 0) {
    for (int i = 0; i < filename_count; i++)
      free(filenames[i]);
    free(filenames);
    return -3;
  }

  SharedSearchState state;
  memset(&state, 0, sizeof(state));
  state.query_fv = &query_fv;
  state.dataset_fvs = fvs;
  state.dataset_count = n;
  state.metric_fn = metric_fn;
  state.next_index = 0;

  state.topk = (SearchResult *)calloc((size_t)top_k, sizeof(SearchResult));
  state.topk_size = top_k;
  state.topk_filled = 0;

  pthread_mutex_init(&state.work_mutex, NULL);
  pthread_mutex_init(&state.topk_mutex, NULL);

  pthread_t *threads =
      (pthread_t *)malloc(sizeof(pthread_t) * (size_t)num_threads);
  int actually_started = 0;
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&threads[i], NULL, worker, &state) == 0) {
      actually_started++;
    } else {
      fprintf(stderr, "warning: pthread_create failed for thread %d\n", i);
    }
  }

  for (int i = 0; i < actually_started; i++) {
    pthread_join(threads[i], NULL);
  }
  free(threads);

  int written = state.topk_filled;
  for (int i = 0; i < written; i++) {
    state.topk[i].rank = i + 1;
    results_out[i] = state.topk[i];
  }

  free(state.topk);
  pthread_mutex_destroy(&state.work_mutex);
  pthread_mutex_destroy(&state.topk_mutex);
  free(fvs);
  for (int i = 0; i < filename_count; i++)
    free(filenames[i]);
  free(filenames);

  double t_end = now_ms();

  if (stats_out != NULL) {
    stats_out->total_dataset_images = n;
    stats_out->threads_used = actually_started > 0 ? actually_started : 1;
    stats_out->cache_hit = cache_hit;
    stats_out->elapsed_ms = t_end - t_start;

    size_t mlen = strlen(metric_used);
    if (mlen >= MAX_METRIC_NAME_LEN)
      mlen = MAX_METRIC_NAME_LEN - 1;
    memcpy(stats_out->metric, metric_used, mlen);
    stats_out->metric[mlen] = '\0';
  }

  return written;
}
