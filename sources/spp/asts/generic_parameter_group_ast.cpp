module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_parameter_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.order_utils;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_comp_optional_ast;
import spp.asts.generic_parameter_comp_required_ast;
import spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_required_ast;
import spp.asts.generic_parameter_type_optional_ast;
import spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;


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
    auto existing_names = std::vector<std::string>();
    for (auto &&p : params) {
        existing_names.push_back(p->name->to_string());
    }
    for (auto &&p : other.params) {
        // Don't add duplicate named parameters.
        auto new_name = p->name->to_string();
        if (genex::contains(existing_names, new_name)) { continue; }
        params.push_back(ast_clone(p));
        existing_names.push_back(new_name);
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
        SPP_STRING_EXTEND(params, ", ");
        SPP_STRING_APPEND(tok_r);
    }
    SPP_STRING_END;
}


auto spp::asts::GenericParameterGroupAst::new_empty()
    -> std::unique_ptr<GenericParameterGroupAst> {
    return std::make_unique<GenericParameterGroupAst>(
        nullptr, decltype(params)(), nullptr);
}


auto spp::asts::GenericParameterGroupAst::get_required_params() const
    -> std::vector<GenericParameterAst*> {
    // Filter by casting.
    auto out = std::vector<GenericParameterAst*>();
    for (auto &&p : params) {
        if (const auto req_type = p->to<GenericParameterTypeRequiredAst>()) {
            out.push_back(req_type->to<GenericParameterAst>());
        }
        else if (const auto req_comp = p->to<GenericParameterCompRequiredAst>()) {
            out.push_back(req_comp->to<GenericParameterAst>());
        }
    }
    return out;
}


auto spp::asts::GenericParameterGroupAst::get_optional_params() const
    -> std::vector<GenericParameterAst*> {
    // Filter by casting.
    auto out = std::vector<GenericParameterAst*>();
    for (auto &&p : params) {
        if (const auto opt_type = p->to<GenericParameterTypeOptionalAst>()) {
            out.push_back(opt_type->to<GenericParameterAst>());
        }
        else if (const auto opt_comp = p->to<GenericParameterCompOptionalAst>()) {
            out.push_back(opt_comp->to<GenericParameterAst>());
        }
    }
    return out;
}


auto spp::asts::GenericParameterGroupAst::get_variadic_param() const
    -> GenericParameterAst* {
    auto variadics = std::vector<GenericParameterAst*>();
    for (auto &&p : params) {
        if (const auto var_type = p->to<GenericParameterTypeVariadicAst>()) {
            variadics.push_back(var_type->to<GenericParameterAst>());
        }
        else if (const auto var_comp = p->to<GenericParameterCompVariadicAst>()) {
            variadics.push_back(var_comp->to<GenericParameterAst>());
        }
    }

    return variadics.empty() ? nullptr : variadics[0];
}


auto spp::asts::GenericParameterGroupAst::get_comp_params() const
    -> std::vector<GenericParameterCompAst*> {
    // Filter by casting.
    auto out = std::vector<GenericParameterCompAst*>();
    for (auto &&p : params) {
        if (const auto comp_param = p->to<GenericParameterCompAst>()) {
            out.push_back(comp_param);
        }
    }
    return out;
}


auto spp::asts::GenericParameterGroupAst::get_type_params() const
    -> std::vector<GenericParameterTypeAst*> {
    // Filter by casting.
    auto out = std::vector<GenericParameterTypeAst*>();
    for (auto &&p : params) {
        if (const auto type_param = p->to<GenericParameterTypeAst>()) {
            out.push_back(type_param);
        }
    }
    return out;
}


auto spp::asts::GenericParameterGroupAst::get_all_params() const
    -> std::vector<GenericParameterAst*> {
    // Return all parameters.
    auto out = std::vector<GenericParameterAst*>();
    for (auto &&p : params) {
        out.push_back(p.get());
    }
    return out;
}


auto spp::asts::GenericParameterGroupAst::opt_to_req() const
    -> std::unique_ptr<GenericParameterGroupAst> {
    // Convert all optional parameters to required parameters.
    auto new_params = std::vector<std::unique_ptr<GenericParameterAst>>();
    for (auto &&p : params) {
        if (const auto opt_type = p->to<GenericParameterTypeOptionalAst>(); opt_type != nullptr) {
            auto param = std::make_unique<GenericParameterTypeRequiredAst>(ast_clone(opt_type->name), ast_clone(opt_type->constraints));
            auto cast_param = std::unique_ptr<GenericParameterAst>(param.release()->to<GenericParameterAst>());
            new_params.emplace_back(std::move(cast_param));
        }
        else if (const auto opt_comp = p->to<GenericParameterCompOptionalAst>(); opt_comp != nullptr) {
            auto param = std::make_unique<GenericParameterCompRequiredAst>(nullptr, ast_clone(opt_comp->name), nullptr, ast_clone(opt_comp->type));
            auto cast_param = std::unique_ptr<GenericParameterAst>(param.release()->to<GenericParameterAst>());
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
    CompilerMetaData *meta)
    -> void {
    // Run the generation steps on the parameters in the group.
    for (auto &&x : params) {
        x->stage_2_gen_top_level_scopes(sm, meta);
    }
}


auto spp::asts::GenericParameterGroupAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run the type qualifier steps on each parameter in the group.
    for (auto &&x : params) {
        x->stage_4_qualify_types(sm, meta);
    }
}


auto spp::asts::GenericParameterGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    const auto param_names = params
        | genex::views::transform([](auto &&x) { return x->name.get(); })
        | genex::to<std::vector>()
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<std::vector>();

    const auto unordered_params = analyse::utils::order_utils::order_params(params
        | genex::views::ptr
        | genex::views::cast_dynamic<mixins::OrderableAst*>()
        | genex::to<std::vector>());

    // Check there are no duplicate parameter names.
    raise_if<analyse::errors::SppIdentifierDuplicateError>(
        not param_names.empty(), {sm->current_scope},
        ERR_ARGS(*param_names[0], *param_names[1], "keyword function-argument"));

    // Check the parameters are in the correct order.
    raise_if<analyse::errors::SppOrderInvalidError>(
        not unordered_params.empty(), {sm->current_scope},
        ERR_ARGS(unordered_params[0].first, *unordered_params[0].second, unordered_params[1].first, *unordered_params[1].second));

    // Run the semantic analysis steps on each parameter in the group.
    for (auto &&x : params) {
        x->stage_7_analyse_semantics(sm, meta);
    }
}


auto spp::asts::GenericParameterGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run the memory checks on each parameter in the group.
    for (auto &&x : params) {
        x->stage_8_check_memory(sm, meta);
    }
}


auto spp::asts::GenericParameterGroupAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Run the code generation steps on each parameter in the group.
    for (auto &&x : params) {
        x->stage_11_code_gen_2(sm, meta, ctx);
    }
    return nullptr;
}
