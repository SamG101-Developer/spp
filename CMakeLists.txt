cmake_minimum_required(VERSION 4.4.0)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
project(spp)

# Set the C++ standard and other compiler options & flags.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_AR gcc-ar)
set(CMAKE_RANLIB gcc-ranlib)
set(CMAKE_NM gcc-nm)

# Local dev builds against a custom GCC at /usr/local/gcc-dev; CI passes
# -DSPP_USE_DEV_RPATH=OFF so it doesn't bake in a path that isn't on the runner.
option(
  SPP_USE_DEV_RPATH
  "Bake in the local dev toolchain rpath (/usr/local/gcc-dev/lib64)"
  ON
)
if (SPP_USE_DEV_RPATH)
  set(CMAKE_INSTALL_RPATH "/usr/local/gcc-dev/lib64;/usr/local/lib")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif ()

# Build options (consumed by CI in the configure-cmake composite action).
option(SPP_WERROR "Treat compiler warnings as errors" ON)
option(SPP_BUILD_TESTS "Build the unit tests" ON)
set(SPP_SANITIZER
  ""
  CACHE STRING
  "Sanitizers to enable, e.g. 'address,undefined,leak' or 'thread'"
)

# Apply sanitizer flags to every target defined below.
if (SPP_SANITIZER)
  string(REPLACE "+" "," _SPP_SAN_LIST "${SPP_SANITIZER}")
  add_compile_options(
    -fsanitize=${_SPP_SAN_LIST}
    -fno-omit-frame-pointer
    -fno-optimize-sibling-calls
  )
  add_link_options(-fsanitize=${_SPP_SAN_LIST})
endif ()

set(CMAKE_LINKER_TYPE LLD)

# Find dependencies.
find_package(Boost 1.91.0 REQUIRED)
find_package(LLVM REQUIRED CONFIG)
find_package(Threads REQUIRED)
find_package(mimalloc CONFIG REQUIRED)
find_package(unordered_dense CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(GenEx CONFIG REQUIRED)
find_package(ColEx CONFIG REQUIRED)
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

# Collect all source files and create the library.
file(GLOB_RECURSE MODULE_FILES CONFIGURE_DEPENDS "headers/*.ixx")
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "sources/*.cpp")
add_library(${PROJECT_NAME}_lib)

# Enable all warnings (SPP_WERROR => treating as errors).
# -Wswitch-enum
target_compile_options(
  ${PROJECT_NAME}_lib
  PRIVATE
  -Wall
  -Wextra
  -Wcast-align
  -Wcast-qual
  -Wconversion
  -Wsign-conversion
  -Wsign-promo
  -Wctor-dtor-privacy
  -Winit-self
  -Wold-style-cast
  -Wundef
  -Wdisabled-optimization
  -Wmissing-declarations
  -Wmissing-include-dirs
  -Wformat=2
  -Wduplicated-cond
  -Wduplicated-branches
  -Wredundant-decls
  -Wlogical-op
  -Wuseless-cast
  -Wdouble-promotion
  -Wstrict-null-sentinel
  -Wswitch-default
  -Wsuggest-override
  -Wzero-as-null-pointer-constant
  -Wimplicit-fallthrough=5
  -Wstrict-overflow=2
  -Wnon-virtual-dtor
  -Wunused
  -Wno-maybe-uninitialized
  -Wno-error=array-bounds
  -Wno-error=overloaded-virtual
  -Wno-null-dereference
  -Wno-unknown-warning-option # for clang to ignore gcc-specific flags
  -Wno-reserved-module-identifier
  -Wno-pessimizing-move # for clang in parser
  -Wodr
)

if (SPP_WERROR)
  target_compile_options(${PROJECT_NAME}_lib PRIVATE -Werror)
endif ()

target_sources(
  ${PROJECT_NAME}_lib
  PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_FILES}
  PRIVATE ${SOURCE_FILES}
)

target_compile_features(
  ${PROJECT_NAME}_lib
  PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:cxx_std_26>
)
target_compile_options(
  ${PROJECT_NAME}_lib
  PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>
)
target_compile_definitions(${PROJECT_NAME}_lib PRIVATE ${LLVM_DEFINITIONS_LIST})
target_include_directories(
  ${PROJECT_NAME}_lib
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/headers
)
target_include_directories(
  ${PROJECT_NAME}_lib
  SYSTEM
  PUBLIC ${LLVM_INCLUDE_DIRS}
)

target_link_libraries(
  ${PROJECT_NAME}_lib
  PUBLIC
  Boost::boost
  LLVMCore
  LLVMSupport
  LLVMCodeGen
  LLVMScalarOpts
  LLVMTransformUtils
  Threads::Threads
  mimalloc
  unordered_dense::unordered_dense
  tomlplusplus::tomlplusplus
  nlohmann_json::nlohmann_json
  magic_enum::magic_enum
  CLI11::CLI11
  ColEx::ColEx
  GenEx::GenEx
)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

# Unit test support.
if (SPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)

  # Rebuild test exe when main exe is rebuilt.
  # never run against a stale binary after updating the repo.
  # add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_tests)
endif ()
