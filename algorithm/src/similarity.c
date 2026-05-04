/*
 * similarity.c
 * ------------
 * Compares two feature vectors and returns a number telling you how different
 * they are. Lower number = images look more alike.
 *
 * We use Euclidean distance (L2 norm). Think of each FeatureVector as a point
 * in 534-dimensional space. Euclidean distance is just the straight-line
 * distance between two such points — the same formula as Pythagoras but
 * extended to many dimensions: sqrt( sum of (a[i] - b[i])^2 ).
 *
 * A distance of 0 means the two images are identical.
 * Larger distances mean the images look more different.
 */

/* We need the FeatureVector type and the HIST_TOTAL_BINS / BLOCK_FEATURES constants */
#include "similarity.h"

/* math.h gives us sqrt() */
#include <math.h>

/*
 * euclidean_distance — computes the L2 distance between feature vectors a and b.
 *
 * We treat all four feature groups (histogram, mean, variance, block_means)
 * as one long concatenated vector and compute the distance across the whole
 * thing in one go. Each component's contribution is just (a[i] - b[i])^2.
 *
 * a — pointer to the first FeatureVector (e.g. the query image)
 * b — pointer to the second FeatureVector (e.g. one dataset image)
 *
 * Returns the distance as a float. The caller can interpret this as:
 *   0.0       → identical images
 *   very high → very different images
 */
float euclidean_distance(const FeatureVector *a, const FeatureVector *b) {
    /* We accumulate the sum of squared differences as a double to keep
     * precision — we only cast to float at the very end. */
    double sum = 0.0;

    /* --- Histogram component ---
     * The histogram has 512 bins. Each bin holds a fraction of pixels (0..1).
     * Comparing histograms tells us whether the two images have similar overall
     * color distributions — e.g. both are mostly blue, or mostly warm-toned. */
    for (int i = 0; i < HIST_TOTAL_BINS; i++) {
        double d = (double)a->histogram[i] - (double)b->histogram[i];
        sum += d * d;   /* square the difference and add to the running total */
    }

    /* --- Per-channel mean component ---
     * 3 values (R, G, B), each normalized to [0, 1].
     * Comparing means tells us if the overall tint of both images is similar. */
    for (int i = 0; i < 3; i++) {
        double d = (double)a->mean[i] - (double)b->mean[i];
        sum += d * d;
    }

    /* --- Per-channel variance component ---
     * 3 values (R, G, B), each normalized to [0, 1].
     * Comparing variances tells us if both images have a similar amount of
     * color contrast — e.g. both are "flat" or both are "high contrast". */
    for (int i = 0; i < 3; i++) {
        double d = (double)a->variance[i] - (double)b->variance[i];
        sum += d * d;
    }

    /* --- Block-mean grid component ---
     * 16 values (4×4 spatial grid), each the average brightness of that region.
     * Comparing block means tells us if the images have a similar spatial layout —
     * e.g. both are bright on top and dark on the bottom (like a sky/ground photo). */
    for (int i = 0; i < BLOCK_FEATURES; i++) {
        double d = (double)a->block_means[i] - (double)b->block_means[i];
        sum += d * d;
    }

    /* Take the square root to get the actual Euclidean distance.
     * Without sqrt we'd have the squared distance, which also works for
     * ranking but gives values that are harder to interpret. */
    return (float)sqrt(sum);
}
