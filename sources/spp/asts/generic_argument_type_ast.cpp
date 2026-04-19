module spp.asts;
import spp.analyse.scopes;
import spp.analyse.utils.scope_utils;
import spp.asts.utils;


spp::asts::GenericArgumentTypeAst::GenericArgumentTypeAst(
    decltype(val) val,
    const utils::OrderableTag order_tag) :
    GenericArgumentAst(order_tag),
    val(std::move(val)) {
}


spp::asts::GenericArgumentTypeAst::~GenericArgumentTypeAst() = default;


auto spp::asts::GenericArgumentTypeAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If the generic arg is generic itself, from type aliasing, do nothing.
    // if (meta->alias_qualifier_scope != nullptr) {
    //     const auto sym = meta->alias_qualifier_scope->get_type_symbol(val, true);
    //     if (sym and sym->is_generic) { return; }
    // }

    // Qualify the type value without generics, then re-add the generics.
    val->stage_4_qualify_types(sm, meta);

    // The value could be a generic from another scope, so only modify if it exists.
    const auto raw = val->without_generics();
    const auto sym = analyse::utils::scope_utils::get_type_symbol(sm->current_scope, raw);
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
