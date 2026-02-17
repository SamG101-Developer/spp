module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.sup_prototype_extension_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;


spp::asts::SupPrototypeExtensionAst::SupPrototypeExtensionAst(
    decltype(tok_sup) &&tok_sup,
    decltype(generic_param_group) &&generic_param_group,
    decltype(name) name,
    decltype(tok_ext) &&tok_ext,
    decltype(super_class) super_class,
    decltype(impl) &&impl) :
    tok_sup(std::move(tok_sup)),
    generic_param_group(std::move(generic_param_group)),
    name(std::move(name)),
    tok_ext(std::move(tok_ext)),
    super_class(std::move(super_class)),
    impl(std::move(impl)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_sup, lex::SppTokenType::KW_SUP, "sup");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->generic_param_group);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_ext, lex::SppTokenType::KW_EXT, "ext");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->impl);
}


spp::asts::SupPrototypeExtensionAst::~SupPrototypeExtensionAst() = default;


auto spp::asts::SupPrototypeExtensionAst::pos_start() const
    -> std::size_t {
    return tok_sup->pos_start();
}


auto spp::asts::SupPrototypeExtensionAst::pos_end() const
    -> std::size_t {
    return impl->pos_start();
}


auto spp::asts::SupPrototypeExtensionAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<SupPrototypeExtensionAst>(
        ast_clone(tok_sup),
        ast_clone(generic_param_group),
        ast_clone(name),
        ast_clone(tok_ext),
        ast_clone(super_class),
        ast_clone(impl));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    return ast;
}


spp::asts::SupPrototypeExtensionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_sup).append(" ");
    SPP_STRING_APPEND(generic_param_group).append(generic_param_group->params.empty() ? "" : " ");
    SPP_STRING_APPEND(name).append(" ");
    SPP_STRING_APPEND(tok_ext).append(" ");
    SPP_STRING_APPEND(super_class).append(" ");
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::SupPrototypeExtensionAst::check_cyclic_extension(
    analyse::scopes::TypeSymbol const &sup_sym,
    analyse::scopes::Scope &check_scope) const
    -> void {
    auto check_cycle = [this, &check_scope](analyse::scopes::Scope const *sc) {
        auto dummy = analyse::utils::type_utils::GenericInferenceMap();
        const auto ext = sc->ast->to<SupPrototypeExtensionAst>();
        return ext and
            analyse::utils::type_utils::relaxed_symbolic_eq(*ext->name, *super_class, *sc, check_scope, dummy, false) and
            analyse::utils::type_utils::symbolic_eq(*ext->super_class, *name, *sc, check_scope, false);
    };

    // Prevent double inheritance by checking if the scopes are already registered the other way around.
    const auto existing_sup_scopes = sup_sym.scope->sup_scopes()
        | genex::views::filter(check_cycle)
        | genex::views::transform([](auto *x) { return std::make_pair(x, x->ast->template to<SupPrototypeExtensionAst>()); })
        | genex::to<std::vector>();

    raise_if<analyse::errors::SppSuperimpositionCyclicExtensionError>(
        not existing_sup_scopes.empty(), {&check_scope},
        ERR_ARGS(*existing_sup_scopes[0].second->super_class, *name));
}


auto spp::asts::SupPrototypeExtensionAst::check_double_extension(
    analyse::scopes::TypeSymbol const &cls_sym,
    analyse::scopes::Scope &check_scope) const
    -> void {
    // Early return for function-classes.
    if (cls_sym.name->name[0] == '$') {
        return;
    }

    auto check_double = [this, &check_scope](analyse::scopes::Scope const *sc) {
        auto dummy = analyse::utils::type_utils::GenericInferenceMap();
        const auto ext = sc->ast->to<SupPrototypeExtensionAst>();
        return ext and
            analyse::utils::type_utils::relaxed_symbolic_eq(*ext->name, *name, *sc, check_scope, dummy, false) and
            analyse::utils::type_utils::symbolic_eq(*ext->super_class, *super_class, *sc, check_scope, false);
    };

    // Prevent double inheritance by checking if the scopes are already registered the other way around.
    auto dummy = analyse::utils::type_utils::GenericInferenceMap();
    auto all_sup_scopes = cls_sym.scope->sup_scopes();
    const auto existing_sup_scopes = all_sup_scopes
        | genex::views::filter(check_double)
        | genex::views::transform([](auto *x) { return std::make_pair(x, x->ast->template to<SupPrototypeExtensionAst>()); })
        | genex::to<std::vector>();

    raise_if<analyse::errors::SppSuperimpositionDoubleExtensionError>(
        not existing_sup_scopes.empty(), {&check_scope},
        ERR_ARGS(*existing_sup_scopes[0].second->super_class, *name));
}


auto spp::asts::SupPrototypeExtensionAst::check_self_extension(
    analyse::scopes::Scope &check_scope) const
    -> void {
    // Check if the superimposition is extending itself.
    raise_if<analyse::errors::SppSuperimpositionSelfExtensionError>(
        analyse::utils::type_utils::symbolic_eq(*name, *super_class, check_scope, check_scope), {&check_scope},
        ERR_ARGS(*name, *super_class));
}


auto spp::asts::SupPrototypeExtensionAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Don't need to pre-process function-classes -- they are introduced from preprocessing functions.
    if (name->type_parts().back()->name[0] == '$') { return; }
    Ast::stage_1_pre_process(ctx);

    // Substitute the "Self" parameter's type with the name of the type being superimposed over.
    const auto self_gen_sub = std::make_unique<GenericArgumentTypeKeywordAst>(
        generate::common_types::self_type(pos_start()), nullptr, name);
    auto gen_sub = std::vector<GenericArgumentAst*>(1);
    gen_sub[0] = self_gen_sub.get();
    super_class = super_class->substitute_generics(gen_sub);

    // Preprocess the implementation.
    impl->stage_1_pre_process(this);

    // Todo: some sort of check that prevents something like "sup [T] T ext Borrow[T]", because this is infinite generation
}


auto spp::asts::SupPrototypeExtensionAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create a new scope for the superimposition extension.
    auto scope_name = analyse::scopes::ScopeBlockName::from_parts(
        "sup-prototype-extension", {name.get(), super_class.get()}, pos_start());
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    Ast::stage_2_gen_top_level_scopes(sm, meta);

    // Check there are optional generic parameters.
    const auto optional = generic_param_group->get_optional_params();
    raise_if<analyse::errors::SppSuperimpositionOptionalGenericParameterError>(
        not optional.empty(), {sm->current_scope},
        ERR_ARGS(*optional[0]));

    // Check every generic parameter is constrained by the type.
    if (name->type_parts().back()->name[0] != '$') {
        const auto unconstrained = generic_param_group->get_all_params()
            | genex::views::filter([this](auto &&x) { return not(name->contains_generic(*x) or super_class->contains_generic(*x)); })
            | genex::to<std::vector>();
        raise_if<analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError>(
            not unconstrained.empty(), {sm->current_scope},
            ERR_ARGS(*unconstrained[0]));
    }

    // Generate symbols for the generic parameter group, and the self type.
    generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    impl->stage_2_gen_top_level_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeExtensionAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward to the implementation.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_3_gen_top_level_aliases(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeExtensionAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward to the implementation.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    generic_param_group->stage_4_qualify_types(sm, meta);
    impl->stage_4_qualify_types(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeExtensionAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move into the superimposition scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // Analyse the type being superimposed over.
    name->stage_7_analyse_semantics(sm, meta);
    SPP_ENFORCE_SECOND_CLASS_BORROW_VIOLATION(name, name, *sm, "superimposition type");
    name = sm->current_scope->get_type_symbol(name)->fq_name();

    // Register the superimposition against the base symbol.
    const auto base_cls_sym = sm->current_scope->get_type_symbol(name->without_generics());
    if (sm->current_scope->parent == sm->current_scope->parent_module()) {
        if (not base_cls_sym->is_generic) {
            analyse::scopes::ScopeManager::normal_sup_blocks[base_cls_sym.get()].emplace_back(sm->current_scope);
        }
        else {
            analyse::scopes::ScopeManager::generic_sup_blocks.emplace_back(sm->current_scope);
        }
    }

    // Add the "Self" symbol into the scope.
    if (name->type_parts().back()->name[0] != '$') {
        const auto cls_sym = sm->current_scope->get_type_symbol(name);
        const auto self_sym = std::make_shared<analyse::scopes::TypeSymbol>(
            std::make_unique<TypeIdentifierAst>(name->pos_start(), "Self", nullptr),
            cls_sym->type, cls_sym->scope, sm->current_scope);
        self_sym->alias_stmt = std::make_unique<TypeStatementAst>(
            SPP_NO_ANNOTATIONS, nullptr,
            TypeIdentifierAst::from_string("Self"), nullptr, nullptr, name);
        sm->current_scope->add_type_symbol(self_sym);
        sm->current_scope->get_type_symbol(name)->aliased_by_symbols.emplace_back(self_sym);
    }

    // Analyse the supertype after Self has been added (allows use in generic arguments to the superclass).
    super_class->stage_7_analyse_semantics(sm, meta);
    SPP_ENFORCE_SECOND_CLASS_BORROW_VIOLATION(super_class, super_class, *sm, "superimposition supertype");
    super_class = sm->current_scope->get_type_symbol(super_class)->fq_name();

    // Check the supertype is not generic.
    const auto sup_sym = sm->current_scope->get_type_symbol(super_class);
    raise_if<analyse::errors::SppGenericTypeInvalidUsageError>(
        sup_sym->is_generic, {sm->current_scope},
        ERR_ARGS(*super_class, *super_class, "superimposition supertype"));

    // Load the implementation and move out of the scope.
    impl->stage_5_load_super_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeExtensionAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    name->stage_7_analyse_semantics(sm, meta);
    // super_class->stage_7_analyse_semantics(sm, meta);

    // Get the symbols.
    const auto cls_sym = sm->current_scope->get_type_symbol(name);
    const auto sup_sym = sm->current_scope->get_type_symbol(super_class);

    auto sup_scopes = sm->current_scope->get_type_symbol(super_class)->scope->sup_scopes();
    sup_scopes.insert(sup_scopes.begin(), sup_sym->scope);
    genex::actions::remove_if(sup_scopes, [](auto &&x) { return x->ast->template to<ClassPrototypeAst>() == nullptr; });

    // Mark the class as copyable if the "Copy" type is the supertype.
    for (const auto sup_scope : sup_scopes) {
        const auto fq_name = sup_scope->ty_sym->fq_name();
        if (analyse::utils::type_utils::symbolic_eq(*fq_name, *generate::common_types_precompiled::COPY, *sup_scope, *sm->current_scope)) {
            sm->current_scope->get_type_symbol(name->without_generics())->is_directly_copyable = true;
            cls_sym->is_directly_copyable = true;
            break;
        }
    }

    // Check every member on the superimposition exists on the supertype.
    for (auto &&member : impl->members) {
        if (const auto ext_member = member->to<SupPrototypeExtensionAst>()) {
            // Get the method and identify the base method it is overriding.
            const auto this_method = ext_member->impl->final_member()->to<FunctionPrototypeAst>();
            const auto base_method = analyse::utils::func_utils::check_for_conflicting_override(
                *member->get_ast_scope(), sup_sym->scope, *this_method, *sm, meta);

            // Check the base method exists.
            raise_if<analyse::errors::SppSuperimpositionExtensionMethodInvalidError>(
                base_method == nullptr, {sm->current_scope},
                ERR_ARGS(*this_method->name, *super_class));

            // Check the base method is virtual or abstract.
            raise_if<analyse::errors::SppSuperimpositionExtensionNonVirtualMethodOverriddenError>(
                not(base_method->abstract_annotation or base_method->virtual_annotation), {sm->current_scope},
                ERR_ARGS(*this_method->name, *base_method->name, *super_class));
        }

        else if (const auto type_member = member->to<TypeStatementAst>()) {
            // Get the associated type from the supertype directly.
            const auto this_type = type_member->new_type;
            const auto base_type = sup_sym->scope->get_type_symbol(this_type, true);

            // Check to see if the base type exists.
            raise_if<analyse::errors::SppSuperimpositionExtensionTypeStatementInvalidError>(
                base_type == nullptr, {sm->current_scope},
                ERR_ARGS(*type_member, *super_class));
        }

        else if (const auto cmp_member = member->to<CmpStatementAst>()) {
            // Get the associated type from the supertype directly.
            const auto this_const = cmp_member->name;
            const auto base_const = sup_sym->scope->get_var_symbol(this_const, true);

            // Check to see if the base type exists.
            raise_if<analyse::errors::SppSuperimpositionExtensionCmpStatementInvalidError>(
                base_const == nullptr, {sm->current_scope},
                ERR_ARGS(*cmp_member, *super_class));
        }
    }

    // Pre-analyse the implementation.
    impl->stage_6_pre_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeExtensionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    // name->stage_7_analyse_semantics(sm, meta);
    // super_class->stage_7_analyse_semantics(sm, meta);
    impl->stage_7_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeExtensionAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_8_check_memory(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeExtensionAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_9_comptime_resolution(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeExtensionAst::stage_10_code_gen_1(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Move to the next scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_10_code_gen_1(sm, meta, ctx);
    sm->move_out_of_current_scope();
    return nullptr;
}


auto spp::asts::SupPrototypeExtensionAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Move to the next scope.
    sm->move_to_next_scope();
    // SPP_ASSERT(sm->current_scope == m_scope);

    // Check if this block is purely generic.
    const auto is_generic_scope =
        genex::any_of(sm->current_scope->all_type_symbols(true), [](auto &&x) { return x->is_generic; }) or
        genex::any_of(sm->current_scope->all_var_symbols(true), [](auto &&x) { return x->memory_info->ast_comptime == nullptr; });

    // Generate the implementation if not a generic scope.
    if (not is_generic_scope) {
        impl->stage_11_code_gen_2(sm, meta, ctx);
    }

    // Generic sup block so not generating for it.
    // Manual scope skipping.
    else {
        const auto final_scope = sm->current_scope->final_child_scope();
        while (sm->current_scope != final_scope) {
            sm->move_to_next_scope(false);
        }
    }

    sm->move_out_of_current_scope();

    return nullptr;
}
