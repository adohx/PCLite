#include "application.h"
#include <cstdio>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: PCLite <input.las>\n");
        return 1;
    }
    return Application(argv[1]).run();
}
