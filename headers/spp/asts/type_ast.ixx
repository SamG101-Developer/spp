module;
#include <spp/macros.hpp>

export module spp.asts.type_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The TypeAst is a base class for all type-related AST nodes in the SPP language.
 */
SPP_EXP_CLS struct spp::asts::TypeAst : PrimaryExpressionAst {
    using PrimaryExpressionAst::PrimaryExpressionAst;
    ~TypeAst() override;
};
