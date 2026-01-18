module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_parameter_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.order_utils;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_optional_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import genex;


spp::asts::FunctionParameterGroupAst::FunctionParameterGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(params) &&params,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    params(std::move(params)),
    tok_r(std::move(tok_r)) {
}


spp::asts::FunctionParameterGroupAst::~FunctionParameterGroupAst() = default;


auto spp::asts::FunctionParameterGroupAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::FunctionParameterGroupAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::FunctionParameterGroupAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<FunctionParameterGroupAst>(
        ast_clone(tok_l),
        ast_clone_vec(params),
        ast_clone(tok_r));
}


spp::asts::FunctionParameterGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(params, ", ");
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::FunctionParameterGroupAst::get_all_params() const
    -> std::vector<FunctionParameterAst*> {
    return params
        | genex::views::ptr
        | genex::to<std::vector>();
}


auto spp::asts::FunctionParameterGroupAst::get_self_param() const
    -> FunctionParameterSelfAst* {
    const auto ps = params
        | genex::views::ptr
        | genex::views::cast_dynamic<FunctionParameterSelfAst*>()
        | genex::to<std::vector>();
    return ps.empty() ? nullptr : ps[0];
}


auto spp::asts::FunctionParameterGroupAst::get_required_params() const
    -> std::vector<FunctionParameterRequiredAst*> {
    return params
        | genex::views::ptr
        | genex::views::cast_dynamic<FunctionParameterRequiredAst*>()
        | genex::to<std::vector>();
}


auto spp::asts::FunctionParameterGroupAst::get_optional_params() const
    -> std::vector<FunctionParameterOptionalAst*> {
    return params
        | genex::views::ptr
        | genex::views::cast_dynamic<FunctionParameterOptionalAst*>()
        | genex::to<std::vector>();
}


auto spp::asts::FunctionParameterGroupAst::get_variadic_param() const
    -> FunctionParameterVariadicAst* {
    const auto ps = params
        | genex::views::ptr
        | genex::views::cast_dynamic<FunctionParameterVariadicAst*>()
        | genex::to<std::vector>();
    return ps.empty() ? nullptr : ps[0];
}


auto spp::asts::FunctionParameterGroupAst::get_non_self_params() const
    -> std::vector<FunctionParameterAst*> {
    return params
        | genex::views::ptr
        | genex::views::filter([](auto &&x) { return not x->template to<FunctionParameterSelfAst>(); })
        | genex::to<std::vector>();
}


auto spp::asts::FunctionParameterGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create sets of parameters based on conditions.
    const auto self_params = params
        | genex::views::ptr
        | genex::views::cast_dynamic<FunctionParameterSelfAst*>()
        | genex::to<std::vector>();

    const auto variadic_params = params
        | genex::views::ptr
        | genex::views::cast_dynamic<FunctionParameterVariadicAst*>()
        | genex::to<std::vector>();

    const auto param_names = params
        | genex::views::transform([](auto &&x) { return x->extract_names(); })
        | genex::views::join
        | genex::to<std::vector>()
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<std::vector>();

    const auto unordered_params = analyse::utils::order_utils::order_params(params
        | genex::views::ptr
        | genex::views::cast_dynamic<mixins::OrderableAst*>()
        | genex::to<std::vector>());

    // Check there is only 1 "self" parameter.
    if (self_params.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppMultipleSelfParametersError>()
            .with_args(*self_params[0], *self_params[1])
            .raises_from(sm->current_scope);
    }

    // Check there is only 1 variadic parameter, and it is last.
    if (variadic_params.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppMultipleVariadicParametersError>()
            .with_args(*variadic_params[0], *variadic_params[1])
            .raises_from(sm->current_scope);
    }

    // Check there are no duplicate parameter names.
    if (not param_names.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierDuplicateError>()
            .with_args(*param_names[0], *param_names[1], "keyword function-argument")
            .raises_from(sm->current_scope);
    }

    // Check the parameters are in the correct order.
    if (not unordered_params.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppOrderInvalidError>()
            .with_args(unordered_params[0].first, *unordered_params[0].second, unordered_params[1].first, *unordered_params[1].second)
            .raises_from(sm->current_scope);
    }

    // Analyse the parameters.
    for (auto const &param : params) {
        param->stage_7_analyse_semantics(sm, meta);
    }
}


auto spp::asts::FunctionParameterGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check each parameter's memory.
    for (auto &&param : params) {
        param->stage_8_check_memory(sm, meta);
    }
}


auto spp::asts::FunctionParameterGroupAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Code generate each parameter.
    for (auto &&param : params) {
        param->stage_11_code_gen_2(sm, meta, ctx);
    }
    return nullptr;
}
