cmake_minimum_required(VERSION 3.31)
project(spp)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_COMPILER_LAUNCHER ccache)
set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")

# Add the "pg" option to enable profile-guided optimization in debug builds
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -pg -g -fno-omit-frame-pointer -fno-inline -fno-inline-functions-called-once -fno-optimize-sibling-calls")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
endif ()

find_package(ColEx REQUIRED)
find_package(GenEx REQUIRED)

find_package(indicators REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(magic_enum REQUIRED)
find_package(rapidfuzz REQUIRED)
find_package(tomlplusplus REQUIRED)
find_package(tsl-robin-map REQUIRED)
find_package(Boost REQUIRED)
find_package(Glob)
find_package(CLI11 REQUIRED)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "sources/*.cpp")

add_library(spp_lib ${SOURCE_FILES})
target_compile_options(spp_lib PRIVATE
        -Wall -Wextra -Werror
        -Wcast-align -Wcast-qual -Wconversion -Wsign-conversion -Wsign-promo -Wctor-dtor-privacy -Winit-self
        -Wold-style-cast -Woverloaded-virtual -Wundef -Wdisabled-optimization -Wmissing-declarations -Wmissing-include-dirs
        -Wnull-dereference -Wformat=2 -Wduplicated-cond -Wduplicated-branches -Wredundant-decls -Wlogical-op
        -Wuseless-cast -Wdouble-promotion -Wstrict-null-sentinel -Wswitch-default -Wswitch-enum -Wswitch-default
        -Wno-null-dereference -Wno-maybe-uninitialized)
#  -Wno-nonnull-compare -Wno-unused)
# Deliberately don't have -Wshadow (pain to re-do all AST constructors)
# Deliberately don't have -Wstrict-overflow=5 (errors in linked libraries)

target_link_libraries(spp_lib PRIVATE ColEx::ColExLib GenEx::GenExLib)
target_link_libraries(spp_lib PRIVATE
        nlohmann_json::nlohmann_json tsl::robin_map magic_enum::magic_enum rapidfuzz::rapidfuzz Boost::boost Glob)

target_include_directories(spp_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/headers)
# target_precompile_headers(spp_lib PUBLIC "pch.hpp")

add_executable(spp main.cpp)
target_link_libraries(spp PRIVATE spp_lib)

# In debug mode, add address sanitizer
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug mode: Adding AddressSanitizer flags")
    set(SANITIZER_FLAGS -fsanitize=address,leak -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -O0)
    target_compile_options(spp_lib PRIVATE ${SANITIZER_FLAGS})
    target_compile_options(spp PRIVATE ${SANITIZER_FLAGS})
    target_link_options(spp_lib PRIVATE ${SANITIZER_FLAGS})
    target_link_options(spp PRIVATE ${SANITIZER_FLAGS})
endif()

enable_testing()
add_subdirectory(tests)
