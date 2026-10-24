# Compiler, archiver and linker set-up, applied to every target.

# /Zi routes debug info through one shared mspdbsrv; /Z7
# puts it in the object files instead.
set(
  CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
  "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

# /EHsc is explicit because CI's -DCMAKE_CXX_FLAGS replaces
# CMake's MSVC default flags, which is where it came from.
if (MSVC)
  add_compile_options(/Zc:preprocessor /EHsc)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
else ()
  add_compile_options(-fvisibility-inlines-hidden)
endif ()

# The LTO-aware binutils wrappers only exist for GCC; clang
# and MSVC runners have no plain gcc-ar, and archiving fails
# if they are forced.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_AR gcc-ar)
  set(CMAKE_RANLIB gcc-ranlib)
  set(CMAKE_NM gcc-nm)
endif ()

# Local development builds against a custom GCC binary at
# "/usr/local/gcc-dev"; CI passes "-DSPP_USE_DEV_RPATH=OFF"
# so it doesn't bake in a path that isn't on the runner.
if (SPP_USE_DEV_RPATH)
  set(CMAKE_INSTALL_RPATH "/usr/local/gcc-dev/lib64;/usr/local/lib")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif ()

if (NOT APPLE)
  set(CMAKE_LINKER_TYPE LLD)
endif ()
