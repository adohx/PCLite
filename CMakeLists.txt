cmake_minimum_required(VERSION 3.20)
project(PCLite)

set(CMAKE_CXX_STANDARD 20)

include(cmake/googletest.cmake)
enable_testing()

include(cmake/utils.cmake)

# Third-party dependencies (fetched automatically)
add_subdirectory(3rd_party)

add_subdirectory(src/utilities)
add_subdirectory(src/core)
#add_subdirectory(src/converter)
add_subdirectory(src/viewer)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE viewer)

# Standalone point cloud viewer tool
add_executable(pcl_viewer tools/pcl_viewer.cpp)
target_link_libraries(pcl_viewer PRIVATE viewer)
target_include_directories(pcl_viewer PRIVATE src/viewer)
target_compile_features(pcl_viewer PRIVATE cxx_std_20)
