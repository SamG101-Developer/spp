module;
#include <spp/macros.hpp>

export module spp.asts.type_ast;
import spp.asts.primary_expression_ast;
import spp.asts.mixins.abstract_type_ast;
import genex;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypePostfixExpressionAst;
    SPP_EXP_CLS struct TypeUnaryExpressionAst;
}


/**
 * The TypeAst is a base class for all type-related AST nodes in the SPP language.
 */
SPP_EXP_CLS struct spp::asts::TypeAst : PrimaryExpressionAst, mixins::AbstractTypeAst, std::enable_shared_from_this<TypeAst> {
    using PrimaryExpressionAst::PrimaryExpressionAst;

    ~TypeAst() override;
};
