module;
#include <spp/macros.hpp>

module spp.asts.function_parameter_optional_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.asts.mixins.orderable_ast;


spp::asts::FunctionParameterOptionalAst::FunctionParameterOptionalAst(
    decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) type,
    decltype(tok_assign) &&tok_assign,
    decltype(default_val) &&default_val) :
    FunctionParameterAst(std::move(var), std::move(tok_colon), std::move(type), mixins::OrderableTag::OPTIONAL_PARAM),
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


auto spp::asts::FunctionParameterOptionalAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(var);
    SPP_PRINT_APPEND(tok_colon).append(" ");
    SPP_PRINT_APPEND(type).append(" ");
    SPP_PRINT_APPEND(tok_assign).append(" ");
    SPP_PRINT_APPEND(default_val);
    SPP_PRINT_END;
}


auto spp::asts::FunctionParameterOptionalAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Perform default analysis steps.
    FunctionParameterAst::stage_7_analyse_semantics(sm, meta);

    // Make sure the default expression the correct type.
    const auto default_type = default_val->infer_type(sm, meta);
    if (not analyse::utils::type_utils::symbolic_eq(*type, *default_type, *sm->current_scope, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
            *extract_name(), *type, *default_val, *default_type).with_scopes({sm->current_scope}).raise();
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
        *default_val, *default_val, *sm, true, true, true, true, true, true, meta);
}
