#pragma once
#include <string>

class Application {
public:
    explicit Application(std::string lasPath);
    int run();

private:
    std::string lasPath_;
};
