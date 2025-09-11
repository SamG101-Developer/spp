#pragma once
#include <spp/asts/_fwd.hpp>
#include <spp/asts/meta/ast_printer.hpp>
#include <spp/asts/mixins/compiler_stages.hpp>
#include <spp/macros.hpp>
#include <spp/pch.hpp>

#include <genex/views/ptr.hpp>
#include <genex/views/transform.hpp>
#include <genex/views/to.hpp>


// #define ast_clone(ast) ((ast) != nullptr ? ast_cast<std::remove_cvref_t<decltype(*ast)>>((ast)->clone()) : nullptr)
//
// #define ast_clone_vec(asts) (asts) | genex::views::transform([](auto &&x) { return ast_clone(x); }) | genex::views::to<std::vector>()
//
// #define ast_clone_vec_shared(asts) (asts) | genex::views::transform([](auto x) { return x; }) | genex::views::to<std::vector>()

#define SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(ast_attr, ...) \
    if ((ast_attr).get() == nullptr) { \
        (ast_attr) = std::remove_cvref_t<decltype(*ast_attr)>::new_empty(__VA_ARGS__); \
    }


namespace spp::asts {
    struct Ast;

    template <typename T>
    auto ast_clone(std::unique_ptr<T> const &ast) -> std::unique_ptr<std::remove_cvref_t<T>> {
        if (ast == nullptr) { return nullptr; }
        return ast_cast<std::remove_cvref_t<T>>(ast->clone());
    }

    template <typename T>
    auto ast_clone(std::shared_ptr<T> const &ast) -> std::unique_ptr<std::remove_cvref_t<T>> {
        if (ast == nullptr) { return nullptr; }
        return ast_cast<std::remove_cvref_t<T>>(ast->clone());
    }

    template <typename T>
    auto ast_clone(T *ast) -> std::unique_ptr<std::remove_cvref_t<T>> {
        if (ast == nullptr) { return nullptr; }
        return ast_cast<std::remove_cvref_t<T>>(ast->clone());
    }

    template <typename T>
    auto ast_clone_vec(std::vector<std::unique_ptr<T>> const &asts) -> std::vector<std::unique_ptr<T>> {
        return asts
            | genex::views::transform([](auto &&x) { return ast_clone(x); })
            | genex::views::to<std::vector>();
    }

    template <typename T>
    auto ast_clone_vec_shared(std::vector<std::shared_ptr<T>> const &asts) -> std::vector<std::shared_ptr<T>> {
        return asts
            | genex::views::transform([](auto x) { return x; })
            | genex::views::to<std::vector>();
    }

    template <typename T>
    auto ast_cast(Ast *ast) -> T* {
        return dynamic_cast<T*>(ast);
    }

    template <typename T>
    auto ast_cast(Ast &ast) -> T& {
        return dynamic_cast<T&>(ast);
    }

    template <typename T>
    auto ast_cast(Ast const *ast) -> T const* {
        return dynamic_cast<T const*>(ast);
    }

    template <typename T>
    auto ast_cast(Ast const &ast) -> T const& {
        return dynamic_cast<T const&>(ast);
    }

    template <typename T, typename U>
    auto ast_cast(std::unique_ptr<U> &&ast) -> std::unique_ptr<T> {
        return std::unique_ptr<T>(ast_cast<T>(ast.release()));
    }

    template <typename T, typename U>
    auto ast_cast(std::shared_ptr<U> const &ast) -> std::shared_ptr<T> {
        return std::shared_ptr<T>(ast_cast<T>(ast.get()));
    }

    auto ast_name(Ast *ast) -> std::shared_ptr<TypeAst>;

    auto ast_body(Ast *ast) -> std::vector<Ast*>;
}

namespace spp::analyse::scopes {
    class Scope;
}


#define SPP_AST_KEY_FUNCTIONS \
    auto pos_start() const -> std::size_t override; \
    auto pos_end() const -> std::size_t override; \
    auto clone() const -> std::unique_ptr<Ast> override; \
    explicit operator std::string() const override; \
    auto print(meta::AstPrinter &printer) const -> std::string override


/**
 * The AST base class is inherited by all other AST classes, provided base functionality, including formatted printing
 * and end position identification.
 */
struct spp::asts::Ast : mixins::CompilerStages {
    friend struct AnnotationAst;
    friend struct ClassAttributeAst;
    friend struct ClassPrototypeAst;
    friend struct CmpStatementAst;
    friend struct FunctionPrototypeAst;
    friend struct TypeStatementAst;
    friend struct UseStatementAst;
    friend class analyse::scopes::ScopeManager;

protected:
    /**
     * The context of an AST is used in certain analysis steps. This might be the parent AST, such as a
     * FunctionPrototypeAst etc.
     */
    Ast *m_ctx = nullptr;

    /**
     * The scope of an AST is used when generating top level scopes, to create a simple link between scope and AST.
     */
    analyse::scopes::Scope *m_scope = nullptr;

public:
    // /**
    //  * Ensure there is no copy constructor for the Ast class. This is to prevent accidental copies of ASTs, which would
    //  * mutate the tree structure and potentially lead to undefined behavior.
    //  */
    // Ast(Ast const &) = delete;
    //
    // /**
    //  * Ensure there is no move constructor for the Ast class. This is to prevent accidental moves of ASTs, which would
    //  * mutate the tree structure and potentially lead to undefined behavior.
    //  */
    // Ast(Ast &&) noexcept = delete;
    //
    // /**
    //  * Ensure there is no copy assignment operator for the Ast class. This is to prevent accidental copies of ASTs,
    //  * which would mutate the tree structure and potentially lead to undefined behavior.
    //  * @return Cannot be called.
    //  */
    // Ast& operator=(Ast const &) = delete;
    //
    // /**
    //  * Ensure there is no move assignment operator for the Ast class. This is to prevent accidental moves of ASTs, which
    //  * would mutate the tree structure and potentially lead to undefined behavior.
    //  * @return Cannot be called.
    //  */
    // Ast& operator=(Ast &&) noexcept = delete;

    ~Ast() override;

    /**
     * The start position is the first position in the source code that contains this AST. An AST will recursively get
     * the start position of the first field, until a TokenAst is reached.
     * @return The first position this AST encompasses.
     */
    virtual auto pos_start() const -> std::size_t = 0;

    /**
     * The end position is the final position in the source code that contains this AST. An AST will recursively get the
     * end position of the final field, until a TokenAst is reached.
     * @return The final position this AST encompasses.
     */
    virtual auto pos_end() const -> std::size_t = 0;

    /**
     * The size of an AST is the number of tokens it encompasses. This is used to determine the size of the AST in the
     * source code, and is used for error reporting. Calculated by subtracting the start position from the end position.
     * @return The size of the AST in tokens.
     */
    auto size() const -> std::size_t;

    /**
     * The clone operator that deepcopies the AST and all its children ASTs. This is used to create a new AST that is a
     * copy of the original AST, preserving its structure and contents. This is useful for creating a new AST that can
     * be modified without affecting the original AST. Can be cast down as needed, as the return type is a
     * @code std::unique_ptr<T>@endcode to the base @c Ast class.
     * @return The cloned AST as a unique pointer to the base Ast class.
     */
    virtual auto clone() const -> std::unique_ptr<Ast> = 0;

    /**
     * Print an AST using raw-formatting. This does not handle indentation, and prints the AST as a single line.
     * Recursively prints child nodes using their respective "operator std::string()" methods.
     */
    virtual explicit operator std::string() const = 0;

    /**
     * Print an AST using auto-formatting. This handles the indentation for the ASTs, and prints braces correctly.
     * Recursively prints child nodes using their respective print methods.
     * @param[out] printer The printer object that tracks indentation and formatting.
     * @return The formatted string representation of the AST.
     */
    virtual auto print(meta::AstPrinter &printer) const -> std::string = 0;

    auto stage_1_pre_process(Ast *ctx) -> void override;
    auto stage_2_gen_top_level_scopes(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

protected:
    /**
     * Create a new AST (base class for all derived ASTs). This constructor is protected to prevent direct instantiation
     * as an AST should always be a specific type of AST, such as a TokenAst, IdentifierAst, etc.
     */
    explicit Ast();
};
