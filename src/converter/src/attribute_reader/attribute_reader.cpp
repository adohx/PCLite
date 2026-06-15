//
// Created by cj on 2026-05-13.
//

#include "attribute_reader.h"

#include <algorithm>
#include <filesystem>

#include "las_reader.h"

AttributeReader::AttributeReader(std::string path) : path_(std::move(path)) {}

AttributeReader::~AttributeReader() = default;

std::shared_ptr<AttributeReader> AttributeReader::createReader(const std::string &path) {
    auto ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                    [](unsigned char c) { return std::tolower(c); });

    if (ext == ".las" || ext == ".laz") {
        return std::make_shared<LasReader>(path);
    }

    if (ext == ".pcd") {
        // TODO: PcdReader is not implemented yet.
        return nullptr;
    }

    return nullptr;
}

Attributes &AttributeReader::getAttributes() {
    return attributes_;
}
