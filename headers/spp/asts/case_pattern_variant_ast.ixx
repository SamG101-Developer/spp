module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantAst;
}


/**
 * Base class for all different case pattern variants. This is used to store different variants in the same list, and to
 * provide the conversion binding for creating variables defined in patterns.
 */
SPP_EXP_CLS struct spp::asts::CasePatternVariantAst : Ast {
    explicit CasePatternVariantAst();
    ~CasePatternVariantAst() override;
};
