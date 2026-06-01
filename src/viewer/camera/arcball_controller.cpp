#include "arcball_controller.h"
#include <algorithm>
#include <cmath>

static constexpr int kLeftButton  = 1;
static constexpr int kRightButton = 3;

// ── helpers ──────────────────────────────────────────────────────────────────

static double vecLen(vec3d v) {
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}
static vec3d vecNorm(vec3d v) {
    double len = vecLen(v);
    if (len < 1e-10) return {0.0, 0.0, 1.0};
    return {v.x/len, v.y/len, v.z/len};
}
static double dot(vec3d a, vec3d b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
static vec3d cross(vec3d a, vec3d b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

// Rodrigues rotation: rotate v around unit axis k by angle (radians)
static vec3d rodrigues(vec3d v, vec3d k, double rad) {
    double c = std::cos(rad), s = std::sin(rad);
    double d = k.x*v.x + k.y*v.y + k.z*v.z;
    return {
        v.x*c + (k.y*v.z - k.z*v.y)*s + k.x*d*(1.0-c),
        v.y*c + (k.z*v.x - k.x*v.z)*s + k.y*d*(1.0-c),
        v.z*c + (k.x*v.y - k.y*v.x)*s + k.z*d*(1.0-c),
    };
}

// ── Shoemake sphere projection ────────────────────────────────────────────────
//
// Maps a screen pixel (x, y) onto the surface of a virtual unit sphere whose
// equator just touches the shorter viewport edge.
//
//  • Inside the unit circle  →  lift onto sphere: z = √(1 − x² − y²)
//  • Outside the unit circle →  project to equatorial hyperbola: z = 0, normalise xy
//
vec3d ArcballController::spherePoint(float x, float y) const {
    float r  = (float)std::min(width_, height_);
    float nx =  (2.f * x - (float)width_)  / r;
    float ny = -(2.f * y - (float)height_) / r;  // flip Y: screen-down → -Y

    float d2 = nx*nx + ny*ny;
    if (d2 <= 1.f)
        return {(double)nx, (double)ny, std::sqrt(1.0 - (double)d2)};

    float len = std::sqrt(d2);
    return {(double)(nx/len), (double)(ny/len), 0.0};
}

// ── CameraController interface ────────────────────────────────────────────────

void ArcballController::onMouseButtonDown(int button, float x, float y) {
    if (button == kLeftButton) {
        prevDragX_ = x;
        prevDragY_ = y;
    }
}

void ArcballController::onMouseDrag(int button, float dx, float dy) {
    if (button == kLeftButton) {
        // Current camera basis in world space
        vec3d arm_dir = vecNorm(arm_);
        vec3d fwd     = {-arm_dir.x, -arm_dir.y, -arm_dir.z};
        vec3d rgt     = vecNorm(cross(fwd, up_));
        vec3d up_cam  = vecNorm(cross(rgt, fwd));

        // Sphere points are in screen space (rgt=+X, up_cam=+Y, arm_dir=+Z).
        // Transform to world space so the rotation axis is correct regardless
        // of where the camera currently is.  Without this, the axis is computed
        // in screen space but applied to world-space vectors, causing the
        // rotation direction to diverge from the mouse after any non-trivial orbit.
        auto toWorld = [&](vec3d p) -> vec3d {
            return {
                p.x*rgt.x + p.y*up_cam.x + p.z*arm_dir.x,
                p.x*rgt.y + p.y*up_cam.y + p.z*arm_dir.y,
                p.x*rgt.z + p.y*up_cam.z + p.z*arm_dir.z,
            };
        };

        vec3d p1 = toWorld(spherePoint(prevDragX_,      prevDragY_));
        vec3d p2 = toWorld(spherePoint(prevDragX_ + dx, prevDragY_ + dy));

        prevDragX_ += dx;
        prevDragY_ += dy;

        vec3d axis = cross(p1, p2);
        if (vecLen(axis) < 1e-10) return;

        double angle = std::acos(std::clamp(dot(p1, p2), -1.0, 1.0));
        vec3d  k     = vecNorm(axis);

        arm_ = rodrigues(arm_, k, -angle);
        up_  = rodrigues(up_,  k, -angle);
        return;
    }
    if (button == kRightButton) applyPan(dx, dy);
}

void ArcballController::onScroll(float delta) {
    double dist    = vecLen(arm_);
    double newDist = dist * (delta > 0.f ? 0.85 : 1.18);
    newDist = std::clamp(newDist, 0.5, 5000.0);
    arm_ = vecNorm(arm_);
    arm_ = {arm_.x * newDist, arm_.y * newDist, arm_.z * newDist};
    // up_ is perpendicular to arm_ — scroll doesn't change orientation
}

void ArcballController::onResize(int w, int h) {
    width_  = w;
    height_ = h;
}

void ArcballController::syncFromCamera(const Camera& cam) {
    fov_    = cam.fov();
    target_ = cam.target();
    auto pos = cam.position();
    arm_ = {pos.x - target_.x, pos.y - target_.y, pos.z - target_.z};
    up_  = cam.up();
}

void ArcballController::applyToCamera(Camera& cam) const {
    cam.setTarget(target_);
    cam.setPosition({target_.x + arm_.x, target_.y + arm_.y, target_.z + arm_.z});
    cam.setUp(up_);
}

void ArcballController::applyPan(float dx, float dy) {
    // Derive camera basis from tracked arm_ and up_ (no world-Y assumption)
    vec3d fwd = vecNorm({-arm_.x, -arm_.y, -arm_.z});  // camera → target
    vec3d rgt = vecNorm(cross(fwd, up_));
    vec3d up  = cross(rgt, fwd);  // re-orthogonalised up

    float fovRad = fov_ * (float)M_PI / 180.f;
    float scale  = 2.f * (float)vecLen(arm_) * std::tan(fovRad * 0.5f) / (float)height_;

    target_.x += (double)(-dx * scale * rgt.x + dy * scale * up.x);
    target_.y += (double)(-dx * scale * rgt.y + dy * scale * up.y);
    target_.z += (double)(-dx * scale * rgt.z + dy * scale * up.z);
}
