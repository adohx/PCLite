//
// Created by cj on 2026-05-27.
//

#ifndef PCLITE_POINTNODE_H
#define PCLITE_POINTNODE_H
#include <memory>
#include <vector>

#include "attributes.h"
#include "bounding_box.h"
#include "object.h"

class PCNode :public Object{
public:
    PCNode(const BoundingBoxd& boundingBox,std::vector<uint8_t> data,std::shared_ptr<PCNode> parent=nullptr);
    ~PCNode() override;

private:
    BoundingBoxd boundingBox_;
    std::shared_ptr<PCNode> parent_;
    std::vector<std::shared_ptr<PCNode>> children_;

    static Attributes attributes_;
    std::vector<std::vector<uint8_t>> data_;
};
#endif //PCLITE_POINTNODE_H
