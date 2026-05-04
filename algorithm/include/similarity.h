/*
 * similarity.h
 * ------------
 * Header for the similarity / distance module.
 *
 * Once we have a FeatureVector for every image, we need a way to ask
 * "how different are these two vectors?" That's what this module does.
 * Lower distance = images look more alike.
 *
 * --- Pluggable distance metrics (function pointers) ---
 *
 * This module exposes more than one distance function:
 *   - euclidean_distance  (L2 norm, the default)
 *   - manhattan_distance  (L1 norm, sum of absolute differences)
 *   - cosine_distance     (1 - cosine similarity, 0 = same direction)
 *
 * All three have the same signature, so we can store any of them in a
 * single function-pointer variable of type `DistanceFunction` and pass
 * it into the search core. The search core never needs to know which
 * metric was chosen — it just calls the function pointer.
 *
 * This is the C-level demonstration of "runtime algorithm selection":
 * the user picks a metric on the command line (or in the UI) and the
 * search uses the matching function without a single `if` or `switch`
 * inside the hot loop.
 */

#ifndef SIMILARITY_H
#define SIMILARITY_H

/* We need the FeatureVector type — it lives in img_features.h */
#include "img_features.h"

/*
 * DistanceFunction — the common signature that every distance metric obeys.
 *
 * It takes two FeatureVector pointers and returns a float.
 *   - 0.0  means the two images are identical for that metric
 *   - higher values mean the images are more different
 *
 * Storing one of these in a variable lets us swap metrics at runtime:
 *
 *     DistanceFunction metric = euclidean_distance;
 *     float d = metric(&query, &candidate);
 *
 * The function call `metric(...)` is identical to calling the named
 * function directly, except the actual code that runs is decided at
 * runtime, not compile time. That is exactly what a function pointer
 * gives you in C.
 */
typedef float (*DistanceFunction)(const FeatureVector *, const FeatureVector *);

/*
 * euclidean_distance — straight-line (L2) distance between two vectors.
 *
 * sqrt( sum (a[i] - b[i])^2 )
 *
 * Returns a float; 0.0 = identical, higher = more different.
 */
float euclidean_distance(const FeatureVector *a, const FeatureVector *b);

/*
 * manhattan_distance — sum of absolute differences (L1 norm).
 *
 * sum( |a[i] - b[i]| )
 *
 * Cheaper than Euclidean (no sqrt, no multiply) and slightly less
 * sensitive to a single very large component difference. Returns a
 * float; 0.0 = identical, higher = more different.
 */
float manhattan_distance(const FeatureVector *a, const FeatureVector *b);

/*
 * cosine_distance — angle-based distance, computed as 1 - cosine_similarity.
 *
 * cosine_similarity = (a . b) / (||a|| * ||b||)
 *
 * Cosine similarity is 1 when the two vectors point in the exact same
 * direction (regardless of length) and 0 when they're orthogonal. We
 * return `1 - similarity` so the result behaves like a distance:
 *   - 0.0 = vectors point the same way (most similar)
 *   - 1.0 = vectors are orthogonal
 *   - up to 2.0 if the vectors point opposite ways
 *
 * If either vector has zero magnitude we fall back to a max distance
 * of 1.0 so the comparison still produces a valid number.
 */
float cosine_distance(const FeatureVector *a, const FeatureVector *b);

/*
 * pick_distance_function — looks up a metric by name and returns the
 * matching function pointer. Returns euclidean_distance as the default
 * when name is NULL or doesn't match a known metric.
 *
 * Recognized names (case-insensitive):
 *   "euclidean"  → euclidean_distance
 *   "manhattan"  → manhattan_distance
 *   "cosine"     → cosine_distance
 *
 * This is the one place in the C layer that maps a string from the
 * user/Python/UI to an actual function. Everywhere else we just pass
 * the function pointer around.
 */
DistanceFunction pick_distance_function(const char *name);

#endif /* SIMILARITY_H */
