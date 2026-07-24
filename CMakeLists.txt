cmake_minimum_required(VERSION 4.1.2)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
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
if(SPP_USE_DEV_RPATH)
    set(CMAKE_INSTALL_RPATH "/usr/local/gcc-dev/lib64")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# Build options (consumed by CI in the configure-cmake composite action).
option(SPP_WERROR "Treat compiler warnings as errors" ON)
option(SPP_BUILD_TESTS "Build the unit tests" ON)
set(SPP_SANITIZER
    ""
    CACHE STRING
    "Sanitizers to enable, e.g. 'address,undefined,leak' or 'thread'"
)

# Apply sanitizer flags to every target defined below (library, executable, tests).
# Accepts either '+' or ',' as the separator (CI passes e.g. 'address+undefined+leak').
if(SPP_SANITIZER)
    string(REPLACE "+" "," _SPP_SAN_LIST "${SPP_SANITIZER}")
    add_compile_options(
        -fsanitize=${_SPP_SAN_LIST}
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
    )
    add_link_options(-fsanitize=${_SPP_SAN_LIST})
endif()

set(CMAKE_LINKER_TYPE LLD)

#if(NOT SPP_SANITIZER AND CMAKE_BUILD_TYPE STREQUAL "Release")
#    message(STATUS "SPP: link-time optimization (LTO) enabled (fat objects)")
#    add_compile_options(-flto=auto -ffat-lto-objects)
#    add_link_options(-flto=auto  -ffat-lto-objects -Wno-odr -Wno-lto-type-mismatch)
#endif()

# Find dependencies.
find_package(Boost 1.91.0 REQUIRED) # Boost libraries
find_package(LLVM REQUIRED CONFIG) # LLVM libraries
find_package(unordered_dense CONFIG REQUIRED) # unordered_dense library
find_package(tomlplusplus CONFIG REQUIRED) # tomlplusplus library
find_package(nlohmann_json CONFIG REQUIRED) # nlohmann_json library
find_package(magic_enum CONFIG REQUIRED) # magic_enum library
find_package(CLI11 CONFIG REQUIRED) # CLI11 library
find_package(GenEx CONFIG REQUIRED) # GenEx library
find_package(ColEx CONFIG REQUIRED) # ColEx library
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

# Collect all source files and create the library.
file(GLOB_RECURSE MODULE_FILES CONFIGURE_DEPENDS "headers/*.ixx")
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "sources/*.cpp")
add_library(${PROJECT_NAME}_lib)

# Enable all warnings (treating them as errors is gated behind SPP_WERROR, applied below). # -Wswitch-enum
target_compile_options(
    ${PROJECT_NAME}_lib
    PRIVATE
        -Wall
        -Wextra
        #        -fvisibility=hidden
        #        -fvisibility-inlines-hidden
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
        -Wodr
)

if(SPP_WERROR)
    target_compile_options(${PROJECT_NAME}_lib PRIVATE -Werror)
endif()

target_sources(
    ${PROJECT_NAME}_lib
    PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_FILES}
    PRIVATE ${SOURCE_FILES}
)
target_compile_features(${PROJECT_NAME}_lib PRIVATE cxx_std_26)
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
        unordered_dense::unordered_dense
        nlohmann_json::nlohmann_json
        tomlplusplus::tomlplusplus
        magic_enum::magic_enum
        Boost::boost
        CLI11::CLI11
        ColEx::ColEx
        GenEx::GenEx
        LLVMCore
        LLVMSupport
        LLVMCodeGen
        LLVMScalarOpts
        LLVMTransformUtils
)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

# Unit test support.
if(SPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)

    # Ensure the test executable is (re)built whenever the main project is built, so tests
    # never run against a stale binary after updating the repo.
    # add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_tests)
endif()
