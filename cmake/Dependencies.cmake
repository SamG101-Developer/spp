# Find every dependency, and map the LLVM components to the
# libraries in LLVM_LIBS.

if (WIN32)
  find_package(ZLIB REQUIRED)
  find_package(zstd CONFIG REQUIRED)
  find_package(LibXml2 CONFIG REQUIRED)
  find_package(pthreads4w CONFIG REQUIRED)
endif ()
find_package(LLVM 23.1.0 CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(mimalloc CONFIG REQUIRED)
find_package(unordered_dense CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(stringzilla CONFIG REQUIRED)
find_package(small_vector CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(GenEx CONFIG REQUIRED)
find_package(ColEx CONFIG REQUIRED)
find_package(NumEx CONFIG REQUIRED)

if (MSVC)
  set_property(
    TARGET ColEx::ColEx GenEx::GenEx NumEx::NumEx
    PROPERTY MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
endif ()

# brew's LLVM (macOS) links LLVMSupport against the zstd dylib
# and libz3, neither of which is part of macOS, so a released
# binary would not run without brew. libzstd.a stands in for
# the one; the other only serves Z3Solver.cpp.o, which nothing
# here pulls out of libLLVMSupport.a.
if (APPLE)
  if (NOT TARGET zstd::libzstd_static)
    message(FATAL_ERROR
      "No static libzstd beside ${zstd_LIBRARY}; macOS links zstd statically.")
  endif ()
  get_target_property(_spp_support_libs LLVMSupport INTERFACE_LINK_LIBRARIES)
  list(TRANSFORM _spp_support_libs REPLACE
    "zstd::libzstd_shared" "zstd::libzstd_static")
  list(FILTER _spp_support_libs EXCLUDE REGEX "libz3[.]")
  set_target_properties(LLVMSupport PROPERTIES
    INTERFACE_LINK_LIBRARIES "${_spp_support_libs}")
endif ()

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_compile_options(${LLVM_DEFINITIONS_LIST})

# Configure LLVM for the correct architecture/system.
if (NOT LLVM_NATIVE_ARCH IN_LIST LLVM_TARGETS_TO_BUILD)
  message(FATAL_ERROR
    "This LLVM (${LLVM_PACKAGE_VERSION}) was built without its own host "
    "backend ${LLVM_NATIVE_ARCH}; it has ${LLVM_TARGETS_TO_BUILD}. "
    "Nothing here can emit an object file for this machine."
  )
endif ()

if (SPP_ALL_TARGETS)
  set(_spp_llvm_targets AllTargetsInfos AllTargetsDescs AllTargetsCodeGens)
else ()
  set(_spp_llvm_targets nativecodegen)
endif ()

llvm_map_components_to_libnames(
  LLVM_LIBS
  core support codegen scalaropts transformutils analysis
  coroutines ipo passes linker target
  ${_spp_llvm_targets})
