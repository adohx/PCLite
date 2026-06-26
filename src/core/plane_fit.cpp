#include "plane_fit.h"

#include <cmath>

namespace {

// Classic Jacobi eigenvalue algorithm (Numerical Recipes formulation) for a
// 3x3 symmetric matrix. `a` is destroyed; eigenvalues end up on its
// diagonal. `v`'s columns become the corresponding eigenvectors.
void jacobiEigen3x3(double a[3][3], double v[3][3]) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            v[i][j] = (i == j) ? 1.0 : 0.0;

    for (int sweep = 0; sweep < 50; ++sweep) {
        int p = 0, q = 1;
        double maxVal = std::fabs(a[0][1]);
        if (std::fabs(a[0][2]) > maxVal) { maxVal = std::fabs(a[0][2]); p = 0; q = 2; }
        if (std::fabs(a[1][2]) > maxVal) { maxVal = std::fabs(a[1][2]); p = 1; q = 2; }
        if (maxVal < 1e-12) break;

        double app = a[p][p], aqq = a[q][q], apq = a[p][q];
        double theta = (aqq - app) / (2.0 * apq);
        double sign = (theta >= 0.0) ? 1.0 : -1.0;
        double t = sign / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
        double c = 1.0 / std::sqrt(t * t + 1.0);
        double s = t * c;
        double tau = s / (1.0 + c);

        double h = t * apq;
        a[p][p] = app - h;
        a[q][q] = aqq + h;
        a[p][q] = a[q][p] = 0.0;

        for (int i = 0; i < 3; ++i) {
            if (i == p || i == q) continue;
            double aip = a[i][p], aiq = a[i][q];
            a[i][p] = a[p][i] = aip - s * (aiq + tau * aip);
            a[i][q] = a[q][i] = aiq + s * (aip - tau * aiq);
        }

        for (int i = 0; i < 3; ++i) {
            double vip = v[i][p], viq = v[i][q];
            v[i][p] = vip - s * (viq + tau * vip);
            v[i][q] = viq + s * (vip - tau * viq);
        }
    }
}

} // namespace

bool fitPlane(const std::vector<vec3f>& points, vec3f& outCentroid, vec3f& outNormal) {
    if (points.size() < 3) return false;

    double cx = 0, cy = 0, cz = 0;
    for (const vec3f& p : points) { cx += p.x; cy += p.y; cz += p.z; }
    double n = static_cast<double>(points.size());
    cx /= n; cy /= n; cz /= n;

    double cov[3][3] = {};
    for (const vec3f& p : points) {
        double dx = p.x - cx, dy = p.y - cy, dz = p.z - cz;
        cov[0][0] += dx * dx; cov[0][1] += dx * dy; cov[0][2] += dx * dz;
        cov[1][1] += dy * dy; cov[1][2] += dy * dz;
        cov[2][2] += dz * dz;
    }
    cov[1][0] = cov[0][1];
    cov[2][0] = cov[0][2];
    cov[2][1] = cov[1][2];

    double v[3][3];
    jacobiEigen3x3(cov, v);

    int minIdx = 0;
    if (cov[1][1] < cov[minIdx][minIdx]) minIdx = 1;
    if (cov[2][2] < cov[minIdx][minIdx]) minIdx = 2;

    double nx = v[0][minIdx], ny = v[1][minIdx], nz = v[2][minIdx];
    double len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len < 1e-12) return false;

    outCentroid = vec3f{static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz)};
    outNormal   = vec3f{static_cast<float>(nx / len), static_cast<float>(ny / len), static_cast<float>(nz / len)};
    return true;
}
