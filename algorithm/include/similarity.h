/*header for distance metrics*/
#ifndef SIMILARITY_H
#define SIMILARITY_H

#include "img_features.h"

/*defining distance function signature*/
typedef float (*DistanceFunction)(const FeatureVector *, const FeatureVector *);

/*declaring euclidean distance function*/
float euclidean_distance(const FeatureVector *a, const FeatureVector *b);

/*declaring manhattan distance function*/
float manhattan_distance(const FeatureVector *a, const FeatureVector *b);

/*declaring cosine distance function*/
float cosine_distance(const FeatureVector *a, const FeatureVector *b);

/*declaring function to pick distance metric*/
DistanceFunction pick_distance_function(const char *name);

#endif /* SIMILARITY_H */
