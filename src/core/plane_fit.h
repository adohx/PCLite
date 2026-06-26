#ifndef PCLITE_PLANE_FIT_H
#define PCLITE_PLANE_FIT_H

#include "vec3.h"
#include <vector>

// Fits a plane (centroid + unit normal) through `points` via PCA: the
// normal is the eigenvector of the smallest eigenvalue of the points'
// covariance matrix. Returns false for fewer than 3 points or a degenerate
// (zero-length) normal.
bool fitPlane(const std::vector<vec3f>& points, vec3f& outCentroid, vec3f& outNormal);

#endif //PCLITE_PLANE_FIT_H
