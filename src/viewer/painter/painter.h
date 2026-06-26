#ifndef PCLITE_PAINTER_H
#define PCLITE_PAINTER_H

#include "../../core/node.h"
#include "mat.h"
#include "vec3.h"
#include <cstdint>

class Camera;

class Painter {
public:
    virtual ~Painter() = default;

    virtual void addNode(Node* node) = 0;
    virtual void removeNode(Node* node) = 0;
    virtual void paint(const Mat4f& viewMatrix, const Mat4f& projMatrix) = 0;

    // Renders into the pick id buffer instead of color, for GPU picking.
    // Only painters whose geometry is pickable (e.g. NodePainter) need to
    // override this; the default no-op leaves the pick buffer untouched.
    virtual void paintPick(const Mat4f& /*viewMatrix*/, const Mat4f& /*projMatrix*/) {}

    // Resolves a node id written by paintPick() back to the Node it came
    // from. Only the painter that assigned the id can answer; others
    // return nullptr so callers can just ask every painter in turn.
    virtual Node* nodeForId(uint32_t /*id*/) const { return nullptr; }

    // Highlights a single point (by index within node->getPoints()) on
    // subsequent paint() calls; node == nullptr clears it. No-op for
    // painters that don't draw individually-pickable points.
    virtual void setHighlight(Node* /*node*/, int /*pointIndex*/) {}

    // Sets (or clears, if active == false) the locally-fitted plane to show
    // as a pick-assist indicator: a ring centered at `center`, oriented to
    // `normal`. `radius` is the neighborhood radius used for the fit, kept
    // for callers that want it for diagnostics/future use, but
    // PlaneFitRingPainter no longer sizes the ring by it -- the ring is a
    // fixed-screen-size cursor decal, independent of the fit's search
    // radius. No-op for painters that don't draw this indicator.
    virtual void setPlaneFit(bool /*active*/, const vec3f& /*center*/,
                             const vec3f& /*normal*/, float /*radius*/) {}

    // Called once per frame with the up-to-date camera, before paint().
    // Most painters don't need camera state directly (it's baked into the
    // view/proj matrices); this is only for painters that need camera-only
    // data the matrices don't carry, e.g. the orbit target point.
    virtual void syncCamera(const Camera& /*camera*/) {}
};

#endif //PCLITE_PAINTER_H
