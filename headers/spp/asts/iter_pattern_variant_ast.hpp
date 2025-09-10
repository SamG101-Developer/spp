#pragma once
#include <spp/asts/ast.hpp>


struct spp::asts::IterPatternVariantAst : virtual Ast {
    using Ast::Ast;

protected:
    /**
     * The @c let statement that iter-of-patterns are converted to, to introduce the variables created by the pattern.
     */
    std::unique_ptr<LetStatementInitializedAst> m_mapped_let;
};
