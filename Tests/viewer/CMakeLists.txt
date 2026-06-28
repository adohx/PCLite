enable_testing()

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

# ── Unit tests (no OpenGL context required) ───────────────────────────────────
collect_sources(UNIT_TEST_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/unit)
add_executable(viewer_unit_test ${UNIT_TEST_SRCS})
target_link_libraries(viewer_unit_test PRIVATE GTest::gtest_main viewer)

# ── Integration tests (require test_data on disk) ─────────────────────────────
collect_sources(INTEGRATION_TEST_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/integration)
add_executable(viewer_integration_test ${INTEGRATION_TEST_SRCS})
target_link_libraries(viewer_integration_test PRIVATE GTest::gtest_main viewer converter)
target_compile_definitions(viewer_integration_test PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/test_data")

include(GoogleTest)
gtest_discover_tests(viewer_unit_test)
gtest_discover_tests(viewer_integration_test)
