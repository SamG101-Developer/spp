# Build the test executable.
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

# GTest configuration.
find_package(GTest REQUIRED)
target_link_libraries(${PROJECT_NAME}_tests PRIVATE spp_lib GTest::gtest GTest::gtest_main)

# Include test frameworks for discovering tests.
include(CTest)
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests)

# Parallel test execution configuration.
#find_program(GTEST_PARALLEL_EXECUTABLE gtest-parallel HINTS ${CMAKE_CURRENT_SOURCE_DIR}/gtest-parallel)
#if (NOT GTEST_PARALLEL_EXECUTABLE)
#    message(${CMAKE_CURRENT_SOURCE_DIR}/gtest-parallel)
#    message(FATAL_ERROR "gtest-parallel executable not found. Please ensure it is built and available in the specified path.")
#else ()
#    add_test(
#            NAME ${PROJECT_NAME}_tests_parallel
#            COMMAND ${GTEST_PARALLEL_EXECUTABLE} $<TARGET_FILE:${PROJECT_NAME}_tests> --workers=20)
#    message(STATUS "gtest-parallel executable found: ${GTEST_PARALLEL_EXECUTABLE}")
#endif ()