# Sanitizers, the static runtime and the allocator, applied to
# every target defined after this is included.

if (SPP_SANITIZER)
  add_compile_options(
    -fsanitize=${SPP_SANITIZER}
    -fno-omit-frame-pointer
    -fno-optimize-sibling-calls)
  add_link_options(-fsanitize=${SPP_SANITIZER})
  if (SPP_SANITIZER MATCHES "undefined")
    add_compile_options(-fno-sanitize-recover=undefined)
  endif ()
endif ()

if (SPP_STATIC_RUNTIME AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_link_options(-static-libstdc++ -static-libgcc -Wl,--as-needed)
  # GCC links the sanitizer runtimes dynamically (Clang statically),
  # and ignores the flag of any runtime it isn't linking.
  if (SPP_SANITIZER AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_link_options(
      -static-libasan -static-libubsan -static-liblsan -static-libtsan)
  endif ()
endif ()

# A comma-separated list can't go in $<BOOL:...>, so the generator
# expressions test this instead.
string(COMPARE NOTEQUAL "${SPP_SANITIZER}" "" _spp_sanitized)

# Only allow mimalloc for release, non-sanitizer, non-macOS builds.
set(
  SPP_NO_MIMALLOC_GENEX
  $<OR:$<CONFIG:Debug>,$<BOOL:${_spp_sanitized}>,$<PLATFORM_ID:Darwin>>)
add_compile_definitions($<${SPP_NO_MIMALLOC_GENEX}:SPP_NO_MIMALLOC>)

# Strengthen the debug and asan builds.
add_compile_options(
  $<$<OR:$<CONFIG:Debug>,$<BOOL:${_spp_sanitized}>>:-D_GLIBCXX_ASSERTIONS>)

if (SPP_STATIC_RUNTIME)
  set(SPP_MIMALLOC_TARGET mimalloc-static)
  # The static archive defines new/delete itself (MI_OVERRIDE),
  # except against the Windows DLL CRT.
  if (NOT WIN32)
    add_compile_definitions(SPP_MIMALLOC_STATIC)
  endif ()
else ()
  set(SPP_MIMALLOC_TARGET mimalloc)
endif ()
