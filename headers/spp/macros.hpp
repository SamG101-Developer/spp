#pragma once

#include <spp/macros-platforms.hpp>

#define SPP_ATTR_NODISCARD [[nodiscard]]
#define SPP_ATTR_DEPRECATED [[deprecated]]
#define SPP_ATTR_MAYBE_UNUSED [[maybe_unused]]
#define SPP_ATTR_NORETURN [[noreturn]]
#define SPP_ATTR_FALLTHROUGH [[fallthrough]]
#define SPP_ATTR_LIKELY [[likely]]
#define SPP_ATTR_UNLIKELY [[unlikely]]
#define SPP_ATTR_NO_UNIQUE_ADDRESS [[no_unique_address]]
#define SPP_ATTR_ASSUME(x) [[assume(x)]]

#if SPP_COMPILER_MSVC
  #define SPP_ATTR_ALWAYS_INLINE [[msvc::forceinline]]
  #define SPP_ATTR_NOINLINE [[msvc::noinline]]
  #define SPP_ATTR_UNREACHABLE
  #define SPP_ATTR_HOT
  #define SPP_ATTR_COLD
#else
  #define SPP_ATTR_ALWAYS_INLINE [[gnu::always_inline]]
  #define SPP_ATTR_NOINLINE [[gnu::noinline]]
  #define SPP_ATTR_UNREACHABLE [[gnu::unreachable]]
  #define SPP_ATTR_HOT [[gnu::hot]]
  #define SPP_ATTR_COLD [[gnu::cold]]
#endif

// An invariant of the compiler's own state, checked in every build: one that does not hold is an internal compiler
// error ("spp::Ice", "spp.utils.ice"), reported at the assertion with the call stack.
#define SPP_ASSERT(x)                                     \
  do {                                                    \
    if (not(x)) { ::spp::Ice("Assertion failed: " #x); }  \
  } while (0)

#if SPP_DEBUG
#define SPP_LOG(x)                                                                                        \
  do {                                                                                                    \
    std::cerr << "[SPP LOG] " << x << " (file " << __FILE__ << ", line " << __LINE__ << ")" << std::endl; \
  } while (0)
#else
#define SPP_LOG(x) do {} while (0)
#endif

#define SPP_ASSERT_LLVM_TYPE_OPAQUE(llvm_type)                                \
  if (auto st = llvm::dyn_cast<llvm::StructType>(llvm_type); st != nullptr) { \
    SPP_ASSERT(st->isOpaque());                                               \
  }

#define SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(ast_attr, ...)                          \
  if ((ast_attr) == nullptr) {                                                    \
    (ast_attr) = std::remove_cvref_t<decltype(*ast_attr)>::NewEmpty(__VA_ARGS__); \
  }

#define SPP_SET_AST_TO_DEFAULT_SHARED_IF_NULLPTR(ast_attr, ...)                         \
  if ((ast_attr) == nullptr) {                                                          \
    (ast_attr) = std::remove_cvref_t<decltype(*ast_attr)>::NewEmptyShared(__VA_ARGS__); \
  }

#define use_common(_Ast)            \
  use(spp::analyse::scopes, class ScopeManager); \
  use(spp::asts::meta, struct CompilerMetaData); \
  use(spp::codegen, struct LlvmCtx);             \
  use(spp::asts, struct _Ast)

#define use_common_templated(_Ast)  \
  use(spp::analyse::scopes, class ScopeManager); \
  use(spp::asts::meta, struct CompilerMetaData); \
  use(spp::codegen, struct LlvmCtx);             \
  use(spp::asts, (typename T) struct _Ast)

#define SPP_STRING_START auto raw_string = Str()

#define SPP_STRING_APPEND(x) raw_string.append(x != nullptr ? x->ToString() : "")

#define SPP_STRING_APPEND_RAW(x) raw_string.append(x)

// Each element's string, with "j" between them (not after the last).
#define SPP_STRING_EXTEND(x, j)                     \
  for (auto first = true; auto const &y : x) {      \
    if (not first) { raw_string.append(Str(j)); }   \
    first = false;                                  \
    raw_string.append(y ? y->ToString() : "");      \
  }

#define SPP_STRING_END return raw_string

#define SPP_AST_KIND(cls)                                                        \
  SPP_ATTR_NODISCARD auto Kind() const noexcept -> spp::asts::AstKind override { \
    return spp::asts::AstKind::k##cls;                                           \
  }

#define SPP_AST_KEY_FUNCTIONS(cls)                                  \
  SPP_ATTR_NODISCARD auto PosStart() const -> std::size_t override; \
  SPP_ATTR_NODISCARD auto PosEnd() const -> std::size_t override;   \
  SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;    \
  SPP_ATTR_NODISCARD auto ToString() const -> Str override;         \
  SPP_AST_KIND(cls)

#define SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL              \
  auto PosStart() const -> std::size_t override { return 0uz; }  \
  auto PosEnd() const -> std::size_t override { return 0uz; }    \
  auto Clone() const -> Unique<Ast> override { return nullptr; } \
  auto ToString() const -> Str override { return ""; }

// Hide a pointer's value from the optimiser, so that a
// null test on it is actually performed. Needed for "this",
// which the compiler is entitled to assume is never null -
// it both warns about the comparison and deletes it.
#if SPP_COMPILER_GCC || SPP_COMPILER_CLANG
#define SPP_OPAQUE_PTR(p) __asm__ volatile("" : "+r"(p))
#else
#define SPP_OPAQUE_PTR(p) ((void) 0)
#endif

#if SPP_COMPILER_GCC

#define SPP_GCC_VTABLE_FIX_BASE \
  virtual auto _spp_key_function() const -> void;

#define SPP_GCC_VTABLE_FIX \
  auto _spp_key_function() const -> void override;

#define SPP_GCC_VTABLE_FIX_IMPL(Type)             \
  SPP_MOD_BEGIN                                   \
  auto Type::_spp_key_function() const -> void {} \
  SPP_MOD_END

#else
#define SPP_GCC_VTABLE_FIX_BASE
#define SPP_GCC_VTABLE_FIX
#define SPP_GCC_VTABLE_FIX_IMPL(Type)
#endif

#define SPP_RETURN_TYPE_OVERLOAD_HELPER(expr) \
  if (auto pe = expr->To<PostfixExpressionAst>(); pe != nullptr and pe->Op->To<PostfixExpressionOperatorFunctionCallAst>() != nullptr)

#define SPP_DEREF_ALLOW_MOVE_HELPER(expr) \
  if (auto pe = expr->To<PostfixExpressionAst>(); pe != nullptr and pe->Op->To<PostfixExpressionOperatorDerefAst>() != nullptr)

#define SPP_EXP_FUN export
#define SPP_EXP_CMP export inline
#define SPP_EXP_CLS export extern "C++"
#define SPP_EXP_CON export

#define SPP_MOD_BEGIN extern "C++" {
#define SPP_MOD_END }

/**
 * Shortcut to create an empty list of annotations for structs that contain an annotation list. The usual @c {} cannot
 * be used, because it is interpreted as an "empty initializer list".
 */
#define SPP_NO_ANNOTATIONS Vec<Unique<asts::AnnotationAst>>()

#define SPP_LLVM_FUNC_INFO analyse::scopes::ScopeManager *sm, asts::FunctionPrototypeAst const *proto, asts::meta::CompilerMetaData *meta

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
#define SPP_CAT_(a, b) a##b
#define SPP_CAT(a, b)  SPP_CAT_(a, b)

#define SPP_EAT(...)

// ---------------------------------------------------------------------------
// Keyword stripping: struct/class/union X, enum class/struct X  ->  X
// ---------------------------------------------------------------------------
// pass 1: strip the first keyword
#define SPP_S1_struct SPP_KW_DONE
#define SPP_S1_class  SPP_KW_DONE
#define SPP_S1_union  SPP_KW_DONE
#define SPP_S1_enum   /* leaves 'class X' / 'struct X' for pass 2 */

// pass 2: strip the marker or the scoped-enum key
#define SPP_S2_SPP_KW_DONE
#define SPP_S2_class
#define SPP_S2_struct

#define SPP_S1_(x) SPP_S1_##x
#define SPP_S1(x)  SPP_S1_(x)
#define SPP_S2_(x) SPP_S2_##x
#define SPP_S2(x)  SPP_S2_(x)

#define SPP_STRIP_KW(x) SPP_S2(SPP_S1(x))

// ---------------------------------------------------------------------------
// Template detection: 1 if x starts with a (...) group, else 0
// ---------------------------------------------------------------------------
#define SPP_IS_PAREN(x)            SPP_IS_PAREN_(SPP_IS_PAREN_PROBE x)
#define SPP_IS_PAREN_(...)         SPP_IS_PAREN_N(__VA_ARGS__, 0, ~)
#define SPP_IS_PAREN_N(a, n, ...)  n
#define SPP_IS_PAREN_PROBE(...)    ~, 1,

// ---------------------------------------------------------------------------
// Declaration / name builders
// ---------------------------------------------------------------------------
#define SPP_TPL(...) SPP_EXP_CLS template <__VA_ARGS__>

// (params) struct X  ->  template <params> SPP_EXP_CLS struct X
#define SPP_DECL_0(x) SPP_EXP_CLS x
#define SPP_DECL_1(x) SPP_TPL x

// (params) struct X  ->  X
#define SPP_NAME_0(x) SPP_STRIP_KW(x)
#define SPP_NAME_1(x) SPP_STRIP_KW(SPP_EAT x)

#define SPP_DECL(x) SPP_CAT(SPP_DECL_, SPP_IS_PAREN(x))(x)
#define SPP_NAME(x) SPP_CAT(SPP_NAME_, SPP_IS_PAREN(x))(x)

// ---------------------------------------------------------------------------
// use(ns, [ (template-params) ] class-key Name [, extra...])
// ---------------------------------------------------------------------------
#define use(ns, Type, ...)                                      \
  namespace ns { SPP_DECL(Type) __VA_OPT__(, __VA_ARGS__); }    \
  using ns::SPP_NAME(Type)

#define use_alias(ns, _Alias) using ns::_Alias
