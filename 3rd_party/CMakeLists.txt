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
