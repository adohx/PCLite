include(FetchContent)

# ── nlohmann/json ─────────────────────────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# Expose include path for subprojects that need it
set(NLOHMANN_JSON_INCLUDE_DIR
    "${nlohmann_json_SOURCE_DIR}/include"
    CACHE PATH "nlohmann/json include directory" FORCE)

# ── spdlog ────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.3
    GIT_SHALLOW    TRUE
)
set(SPDLOG_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(SPDLOG_BUILD_TESTS    OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(spdlog)

# ── LASzip ────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    laszip
    GIT_REPOSITORY https://github.com/LASzip/LASzip.git
    GIT_TAG        3.4.4
    GIT_SHALLOW    TRUE
)
set(LASZIP_BUILD_STATIC ON CACHE INTERNAL "")
FetchContent_MakeAvailable(laszip)

# laszip's targets don't export their include dirs; expose the dll/ headers
# (laszip_api.h: laszip_header_struct / laszip_point_struct / laszip_* API,
# implemented directly by the laszip library target, no dlopen involved)
set(LASZIP_INCLUDE_DIR
    "${laszip_SOURCE_DIR}/dll"
    CACHE PATH "LASzip C API include directory" FORCE)

# LASzip's own CMakeLists.txt appends its major version to the target name
# on Windows only (laszip -> laszip3), so the actual library target name is
# platform-dependent; expose the resolved name for subprojects to link against.
set(LASZIP_LIB_NAME laszip)
if(WIN32)
    set(LASZIP_LIB_NAME laszip3)
endif()
set(LASZIP_LIB_NAME "${LASZIP_LIB_NAME}" CACHE STRING "Resolved LASzip CMake target name" FORCE)

# ── glad (OpenGL loader) ──────────────────────────────────────────────────────
# The viewer renders through core-profile OpenGL 3.3 (shaders/VAO/VBO), which
# needs far more entry points than the system GL 1.1 headers declare. Rather
# than pull in glad's own CMake generator (which shells out to a Python/Jinja2
# step at configure time), the generated output (gl:core=3.3, no extensions)
# is vendored directly here, the same way utilities/3rd_party/ThreadPool is.
add_library(glad STATIC "${CMAKE_CURRENT_SOURCE_DIR}/glad/src/gl.c")
target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/glad/include")

# ── Dear ImGui ──────────────────────────────────────────────────────────────
# ImGui ships no CMakeLists.txt of its own, so the library is assembled here
# from its sources plus the SDL2/OpenGL3 backends. The OpenGL3 backend uses
# its own bundled loader internally (self-contained within
# imgui_impl_opengl3.cpp), so it doesn't need to link against glad. Pinned to
# the "docking" branch (not yet merged to master) since MainWindow relies on
# ImGui's dockspace API. NOTE: ocornut periodically rebases "docking" onto
# master, which rewrites its history and can make a previously-pinned commit
# unreachable (build fails with "fatal: reference is not a tree"). If that
# happens, re-pin to the branch's current tip:
#   git ls-remote https://github.com/ocornut/imgui.git docking
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        5098ce161db7f3919630b2b575347fc3b2ce5c62
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

find_package(OpenGL REQUIRED)
find_package(SDL2   REQUIRED)

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")
target_include_directories(imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends")
target_link_libraries(imgui PUBLIC OpenGL::GL SDL2::SDL2)
