#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/sup_implementation_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/generate/common_types.hpp>

#include <genex/views/filter.hpp>
#include <genex/views/map.hpp>
#include <genex/views/to.hpp>


spp::asts::SupPrototypeExtensionAst::SupPrototypeExtensionAst(
    decltype(tok_sup) &&tok_sup,
    decltype(generic_param_group) &&generic_param_group,
    decltype(name) &&name,
    decltype(tok_ext) &&tok_ext,
    decltype(super_class) &&super_class,
    decltype(impl) &&impl) :
    tok_sup(std::move(tok_sup)),
    generic_param_group(std::move(generic_param_group)),
    name(std::move(name)),
    tok_ext(std::move(tok_ext)),
    super_class(std::move(super_class)),
    impl(std::move(impl)) {
}


spp::asts::SupPrototypeExtensionAst::~SupPrototypeExtensionAst() = default;


auto spp::asts::SupPrototypeExtensionAst::pos_start() const -> std::size_t {
    return tok_sup->pos_start();
}


auto spp::asts::SupPrototypeExtensionAst::pos_end() const -> std::size_t {
    return impl->pos_end();
}


auto spp::asts::SupPrototypeExtensionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<SupPrototypeExtensionAst>(
        ast_clone(tok_sup),
        ast_clone(generic_param_group),
        ast_clone(name),
        ast_clone(tok_ext),
        ast_clone(super_class),
        ast_clone(impl));
}


spp::asts::SupPrototypeExtensionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_sup);
    SPP_STRING_APPEND(generic_param_group);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_ext);
    SPP_STRING_APPEND(super_class);
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::SupPrototypeExtensionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_sup);
    SPP_PRINT_APPEND(generic_param_group);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_ext);
    SPP_PRINT_APPEND(super_class);
    SPP_PRINT_APPEND(impl);
    SPP_PRINT_END;
}


auto spp::asts::SupPrototypeExtensionAst::m_check_cyclic_extension(
    analyse::scopes::TypeSymbol const &sup_sym,
    analyse::scopes::Scope const &check_scope)
    -> void {
    // Prevent double inheritance by checking if the scopes are already registered the other way around.
    const auto existing_sup_scopes = sup_sym.scope->sup_scopes()
        | genex::views::filter([](auto &&x) { return ast_cast<SupPrototypeExtensionAst>(x->ast); })
        | genex::views::map([](auto &&x) { return std::make_pair(x, ast_cast<SupPrototypeExtensionAst>(x->ast)); })
        | genex::views::filter([&](auto &&x) { return analyse::utils::type_utils::relaxed_symbolic_eq(*super_class, *x.second->name, check_scope, *x.first); })
        | genex::views::filter([&](auto &&x) { return analyse::utils::type_utils::symbolic_eq(*x.second->super_class, *name, *x.first, check_scope); })
        | genex::views::to<std::vector>();

    if (not existing_sup_scopes.empty()) {
        analyse::errors::SppSuperimpositionCyclicExtensionError(*existing_sup_scopes[0].second->super_class, *name).scopes({&check_scope}).raise();
    }
}


auto spp::asts::SupPrototypeExtensionAst::m_check_double_extension(
    analyse::scopes::TypeSymbol const &cls_sym,
    analyse::scopes::Scope const &check_scope)
    -> void {
    // Early return for function-classes.
    if (cls_sym.name->name[0] == '$') {
        return;
    }

    // Prevent double inheritance by checking if the scopes are already registered the other way around.
    const auto existing_sup_scopes = cls_sym.scope->sup_scopes()
        | genex::views::filter([](auto &&x) { return ast_cast<SupPrototypeExtensionAst>(x->ast); })
        | genex::views::map([](auto &&x) { return std::make_pair(x, ast_cast<SupPrototypeExtensionAst>(x->ast)); })
        | genex::views::filter([&](auto &&x) { return analyse::utils::type_utils::relaxed_symbolic_eq(*super_class, *x.second->name, check_scope, *x.first); })
        | genex::views::filter([&](auto &&x) { return analyse::utils::type_utils::symbolic_eq(*x.second->super_class, *name, *x.first, check_scope); })
        | genex::views::to<std::vector>();

    if (not existing_sup_scopes.empty()) {
        analyse::errors::SppSuperimpositionCyclicExtensionError(*existing_sup_scopes[0].second->super_class, *name).scopes({&check_scope}).raise();
    }
}


auto spp::asts::SupPrototypeExtensionAst::m_check_self_extension(
    analyse::scopes::Scope const &check_scope)
    -> void {
    // Check if the superimposition is extending itself.
    if (analyse::utils::type_utils::symbolic_eq(*name, *super_class, check_scope, check_scope)) {
        analyse::errors::SppSuperimpositionSelfExtensionError(*name, *super_class).scopes({&check_scope}).raise();
    }
}


auto spp::asts::SupPrototypeExtensionAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Don't need to pre-process function-classes -- they are introduced from preprocessing functions.
    if (name->type_parts().back()->name[0] == '$') { return; }
    Ast::stage_1_pre_process(ctx);

    // Substitute the "Self" parameter's type with the name of the type being superimposed over.
    const auto self_gen_sub = std::make_unique<GenericArgumentTypeKeywordAst>(generate::common_types::self_type(pos_start()), nullptr, ast_name(ctx));
    auto gen_sub = std::vector<GenericArgumentAst*>();
    gen_sub.emplace_back(self_gen_sub.get());
    super_class = super_class->substitute_generics(gen_sub);

    // Preprocess the implementation.
    impl->stage_1_pre_process(ctx);

    // Todo: some sort of check that prevents something like "sup [T] T ext Borrow[T]", because this is infinite generation
}


auto spp::asts::SupPrototypeExtensionAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Create a new scope for the superimposition extension.
    auto scope_name = analyse::scopes::ScopeBlockName("<sup#" + static_cast<std::string>(*name) + " ext " + static_cast<std::string>(*super_class) + "#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);

    // Check there are optional generic parameters.
    if (auto optional = generic_param_group->get_optional_params(); not optional.empty()) {
        analyse::errors::SppSuperipositionOptionalGenericPararmeterError(*optional[0]).scopes({sm->current_scope}).raise();
    }

    // Check every generic parameter is constrained by the type.
    if (auto unconstrained = generic_param_group->params | genex::views::filter([this](auto &&x) { return not(name->contains_generic(*x) or super_class->contains_generic(*x)); }) | genex::views::to<std::vector>(); not unconstrained.empty()) {
        analyse::errors::SppSuperipositionUnconstrainedGenericPararmeterError(*unconstrained[0]).scopes({sm->current_scope}).raise();
    }

    // No conventions allowed on the name.
    if (auto &&conv = name->get_convention(); conv != nullptr) {
        analyse::errors::SppSecondClassBorrowViolationError(*name, *conv, "superimposition type").scopes({sm->current_scope}).raise();
    }

    // No conventions allowed on the super class.
    if (auto &&conv = super_class->get_convention(); conv != nullptr) {
        analyse::errors::SppSecondClassBorrowViolationError(*super_class, *conv, "superimposition type").scopes({sm->current_scope}).raise();
    }

    // Generate symbols for the generic parameter group, and the self type.
    generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    impl->stage_2_gen_top_level_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeExtensionAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward to the implementation.
    sm->move_to_next_scope();
    impl->stage_3_gen_top_level_aliases(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeExtensionAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward to the implementation.
    sm->move_to_next_scope();
    impl->stage_4_qualify_types(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeExtensionAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Move into the superimposition scope.
    sm->move_to_next_scope();

    // Analyse the type being superimposed over.
    name->stage_7_analyse_semantics(sm, meta);
    name = sm->current_scope->get_type_symbol(*name)->fq_name();

    // Register the superimposition against the base symbol.
    const auto base_cls_sym = sm->current_scope->get_type_symbol(*name->without_generics());
    if (sm->current_scope->parent == sm->current_scope->parent_module()) {
        if (not base_cls_sym->is_generic) {
            sm->normal_sup_blocks.try_emplace(base_cls_sym, std::vector<analyse::scopes::Scope*>{}).first->second.emplace_back(sm->current_scope);
        }
        else {
            sm->generic_sup_blocks.emplace_back(sm->current_scope);
        }
    }

    // Add the "Self" symbol into the scope.
    if (name->type_parts().back()->name[0] == '$') {
        const auto cls_sym = sm->current_scope->get_type_symbol(*name);
        sm->current_scope->add_symbol(std::make_unique<analyse::scopes::AliasSymbol>(
            std::make_unique<TypeIdentifierAst>(name->pos_start(), "Self", nullptr), nullptr, cls_sym->scope,
            sm->current_scope, cls_sym));
    }

    // Analyse the supertype after Self has been added (allows use in generic arguments to the superclass).
    super_class->stage_7_analyse_semantics(sm, meta);
    super_class = sm->current_scope->get_type_symbol(*super_class)->fq_name();

    // Check the supertype is not generic.
    const auto sup_sym = sm->current_scope->get_type_symbol(*super_class);
    if (sup_sym->is_generic) {
        analyse::errors::SppGenericTypeInvalidUsageError(*super_class, *super_class, "superimposition supertype").scopes({sm->current_scope}).raise();
    }

    // Load the implementation and move out of the scope.
    impl->stage_5_load_super_scopes(sm, meta);
    sm->move_out_of_current_scope();
}
