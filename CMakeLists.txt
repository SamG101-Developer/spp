cmake_minimum_required(VERSION 4.4.0)
# Configure the "import std" module before the project is
# declared.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
project(spp)

# Set globally applied flags for CMake - enabling the std
# module for the version specified above, and setting the
# threading library preference.
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)

# The compiler's own version, single-sourced from the VERSION
# file at the repository root.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SPP_VERSION LIMIT_COUNT 1)
string(STRIP "${SPP_VERSION}" SPP_VERSION)
set_property(
  DIRECTORY APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/headers/spp/version.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/spp/version.hpp"
  @ONLY)

# Build options (consumed by CI in the configure-cmake
# composite action).
option(SPP_WERROR "Treat compiler warnings as errors" ON)
option(SPP_BUILD_TESTS "Build the unit tests" ON)
option(SPP_ALL_TARGETS "Link every LLVM backend" OFF)
option(SPP_STATIC_RUNTIME "Link the C++ runtime and mimalloc statically" OFF)
option(SPP_USE_DEV_RPATH "Bake in the local dev toolchain rpath" ON)
option(SPP_COVERAGE "Instrument for source-based coverage (clang only)" OFF)
set(SPP_SANITIZER "" CACHE STRING "Sanitizers to enable")

include(cmake/Toolchain.cmake)
include(cmake/Runtime.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Warnings.cmake)

# Collect all source files and create the library.
file(GLOB_RECURSE MODULE_FILES CONFIGURE_DEPENDS "headers/*.ixx")
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "sources/*.cpp")
add_library(${PROJECT_NAME}_lib)
spp_target_warnings(${PROJECT_NAME}_lib)

target_sources(
  ${PROJECT_NAME}_lib
  PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_FILES}
  PRIVATE ${SOURCE_FILES})

target_compile_features(
  ${PROJECT_NAME}_lib
  PUBLIC $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:cxx_std_26>)

target_compile_options(
  ${PROJECT_NAME}_lib
  PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>)

target_compile_definitions(
  ${PROJECT_NAME}_lib
  PRIVATE
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=127193
  ANKERL_UNORDERED_DENSE_HAS_SSE2=0
  TOML_EXCEPTIONS=1
  $<$<BOOL:${SPP_ALL_TARGETS}>:SPP_ALL_TARGETS>)

target_include_directories(
  ${PROJECT_NAME}_lib
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/headers
  ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_include_directories(
  ${PROJECT_NAME}_lib
  SYSTEM
  PUBLIC ${LLVM_INCLUDE_DIRS})

target_link_libraries(
  ${PROJECT_NAME}_lib
  PUBLIC
  ${LLVM_LIBS}
  Threads::Threads
  $<$<CXX_COMPILER_ID:GNU>:stdc++exp>
  $<$<NOT:${SPP_NO_MIMALLOC_GENEX}>:${SPP_MIMALLOC_TARGET}>
  unordered_dense::unordered_dense
  tomlplusplus::tomlplusplus
  nlohmann_json::nlohmann_json
  magic_enum::magic_enum
  stringzilla::header
  gch::small_vector
  CLI11::CLI11
  ColEx::ColEx
  GenEx::GenEx
  NumEx::NumEx
)

if (WIN32)
  target_link_libraries(${PROJECT_NAME}_lib PRIVATE pthreads4w::pthreadVC3)
endif ()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

# Unit test support.
if (SPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif ()
