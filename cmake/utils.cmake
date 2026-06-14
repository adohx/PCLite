# collect_sources(out_var dir [RECURSE])
#
# Collects all .cpp and .h files under `dir` and stores their absolute paths
# in `out_var`.  Pass RECURSE to search subdirectories as well.
#
# Usage:
#   collect_sources(MY_SRCS ${CMAKE_CURRENT_SOURCE_DIR})
#   collect_sources(MY_SRCS ${CMAKE_CURRENT_SOURCE_DIR} RECURSE)
#   add_library(mylib ${MY_SRCS})
function(collect_sources out_var dir)
    cmake_parse_arguments(ARG "RECURSE" "" "" ${ARGN})

    if(ARG_RECURSE)
        file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
            "${dir}/*.cpp"
            "${dir}/*.h"
        )
    else()
        file(GLOB _sources CONFIGURE_DEPENDS
            "${dir}/*.cpp"
            "${dir}/*.h"
        )
    endif()

    set(${out_var} ${_sources} PARENT_SCOPE)
endfunction()