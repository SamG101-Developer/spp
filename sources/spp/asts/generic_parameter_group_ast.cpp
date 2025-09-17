#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/function_parameter_self_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_comp_required_ast.hpp>
#include <spp/asts/generic_parameter_comp_optional_ast.hpp>
#include <spp/asts/generic_parameter_comp_variadic_ast.hpp>
#include <spp/asts/generic_parameter_type_required_ast.hpp>
#include <spp/asts/generic_parameter_type_optional_ast.hpp>
#include <spp/asts/generic_parameter_type_variadic_ast.hpp>
#include <spp/asts/generic_parameter_type_inline_constraints_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/pch.hpp>

#include <genex/views/cast.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/duplicates.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/materialize.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/to.hpp>


spp::asts::GenericParameterGroupAst::GenericParameterGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(params) &&params,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    params(std::move(params)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
}


spp::asts::GenericParameterGroupAst::~GenericParameterGroupAst() = default;


auto spp::asts::GenericParameterGroupAst::new_empty()
    -> std::unique_ptr<GenericParameterGroupAst> {
    auto x = std::make_unique<GenericParameterGroupAst>(nullptr, decltype(params)(), nullptr);
    return x;
}


auto spp::asts::GenericParameterGroupAst::get_required_params() const
    -> std::vector<GenericParameterAst*> {
    auto required_type = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterTypeRequiredAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::views::to<std::vector>();

    auto required_comp = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterCompRequiredAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::views::to<std::vector>();

    return genex::views::concat(required_type, required_comp)
        | genex::views::to<std::vector>();
}


auto spp::asts::GenericParameterGroupAst::get_optional_params() const
    -> std::vector<GenericParameterAst*> {
    auto optional_type = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterTypeOptionalAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::views::to<std::vector>();

    auto optional_comp = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterCompOptionalAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::views::to<std::vector>();

    return genex::views::concat(optional_type, optional_comp)
        | genex::views::to<std::vector>();
}


auto spp::asts::GenericParameterGroupAst::get_variadic_param() const
    -> GenericParameterAst* {
    auto variadic_type = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterTypeVariadicAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::views::to<std::vector>();

    auto variadic_comp = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterCompVariadicAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::views::to<std::vector>();

    const auto variadics = genex::views::concat(variadic_type, variadic_comp)
        | genex::views::to<std::vector>();

    if (variadics.size() > 1) {
        // This should have been caught in parsing, but just in case.
        throw std::runtime_error("Internal compiler error: Multiple variadic generic parameters found.");
    }

    return variadics.empty() ? nullptr : variadics[0];
}


auto spp::asts::GenericParameterGroupAst::get_comp_params() const
    -> std::vector<GenericParameterCompAst*> {
    return params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterCompAst*>()
        | genex::views::to<std::vector>();
}


auto spp::asts::GenericParameterGroupAst::get_type_params() const
    -> std::vector<GenericParameterTypeAst*> {
    return params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterTypeAst*>()
        | genex::views::to<std::vector>();
}


auto spp::asts::GenericParameterGroupAst::get_all_params() const
    -> std::vector<GenericParameterAst*> {
    return params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::views::to<std::vector>();
}


auto spp::asts::GenericParameterGroupAst::opt_to_req() const
    -> std::unique_ptr<GenericParameterGroupAst> {
    // Convert all optional parameters to required parameters.
    auto new_params = std::vector<std::unique_ptr<GenericParameterAst>>();
    for (auto &&p : params) {
        if (const auto opt_type = ast_cast<GenericParameterTypeOptionalAst>(p.get())) {
            auto param = std::make_unique<GenericParameterTypeRequiredAst>(ast_clone(opt_type->name), ast_clone(opt_type->constraints));
            auto cast_param = ast_cast<GenericParameterAst>(std::move(param));
            new_params.emplace_back(std::move(cast_param));
        }
        else if (const auto opt_comp = ast_cast<GenericParameterCompOptionalAst>(p.get())) {
            auto param = std::make_unique<GenericParameterCompRequiredAst>(nullptr, ast_clone(opt_comp->name), nullptr, ast_clone(opt_comp->type));
            auto cast_param = ast_cast<GenericParameterAst>(std::move(param));
            new_params.emplace_back(std::move(cast_param));
        }
        else {
            new_params.emplace_back(ast_clone(p));
        }
    }

    return std::make_unique<GenericParameterGroupAst>(nullptr, std::move(new_params), nullptr);
}


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


auto spp::asts::GenericParameterGroupAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Run the generation steps on the parameters in the group.
    params | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });
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
        | genex::views::ptr
        | genex::views::cast_dynamic<FunctionParameterSelfAst*>()
        | genex::views::filter([](auto &&x) { return x != nullptr; })
        | genex::views::to<std::vector>();

    if (self_params.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppMultipleSelfParametersError>().with_args(
            *self_params[0], *self_params[1]).with_scopes({sm->current_scope}).raise();
    }

    // Check there are no duplicate parameter names.
    const auto param_names = params
        | genex::views::transform([](auto &&x) { return x->name.get(); })
        | genex::views::materialize()
        | genex::views::duplicates()
        | genex::views::to<std::vector>();

    if (not param_names.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierDuplicateError>().with_args(
            *param_names[0], *param_names[1], "keyword function-argument").with_scopes({sm->current_scope}).raise();
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
