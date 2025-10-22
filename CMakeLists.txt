cmake_minimum_required(VERSION 3.31)
project(spp)

# Set the C++ standard and other compiler options.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_COMPILER_LAUNCHER ccache)
set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")

# Find dependencies.
find_package(ColEx REQUIRED) # colored console output
find_package(GenEx REQUIRED) # std::generate based iteration utilities
find_package(absl REQUIRED) # hashing and string utilities
find_package(indicators REQUIRED) # progress bars
find_package(nlohmann_json REQUIRED) # json parsing
find_package(tomlplusplus REQUIRED) # toml parsing
find_package(magic_enum REQUIRED) # enum reflection
find_package(rapidfuzz REQUIRED) # fuzzy string matching
find_package(Boost REQUIRED) # various utilities
find_package(Glob) # file globbing
find_package(CLI11 REQUIRED) # command line parsing
find_package(LLVM REQUIRED CONFIG) # LLVM libraries

# Collect all source files and create the library.
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "sources/*.cpp")
add_library(spp_lib ${SOURCE_FILES})

# Enable all warnings and treat them as errors.
target_compile_options(spp_lib PRIVATE
        -Wall -Wextra -Werror
        -Wcast-align -Wcast-qual -Wconversion -Wsign-conversion -Wsign-promo -Wctor-dtor-privacy -Winit-self
        -Wold-style-cast -Woverloaded-virtual -Wundef -Wdisabled-optimization -Wmissing-declarations -Wmissing-include-dirs
        -Wnull-dereference -Wformat=2 -Wduplicated-cond -Wduplicated-branches -Wredundant-decls -Wlogical-op
        -Wuseless-cast -Wdouble-promotion -Wstrict-null-sentinel -Wswitch-default -Wswitch-enum -Wno-null-dereference
        -Wno-maybe-uninitialized)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(spp_lib PRIVATE
            -O0 -pg -g -fno-omit-frame-pointer -fno-inline -fno-inline-functions-called-once
            -fno-optimize-sibling-calls)
    target_link_options(spp_lib PRIVATE -pg)
endif ()

target_link_libraries(spp_lib PRIVATE
        absl::hash absl::strings nlohmann_json::nlohmann_json magic_enum::magic_enum rapidfuzz::rapidfuzz Boost::boost
        Glob LLVMCore LLVMIRReader LLVMSupport)

target_include_directories(spp_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/headers)
# target_precompile_headers(spp_lib PUBLIC "pch.hpp")

add_executable(spp main.cpp)
target_link_libraries(spp PRIVATE spp_lib)

# In debug mode, add the sanitizers.
#if (CMAKE_BUILD_TYPE STREQUAL "Debug")
#    message(STATUS "Debug mode: Adding AddressSanitizer flags")
#    set(SANITIZER_FLAGS -fsanitize=address,leak -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -O0)
#    target_compile_options(spp_lib PRIVATE ${SANITIZER_FLAGS})
#    target_compile_options(spp PRIVATE ${SANITIZER_FLAGS})
#    target_link_options(spp_lib PRIVATE ${SANITIZER_FLAGS})
#    target_link_options(spp PRIVATE ${SANITIZER_FLAGS})
#endif ()

# Unit test support.
enable_testing()
add_subdirectory(tests)
