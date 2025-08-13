#ifndef CASE_PATTERN_VARIANT_AST_HPP
#define CASE_PATTERN_VARIANT_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * Base class for all different case pattern variants. This is used to store different variants in the same list, and to
 * provide the conversion binding for creating variables defined in patterns.
 */
struct spp::asts::CasePatternVariantAst : virtual Ast {
    using Ast::Ast;

protected:
    /**
     * The @c let statement that case-of-patterns are converted to, to introduce the variables created by the pattern.
     */
    std::unique_ptr<LetStatementInitializedAst> m_mapped_let;

public:
    virtual auto convert_to_variable(mixins::CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst>;
};


#endif //CASE_PATTERN_VARIANT_AST_HPP
