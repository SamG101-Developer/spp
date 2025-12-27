#pragma once

#define SPP_VERSION "0.1.0"

#define SPP_ATTR_NODISCARD [[nodiscard]]

#define SPP_ATTR_DEPRECATED [[deprecated]]

#define SPP_ATTR_MAYBE_UNUSED [[maybe_unused]]

#define SPP_ATTR_NORETURN [[noreturn]]

#define SPP_ATTR_FALLTHROUGH [[fallthrough]]

#define SPP_ATTR_LIKELY [[likely]]

#define SPP_ATTR_UNLIKELY [[unlikely]]

#define SPP_ATTR_NO_UNIQUE_ADDRESS [[no_unique_address]]

#define SPP_ATTR_ASSUME(x) [[assume(x)]]

#define SPP_ATTR_ALWAYS_INLINE [[gnu::always_inline]] inline

#define SPP_ATTR_NOINLINE [[gnu::noinline]]

#define SPP_ATTR_UNREACHABLE [[gnu::unreachable]]

#define SPP_ATTR_HOT [[gnu::hot]]

#define SPP_ATTR_COLD [[gnu::cold]]

#define SPP_INSTANT_INDIRECT [](auto &&x) -> decltype(auto) { return *x; }

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


#define SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(ast_attr, ...)                               \
    if ((ast_attr).get() == nullptr) {                                                 \
        (ast_attr) = std::remove_cvref_t<decltype(*ast_attr)>::new_empty(__VA_ARGS__); \
    }

#define SPP_PRINT_START auto formatted_string = std::string()

#define SPP_PRINT_APPEND(x) formatted_string.append(x->print(printer))

#define SPP_PRINT_EXTEND(x, j) formatted_string.append(x | genex::views::transform([&](auto &&x) { return x->print(printer); }) | genex::views::intersperse(std::string(j)) | genex::views::join | genex::to<std::string>())

#define SPP_PRINT_END return formatted_string

#define SPP_STRING_START auto raw_string = std::string()

#define SPP_STRING_APPEND(x) raw_string.append(x != nullptr ? static_cast<std::string>(*x) : "")

#define SPP_STRING_EXTEND(x, j) raw_string.append(x | genex::views::transform([&](auto &&x) { return static_cast<std::string>(*x); }) | genex::views::intersperse(std::string(j)) | genex::views::join | genex::to<std::string>())

#define SPP_STRING_END return raw_string

#define SPP_AST_KEY_FUNCTIONS                                          \
    auto pos_start() const -> std::size_t override;                    \
    auto pos_end() const -> std::size_t override;                      \
    auto clone() const -> std::unique_ptr<Ast> override;               \
    explicit operator std::string() const override;                    \
    auto print(AstPrinter &printer) const -> std::string override

#define SPP_ENFORCE_EXPRESSION_SUBTYPE(ast)                                                     \
    if (ast and ((ast->to<TypeAst>() != nullptr) or (ast->to<TokenAst>() != nullptr))) {        \
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionTypeInvalidError>() \
            .with_args(*ast)                                                                    \
            .raises_from(sm->current_scope);                                                    \
    }


#define SPP_ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TOKEN(ast)                                         \
    if (ast and ast->to<TypeAst>() != nullptr) {                                                \
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionTypeInvalidError>() \
            .with_args(*ast)                                                                    \
            .raises_from(sm->current_scope);                                                    \
    }


#define SPP_ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TYPE(ast)                                          \
    if (ast and ast->to<TokenAst>() != nullptr) {                                               \
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionTypeInvalidError>() \
            .with_args(*ast)                                                                    \
            .raises_from(sm->current_scope);                                                    \
    }


#define SPP_RETURN_TYPE_OVERLOAD_HELPER(expr) \
    if (auto pe = expr->to<PostfixExpressionAst>(); pe != nullptr and pe->op->to<PostfixExpressionOperatorFunctionCallAst>() != nullptr)


#define SPP_EXP_CLS export extern "C++"

#define SPP_EXP_ENUM export extern "C++"

#define SPP_EXP_FUN export

#define SPP_EXP_CMP export inline


/**
 * Shortcut to create an empty list of annotations for structs that contain an annotation list. The usual @c {} cannot
 * be used, because it is interpreted as an "empty initializer list".
 */
#define SPP_NO_ANNOTATIONS std::vector<std::unique_ptr<asts::AnnotationAst>>()

#define SPP_LLVM_FUNC_INFO analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto

#define co_yield_from(expr)  \
    for (auto && val : expr) \
        co_yield val;
