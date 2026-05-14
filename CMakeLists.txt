cmake_minimum_required(VERSION 3.20)
project(PCLite)

set(CMAKE_CXX_STANDARD 20)

include(cmake/googletest.cmake)
enable_testing()

include(cmake/utils.cmake)

add_subdirectory(src/utilities)
add_subdirectory(src/core)
add_subdirectory(src/converter)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE converter)

