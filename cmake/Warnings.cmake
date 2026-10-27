# Enable all warnings on a target (SPP_WERROR => treating as errors).
function(spp_target_warnings target)
  if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${target} PRIVATE /W4 /external:templates-)
    return()
  endif ()

  target_compile_options(
    ${target}
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wcast-align
    -Wcast-qual
    -Wconversion
    -Wsign-conversion
    -Wsign-promo
    -Wctor-dtor-privacy
    -Winit-self
    -Wold-style-cast
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
    target_compile_options(${target} PRIVATE -Werror)
    if (SPP_SANITIZER)
      target_compile_options(${target} PRIVATE -Wno-error=disabled-optimization)
    endif ()
  endif ()
endfunction()
