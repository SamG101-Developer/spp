module;
#include <spp/macros.hpp>

export module spp.asts.iter_pattern_variant_ast;
import spp.asts.ast;

import std;


SPP_EXP_CLS struct spp::asts::IterPatternVariantAst : virtual Ast {
    using Ast::Ast;

protected:
    /**
     * The @c let statement that iter-of-patterns are converted to, to introduce the variables created by the pattern.
     */
    std::unique_ptr<LetStatementInitializedAst> m_mapped_let;
};
