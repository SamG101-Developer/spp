module spp.asts.generic_argument_type_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;


spp::asts::GenericArgumentTypeAst::GenericArgumentTypeAst(
    decltype(val) val,
    const decltype(m_order_tag) order_tag) :
    GenericArgumentAst(order_tag),
    val(std::move(val)) {
}


spp::asts::GenericArgumentTypeAst::~GenericArgumentTypeAst() = default;


auto spp::asts::GenericArgumentTypeAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the type value without generics, then re-add the generics.
    val->stage_4_qualify_types(sm, meta);

    // The value could be a generic from another scope, so only modify if it exists.
    const auto raw = val->without_generics();
    const auto sym = sm->current_scope->get_type_symbol(raw);
    if (sym and sym->alias_stmt != nullptr) {
        auto temp = sym->fq_name()->with_convention(ast_clone(val->get_convention()));
        val = std::move(temp);
    }
    else if (sym != nullptr) {
        auto temp = sym->fq_name()->with_convention(ast_clone(val->get_convention()));
        temp = temp->with_generics(std::move(val->type_parts().back()->generic_arg_group));
        val = std::move(temp);
    }
}
