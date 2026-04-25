module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantAst;
    SPP_EXP_CLS struct LetStatementInitializedAst;
    SPP_EXP_CLS struct LocalVariableAst;
}


/**
 * Base class for all different case pattern variants. This is used to store different variants in the same list, and to
 * provide the conversion binding for creating variables defined in patterns.
 */
SPP_EXP_CLS struct spp::asts::CasePatternVariantAst : virtual Ast {
    using Ast::Ast;
    /**
     * Handle pattern matching for case expressions (this will be reimplemnted in all the other patterns, but due to a
     * GCC modules bug, it must be defined here too).
     * @param sm The scope manager to use for comptime generation.
     * @param meta Associated metadata.
     */
    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    /**
     * Case patterns can introduce varialbes via the bindings. To neatly introduce all required bindings into scope,
     * there is a conversion mechainsm, which can also handle the nested bindings. This is overridden on the different
     * destructuring patterns.
     * @param meta Associated metadata.
     * @return The converted variable AST.
     */
    virtual auto convert_to_variable(CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst>;

protected:
    /**
     * The @c let statement that case-of-patterns are converted to, to introduce the variables created by the pattern.
     */
    std::unique_ptr<LetStatementInitializedAst> m_mapped_let;
};
