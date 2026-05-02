module;
#include <spp/macros.hpp>

export module spp.asts.ast;
import spp.asts.mixins.compiler_stages;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct ClassAttributeAst;
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct CmpStatementAst;
    SPP_EXP_CLS struct CoroutinePrototypeAst;
    SPP_EXP_CLS struct SubroutinePrototypeAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeStatementAst;
}

namespace spp::asts::meta {
    SPP_EXP_CLS class AstPrinter;
}

/**
 * The AST base class is inherited by all other AST classes, provided base functionality, including formatted printing
 * and end position identification.
 */
SPP_EXP_CLS struct spp::asts::Ast : mixins::CompilerStages {
    SPP_GCC_VTABLE_FIX_BASE

    ~Ast() override;

    /**
     * The start position is the first position in the source code that contains this AST. An AST will recursively get
     * the start position of the first field, until a TokenAst is reached.
     * @return The first position this AST encompasses.
     */
    SPP_ATTR_NODISCARD virtual auto PosStart() const -> std::size_t = 0;

    /**
     * The end position is the final position in the source code that contains this AST. An AST will recursively get the
     * end position of the final field, until a TokenAst is reached.
     * @return The final position this AST encompasses.
     */
    SPP_ATTR_NODISCARD virtual auto PosEnd() const -> std::size_t = 0;

    /**
     * The size of an AST is the number of tokens it encompasses. This is used to determine the size of the AST in the
     * source code, and is used for error reporting. Calculated by subtracting the start position from the end position.
     * @return The size of the AST in tokens.
     */
    SPP_ATTR_NODISCARD auto Size() const -> std::size_t;

    /**
     * The clone operator that deep-copies the AST and all its children ASTs. This is used to create a new AST that is a
     * copy of the original AST, preserving its structure and contents. This is useful for creating a new AST that can
     * be modified without affecting the original AST. Can be cast down as needed, as the return type is a
     * @code Unique<T>@endcode to the base @c Ast class.
     * @return The cloned AST as a unique pointer to the base Ast class.
     */
    SPP_ATTR_NODISCARD virtual auto Clone() const -> Unique<Ast> = 0;

    /**
     * Print an AST using raw-formatting. This does not handle indentation, and prints the AST as a single line.
     * Recursively prints child nodes using their respective "to_string()" methods.
     */
    SPP_ATTR_NODISCARD virtual auto ToString() const -> Str = 0;

    /**
     * Overridable hash function for AST nodes, used for hashing ASTs in data structures (particularly in Ankerl's hash
     * map). Implemented in the @c IdentifierAst and @c TypeIdentifierAst nodes.
     * @return The hash value of the AST.
     */
    SPP_ATTR_NODISCARD virtual auto AnkerlHash() const -> std::size_t;

    /**
     * Non-constant node casting to a target @T type. This uses @c dynamic_cast to safely cast the AST node to the
     * desired type, returning @c nullptr if the cast is impossible. Supports cross casting to AST mixin types too.
     * @tparam T The target AST type to cast to.
     * @return The cast AST node, or @c nullptr if the cast is not possible.
     */
    template <typename T>
    auto To() -> T* {
        return dynamic_cast<T*>(this);
    }

    /**
     * Constant node casting to a target @T type. This uses @c dynamic_cast to safely cast the AST node to the
     * desired type, returning @c nullptr if the cast is impossible. Supports cross casting to AST mixin types too.
     * @tparam T The target AST type to cast to.
     * @return The cast AST node, or @c nullptr if the cast is not possible.
     */
    template <typename T>
    auto To() const -> T const* {
        return dynamic_cast<T const*>(this);
    }

    /**
     * Default behaviour: bind the context to this AST, for future analysis stages.
     * @param ctx The context AST.
     */
    auto Stage1_PreProcess(Ast *ctx) -> void override;

    /**
     * Default behaviour: bind the scope to this AST, for future analysis stages.
     * @param sm The scope manager to use for setting the scope of this AST (current scope).
     * @param meta Associated metadata (unused in default implementation).
     */
    auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    SPP_ATTR_NODISCARD auto GetAstCtx() const -> Ast*;

    SPP_ATTR_NODISCARD auto GetAstScope() const -> analyse::scopes::Scope*;

    auto SetAstCtx(Ast *ctx) -> void;

    auto SetAstScope(analyse::scopes::Scope *scope) -> void;

protected:
    /**
     * The context of an AST is used in certain analysis steps. This might be the parent AST, such as a
     * FunctionPrototypeAst etc.
     */
    Ast *_Ctx = nullptr;

    /**
     * The scope of an AST is used when generating top level scopes, to create a simple link between scope and AST.
     */
    analyse::scopes::Scope *_Scope = nullptr;

    /**
     * Create a new AST (base class for all derived ASTs). This constructor is protected to prevent direct instantiation
     * as an AST should always be a specific type of AST, such as a TokenAst, IdentifierAst, etc.
     */
    explicit Ast();
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::Ast)
