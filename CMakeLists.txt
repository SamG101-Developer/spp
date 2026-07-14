cmake_minimum_required(VERSION 4.1.2)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
project(spp)

# Set the C++ standard and other compiler options & flags.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_INSTALL_RPATH "/usr/local/gcc-dev/lib64")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(ANKERL_ENABLE_MODULES ON CACHE BOOL "" FORCE)
set(TOMLPLUSPLUS_BUILD_MODULES ON CACHE BOOL "" FORCE)
set(CLI11_MODULES ON CACHE BOOL "" FORCE)

# Find dependencies.
function(fetch_git_repo name repo tag)
    FetchContent_Declare(${name} GIT_REPOSITORY ${repo} GIT_TAG ${tag} GIT_SHALLOW TRUE)
endfunction()

fetch_git_repo(unordered_dense https://github.com/martinus/unordered_dense.git main)
fetch_git_repo(tomlplusplus https://github.com/marzer/tomlplusplus.git master)
fetch_git_repo(nlohmann_json https://github.com/nlohmann/json.git develop)
fetch_git_repo(stringzilla https://github.com/ashvardanian/stringzilla.git main)
fetch_git_repo(magic_enum https://github.com/Neargye/magic_enum.git master)
fetch_git_repo(CLI11 https://github.com/CLIUtils/CLI11.git main)
fetch_git_repo(ColEx https://github.com/SamG101-Developer/ColEx.git master)
fetch_git_repo(OpEx https://github.com/SamG101-Developer/OpEx.git master)

FetchContent_MakeAvailable(
        unordered_dense tomlplusplus nlohmann_json stringzilla magic_enum CLI11 ColEx OpEx)

find_package(Boost 1.91.0 REQUIRED) # Boost libraries
find_package(LLVM 22.0.0 REQUIRED CONFIG) # LLVM libraries
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

# Collect all source files and create the library.
file(GLOB_RECURSE MODULE_FILES CONFIGURE_DEPENDS "headers/*.ixx")
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "sources/*.cpp")
add_library(${PROJECT_NAME}_lib)

# Enable all warnings and treat them as errors. # -Wswitch-enum
target_compile_options(${PROJECT_NAME}_lib PRIVATE
        -Wall -Wextra -Werror -fmodules -fvisibility=hidden -fvisibility-inlines-hidden
        -Wcast-align -Wcast-qual -Wconversion -Wsign-conversion -Wsign-promo -Wctor-dtor-privacy -Winit-self
        -Wold-style-cast -Wundef -Wdisabled-optimization -Wmissing-declarations -Wmissing-include-dirs
        -Wformat=2 -Wduplicated-cond -Wduplicated-branches -Wredundant-decls -Wlogical-op
        -Wuseless-cast -Wdouble-promotion -Wstrict-null-sentinel -Wswitch-default -Wsuggest-override
        -Wzero-as-null-pointer-constant -Wimplicit-fallthrough=5 -Wstrict-overflow=2 -Wnon-virtual-dtor
        -Wno-maybe-uninitialized -Wno-error=array-bounds -Wno-error=overloaded-virtual -Wno-null-dereference -Wodr)

target_sources(${PROJECT_NAME}_lib PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_FILES} PRIVATE ${SOURCE_FILES})
target_compile_features(${PROJECT_NAME}_lib PRIVATE cxx_std_26)
target_compile_definitions(${PROJECT_NAME}_lib PRIVATE ${LLVM_DEFINITIONS_LIST})
target_include_directories(${PROJECT_NAME}_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/headers)

target_link_libraries(${PROJECT_NAME}_lib PUBLIC
        unordered_dense unordered_dense_module
        nlohmann_json tomlplusplus::tomlplusplus tomlplusplus_modules
        magic_enum Boost::boost CLI11_Module ColEx OpEx
        LLVMCore LLVMSupport LLVMCodeGen LLVMScalarOpts LLVMTransformUtils)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

# Unit test support.
enable_testing()
add_subdirectory(tests)
