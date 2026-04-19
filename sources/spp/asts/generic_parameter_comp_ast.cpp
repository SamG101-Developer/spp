module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.scope_utils;
import spp.analyse.utils.type_utils;
import spp.asts.utils;


spp::asts::GenericParameterCompAst::GenericParameterCompAst(
    decltype(tok_cmp) &&tok_cmp,
    decltype(name) name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) type,
    const utils::OrderableTag order_tag) :
    GenericParameterAst(std::move(name), order_tag),
    tok_cmp(std::move(tok_cmp)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)) {
}


spp::asts::GenericParameterCompAst::~GenericParameterCompAst() = default;


auto spp::asts::GenericParameterCompAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Create a variable symbol for this constant in the current scope (class / function).
    auto sym = std::make_unique<analyse::scopes::VariableSymbol>(
        IdentifierAst::from_type(*name), type, sm->current_scope,
        false, true, utils::Visibility::PUBLIC);
    sym->memory_info->ast_pins.emplace_back(name.get());
    sym->memory_info->ast_comptime = ast_clone(this);
    sym->memory_info->initialized_by(*this, sm->current_scope);
    sym->comptime_value = ast_clone(this); // TODO: this or name?
    analyse::utils::scope_utils::add_var_symbol(sm->current_scope, std::move(sym));
}


auto spp::asts::GenericParameterCompAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the type on the generic parameter.
    meta->save();
    meta->ignore_cmp_generic = name;

    // Ensure that the convention type doesn't have a convention.
    SPP_ENFORCE_SECOND_CLASS_BORROW_VIOLATION(
        type, type, *sm, "function return type");

    // Check the type exists and qualify.
    type->stage_7_analyse_semantics(sm, meta);
    type = sm->current_scope->get_type_symbol(type)->fq_name();
    const auto sym = sm->current_scope->get_var_symbol(
        IdentifierAst::from_type(*name));
    sym->type = type;
    meta->restore();
}


auto spp::asts::GenericParameterCompAst::stage_7_analyse_semantics(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // Analyse the type.
    // type->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::GenericParameterCompAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* {
    // The compile time constants' symbols need to be allocated into the function.
    // Todo: need to be done as a "constant" (see GlobalConstantAst)
    auto cast_name = IdentifierAst::from_type(*name);
    const auto is_opt = to<GenericParameterCompOptionalAst>();

    meta->save();
    meta->let_stmt_explicit_type = type;
    meta->let_stmt_from_uninitialized = not is_opt;
    meta->let_stmt_value = is_opt ? is_opt->default_val.get() : nullptr;
    const auto var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, std::move(cast_name), nullptr);
    const auto alloca = var->stage_11_code_gen_2(sm, meta, ctx);
    meta->restore();

    return alloca;
}
