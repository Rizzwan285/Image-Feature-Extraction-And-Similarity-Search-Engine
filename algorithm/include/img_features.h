/*
 * img_features.h
 * --------------
 * Header for the feature extraction module.
 *
 * "Feature extraction" just means: take an image and turn it into a list of
 * numbers that describe what it looks like. We then compare those numbers
 * between images to decide how similar they are.
 *
 * NOTE: This file is called img_features.h (not features.h) on purpose.
 * The C standard library's <stdio.h> internally pulls in a system header
 * also called <features.h>. If we named ours the same thing, our local copy
 * would shadow the system one and cause confusing compile errors. The "img_"
 * prefix avoids that collision entirely.
 *
 * Each image gets squeezed into a FeatureVector with 534 floats total:
 *   - 512 from the RGB color histogram  (8 bins × 8 bins × 8 bins)
 *   -   3 from the per-channel mean     (average red, green, blue)
 *   -   3 from the per-channel variance (how spread-out each channel is)
 *   -  16 from the 4×4 block-mean grid  (captures rough spatial layout)
 */

/* Standard include guard — prevents this header being processed more than once */
#ifndef IMG_FEATURES_H
#define IMG_FEATURES_H

/* We need the Image struct defined in image_io.h */
#include "image_io.h"

/* How many buckets each color channel gets in the histogram.
 * 8 means we use the top 3 bits of each byte (0-255 → 0-7). */
#define HIST_BINS_PER_CHANNEL 8

/* Total histogram bins: 8 red × 8 green × 8 blue = 512.
 * Each bin counts how many pixels fell into that particular color range. */
#define HIST_TOTAL_BINS (HIST_BINS_PER_CHANNEL * HIST_BINS_PER_CHANNEL * HIST_BINS_PER_CHANNEL)

/* We divide each image into a 4×4 grid of rectangular blocks */
#define BLOCK_GRID 4

/* Total number of blocks: 4 columns × 4 rows = 16 */
#define BLOCK_FEATURES (BLOCK_GRID * BLOCK_GRID)

/*
 * FeatureVector — the numerical "fingerprint" of one image.
 *
 * filename     — which image this fingerprint belongs to (carried along for convenience)
 * histogram    — normalized RGB color histogram (512 floats, sums to 1.0)
 * mean         — average pixel value for R, G, B channels separately, scaled to [0, 1]
 * variance     — spread of pixel values for each channel, scaled to [0, 1]
 * block_means  — average brightness in each of the 16 spatial blocks, in [0, 1]
 */
typedef struct {
    char filename[MAX_FILENAME_LEN];
    float histogram[HIST_TOTAL_BINS];
    float mean[3];
    float variance[3];
    float block_means[BLOCK_FEATURES];
} FeatureVector;

/*
 * extract_features — builds the FeatureVector for one image.
 *
 * img — the image to analyze (must be loaded with load_ppm first)
 * fv  — the FeatureVector struct we'll fill in with the computed numbers
 *
 * This loops over every pixel once and calculates all three feature types in
 * a single pass, so it's reasonably efficient.
 */
void extract_features(const Image *img, FeatureVector *fv);

/*
 * save_features_to_cache — writes an array of FeatureVectors to a binary file.
 *
 * The format is simple: first a 32-bit integer saying how many vectors follow,
 * then the raw struct bytes for each one. This lets us skip re-extracting
 * features on the next run if the dataset hasn't changed.
 *
 * cache_path — where to write the file
 * fvs        — array of FeatureVectors to save
 * count      — how many entries are in the array
 *
 * Returns 0 on success, non-zero if the file couldn't be written.
 */
int save_features_to_cache(const char *cache_path,
                           const FeatureVector *fvs,
                           int count);

/*
 * load_features_from_cache — reads FeatureVectors back from a cache file.
 *
 * cache_path — path to the cache file written by save_features_to_cache
 * fvs_out    — we'll allocate an array and store the pointer here;
 *              the caller is responsible for freeing it later
 * count_out  — we'll store the number of entries here
 *
 * Returns 0 on success. Returns non-zero if the file doesn't exist,
 * can't be read, or looks corrupted — in that case fvs_out is not set.
 */
int load_features_from_cache(const char *cache_path,
                             FeatureVector **fvs_out,
                             int *count_out);

#endif /* IMG_FEATURES_H */
