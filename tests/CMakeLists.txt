# Build the test executable.
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

# GTest configuration.
find_package(GTest REQUIRED)
target_link_libraries(${PROJECT_NAME}_tests PRIVATE spp_lib TestEx::TestEx GTest::gtest)

# Include test frameworks for discovering tests.
include(CTest)
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests)
