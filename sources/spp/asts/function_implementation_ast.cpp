module spp.asts;
import spp.analyse.scopes;
import spp.analyse.utils.scope_utils;
import spp.asts.utils;


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
    // Inject the argument values. Todo: && & std::move?
    for (auto const &[arg_name, arg_comp] : meta->cmp_args) {
        const auto arg_sym = analyse::utils::scope_utils::get_var_symbol(sm->current_scope, arg_name);
        arg_sym->comptime_value = ast_clone(arg_comp);
    }

    // Comptime resolve each member of the inner scope.
    for (auto const &member : this->members) {
        const auto did_ret = member->to<RetStatementAst>() != nullptr;
        member->stage_9_comptime_resolution(sm, meta);
        if (did_ret) { break; }
    }
}
