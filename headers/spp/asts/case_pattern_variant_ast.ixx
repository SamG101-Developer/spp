module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_ast;
import spp.asts.ast;

import std;


/**
 * Base class for all different case pattern variants. This is used to store different variants in the same list, and to
 * provide the conversion binding for creating variables defined in patterns.
 */
SPP_EXP_CLS struct spp::asts::CasePatternVariantAst : virtual Ast {
    using Ast::Ast;

protected:
    /**
     * The @c let statement that case-of-patterns are converted to, to introduce the variables created by the pattern.
     */
    std::unique_ptr<LetStatementInitializedAst> m_mapped_let;

public:
    virtual auto convert_to_variable(CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst>;
};
