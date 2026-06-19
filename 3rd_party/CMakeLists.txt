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
# implemented directly by the `laszip` library target, no dlopen involved)
set(LASZIP_INCLUDE_DIR
    "${laszip_SOURCE_DIR}/dll"
    CACHE PATH "LASzip C API include directory" FORCE)

# ── Dear ImGui ──────────────────────────────────────────────────────────────
# ImGui ships no CMakeLists.txt of its own, so the library is assembled here
# from its sources plus the SDL2/OpenGL2 backends (matches the legacy
# fixed-function GL the viewer already uses, so no GL loader is needed for
# ImGui itself; Viewport separately loads the handful of FBO entry points it
# needs to render the scene off-screen). Pinned to the "docking" branch
# (not yet merged to master) since MainWindow relies on ImGui's dockspace API.
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        036bf939b6f8d74ad76bcf926b757c56e68c54ff
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
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp")
target_include_directories(imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends")
target_link_libraries(imgui PUBLIC OpenGL::GL SDL2::SDL2)
