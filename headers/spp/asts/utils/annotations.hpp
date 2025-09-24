#pragma once

#include <cstdint>
#include <map>
#include <string_view>

/// @cond
namespace spp::asts::utils {
    enum class BuiltinAnnotations : std::uint8_t;
}
/// @endcond


enum class spp::asts::utils::BuiltinAnnotations : std::uint8_t {
    /**
     * A virtual method is a method that can be overridden in derived classes. It allows for polymorphism. Any method
     * that is desired to be overridden must be marked either @c @virtualmethod or @c @abstractmethod. A method is
     * defaulted to "final".
     */
    VIRTUAL_METHOD,

    /**
     * An abstract method is a method that must be overridden in derived classes. It cannot have an implementation
     * and must be implemented in derived classes (@c {}). It is a way to enforce that a method is implemented in
     * derived classes.
     */
    ABSTRACT_METHOD,

    /**
     * Mark a function as "ffi" (in a stub.spp file) to indicate that it is a foreign function interface, and will have
     * the default body @c {}. The @c @ffi annotation tells the compiler to check the corresponding dll for the
     * implementation during linking.
     */
    FFI,

    /**
     * A no-impl function si a function that will be implemented but currently doesn't have a body. This is just for
     * easier development prototyping. This will be removed once the compiler has been fully implemented.
     */
    NO_IMPL,

    /**
     * A public AST at the module level is accessible from any module. A public AST at the type level is accessible from
     * anywhere in the code. This is the default visibility for ASTs.
     */
    PUBLIC,

    /**
     * A protected AST at the module level is accessible from the module it is defined in, and any sibling modules, and
     * children modules of the current package. A protected AST at the type level is accessible from the current type
     * and any subclasses.
     */
    PROTECTED,

    /**
     * A private AST at the module level is only accessible from the module it is defined in. A private AST at the type
     * level is only accessible from the current type.
     */
    PRIVATE,

    /**
     * A hot function is a function that is called very often. This maps to the LLVM @c hot attribute, which indicates
     * that the function is likely to be executed. This can be used to optimize generated code.
     */
    HOT,

    /**
     * A cold function is a function that isn't called very often. This maps to the LLVM @c cold attribute, which
     * indicates that the function is unlikely to be executed. This can be used to optimize generated code.
     */
    COLD,

    /**
     * A compiler builtin is an intrinsic that maps to injected LLVM ir that the compiler maintains a map of. This
     * is used to mark functions that are compiler intrinsics, such as @c @abs.
     */
    COMPILER_BUILTIN,

    /**
     * An always-inline function is a function that the compiler will always inline, regardless of the compiler's
     * default choice based on analysis of the function's size and complexity. This maps to the LLVM @c alwaysinline
     * attribute, which is a strong hint to the compiler to inline the function regardless of its size.
     */
    ALWAYS_INLINE,

    /**
     * An inline function is a function that the compiler will try inline if it deems it appropriate, but potentially
     * wouldn't have anyway. This maps to the LLVM @c inline attribute, which is a hint to the compiler to inline
     * the function if it is small enough.
     */
    INLINE,

    /**
     * A never-inline function is a function that the compiler will never inline, regardless of its size or complexity.
     * This maps to the LLVM @c noinline attribute, which is a strong hint to the compiler to not inline the function
     * even if it is small enough. This is useful for functions that are performance-critical and should not be inlined.
     */
    NEVER_INLINE
};
