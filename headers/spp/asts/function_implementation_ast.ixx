module;
#include <spp/macros.hpp>

export module spp.asts.function_implementation_ast;
import spp.asts.inner_scope_expression_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct FunctionImplementationAst;
}

COMMON_AST_IMPORTS

/**
 * The FunctionImplementationAst represents the implementation of a function. It is used to define the body of a
 * function and contains the statements that make up the function's implementation. Semantically equivalent to a basic
 * InnerScopeAst.
 */
SPP_EXP_CLS struct spp::asts::FunctionImplementationAst : InnerScopeExpressionAst {
    static auto NewEmpty() -> Unique<FunctionImplementationAst>;

    using InnerScopeExpressionAst::InnerScopeExpressionAst;

    SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;

    ~FunctionImplementationAst() override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;
};
