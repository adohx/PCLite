cmake_minimum_required(VERSION 3.20)
project(converter)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

collect_sources(CONVERTER_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src RECURSE)

add_library(${PROJECT_NAME} ${CONVERTER_SRCS})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src )
target_include_directories(${PROJECT_NAME} PRIVATE ${LASZIP_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC core utilities nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LASZIP_LIB_NAME})