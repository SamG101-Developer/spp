module;
#include <spp/macros.hpp>

export module spp.asts.ast;
import spp.asts.mixins.compiler_stages;
import spp.codegen.llvm_ctx;
import genex;
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
    friend struct spp::analyse::scopes::ScopeManager;
    friend struct spp::asts::AnnotationAst;
    friend struct spp::asts::ClassAttributeAst;
    friend struct spp::asts::ClassPrototypeAst;
    friend struct spp::asts::CmpStatementAst;
    friend struct spp::asts::CoroutinePrototypeAst;
    friend struct spp::asts::SubroutinePrototypeAst;
    friend struct spp::asts::TypeStatementAst;

protected:
    using AstPrinter = spp::asts::meta::AstPrinter;

    /**
     * The context of an AST is used in certain analysis steps. This might be the parent AST, such as a
     * FunctionPrototypeAst etc.
     */
    Ast *m_ctx = nullptr;

    /**
     * The scope of an AST is used when generating top level scopes, to create a simple link between scope and AST.
     */
    analyse::scopes::Scope *m_scope = nullptr;

    /**
     * Create a new AST (base class for all derived ASTs). This constructor is protected to prevent direct instantiation
     * as an AST should always be a specific type of AST, such as a TokenAst, IdentifierAst, etc.
     */
    explicit Ast() = default;

public:
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
     * The clone operator that deep-copies the AST and all its children ASTs. This is used to create a new AST that is a
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

    virtual auto ankerl_hash() const -> std::size_t;

    template <typename T>
    auto to() -> T* {
        return dynamic_cast<T*>(this);
    }

    template <typename T>
    auto to() const -> T const* {
        return dynamic_cast<T const*>(this);
    }

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto get_ast_scope() const -> analyse::scopes::Scope* {
        return m_scope;
    }

    auto set_ast_scope(analyse::scopes::Scope *scope) -> void {
        m_scope = scope;
    }
};


spp::asts::Ast::~Ast() = default;
