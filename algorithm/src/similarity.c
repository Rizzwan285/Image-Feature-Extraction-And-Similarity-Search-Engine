/*implementing distance metrics*/

#include "similarity.h"
#include <math.h>
#include <strings.h>
#include <stddef.h>

/*calculating euclidean distance*/
float euclidean_distance(const FeatureVector *a, const FeatureVector *b) {
    double sum = 0.0;

    for (int i = 0; i < HIST_TOTAL_BINS; i++) {
        double d = (double)a->histogram[i] - (double)b->histogram[i];
        sum += d * d;
    }

    for (int i = 0; i < 3; i++) {
        double d = (double)a->mean[i] - (double)b->mean[i];
        sum += d * d;
    }

    for (int i = 0; i < 3; i++) {
        double d = (double)a->variance[i] - (double)b->variance[i];
        sum += d * d;
    }

    for (int i = 0; i < BLOCK_FEATURES; i++) {
        double d = (double)a->block_means[i] - (double)b->block_means[i];
        sum += d * d;
    }

    return (float)sqrt(sum);
}

/*calculating manhattan distance*/
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

/*calculating cosine distance*/
float cosine_distance(const FeatureVector *a, const FeatureVector *b) {
    double dot = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;
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

    /*guarding against divide-by-zero*/
    if (norm_a <= 0.0 || norm_b <= 0.0) {
        return 1.0f;
    }

    double cos_sim = dot / (sqrt(norm_a) * sqrt(norm_b));

    /*clamping cosine similarity*/
    if (cos_sim > 1.0)  cos_sim = 1.0;
    if (cos_sim < -1.0) cos_sim = -1.0;

    return (float)(1.0 - cos_sim);
}

/*picking distance function from string*/
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
    return euclidean_distance;
}
