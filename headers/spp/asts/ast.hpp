#ifndef AST_HPP
#define AST_HPP

#include <spp/asts/meta/ast_printer.hpp>
#include <spp/asts/mixins/compiler_stages.hpp>

#include <memory>


namespace spp::asts {
    struct Ast;

    template <typename T>
    auto ast_cast(Ast *ast) -> T* {
        return dynamic_cast<T*>(ast);
    }

    template <typename T>
    auto ast_cast(Ast const *ast) -> T const* {
        return dynamic_cast<T const*>(ast);
    }

    template <typename T, typename U>
    auto ast_cast(std::unique_ptr<U> &&ast) -> std::unique_ptr<T> {
        return std::unique_ptr<T>(ast_cast<T>(ast.release()));
    }
}

namespace spp::analyse::scopes {
    class Scope;
}


#define SPP_AST_KEY_FUNCTIONS \
    auto pos_start() const -> std::size_t override;\
    auto pos_end() const -> std::size_t override;\
    explicit operator std::string() const override;\
    auto print(meta::AstPrinter &printer) const -> std::string override


/**
 * The AST base class is inherited by all other AST classes, provided base functionality, including formatted printing
 * and end position identification.
 */
struct spp::asts::Ast : mixins::CompilerStages {
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
     * Print an AST using raw-formatting. This does not handle indentation, and prints the AST as a single line.
     * Recursively prints child nodes using their respective "operator UnicodeString()" methods.
     */
    virtual explicit operator std::string() const = 0;

    /**
     * Print an AST using auto-formatting. This handles the indentation for the ASTs, and prints braces correctly.
     * Recursively prints child nodes using their respective print methods.
     * @param[out] printer The printer object that tracks indentation and formatting.
     * @return The formatted string representation of the AST.
     */
    virtual auto print(meta::AstPrinter &printer) const -> std::string = 0;

protected:
    /**
     * Create a new AST (base class for all derived ASTs). This constructor is protected to prevent direct instantiation
     * as an AST should always be a specific type of AST, such as a TokenAst, IdentifierAst, etc.
     */
    explicit Ast();

private:
    /**
     * The context of an AST is used in certain analysis steps. This might be the parent AST, such as a
     * FunctionPrototypeAst etc.
     */
    Ast *m_ctx = nullptr;

    /**
     * The scope of an AST is used when generating top level scopes, to create a simple link between scope and AST.
     */
    analyse::scopes::Scope *m_scope = nullptr;
};


#endif //AST_HPP
