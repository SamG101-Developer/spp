#include <algorithm>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/function_parameter_self_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>

#include <genex/views/cast.hpp>
#include <genex/views/duplicates.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/materialize.hpp>
#include <genex/views/ptr.hpp>


spp::asts::GenericParameterGroupAst::GenericParameterGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(params) &&params,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    params(std::move(params)),
    tok_r(std::move(tok_r)) {
}


spp::asts::GenericParameterGroupAst::~GenericParameterGroupAst() = default;


auto spp::asts::GenericParameterGroupAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::GenericParameterGroupAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::GenericParameterGroupAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterGroupAst>(
        ast_clone(tok_l),
        ast_clone_vec(params),
        ast_clone(tok_r));
}


spp::asts::GenericParameterGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(params);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::GenericParameterGroupAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(params);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::GenericParameterGroupAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Run the type qualifier steps on each parameter in the group.
    params | genex::views::for_each([sm, meta](auto &&x) { x->stage_4_qualify_types(sm, meta); });
}


auto spp::asts::GenericParameterGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
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
        | genex::views::map([](auto &&x) { return x->name.get(); })
        | genex::views::materialize
        | genex::views::duplicates()
        | genex::views::to<std::vector>();

    if (not param_names.empty()) {
        analyse::errors::SppIdentifierDuplicateError(*param_names[0], *param_names[1], "keyword function-argument")
            .scopes({sm->current_scope})
            .raise();
    }

    // Run the semantic analysis steps on each parameter in the group.
    params | genex::views::for_each([sm, meta](auto &&x) { x->stage_7_analyse_semantics(sm, meta); });
}


auto spp::asts::GenericParameterGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Run the memory checks on each parameter in the group.
    params | genex::views::for_each([sm, meta](auto &&x) { x->stage_8_check_memory(sm, meta); });
}
