# Find and configure GoogleTest.
find_package(GTest REQUIRED)
set(gtest_disable_pthreads on)
set(BUILD_GTEST ON)
set(BUILD_GMOCK OFF)

# Build the test executable.
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(spp_tests ${TEST_SOURCES})
target_link_libraries(spp_tests PRIVATE spp_lib GTest::gtest_main)

# In debug mode, add address sanitizer
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug mode: Adding AddressSanitizer flags")
    target_compile_options(spp_tests PRIVATE -fsanitize=address -fno-omit-frame-pointer -g -O0)
    target_link_options(spp_tests PRIVATE -fsanitize=address)
endif()

# Register the tests.
include(GoogleTest)
gtest_discover_tests(spp_tests)
