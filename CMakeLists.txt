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

# main.cpp transitively includes <SDL2/SDL.h> (via application.h ->
# main_window.h), and SDL2 renames main() to SDL_main() on platforms where
# SDL_MAIN_AVAILABLE is set (Windows), expecting SDL2main.lib to provide the
# real entry point. We already provide a plain main() ourselves, so opt out
# of that rename instead of chasing vcpkg's exact SDL2main target name.
target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)

# Lets `cmake --install --prefix <dir>` stage a self-contained release
# package: the executable plus whatever runtime shared libraries it actually
# needs (e.g. SDL2.dll on Windows, wherever vcpkg links it dynamically --
# this is a no-op on platforms/triplets where it's linked statically).
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    install(FILES $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> DESTINATION . OPTIONAL)
endif()
