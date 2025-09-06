#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/generic_parameter_comp_optional_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericParameterCompOptionalAst::GenericParameterCompOptionalAst(
    decltype(tok_cmp) &&tok_cmp,
    decltype(name) &&name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type,
    decltype(tok_assign) &&tok_assign,
    decltype(default_val) &&default_val) :
    GenericParameterCompAst(std::move(tok_cmp), std::move(name), std::move(tok_colon), std::move(type)),
    tok_assign(std::move(tok_assign)),
    default_val(std::move(default_val)) {
}


spp::asts::GenericParameterCompOptionalAst::~GenericParameterCompOptionalAst() = default;


auto spp::asts::GenericParameterCompOptionalAst::pos_start() const -> std::size_t {
    return tok_cmp->pos_start();
}


auto spp::asts::GenericParameterCompOptionalAst::pos_end() const -> std::size_t {
    return default_val->pos_end();
}


auto spp::asts::GenericParameterCompOptionalAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterCompOptionalAst>(
        ast_clone(tok_cmp),
        ast_clone(name),
        ast_clone(tok_colon),
        ast_clone(type),
        ast_clone(tok_assign),
        ast_clone(default_val));
}


spp::asts::GenericParameterCompOptionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_cmp);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(default_val);
    SPP_STRING_END;
}


auto spp::asts::GenericParameterCompOptionalAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_cmp);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_colon);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(default_val);
    SPP_PRINT_END;
}


auto spp::asts::GenericParameterCompOptionalAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the default value.
    GenericParameterCompAst::stage_7_analyse_semantics(sm, meta);
    default_val->stage_7_analyse_semantics(sm, meta);

    // Make sure the default expression is of the correct type.
    const auto default_type = default_val->infer_type(sm, meta);
    if (not analyse::utils::type_utils::symbolic_eq(*type, *default_type, *sm->current_scope, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
            *name, *type, *default_val, *default_type).with_scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::GenericParameterCompOptionalAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the default value for memory issues.
    default_val->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *default_val, *default_val, *sm, true, true, true, true, true, true, meta);
}
