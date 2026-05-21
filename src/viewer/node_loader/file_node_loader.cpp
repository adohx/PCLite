#include "file_node_loader.h"
#include "node.h"

FileNodeLoader::FileNodeLoader(const std::string& path)
    : file_(path, std::ios::binary) {}

std::vector<uint8_t> FileNodeLoader::load(const Node& node) {
    std::vector<uint8_t> data(node.byteSize());
    file_.seekg((std::streamoff)node.address());
    file_.read(reinterpret_cast<char*>(data.data()), (std::streamsize)node.byteSize());
    return data;
}
