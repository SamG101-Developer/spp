#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/analyse/utils/func_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/sup_implementation_ast.hpp>
#include <spp/asts/sup_member_ast.hpp>
#include <spp/asts/sup_prototype_functions_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>

#include <genex/views/filter.hpp>
#include <genex/views/to.hpp>


spp::asts::SupPrototypeFunctionsAst::SupPrototypeFunctionsAst(
    decltype(tok_sup) &&tok_sup,
    decltype(generic_param_group) &&generic_param_group,
    decltype(name) &&name,
    decltype(impl) &&impl) :
    tok_sup(std::move(tok_sup)),
    generic_param_group(std::move(generic_param_group)),
    name(std::move(name)),
    impl(std::move(impl)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_sup, lex::SppTokenType::KW_SUP, "sup");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->generic_param_group);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->impl);
}


spp::asts::SupPrototypeFunctionsAst::~SupPrototypeFunctionsAst() = default;


auto spp::asts::SupPrototypeFunctionsAst::pos_start() const -> std::size_t {
    return tok_sup->pos_start();
}


auto spp::asts::SupPrototypeFunctionsAst::pos_end() const -> std::size_t {
    return impl->pos_end();
}


auto spp::asts::SupPrototypeFunctionsAst::clone() const -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<SupPrototypeFunctionsAst>(
        ast_clone(tok_sup),
        ast_clone(generic_param_group),
        ast_clone(name),
        ast_clone(impl));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    return ast;
}


spp::asts::SupPrototypeFunctionsAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_sup);
    SPP_STRING_APPEND(generic_param_group);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::SupPrototypeFunctionsAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_sup);
    SPP_PRINT_APPEND(generic_param_group);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(impl);
    SPP_PRINT_END;
}


auto spp::asts::SupPrototypeFunctionsAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Pre-process the AST by calling the base class method and then processing the implementation.
    Ast::stage_1_pre_process(ctx);
    impl->stage_1_pre_process(this);
}


auto spp::asts::SupPrototypeFunctionsAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Create a new scope for the superimposition extension.
    auto scope_name = analyse::scopes::ScopeBlockName("<sup#" + static_cast<std::string>(*name) + "#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);

    // Check there are optional generic parameters.
    if (const auto optional = generic_param_group->get_optional_params(); not optional.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSuperimpositionOptionalGenericParameterError>().with_args(
            *optional[0]).with_scopes({sm->current_scope}).raise();
    }

    // Check every generic parameter is constrained by the type.
    if (const auto unconstrained = generic_param_group->get_all_params() | genex::views::filter([this](auto &&x) { return not name->contains_generic(*x); }) | genex::views::to<std::vector>(); not unconstrained.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError>().with_args(
            *unconstrained[0]).with_scopes({sm->current_scope}).raise();
    }

    // No conventions allowed on the name.
    if (auto &&conv = name->get_convention(); conv != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
            *name, *conv, "superimposition type").with_scopes({sm->current_scope}).raise();
    }


    // Generate symbols for the generic parameter group, and the self type.
    generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    impl->stage_2_gen_top_level_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward to the implementation.
    sm->move_to_next_scope();
    impl->stage_3_gen_top_level_aliases(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward to the implementation.
    sm->move_to_next_scope();
    impl->stage_4_qualify_types(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Move into the superimposition scope.
    sm->move_to_next_scope();

    // Analyse the type being superimposed over.
    name->stage_7_analyse_semantics(sm, meta);
    name = sm->current_scope->get_type_symbol(name)->fq_name();

    // Register the superimposition against the base symbol.
    const auto base_cls_sym = sm->current_scope->get_type_symbol(name->without_generics());
    if (sm->current_scope->parent == sm->current_scope->parent_module()) {
        if (not base_cls_sym->is_generic) {
            sm->normal_sup_blocks.try_emplace(base_cls_sym.get(), std::vector<analyse::scopes::Scope*>()).first->second.emplace_back(sm->current_scope);
        }
        else {
            sm->generic_sup_blocks.emplace_back(sm->current_scope);
        }
    }

    // Add the "Self" symbol into the scope.
    if (name->type_parts().back()->name[0] == '$') {
        const auto cls_sym = sm->current_scope->get_type_symbol(name);
        sm->current_scope->add_type_symbol(std::make_unique<analyse::scopes::AliasSymbol>(
            std::make_unique<TypeIdentifierAst>(name->pos_start(), "Self", nullptr), nullptr, cls_sym->scope,
            sm->current_scope, cls_sym));
    }

    // Load the implementation and move out of the scope.
    impl->stage_5_load_super_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->move_to_next_scope();
    name->stage_7_analyse_semantics(sm, meta);
    impl->stage_6_pre_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->move_to_next_scope();
    name->stage_7_analyse_semantics(sm, meta);
    impl->stage_7_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->move_to_next_scope();
    impl->stage_8_check_memory(sm, meta);
    sm->move_out_of_current_scope();
}
