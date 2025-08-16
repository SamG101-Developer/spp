#include <algorithm>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/order_utils.hpp>
#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/function_parameter_self_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>

#include <genex/views/cast.hpp>
#include <genex/views/duplicates.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/flat.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/to.hpp>


spp::asts::FunctionParameterGroupAst::FunctionParameterGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(params) &&params,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    params(std::move(params)),
    tok_r(std::move(tok_r)) {
}


spp::asts::FunctionParameterGroupAst::~FunctionParameterGroupAst() = default;


auto spp::asts::FunctionParameterGroupAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::FunctionParameterGroupAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::FunctionParameterGroupAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<FunctionParameterGroupAst>(
        ast_clone(tok_l),
        ast_clone_vec(params),
        ast_clone(tok_r));
}


spp::asts::FunctionParameterGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(params);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::FunctionParameterGroupAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(params);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::FunctionParameterGroupAst::get_self_param() const -> FunctionParameterSelfAst* {
    const auto ps = params
        | genex::views::ptr_unique
        | genex::views::cast.operator()<FunctionParameterSelfAst*>()
        | genex::views::filter([](auto &&x) { return x != nullptr; })
        | genex::views::to<std::vector>();
    return ps.empty() ? nullptr : ps[0];
}


auto spp::asts::FunctionParameterGroupAst::get_required_params() const -> std::vector<FunctionParameterRequiredAst*> {
    return params
        | genex::views::ptr_unique
        | genex::views::cast.operator()<FunctionParameterRequiredAst*>()
        | genex::views::filter([](auto &&x) { return x != nullptr; })
        | genex::views::to<std::vector>();
}


auto spp::asts::FunctionParameterGroupAst::get_optional_params() const -> std::vector<FunctionParameterOptionalAst*> {
    return params
        | genex::views::ptr_unique
        | genex::views::cast.operator()<FunctionParameterOptionalAst*>()
        | genex::views::filter([](auto &&x) { return x != nullptr; })
        | genex::views::to<std::vector>();
}


auto spp::asts::FunctionParameterGroupAst::get_variadic_param() const -> FunctionParameterVariadicAst* {
    const auto ps = params
        | genex::views::ptr_unique
        | genex::views::cast.operator()<FunctionParameterVariadicAst*>()
        | genex::views::filter([](auto &&x) { return x != nullptr; })
        | genex::views::to<std::vector>();
    return ps.empty() ? nullptr : ps[0];
}


auto spp::asts::FunctionParameterGroupAst::get_non_self_params() const -> std::vector<FunctionParameterAst*> {
    return params
        | genex::views::ptr_unique
        | genex::views::filter([](auto &&x) { return not ast_cast<FunctionParameterSelfAst>(x); })
        | genex::views::to<std::vector>();
}


auto spp::asts::FunctionParameterGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *)
    -> void {
    // Check there is only 1 "self" parameter.
    const auto self_params = params
        | genex::views::ptr_unique
        | genex::views::cast.operator()<FunctionParameterSelfAst*>()
        | genex::views::filter([](auto &&x) { return x != nullptr; })
        | genex::views::to<std::vector>();

    if (self_params.size() > 1) {
        analyse::errors::SppMultipleSelfParametersError(*self_params[0], *self_params[1])
            .scopes({sm->current_scope})
            .raise();
    }

    // Check there are no duplicate parameter names.
    const auto param_names = params
        | genex::views::map([](auto &&x) { return x->extract_names(); })
        | genex::views::flat
        | genex::views::duplicates()
        | genex::views::to<std::vector>();

    if (not param_names.empty()) {
        analyse::errors::SppIdentifierDuplicateError(*param_names[0], *param_names[1], "keyword function-argument")
            .scopes({sm->current_scope})
            .raise();
    }

    // Check the arguments are in the correct order.
    const auto unordered_args = analyse::utils::order_utils::order_args(params
        | genex::views::cast.operator()<mixins::OrderableAst>()
        | genex::views::ptr_unique
        | genex::views::to<std::vector>());

    if (not unordered_args.empty()) {
        analyse::errors::SppOrderInvalidError(unordered_args[0].first, *unordered_args[0].second, unordered_args[1].first, *unordered_args[1].second)
            .scopes({sm->current_scope})
            .raise();
    }
}
