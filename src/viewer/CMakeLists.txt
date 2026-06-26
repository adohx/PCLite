cmake_minimum_required(VERSION 3.20)
project(viewer)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

# Collect each source subdirectory individually so tests/ is excluded from the library.
foreach(subdir camera layer node_loader node_management painter window)
    collect_sources(_subdir_srcs ${CMAKE_CURRENT_SOURCE_DIR}/${subdir} RECURSE)
    list(APPEND VIEWER_SRCS ${_subdir_srcs})
endforeach()

find_package(OpenGL REQUIRED)
find_package(SDL2   REQUIRED)

add_library(${PROJECT_NAME} ${VIEWER_SRCS})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${NLOHMANN_JSON_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC
    core OpenGL::GL SDL2::SDL2 spdlog::spdlog imgui glad)

add_subdirectory(tests)
