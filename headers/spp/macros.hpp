#pragma once

constexpr auto SPP_VERSION = "0.1.0";

#define SPP_ATTR_NODISCARD [[nodiscard]]

#define SPP_ATTR_DEPRECATED [[deprecated]]

#define SPP_ATTR_MAYBE_UNUSED [[maybe_unused]]

#define SPP_ATTR_NORETURN [[noreturn]]

#define SPP_ATTR_FALLTHROUGH [[fallthrough]]

#define SPP_ATTR_LIKELY [[likely]]

#define SPP_ATTR_UNLIKELY [[unlikely]]

#define SPP_ATTR_NO_UNIQUE_ADDRESS [[no_unique_address]]

#define SPP_ATTR_ASSUME(x) [[assume(x)]]

#define SPP_ATTR_ALWAYS_INLINE [[gnu::always_inline]]

#define SPP_ATTR_NOINLINE [[gnu::noinline]]

#define SPP_ATTR_UNREACHABLE [[gnu::unreachable]]

#define SPP_ATTR_HOT [[gnu::hot]]

#define SPP_ATTR_COLD [[gnu::cold]]

#define SPP_IS_DEBUG_BUILD (defined(_DEBUG) || !defined(NDEBUG))

#ifndef NDEBUG
#define SPP_ASSERT(x)                                                                                         \
        do {                                                                                                      \
            if (!(x)) {                                                                                           \
                std::cerr << "Assertion failed: " #x ", file " << __FILE__ << ", line " << __LINE__ << std::endl; \
                std::abort();                                                                                     \
            }                                                                                                     \
        } while (0)
#else
#define SPP_ASSERT(x) do {} while (0)
#endif

#ifndef NDEBUG
#define SPP_LOG(x)                                                                                            \
        do {                                                                                                      \
            std::cerr << "[SPP LOG] " << x << " (file " << __FILE__ << ", line " << __LINE__ << ")" << std::endl; \
        } while (0)
#else
#define SPP_LOG(x) do {} while (0)
#endif

#define SPP_ASSERT_LLVM_TYPE_OPAQUE(llvm_type)                                  \
    if (auto st = llvm::dyn_cast<llvm::StructType>(llvm_type); st != nullptr) { \
        SPP_ASSERT(st->isOpaque());                                             \
    }

#define SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(ast_attr, ...)                              \
    if ((ast_attr) == nullptr) {                                                        \
        (ast_attr) = std::remove_cvref_t<decltype(*ast_attr)>::NewEmpty(__VA_ARGS__); \
    }

#define SPP_SET_AST_TO_DEFAULT_SHARED_IF_NULLPTR(ast_attr, ...)                             \
    if ((ast_attr) == nullptr) {                                                              \
        (ast_attr) = std::remove_cvref_t<decltype(*ast_attr)>::NewEmptyShared(__VA_ARGS__); \
    }

#define SPP_STRING_START auto raw_string = Str()

#define SPP_STRING_APPEND(x) raw_string.append(x != nullptr ? x->ToString() : "")

#define SPP_STRING_EXTEND(x, j) raw_string.append(std::ranges::to<Str>(std::views::join_with(std::views::transform(x, [&](auto &&x) { return x != nullptr ? x->ToString() : ""; }), Str(j))))

#define SPP_STRING_END return raw_string

#define SPP_AST_KEY_FUNCTIONS                                         \
    SPP_ATTR_NODISCARD auto PosStart() const -> std::size_t override; \
    SPP_ATTR_NODISCARD auto PosEnd() const -> std::size_t override;   \
    SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;    \
    SPP_ATTR_NODISCARD auto ToString() const -> Str override;

#define SPP_GCC_VTABLE_FIX_BASE \
    virtual auto _spp_key_function() const -> void;

#define SPP_GCC_VTABLE_FIX \
    auto _spp_key_function() const -> void override;

#define SPP_GCC_VTABLE_FIX_IMPL(Type)               \
    SPP_MOD_BEGIN                                   \
    auto Type::_spp_key_function() const -> void {} \
    SPP_MOD_END

#define SPP_RETURN_TYPE_OVERLOAD_HELPER(expr) \
    if (auto pe = expr->To<PostfixExpressionAst>(); pe != nullptr and pe->Op->To<PostfixExpressionOperatorFunctionCallAst>() != nullptr)

#define SPP_DEREF_ALLOW_MOVE_HELPER(expr) \
    if (auto pe = expr->To<PostfixExpressionAst>(); pe != nullptr and pe->Op->To<PostfixExpressionOperatorDerefAst>() != nullptr)

#define SPP_EXP_FUN export
#define SPP_EXP_CMP export inline
#define SPP_EXP_CLS export extern "C++"

#define SPP_MOD_BEGIN extern "C++" {
#define SPP_MOD_END }

/**
 * Shortcut to create an empty list of annotations for structs that contain an annotation list. The usual @c {} cannot
 * be used, because it is interpreted as an "empty initializer list".
 */
#define SPP_NO_ANNOTATIONS Vec<Unique<asts::AnnotationAst>>()

#define SPP_LLVM_FUNC_INFO analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto

#define COMMON_AST_IMPORTS                   \
    namespace spp::analyse::scopes {         \
        SPP_EXP_CLS class ScopeManager;      \
    }                                        \
                                             \
    namespace spp::asts::meta {              \
        SPP_EXP_CLS struct CompilerMetaData; \
    }
