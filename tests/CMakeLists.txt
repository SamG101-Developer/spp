# Build the test executable.
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(spp_tests ${TEST_SOURCES})
target_link_libraries(spp_tests PRIVATE spp_lib TestEx::TestEx)


# Include test frameworks for discovering tests.
include(CTest)
include(GoogleTest)
gtest_add_tests(TARGET spp_tests SOURCES ${TEST_SOURCES})
#gtest_discover_tests(spp_tests)
