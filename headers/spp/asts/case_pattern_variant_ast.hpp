#ifndef CASE_PATTERN_VARIANT_AST_HPP
#define CASE_PATTERN_VARIANT_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::CasePatternVariantAst : virtual Ast {
    using Ast::Ast;

protected:
    /**
     * The @c let statement that case-of-patterns are converted to, to introduce the variables created by the pattern.
     */
    std::unique_ptr<LetStatementInitializedAst> m_mapped_let;
};


#endif //CASE_PATTERN_VARIANT_AST_HPP
