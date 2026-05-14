cmake_minimum_required(VERSION 3.20)
project(core)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

collect_sources(CORE_SRCS ${CMAKE_CURRENT_SOURCE_DIR})

add_library(${PROJECT_NAME} ${CORE_SRCS})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})