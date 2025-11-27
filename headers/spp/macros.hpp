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

#define SPP_NO_ASAN __attribute__((no_sanitize_address))

#define SPP_IS_DEBUG_BUILD (defined(_DEBUG) || !defined(NDEBUG))

#define SPP_ASSERT(x)                                                                                         \
    do {                                                                                                      \
        if (!(x)) {                                                                                           \
            std::cerr << "Assertion failed: " #x ", file " << __FILE__ << ", line " << __LINE__ << std::endl; \
            std::abort();                                                                                     \
        }                                                                                                     \
    } while (0)


#define SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(ast_attr, ...)                               \
    if ((ast_attr).get() == nullptr) {                                                 \
        (ast_attr) = std::remove_cvref_t<decltype(*ast_attr)>::new_empty(__VA_ARGS__); \
    }

#define SPP_PRINT_START auto formatted_string = std::string()

#define SPP_PRINT_APPEND(x) formatted_string.append(x->print(printer))

#define SPP_PRINT_EXTEND(x) formatted_string.append(x | genex::views::transform([&](auto &&x) { return x->print(printer); }) | genex::views::intersperse(std::string(", ")) | genex::views::join | genex::to<std::string>())

#define SPP_PRINT_END return formatted_string

#define SPP_STRING_START auto raw_string = std::string()

#define SPP_STRING_APPEND(x) raw_string.append(x != nullptr ? static_cast<std::string>(*x) : "")

#define SPP_STRING_EXTEND(x) raw_string.append(x | genex::views::transform([&](auto &&x) { return static_cast<std::string>(*x); }) | genex::views::intersperse(std::string(", ")) | genex::views::join | genex::to<std::string>())

#define SPP_STRING_END return raw_string

#define SPP_AST_KEY_FUNCTIONS                                          \
    auto pos_start() const -> std::size_t override;                    \
    auto pos_end() const -> std::size_t override;                      \
    auto clone() const -> std::unique_ptr<Ast> override;               \
    explicit operator std::string() const override;                    \
    auto print(meta::AstPrinter &printer) const -> std::string override

#define SPP_ENFORCE_EXPRESSION_SUBTYPE(ast)                                                                    \
    if ((ast_cast<TypeAst>(ast) != nullptr) or (ast_cast<TokenAst>(ast) != nullptr)) {                     \
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionTypeInvalidError>().with_args( \
            *ast).with_scopes({sm->current_scope}).raise();                                                \
    }


#define SPP_ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TOKEN(ast)                                                        \
    if (ast_cast<TypeAst>(ast) != nullptr) {                                                               \
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionTypeInvalidError>().with_args( \
            *ast).with_scopes({sm->current_scope}).raise();                                                \
    }


#define SPP_ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TYPE(ast)                                                         \
    if (ast_cast<TokenAst>(ast) != nullptr) {                                                              \
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionTypeInvalidError>().with_args( \
            *ast).with_scopes({sm->current_scope}).raise();                                                \
    }


#define SPP_RETURN_TYPE_OVERLOAD_HELPER(expr) \
    if (auto pe = ast_cast<PostfixExpressionAst>(expr); pe != nullptr and ast_cast<PostfixExpressionOperatorFunctionCallAst>(pe->op.get()) != nullptr)


#define SPP_EXP_CLS export extern "C++"

#define SPP_EXP_FUN export


/**
 * Shortcut to create an empty list of annotations for structs that contain an annotation list. The usual @c {} cannot
 * be used, because it is interpreted as an "empty initializer list".
 */
#define SPP_NO_ANNOTATIONS std::vector<std::unique_ptr<asts::AnnotationAst>>()
