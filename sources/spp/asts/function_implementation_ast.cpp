module;
#include <spp/macros.hpp>

module spp.asts.function_implementation_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
auto spp::asts::FunctionImplementationAst::NewEmpty()
    -> Unique<FunctionImplementationAst> {
    return MakeUnique<FunctionImplementationAst>(nullptr, decltype(Members)(), nullptr);
}

spp::asts::FunctionImplementationAst::~FunctionImplementationAst() = default;

auto spp::asts::FunctionImplementationAst::Clone() const
    -> Unique<Ast> {
    auto ast = MakeUnique<FunctionImplementationAst>(
        AstClone(TokL), AstCloneVec(Members), AstClone(TokR));
    return ast;
}

auto spp::asts::FunctionImplementationAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Inject the argument values. Todo: && & std::move?
    for (auto const &[arg_name, arg_comp] : meta->CmpArgs) {
        const auto arg_sym = sm->CurrentScope->GetVarSymbol(arg_name);
        arg_sym->CompTimeValue = AstClone(arg_comp);
    }

    // Comptime resolve each member of the inner scope.
    for (auto const &member : this->Members) {
        const auto did_ret = member->To<RetStatementAst>() != nullptr;
        member->Stage9_CompTimeResolve(sm, meta);
        if (did_ret) { break; }
    }

    // Exit the scope.
}

SPP_MOD_END
