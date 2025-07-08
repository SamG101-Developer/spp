#ifndef USE_STATEMENT_AST_HPP
#define USE_STATEMENT_AST_HPP

#include <spp/asts/statement_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The UseStatementAst reduces a fully qualified tpy into the current scope, making the symbol accessible without the
 * associated namespace. It is internal mapped to a TypeStatementAst: @code use std::Str@endcode is equivalent to
 * @code type Str = std::Str@endcode.
 */
struct spp::asts::UseStatementAst final : Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The list of annotations that are applied to this use statement. Typically, access modifiers in this context.
     */
    std::vector<std::unique_ptr<AnnotationAst>> annotations;

    /**
     * The @c use token that starts this statement.
     */
    std::unique_ptr<TokenAst> tok_use;

    /**
     * The old (fully qualified) type that this use statement is reducing. For example, for @code use std::Str@endcode,
     * the fully qualified type is @c std::Str.
     */
    std::unique_ptr<TypeAst> old_type;

private:
    /**
     * The @c m_generated flag indicates whether this use statement has been generated yet. This is required, because
     * @c use statements can be defined at the top level (module/sup) or inside function bodies. If defined inside a
     * function body, all steps of the analysis must be run together, otherwise they are ran in their correct layer.
     */
    bool m_generated = false;

    /**
     * The @c m_conversion is the type statement that is generated from this use statement. It is used to analyse new
     * types in a uniform way with @code type Str = std::Str@endcode.
     */
    std::unique_ptr<TypeStatementAst> m_conversion;
};


#endif //USE_STATEMENT_AST_HPP
