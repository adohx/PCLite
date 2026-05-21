#ifndef PCLITE_FILE_NODE_LOADER_H
#define PCLITE_FILE_NODE_LOADER_H

#include "node_loader.h"
#include <fstream>
#include <string>

class FileNodeLoader : public NodeLoader {
public:
    explicit FileNodeLoader(const std::string& octreePath);

    std::vector<uint8_t> load(const Node& node) override;

private:
    std::ifstream file_;
};

#endif //PCLITE_FILE_NODE_LOADER_H
