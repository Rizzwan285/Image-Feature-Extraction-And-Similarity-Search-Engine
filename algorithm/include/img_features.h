/*header for feature extraction module*/
#ifndef IMG_FEATURES_H
#define IMG_FEATURES_H

#include "image_io.h"

/*defining histogram bins per channel*/
#define HIST_BINS_PER_CHANNEL 8

/*defining total histogram bins*/
#define HIST_TOTAL_BINS (HIST_BINS_PER_CHANNEL * HIST_BINS_PER_CHANNEL * HIST_BINS_PER_CHANNEL)

/*defining grid layout and total blocks*/
#define BLOCK_GRID 4
#define BLOCK_FEATURES (BLOCK_GRID * BLOCK_GRID)

/*defining feature vector structure*/
typedef struct {
    char filename[MAX_FILENAME_LEN];
    float histogram[HIST_TOTAL_BINS];
    float mean[3];
    float variance[3];
    float block_means[BLOCK_FEATURES];
} FeatureVector;

/*extracting features for an image*/
void extract_features(const Image *img, FeatureVector *fv);

/*saving feature vectors to cache*/
int save_features_to_cache(const char *cache_path,
                           const FeatureVector *fvs,
                           int count);

/*loading feature vectors from cache*/
int load_features_from_cache(const char *cache_path,
                             FeatureVector **fvs_out,
                             int *count_out);

#endif /* IMG_FEATURES_H */
