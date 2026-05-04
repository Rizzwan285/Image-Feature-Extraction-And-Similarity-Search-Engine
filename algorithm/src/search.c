/*
 * search.c
 * --------
 * This is the threading core of the project — the most important C file.
 *
 * What it does at a high level:
 *   1. Load the query image and compute its feature vector.
 *   2. List all .ppm files in the dataset directory.
 *   3. Either load their feature vectors from the cache, or compute them fresh.
 *   4. Spawn N worker threads. Each thread grabs dataset images one at a time,
 *      computes the distance to the query, and tries to add the result to a
 *      shared "top K" list. That list is protected by a mutex so threads
 *      don't stomp on each other.
 *   5. Wait for all threads to finish, then sort and return the results.
 *
 * Key C concepts demonstrated here:
 *   - pthread_create / pthread_join  (creating and waiting for threads)
 *   - pthread_mutex_t                (locking shared data between threads)
 *   - struct with function pointers  (passing state to thread workers)
 *   - dynamic memory allocation      (malloc / realloc / free)
 *   - directory traversal            (opendir / readdir / closedir)
 */

/* _POSIX_C_SOURCE enables POSIX extensions like strdup and gettimeofday
 * which aren't in standard C11 but are available on Linux/macOS */
#define _POSIX_C_SOURCE 200809L

/* Our own headers — bring in the types we defined */
#include "search.h"
#include "img_features.h"
#include "similarity.h"

/* Standard C and POSIX headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>     /* opendir, readdir, closedir — for listing directories */
#include <sys/stat.h>   /* stat — for file metadata (not used directly but often needed) */
#include <sys/time.h>   /* gettimeofday — to measure elapsed time */
#include <pthread.h>    /* pthread_create, pthread_join, pthread_mutex_t, etc. */

/*
 * SharedSearchState — everything the worker threads need to do their job.
 *
 * This struct is allocated once in run_search() and a pointer to it is
 * passed to every worker thread. Threads share this single struct, which
 * is why the two mutex fields are needed — to stop threads from reading
 * and writing the same memory at the same time.
 *
 * query_fv      — the feature vector of the query image (read-only for threads)
 * dataset_fvs   — all the dataset feature vectors (read-only for threads)
 * dataset_count — how many dataset images there are
 *
 * next_index    — the next dataset image index a thread should process.
 *                 Protected by work_mutex so two threads never process the same image.
 * work_mutex    — mutex that guards next_index
 *
 * topk          — the shared sorted top-K results array.
 *                 Protected by topk_mutex so threads don't corrupt it.
 * topk_size     — how many slots are in the array (= the requested top_k)
 * topk_filled   — how many slots have been filled so far
 * topk_mutex    — mutex that guards the topk array and topk_filled counter
 */
typedef struct {
    /* Read-only inputs — threads only read these, never write */
    const FeatureVector *query_fv;
    const FeatureVector *dataset_fvs;
    int dataset_count;

    /* Work queue — protected by work_mutex */
    int next_index;
    pthread_mutex_t work_mutex;

    /* Shared results — protected by topk_mutex */
    SearchResult *topk;
    int topk_size;
    int topk_filled;
    pthread_mutex_t topk_mutex;
} SharedSearchState;

/*
 * try_insert_topk — attempts to insert a new search result into the sorted top-K array.
 *
 * The array is kept sorted by distance ascending (closest match at index 0).
 * If the array is already full and this result is worse than the worst in the
 * array, we drop it immediately. Otherwise we find where it belongs, shift
 * everything after that position one slot to the right, and insert it.
 *
 * This is essentially an insertion step in an insertion sort.
 * The array is tiny (at most 32 entries) so linear scan is fine.
 *
 * IMPORTANT: the caller MUST hold topk_mutex before calling this function.
 * If two threads called this at the same time without the mutex, they could
 * both try to shift entries and corrupt the array.
 *
 * state    — the shared state that owns the topk array
 * filename — the dataset image filename to insert
 * distance — its distance from the query image
 */
static void try_insert_topk(SharedSearchState *state,
                            const char *filename,
                            float distance) {
    /* Fast reject: if the array is already full and this result is worse
     * than the worst entry currently in the array, there's nothing to do */
    if (state->topk_filled == state->topk_size &&
        distance >= state->topk[state->topk_size - 1].distance) {
        return;
    }

    /* Find the position where this result should be inserted.
     * We scan from the front (best results) and stop when we find an entry
     * that's worse than ours — we'll insert just before it. */
    int pos = state->topk_filled;  /* default: append at the end */
    for (int i = 0; i < state->topk_filled; i++) {
        if (distance < state->topk[i].distance) {
            pos = i;    /* insert before position i */
            break;
        }
    }

    /* "last" is the index of the last valid entry after the insert.
     * If the array isn't full yet, we grow it by 1. If it's full, we
     * overwrite the last (worst) slot — effectively evicting it. */
    int last = (state->topk_filled < state->topk_size)
               ? state->topk_filled
               : state->topk_size - 1;

    /* Shift entries from position `pos` to `last` one slot to the right
     * to make room for the new entry at position `pos` */
    for (int i = last; i > pos; i--) {
        state->topk[i] = state->topk[i - 1];
    }

    /* Write the new entry into the freed-up slot at position pos */
    size_t flen = strlen(filename);
    if (flen >= MAX_FILENAME_LEN) flen = MAX_FILENAME_LEN - 1;  /* clamp to buffer */
    memcpy(state->topk[pos].filename, filename, flen);
    state->topk[pos].filename[flen] = '\0';
    state->topk[pos].distance = distance;
    state->topk[pos].rank = 0;  /* ranks are assigned after all threads finish */

    /* If there was still empty space in the array, count this as a new entry */
    if (state->topk_filled < state->topk_size) {
        state->topk_filled++;
    }
}

/*
 * worker — the function each worker thread runs.
 *
 * Each thread runs this in a loop:
 *   1. Lock work_mutex, grab the next dataset index, unlock.
 *   2. If no more images, exit.
 *   3. Compute the distance between dataset[idx] and the query.
 *   4. Lock topk_mutex, try to insert the result, unlock.
 *   5. Repeat.
 *
 * This pattern (each thread grabs the next available unit of work) is
 * called "work stealing" or a "work queue". It naturally load-balances
 * across threads without needing to pre-assign image ranges.
 *
 * arg — must be a pointer to SharedSearchState (pthread requires void*)
 *
 * Returns NULL (pthread workers must return void*)
 */
static void *worker(void *arg) {
    /* Cast the void* back to our actual struct type */
    SharedSearchState *state = (SharedSearchState *)arg;

    while (1) {
        /* --- Grab the next dataset index to process ---
         * We lock, read and increment the counter, then unlock immediately.
         * This keeps the lock held for the absolute minimum time. */
        pthread_mutex_lock(&state->work_mutex);
        int idx = state->next_index;
        if (idx >= state->dataset_count) {
            /* No more images left — this thread is done */
            pthread_mutex_unlock(&state->work_mutex);
            break;
        }
        state->next_index++;    /* claim this index for ourselves */
        pthread_mutex_unlock(&state->work_mutex);

        /* --- Compute the distance (no lock needed here) ---
         * We're only reading from query_fv and dataset_fvs, which are
         * written once before threads start and never modified after.
         * Reading from immutable shared data is always safe. */
        const FeatureVector *fv = &state->dataset_fvs[idx];
        float dist = euclidean_distance(state->query_fv, fv);

        /* --- Try to add this result to the shared top-K list ---
         * We must hold topk_mutex here because try_insert_topk modifies
         * the shared array and multiple threads could call it at the same time. */
        pthread_mutex_lock(&state->topk_mutex);
        try_insert_topk(state, fv->filename, dist);
        pthread_mutex_unlock(&state->topk_mutex);
    }

    return NULL;    /* pthread worker functions must return a pointer */
}

/* ------------------------------------------------------------------ */
/* Helper functions for listing files and building the feature dataset */
/* ------------------------------------------------------------------ */

/*
 * has_ppm_extension — quick check whether a filename ends in ".ppm"
 * Returns 1 (true) or 0 (false).
 */
static int has_ppm_extension(const char *name) {
    size_t n = strlen(name);
    if (n < 4) return 0;    /* ".ppm" is 4 chars, so anything shorter can't match */
    return (strcmp(name + n - 4, ".ppm") == 0);
}

/*
 * list_ppm_files — scans a directory and collects the names of all .ppm files.
 *
 * We allocate a dynamic array of strings and grow it as needed with realloc.
 * The caller is responsible for freeing each string (with free) and then
 * freeing the array itself.
 *
 * dir       — the directory path to scan
 * out_names — we'll store the pointer to our newly allocated array here
 *
 * Returns the number of .ppm files found, or -1 on error.
 */
static int list_ppm_files(const char *dir, char ***out_names) {
    /* opendir opens the directory for reading */
    DIR *d = opendir(dir);
    if (d == NULL) return -1;   /* directory doesn't exist or can't be opened */

    /* Start with space for 32 filenames and double it whenever we run out */
    int capacity = 32;
    int count = 0;
    char **names = (char **)malloc(sizeof(char *) * capacity);
    if (names == NULL) { closedir(d); return -1; }

    struct dirent *entry;
    /* readdir returns one directory entry per call, NULL when there are no more */
    while ((entry = readdir(d)) != NULL) {
        /* Skip anything that doesn't end in .ppm */
        if (!has_ppm_extension(entry->d_name)) continue;

        /* If we've filled the array, double its capacity with realloc */
        if (count == capacity) {
            capacity *= 2;
            char **bigger = (char **)realloc(names, sizeof(char *) * capacity);
            if (bigger == NULL) {
                /* realloc failed — clean up everything we've allocated so far */
                for (int i = 0; i < count; i++) free(names[i]);
                free(names);
                closedir(d);
                return -1;
            }
            names = bigger;
        }

        /* strdup makes a heap-allocated copy of the filename string */
        names[count] = strdup(entry->d_name);
        if (names[count] == NULL) {
            /* strdup can fail if we're out of memory */
            for (int i = 0; i < count; i++) free(names[i]);
            free(names);
            closedir(d);
            return -1;
        }
        count++;
    }

    closedir(d);
    *out_names = names;     /* hand the array pointer back to the caller */
    return count;
}

/*
 * build_or_load_dataset_features — returns FeatureVectors for the whole dataset.
 *
 * First we try to load them from the cache file (fast path).
 * If the cache doesn't exist, is empty, or has a different set of images,
 * we extract features from scratch (slow path) and then save a fresh cache.
 *
 * dataset_dir    — where the .ppm files live
 * cache_path     — path to the binary cache file
 * filenames      — the list of .ppm filenames we got from list_ppm_files
 * filename_count — how many filenames we have
 * out_fvs        — we set *out_fvs to the malloc'd array of FeatureVectors
 * cache_hit_out  — we set *cache_hit_out to 1 if the cache was used, else 0
 *
 * Returns the number of feature vectors, or -1 on fatal error.
 */
static int build_or_load_dataset_features(const char *dataset_dir,
                                          const char *cache_path,
                                          char **filenames,
                                          int filename_count,
                                          FeatureVector **out_fvs,
                                          int *cache_hit_out) {
    *cache_hit_out = 0;     /* assume cache miss until proven otherwise */

    /* --- Try to load from the cache file --- */
    FeatureVector *cached = NULL;
    int cached_count = 0;
    if (load_features_from_cache(cache_path, &cached, &cached_count) == 0 &&
        cached_count == filename_count) {
        /* The cache loaded and has the same number of images. Now verify that
         * every filename in our current directory is present in the cache.
         * This is an order-insensitive check — the cache can be in any order. */
        int all_present = 1;
        for (int i = 0; i < filename_count && all_present; i++) {
            int found = 0;
            for (int j = 0; j < cached_count; j++) {
                if (strcmp(cached[j].filename, filenames[i]) == 0) {
                    found = 1; break;
                }
            }
            if (!found) all_present = 0;    /* a file in the directory isn't in the cache */
        }

        if (all_present) {
            /* Cache is valid — use it and skip all the heavy extraction */
            *out_fvs = cached;
            *cache_hit_out = 1;
            return cached_count;
        }

        /* Cache was stale — free it and fall through to fresh extraction */
        free(cached);
        cached = NULL;
    } else if (cached != NULL) {
        /* load succeeded but count didn't match — free the stale data */
        free(cached);
    }

    /* --- Cache miss: extract features from every image in the dataset --- */
    FeatureVector *fvs = (FeatureVector *)malloc(sizeof(FeatureVector) * (size_t)filename_count);
    if (fvs == NULL) return -1;     /* out of memory */

    for (int i = 0; i < filename_count; i++) {
        /* Build the full path: dataset_dir + "/" + filename */
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dataset_dir, filenames[i]);

        /* Load the PPM image for this dataset entry */
        Image img;
        memset(&img, 0, sizeof(img));   /* zero it out before use */
        if (load_ppm(path, &img) != 0) {
            /* Couldn't read this image — warn but keep going.
             * We write a zero feature vector so its distance will be very large
             * and it'll end up at the bottom of any ranking. */
            fprintf(stderr, "warning: skipping unreadable %s\n", path);
            memset(&fvs[i], 0, sizeof(fvs[i]));
            strncpy(fvs[i].filename, filenames[i], MAX_FILENAME_LEN - 1);
            continue;
        }

        /* Extract the feature vector and then immediately free the pixel data
         * — we only need the numbers from now on, not the raw pixels */
        extract_features(&img, &fvs[i]);
        free_image(&img);
    }

    /* Save this fresh extraction as the new cache for next time */
    save_features_to_cache(cache_path, fvs, filename_count);

    *out_fvs = fvs;
    return filename_count;
}

/*
 * now_ms — returns the current wall-clock time in milliseconds.
 *
 * We call this before and after the search to measure elapsed time,
 * which gets reported in the JSON output and shown in the UI stats bar.
 */
static double now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);    /* fills tv.tv_sec (seconds) and tv.tv_usec (microseconds) */
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

/*
 * run_search — the main entry point. Called by main.c.
 *
 * This ties everything together: load query → list dataset → get features
 * → spawn threads → join threads → assign ranks → return results.
 *
 * query_ppm   — path to the query .ppm file
 * dataset_dir — directory of dataset .ppm files
 * cache_path  — binary cache file path
 * top_k       — how many top matches to return
 * num_threads — how many worker threads to spawn
 * results_out — caller provides this array; we fill in the top_k best matches
 * stats_out   — we fill in timing and diagnostic info here (can be NULL)
 *
 * Returns number of results written (≤ top_k), or a negative error code.
 */
int run_search(const char *query_ppm,
               const char *dataset_dir,
               const char *cache_path,
               int top_k,
               int num_threads,
               SearchResult *results_out,
               SearchStats *stats_out) {
    /* Clamp the parameters to sane ranges */
    if (top_k <= 0) top_k = 5;
    if (top_k > MAX_TOP_K) top_k = MAX_TOP_K;
    if (num_threads <= 0) num_threads = 1;
    if (num_threads > 16) num_threads = 16;

    /* Record the start time so we can compute elapsed time at the end */
    double t_start = now_ms();

    /* --- Step 1: Load the query image and get its feature vector ---
     * We always re-extract the query's features fresh (no caching for queries)
     * because the query changes every time the user picks a different image. */
    Image query_img;
    memset(&query_img, 0, sizeof(query_img));
    if (load_ppm(query_ppm, &query_img) != 0) {
        fprintf(stderr, "run_search: failed to load query %s\n", query_ppm);
        return -1;
    }
    FeatureVector query_fv;
    extract_features(&query_img, &query_fv);    /* compute the numbers */
    free_image(&query_img);                      /* done with raw pixels */

    /* --- Step 2: List all .ppm files in the dataset directory --- */
    char **filenames = NULL;
    int filename_count = list_ppm_files(dataset_dir, &filenames);
    if (filename_count <= 0) {
        fprintf(stderr, "run_search: no PPM images in %s\n", dataset_dir);
        return -2;
    }

    /* --- Step 3: Get feature vectors for the whole dataset ---
     * Either from the cache (fast) or by extracting them fresh (slow). */
    FeatureVector *fvs = NULL;
    int cache_hit = 0;
    int n = build_or_load_dataset_features(dataset_dir, cache_path,
                                           filenames, filename_count,
                                           &fvs, &cache_hit);
    if (n <= 0) {
        /* Clean up what we allocated before returning */
        for (int i = 0; i < filename_count; i++) free(filenames[i]);
        free(filenames);
        return -3;
    }

    /* --- Step 4: Set up the shared state struct for the worker threads ---
     * All threads get a pointer to this same struct. The mutexes inside it
     * make sure they don't step on each other. */
    SharedSearchState state;
    memset(&state, 0, sizeof(state));
    state.query_fv = &query_fv;         /* read-only for threads */
    state.dataset_fvs = fvs;            /* read-only for threads */
    state.dataset_count = n;
    state.next_index = 0;               /* start at the first dataset image */

    /* Allocate the shared results array — cleared to zero by calloc */
    state.topk = (SearchResult *)calloc((size_t)top_k, sizeof(SearchResult));
    state.topk_size = top_k;
    state.topk_filled = 0;

    /* Initialize the two mutexes before any threads try to use them */
    pthread_mutex_init(&state.work_mutex, NULL);
    pthread_mutex_init(&state.topk_mutex, NULL);

    /* --- Step 5: Spawn the worker threads ---
     * Each thread gets the same SharedSearchState pointer as its argument.
     * pthread_create returns 0 on success; non-zero means it failed to start. */
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * (size_t)num_threads);
    int actually_started = 0;
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker, &state) == 0) {
            actually_started++;     /* count threads that actually started */
        } else {
            fprintf(stderr, "warning: pthread_create failed for thread %d\n", i);
        }
    }

    /* --- Step 6: Wait for all threads to finish ---
     * pthread_join blocks until the given thread exits. We wait for each
     * thread in turn. Only after all are joined can we safely read the results. */
    for (int i = 0; i < actually_started; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);  /* done with the thread handles */

    /* --- Step 7: Assign 1-based ranks and copy to the output array ---
     * The topk array is already sorted (closest first) by the insertion logic.
     * We just number them 1, 2, 3, ... and copy into results_out. */
    int written = state.topk_filled;
    for (int i = 0; i < written; i++) {
        state.topk[i].rank = i + 1;        /* rank 1 = best match */
        results_out[i] = state.topk[i];    /* copy the whole struct */
    }

    /* --- Cleanup: free everything we allocated --- */
    free(state.topk);
    pthread_mutex_destroy(&state.work_mutex);   /* release mutex resources */
    pthread_mutex_destroy(&state.topk_mutex);
    free(fvs);                                  /* dataset feature vectors */
    for (int i = 0; i < filename_count; i++) free(filenames[i]);  /* individual filenames */
    free(filenames);                            /* the filename array itself */

    double t_end = now_ms();

    /* Fill in the stats if the caller wants them */
    if (stats_out != NULL) {
        stats_out->total_dataset_images = n;
        stats_out->threads_used = actually_started > 0 ? actually_started : 1;
        stats_out->cache_hit = cache_hit;
        stats_out->elapsed_ms = t_end - t_start;
    }

    return written;     /* how many results we put into results_out */
}
