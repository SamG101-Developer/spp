module;
#include <spp/macros.hpp>

export module spp.abstract:abstract_ast;
import std;


namespace spp {
    SPP_EXP_CLS struct AbstractAst;
}


SPP_EXP_CLS struct spp::AbstractAst {
    /**
     * The context of an AST is used in certain analysis steps. This might be the parent AST, such as a
     * FunctionPrototypeAst etc.
     */
    AbstractAst *m_ctx = nullptr;

    AbstractAst();

    virtual ~AbstractAst();

    /**
     * Print an AST using raw-formatting. This does not handle indentation, and prints the AST as a single line.
     * Recursively prints child nodes using their respective "operator std::string()" methods.
     */
    SPP_ATTR_NODISCARD virtual explicit operator std::string() const = 0;

    SPP_ATTR_NODISCARD auto to_string() const -> std::string {
        return static_cast<std::string>(*this);
    }

    /**
     * Non-constant node casting to a target @T type. This uses @c dynamic_cast to safely cast the AST node to the
     * desired type, returning @c nullptr if the cast is impossible. Supports cross casting to AST mixin types too.
     * @tparam T The target AST type to cast to.
     * @return The cast AST node, or @c nullptr if the cast is not possible.
     */
    template <typename T>
    auto to() -> T* {
        return dynamic_cast<T*>(this);
    }

    /**
     * Constant node casting to a target @T type. This uses @c dynamic_cast to safely cast the AST node to the
     * desired type, returning @c nullptr if the cast is impossible. Supports cross casting to AST mixin types too.
     * @tparam T The target AST type to cast to.
     * @return The cast AST node, or @c nullptr if the cast is not possible.
     */
    template <typename T>
    auto to() const -> T const* {
        return dynamic_cast<T const*>(this);
    }

    SPP_ATTR_NODISCARD auto get_ast_ctx() const -> AbstractAst* {
        return m_ctx;
    }

    auto set_ast_ctx(AbstractAst *ctx) -> void {
        m_ctx = ctx;
    }
};
