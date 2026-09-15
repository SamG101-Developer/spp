#pragma once

#if defined (__APPLE__) && defined (__MACH__)
  #include <TargetConditionals.h>
#endif

#define SPP_PLATFORM_WINDOWS 0
#define SPP_PLATFORM_UNIX    0
#define SPP_PLATFORM_MACOS   0
#define SPP_PLATFORM_ANDROID 0
#define SPP_PLATFORM_IOS     0

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
  #undef SPP_PLATFORM_WINDOWS
  #define SPP_PLATFORM_WINDOWS 1
#elif defined (__ANDROID__)
  #undef SPP_PLATFORM_ANDROID
  #define SPP_PLATFORM_ANDROID 1
#elif defined (__linux__)
  #undef SPP_PLATFORM_UNIX
  #define SPP_PLATFORM_UNIX 1
#elif defined (__APPLE__) && defined (__MACH__) && (TARGET_OS_IPHONE || TARGET_OS_SIMULATOR)
  #undef SPP_PLATFORM_IOS
  #define SPP_PLATFORM_IOS 1
#elif defined (__APPLE__) && defined (__MACH__)
  #undef SPP_PLATFORM_MACOS
  #define SPP_PLATFORM_MACOS 1
#else
  #error "SPP: Unsupported platform"
#endif

#define SPP_COMPILER_GCC   0
#define SPP_COMPILER_CLANG 0
#define SPP_COMPILER_MSVC  0

#if defined(__clang__)
  #undef  SPP_COMPILER_CLANG
  #define SPP_COMPILER_CLANG 1
#elif defined(__GNUC__) && !defined(__clang__)
  #undef  SPP_COMPILER_GCC
  #define SPP_COMPILER_GCC 1
#elif defined(_MSC_VER)
  #undef  SPP_COMPILER_MSVC
  #define SPP_COMPILER_MSVC 1
#else
  #error "SPP: Unsupported compiler"
#endif

// MSVC's debug CRT sets _DEBUG; every other toolchain only
// tells us the opposite, via NDEBUG in release builds.
#define SPP_DEBUG 0

#if defined(_DEBUG) || !defined(NDEBUG)
  #undef  SPP_DEBUG
  #define SPP_DEBUG 1
#endif

#define GCC_BUGZILLA_127193_ALIGNMENT_AND_MODULES
#define GCC_BUGZILLA_127228_STRING_M_CREATE_CHANGE
#define GCC_BUGZILLA_127341_VTABLE_TYPEINFO_MISSING
#define GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT

#define MSVC_DEVCOM_11096133_CONSTRAINT_LEXICAL_EQ \
  /* https://developercommunity.visualstudio.com/t/Out-of-class-method-definition-does-not/11096133 */
