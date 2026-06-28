enable_testing()

set(EXECUTABLE_NAME converter_test)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)
collect_sources(TEST_SRC .)

add_executable(${EXECUTABLE_NAME}
        ${TEST_SRC}
)

target_link_libraries(${EXECUTABLE_NAME}
        GTest::gtest_main
        converter
)
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
        TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/test_data")

include(GoogleTest)
gtest_discover_tests(${EXECUTABLE_NAME})