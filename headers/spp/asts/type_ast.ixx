module;
#include <spp/macros.hpp>

export module spp.asts.type_ast;
import spp.asts.primary_expression_ast;
import spp.asts.mixins.abstract_type_ast;

import std;


/**
 * The TypeAst is a base class for all type-related AST nodes in the SPP language.
 */
SPP_EXP_CLS struct spp::asts::TypeAst : PrimaryExpressionAst, mixins::AbstractTypeAst, std::enable_shared_from_this<TypeAst> {
    using PrimaryExpressionAst::PrimaryExpressionAst;

    ~TypeAst() override;
};
