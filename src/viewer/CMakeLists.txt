cmake_minimum_required(VERSION 3.20)
project(viewer)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

collect_sources(VIEWER_SRCS ${CMAKE_CURRENT_SOURCE_DIR} RECURSE)

find_package(OpenGL REQUIRED)
find_package(SDL2   REQUIRED)

add_library(${PROJECT_NAME} ${VIEWER_SRCS})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC core OpenGL::GL OpenGL::GLU SDL2::SDL2)
