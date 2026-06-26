enable_testing()

set(EXECUTABLE_NAME project_manager_test)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)
collect_sources(TEST_SRC .)

add_executable(${EXECUTABLE_NAME}
        ${TEST_SRC}
)

target_link_libraries(${EXECUTABLE_NAME}
        GTest::gtest_main
        pclite_project
)

include(GoogleTest)
gtest_discover_tests(${EXECUTABLE_NAME})
