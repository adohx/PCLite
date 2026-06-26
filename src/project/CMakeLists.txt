cmake_minimum_required(VERSION 3.20)
project(pclite_project)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

collect_sources(PROJECT_MGMT_SRCS ${CMAKE_CURRENT_SOURCE_DIR})

add_library(${PROJECT_NAME} ${PROJECT_MGMT_SRCS})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)

add_subdirectory(tests)
