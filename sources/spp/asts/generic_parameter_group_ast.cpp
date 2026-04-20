module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.utils.order_utils;
import spp.analyse.utils.scope_utils;
import spp.analyse.utils.type_utils;
import spp.asts.utils;
import spp.lex;
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


auto spp::asts::GenericParameterGroupAst::merge_generics(
    decltype(params) &&other_params) -> void {
    // Merge the given parameters into this generic parameter group, ensuring no duplicates by name.
    auto existing_names = std::vector<std::string>();
    for (auto &&p : params) {
        existing_names.push_back(p->name->to_string());
    }
    for (auto &&p : other_params) {
        // Don't add duplicate named parameters.
        auto new_name = p->name->to_string();
        if (genex::contains(existing_names, new_name)) { continue; }
        params.push_back(ast_clone(p));
        existing_names.push_back(new_name);
    }
}


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
    merge_generics(ast_clone_vec(other.params));
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


auto spp::asts::GenericParameterGroupAst::new_empty_shared()
    -> std::shared_ptr<GenericParameterGroupAst> {
    return std::make_shared<GenericParameterGroupAst>(
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

    // Do the constraints after all the parameters are qualified. This is because of external generic symbols using
    // non-qualified types when analysing generically subsitututed contraint types.
    for (auto &&x : get_type_params()) {
        if (x->constraints != nullptr) {
            x->constraints->stage_7_analyse_semantics(sm, meta);

            // Attach the scopes of the cosntraint types as sup-scopes to the generic scope.
            for (auto const &constraint : x->constraints->constraints) {
                auto constraint_scope = analyse::utils::scope_utils::get_type_symbol(*sm->current_scope, constraint)->scope;
                for (auto const &dummy_scope : x->m_dummy_scopes) {
                    dummy_scope->direct_sup_scopes.emplace_back(constraint_scope);
                }
            }
        }
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

    // Mark copyable generics.
    for (auto &&param : get_type_params()) {
        if (param->constraints == nullptr) { continue; }
        for (auto &&constraint: param->constraints->constraints) {
            if (analyse::utils::type_utils::is_type_copyable(*constraint, *sm)) {
                const auto generic_sym = analyse::utils::scope_utils::get_type_symbol(*sm->current_scope, param->name);
                generic_sym->is_directly_copyable = true;
            }
        }
    }

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
    codegen::LlvmCtx *ctx)
    -> llvm::Value* {
    // Run the code generation steps on each parameter in the group.
    for (auto &&x : params) {
        x->stage_11_code_gen_2(sm, meta, ctx);
    }
    return nullptr;
}
