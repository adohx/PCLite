cmake_minimum_required(VERSION 3.20)
project(PCLite)

set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    add_compile_definitions(_USE_MATH_DEFINES)
endif()

include(cmake/googletest.cmake)
enable_testing()

include(cmake/utils.cmake)

# Third-party dependencies (fetched automatically)
add_subdirectory(3rd_party)

add_subdirectory(src/utilities)
add_subdirectory(src/core)
add_subdirectory(src/project)
add_subdirectory(src/converter)
add_subdirectory(src/viewer)

# Tests live under Tests/ rather than nested inside each src/<module>/tests;
# added last so the module library targets they link against already exist.
add_subdirectory(Tests)

add_executable(${PROJECT_NAME} main.cpp application.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE converter viewer pclite_project)
target_include_directories(${PROJECT_NAME} PRIVATE src/converter/src src/viewer src/project)
