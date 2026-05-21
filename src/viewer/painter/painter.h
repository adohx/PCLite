#ifndef PCLITE_PAINTER_H
#define PCLITE_PAINTER_H

#include "node.h"

class Painter {
public:
    virtual ~Painter() = default;

    virtual void addNode(Node* node) = 0;
    virtual void removeNode(Node* node) = 0;
    virtual void paint() = 0;
};

#endif //PCLITE_PAINTER_H
