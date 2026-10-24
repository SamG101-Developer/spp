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
