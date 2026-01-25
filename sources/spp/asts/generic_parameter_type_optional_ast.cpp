module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_type_optional_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;


spp::asts::GenericParameterTypeOptionalAst::GenericParameterTypeOptionalAst(
    decltype(name) &&name,
    decltype(constraints) &&constraints,
    decltype(tok_assign) &&tok_assign,
    decltype(default_val) &&default_val) :
    GenericParameterTypeAst(std::move(name), std::move(constraints), utils::OrderableTag::OPTIONAL_PARAM),
    tok_assign(std::move(tok_assign)),
    default_val(std::move(default_val)) {
}


spp::asts::GenericParameterTypeOptionalAst::~GenericParameterTypeOptionalAst() = default;


auto spp::asts::GenericParameterTypeOptionalAst::pos_start() const
    -> std::size_t {
    return name->pos_start();
}


auto spp::asts::GenericParameterTypeOptionalAst::pos_end() const
    -> std::size_t {
    return default_val->pos_end();
}


auto spp::asts::GenericParameterTypeOptionalAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterTypeOptionalAst>(
        ast_clone(name),
        ast_clone(constraints),
        ast_clone(tok_assign),
        ast_clone(default_val));
}


spp::asts::GenericParameterTypeOptionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(constraints);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(default_val);
    SPP_STRING_END;
}


auto spp::asts::GenericParameterTypeOptionalAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Default behaviour (inline constraints).
    GenericParameterTypeAst::stage_4_qualify_types(sm, meta);

    // Handle the default type.
    default_val->stage_7_analyse_semantics(sm, meta);
    if (const auto sym = sm->current_scope->get_type_symbol(default_val->without_generics()); sym != nullptr) {
        auto temp = sym->fq_name()->with_convention(ast_clone(default_val->get_convention()));
        temp = temp->with_generics(std::move(default_val->type_parts().back()->generic_arg_group));
        default_val = std::move(temp);
    }
}


auto spp::asts::GenericParameterTypeOptionalAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the name and default value of the generic type parameter.
    GenericParameterTypeAst::stage_7_analyse_semantics(sm, meta);
    default_val->stage_7_analyse_semantics(sm, meta);
}
