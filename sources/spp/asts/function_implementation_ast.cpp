module spp.asts.function_implementation_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;


spp::asts::FunctionImplementationAst::~FunctionImplementationAst() = default;


auto spp::asts::FunctionImplementationAst::new_empty()
    -> std::unique_ptr<FunctionImplementationAst> {
    return std::make_unique<FunctionImplementationAst>();
}


auto spp::asts::FunctionImplementationAst::clone() const
    -> std::unique_ptr<Ast> {
    auto *c = InnerScopeAst::clone().release()->to<InnerScopeAst>();
    return std::make_unique<FunctionImplementationAst>(
        std::move(c->tok_l),
        std::move(c->members),
        std::move(c->tok_r));
}


auto spp::asts::FunctionImplementationAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Inject the argument values.
    for (auto const &[arg_name, arg_comp] : meta->cmp_args) {
        const auto arg_sym = sm->current_scope->get_var_symbol(arg_name);
        arg_sym->comptime_value = ast_clone(arg_comp);
    }

    // Comptime resolve each member of the inner scope.
    for (auto const &member : this->members) {
        const auto did_ret = member->to<RetStatementAst>() != nullptr;
        member->stage_9_comptime_resolution(sm, meta);
        if (did_ret) { break; }
    }
}
