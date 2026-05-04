/*
 * similarity.h
 * ------------
 * Header for the similarity / distance module.
 *
 * Once we have a FeatureVector for every image, we need a way to ask
 * "how different are these two vectors?" That's what this module does.
 * Lower distance = images look more alike.
 */

#ifndef SIMILARITY_H
#define SIMILARITY_H

/* We need the FeatureVector type — it lives in img_features.h */
#include "img_features.h"

/*
 * euclidean_distance — computes the straight-line (L2) distance between
 * two feature vectors.
 *
 * Think of each FeatureVector as a point in 534-dimensional space. This
 * function measures the distance between two such points. If the points
 * are on top of each other the distance is 0 (identical images). The
 * further apart they are, the less visually similar the images are.
 *
 * a — feature vector of the first image
 * b — feature vector of the second image
 *
 * Returns a float; lower is more similar. An image compared to itself
 * always returns exactly 0.0.
 */
float euclidean_distance(const FeatureVector *a, const FeatureVector *b);

#endif /* SIMILARITY_H */
