module;
#include <genex/to_container.hpp>
#include <genex/views/cast_dynamic.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/duplicates.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/intersperse.hpp>
#include <genex/views/join.hpp>
#include <genex/views/materialize.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/transform.hpp>

#include <spp/macros.hpp>

module spp.asts.generic_parameter_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.utils.order_utils;
import spp.asts.token_ast;
import spp.asts.generic_parameter_comp_required_ast;
import spp.asts.generic_parameter_type_required_ast;
import spp.asts.generic_parameter_comp_optional_ast;
import spp.asts.generic_parameter_type_optional_ast;
import spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.mixins.orderable_ast;
import spp.lex.tokens;


spp::asts::GenericParameterGroupAst::GenericParameterGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(params) &&params,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    params(std::move(params)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_SQUARE_BRACKET, "[");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_SQUARE_BRACKET, "]");
}


spp::asts::GenericParameterGroupAst::~GenericParameterGroupAst() = default;


auto spp::asts::GenericParameterGroupAst::operator+(
    GenericParameterGroupAst const &other) const
    -> std::unique_ptr<GenericParameterGroupAst> {
    auto new_params = ast_clone(this);
    *new_params += other;
    return new_params;
}


auto spp::asts::GenericParameterGroupAst::operator+=(
    GenericParameterGroupAst const &other)
    -> GenericParameterGroupAst& {
    for (auto &&p : other.params) {
        params.push_back(ast_clone(p));
    }
    return *this;
}


auto spp::asts::GenericParameterGroupAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::GenericParameterGroupAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::GenericParameterGroupAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterGroupAst>(
        ast_clone(tok_l),
        ast_clone_vec(params),
        ast_clone(tok_r));
}


spp::asts::GenericParameterGroupAst::operator std::string() const {
    SPP_STRING_START;
    if (not params.empty()) {
        SPP_STRING_APPEND(tok_l);
        SPP_STRING_EXTEND(params);
        SPP_STRING_APPEND(tok_r);
    }
    SPP_STRING_END;
}


auto spp::asts::GenericParameterGroupAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    if (not params.empty()) {
        SPP_PRINT_APPEND(tok_l);
        SPP_PRINT_EXTEND(params);
        SPP_PRINT_APPEND(tok_r);
    }
    SPP_PRINT_END;
}


auto spp::asts::GenericParameterGroupAst::new_empty()
    -> std::unique_ptr<GenericParameterGroupAst> {
    return std::make_unique<GenericParameterGroupAst>(
        nullptr, decltype(params)(), nullptr);
}


auto spp::asts::GenericParameterGroupAst::get_required_params() const
    -> std::vector<GenericParameterAst*> {
    auto required_type = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterTypeRequiredAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::to<std::vector>();

    auto required_comp = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterCompRequiredAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::to<std::vector>();

    return genex::views::concat(required_type, required_comp)
        | genex::to<std::vector>();
}


auto spp::asts::GenericParameterGroupAst::get_optional_params() const
    -> std::vector<GenericParameterAst*> {
    auto optional_type = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterTypeOptionalAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::to<std::vector>();

    auto optional_comp = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterCompOptionalAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::to<std::vector>();

    return genex::views::concat(optional_type, optional_comp)
        | genex::to<std::vector>();
}


auto spp::asts::GenericParameterGroupAst::get_variadic_param() const
    -> GenericParameterAst* {
    auto variadic_type = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterTypeVariadicAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::to<std::vector>();

    auto variadic_comp = params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterCompVariadicAst*>()
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::to<std::vector>();

    const auto variadics = genex::views::concat(variadic_type, variadic_comp)
        | genex::to<std::vector>();

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
        | genex::to<std::vector>();
}


auto spp::asts::GenericParameterGroupAst::get_type_params() const
    -> std::vector<GenericParameterTypeAst*> {
    return params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterTypeAst*>()
        | genex::to<std::vector>();
}


auto spp::asts::GenericParameterGroupAst::get_all_params() const
    -> std::vector<GenericParameterAst*> {
    return params
        | genex::views::ptr
        | genex::views::cast_dynamic<GenericParameterAst*>()
        | genex::to<std::vector>();
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


auto spp::asts::GenericParameterGroupAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Run the generation steps on the parameters in the group.
    params | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });
}


auto spp::asts::GenericParameterGroupAst::stage_4_qualify_types(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Run the type qualifier steps on each parameter in the group.
    params | genex::views::for_each([sm, meta](auto &&x) { x->stage_4_qualify_types(sm, meta); });
}


auto spp::asts::GenericParameterGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {

    const auto param_names = params
        | genex::views::transform([](auto &&x) { return x->name.get(); })
        | genex::views::materialize
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<std::vector>();

    const auto unordered_params = analyse::utils::order_utils::order_params(params
        | genex::views::ptr
        | genex::views::cast_dynamic<mixins::OrderableAst*>()
        | genex::to<std::vector>());

    // Check there are no duplicate parameter names.
    if (not param_names.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierDuplicateError>().with_args(
            *param_names[0], *param_names[1], "keyword function-argument").with_scopes({sm->current_scope}).raise();
    }

    // Check the parameters are in the correct order.
    if (not unordered_params.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppOrderInvalidError>().with_args(
            unordered_params[0].first, *unordered_params[0].second, unordered_params[1].first, *unordered_params[1].second).with_scopes({sm->current_scope}).raise();
    }

    // Run the semantic analysis steps on each parameter in the group.
    params | genex::views::for_each([sm, meta](auto &&x) { x->stage_7_analyse_semantics(sm, meta); });
}


auto spp::asts::GenericParameterGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Run the memory checks on each parameter in the group.
    params | genex::views::for_each([sm, meta](auto &&x) { x->stage_8_check_memory(sm, meta); });
}
