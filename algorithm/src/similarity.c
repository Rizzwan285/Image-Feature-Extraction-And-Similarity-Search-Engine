/*
 * similarity.c
 * ------------
 * Implements the distance / similarity metrics.
 *
 * Each function takes two FeatureVector pointers and returns a number where
 * lower = images look more alike. All three functions share the same
 * signature so they can be plugged into the search core through the
 * DistanceFunction function pointer typedef declared in similarity.h.
 *
 * Why three different metrics?
 *   - Euclidean (L2) — straight-line distance, the most common default.
 *   - Manhattan (L1) — sum of absolute differences; faster (no sqrt).
 *   - Cosine        — measures angle between vectors, ignores magnitude.
 *
 * Different metrics rank images differently. For colour-histogram-like
 * features, cosine often groups images by overall colour palette while
 * Euclidean weighs absolute brightness more strongly. Letting the user
 * switch metrics at runtime is the whole point of using a function
 * pointer: the search core never has to know which one is in use.
 */

/* We need the FeatureVector type and the HIST_TOTAL_BINS / BLOCK_FEATURES constants */
#include "similarity.h"

/* math.h gives us sqrt() and fabs() */
#include <math.h>

/* strcasecmp lives here on POSIX systems */
#include <strings.h>

/* NULL */
#include <stddef.h>

/*
 * --- helpers -------------------------------------------------------------
 *
 * The three metrics all need to walk through the same four feature groups
 * (histogram, mean, variance, block_means). To avoid copy-pasting four
 * loops into every metric we use a tiny pattern: a callback-shaped macro
 * isn't worth it here, so we just write three short functions that share
 * the same structure. Keeping them flat makes them easy to explain in the
 * presentation.
 */

/*
 * euclidean_distance — L2 norm: sqrt( sum (a[i] - b[i])^2 ).
 *
 * Each component's contribution is the square of its difference. We
 * accumulate as a double for precision and only cast to float at the end.
 */
float euclidean_distance(const FeatureVector *a, const FeatureVector *b) {
    double sum = 0.0;

    /* Histogram: 512 bins, each in [0, 1] */
    for (int i = 0; i < HIST_TOTAL_BINS; i++) {
        double d = (double)a->histogram[i] - (double)b->histogram[i];
        sum += d * d;
    }

    /* Per-channel mean: 3 floats */
    for (int i = 0; i < 3; i++) {
        double d = (double)a->mean[i] - (double)b->mean[i];
        sum += d * d;
    }

    /* Per-channel variance: 3 floats */
    for (int i = 0; i < 3; i++) {
        double d = (double)a->variance[i] - (double)b->variance[i];
        sum += d * d;
    }

    /* 4×4 block-mean grid: 16 floats */
    for (int i = 0; i < BLOCK_FEATURES; i++) {
        double d = (double)a->block_means[i] - (double)b->block_means[i];
        sum += d * d;
    }

    return (float)sqrt(sum);
}

/*
 * manhattan_distance — L1 norm: sum( |a[i] - b[i]| ).
 *
 * No squaring, no square root — just absolute differences added up. This
 * is the cheapest of the three metrics and is sometimes called "taxicab"
 * distance because it measures distance the way a taxi has to drive
 * along city blocks rather than in a straight line.
 */
float manhattan_distance(const FeatureVector *a, const FeatureVector *b) {
    double sum = 0.0;

    for (int i = 0; i < HIST_TOTAL_BINS; i++) {
        sum += fabs((double)a->histogram[i] - (double)b->histogram[i]);
    }
    for (int i = 0; i < 3; i++) {
        sum += fabs((double)a->mean[i] - (double)b->mean[i]);
    }
    for (int i = 0; i < 3; i++) {
        sum += fabs((double)a->variance[i] - (double)b->variance[i]);
    }
    for (int i = 0; i < BLOCK_FEATURES; i++) {
        sum += fabs((double)a->block_means[i] - (double)b->block_means[i]);
    }

    return (float)sum;
}

/*
 * cosine_distance — 1 - cosine_similarity.
 *
 * cosine_similarity = (a . b) / (||a|| * ||b||)
 *
 * Where:
 *   a . b      = sum of element-wise products (the dot product)
 *   ||a||      = sqrt( sum of a[i] * a[i] )         (the L2 magnitude)
 *
 * Returning `1 - cos_sim` makes the result behave like a distance:
 *   - 0.0 → vectors point in the exact same direction (most similar)
 *   - 1.0 → orthogonal vectors
 *   - 2.0 → vectors point in exactly opposite directions
 *
 * If either vector has zero magnitude (an all-zero feature vector, which
 * can happen for a corrupt / unreadable image), we return 1.0 so the
 * caller still gets a valid number to rank with.
 */
float cosine_distance(const FeatureVector *a, const FeatureVector *b) {
    double dot = 0.0;       /* sum of a[i] * b[i] */
    double norm_a = 0.0;    /* sum of a[i] * a[i] */
    double norm_b = 0.0;    /* sum of b[i] * b[i] */

    /* Walk the four feature groups in one pass each, accumulating all
     * three running sums. This keeps us at one pass per group like the
     * other metrics. */
    for (int i = 0; i < HIST_TOTAL_BINS; i++) {
        double x = (double)a->histogram[i];
        double y = (double)b->histogram[i];
        dot    += x * y;
        norm_a += x * x;
        norm_b += y * y;
    }
    for (int i = 0; i < 3; i++) {
        double x = (double)a->mean[i];
        double y = (double)b->mean[i];
        dot    += x * y;
        norm_a += x * x;
        norm_b += y * y;
    }
    for (int i = 0; i < 3; i++) {
        double x = (double)a->variance[i];
        double y = (double)b->variance[i];
        dot    += x * y;
        norm_a += x * x;
        norm_b += y * y;
    }
    for (int i = 0; i < BLOCK_FEATURES; i++) {
        double x = (double)a->block_means[i];
        double y = (double)b->block_means[i];
        dot    += x * y;
        norm_a += x * x;
        norm_b += y * y;
    }

    /* Guard against divide-by-zero: an all-zero vector has no direction
     * to compare. Return 1.0 (= maximally far apart) in that case. */
    if (norm_a <= 0.0 || norm_b <= 0.0) {
        return 1.0f;
    }

    double cos_sim = dot / (sqrt(norm_a) * sqrt(norm_b));

    /* Floating-point rounding can push the result slightly outside
     * [-1, 1]; clamp before subtracting from 1 so the distance stays
     * within [0, 2]. */
    if (cos_sim > 1.0)  cos_sim = 1.0;
    if (cos_sim < -1.0) cos_sim = -1.0;

    return (float)(1.0 - cos_sim);
}

/*
 * pick_distance_function — string → function pointer lookup.
 *
 * This is the single place where a textual metric name (from the CLI,
 * Python bridge, or React UI) is converted into an actual C function.
 * Everywhere else in the algorithm layer we pass the function pointer
 * around opaquely.
 *
 * NULL or unrecognized names default to Euclidean so old call-sites
 * that don't know about metrics keep working unchanged.
 */
DistanceFunction pick_distance_function(const char *name) {
    if (name == NULL) {
        return euclidean_distance;
    }
    if (strcasecmp(name, "euclidean") == 0) {
        return euclidean_distance;
    }
    if (strcasecmp(name, "manhattan") == 0) {
        return manhattan_distance;
    }
    if (strcasecmp(name, "cosine") == 0) {
        return cosine_distance;
    }
    /* Unknown metric — be permissive and fall back to the default */
    return euclidean_distance;
}
