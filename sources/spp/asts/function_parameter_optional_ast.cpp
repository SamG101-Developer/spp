module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.asts.utils;


spp::asts::FunctionParameterOptionalAst::FunctionParameterOptionalAst(
    decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) type,
    decltype(tok_assign) &&tok_assign,
    decltype(default_val) &&default_val) :
    FunctionParameterAst(std::move(var), std::move(tok_colon), std::move(type), utils::OrderableTag::OPTIONAL_PARAM),
    tok_assign(std::move(tok_assign)),
    default_val(std::move(default_val)) {
}


spp::asts::FunctionParameterOptionalAst::~FunctionParameterOptionalAst() = default;


auto spp::asts::FunctionParameterOptionalAst::pos_start() const
    -> std::size_t {
    return default_val->pos_start();
}


auto spp::asts::FunctionParameterOptionalAst::pos_end() const
    -> std::size_t {
    return default_val->pos_end();
}


auto spp::asts::FunctionParameterOptionalAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<FunctionParameterOptionalAst>(
        ast_clone(var),
        ast_clone(tok_colon),
        ast_clone(type),
        ast_clone(tok_assign),
        ast_clone(default_val));
}


spp::asts::FunctionParameterOptionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(var);
    SPP_STRING_APPEND(tok_colon).append(" ");
    SPP_STRING_APPEND(type).append(" ");
    SPP_STRING_APPEND(tok_assign).append(" ");
    SPP_STRING_APPEND(default_val);
    SPP_STRING_END;
}


auto spp::asts::FunctionParameterOptionalAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Perform default analysis steps.
    FunctionParameterAst::stage_7_analyse_semantics(sm, meta);

    // Make sure the default expression the correct type. Only do this from true stage 7 analysis, not via stage 5.
    if (meta->current_stage >= 9) {
        default_val->stage_7_analyse_semantics(sm, meta);
        const auto default_type = default_val->infer_type(sm, meta);
        raise_if<analyse::errors::SppTypeMismatchError>(
            not analyse::utils::type_utils::symbolic_eq(*type, *default_type, *sm->current_scope, *sm->current_scope),
            {sm->current_scope}, ERR_ARGS(*extract_name(), *type, *default_val, *default_type));
    }
}


auto spp::asts::FunctionParameterOptionalAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Perform default memory checking steps.
    FunctionParameterAst::stage_8_check_memory(sm, meta);

    // Check the memory status of the default value expression.
    default_val->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *default_val, *default_val, *sm, true, true, true, true, true, meta);
}
