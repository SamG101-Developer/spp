module;
#include <spp/macros.hpp>

export module spp.asts.function_implementation_ast;
import spp.asts.inner_scope_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionImplementationAst;
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS using FunctionMemberAst = StatementAst;
}


/**
 * The FunctionImplementationAst represents the implementation of a function. It is used to define the body of a
 * function and contains the statements that make up the function's implementation. Semantically equivalent to a basic
 * InnerScopeAst.
 */
SPP_EXP_CLS struct spp::asts::FunctionImplementationAst :
    InnerScopeAst<std::unique_ptr<FunctionMemberAst>> {
    using InnerScopeAst::InnerScopeAst;

    ~FunctionImplementationAst() override;

    static auto new_empty() -> std::unique_ptr<FunctionImplementationAst>;

    auto clone() const -> std::unique_ptr<Ast> override;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
