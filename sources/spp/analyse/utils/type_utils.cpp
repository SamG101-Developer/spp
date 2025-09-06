#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/class_attribute_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_comp_ast.hpp>
#include <spp/asts/generic_argument_comp_keyword_ast.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/generic_argument_type_positional_ast.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/integer_literal_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/mixins/compiler_stages.hpp>
#include <spp/utils/strings.hpp>

#include <genex/actions/concat.hpp>
#include <genex/algorithms/contains.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/views/cast.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/flatten.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/transform.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/remove_if.hpp>
#include <genex/views/to.hpp>
#include <opex/cast.hpp>


auto spp::analyse::utils::type_utils::is_type_indexable(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Test for the tuple or array type.
    return
        is_type_tuple(*type.without_generics(), scope) or is_type_array(*type.without_generics(), scope);
}


auto spp::analyse::utils::type_utils::is_type_array(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::array::Arr[T, n]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::ARR, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_tuple(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::tuple::Tup[Ts...]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::TUP, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_variant(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::variant::Variant[Ts...]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::VAR, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_boolean(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::bool::Bool".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::BOOL, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_void(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::void::Void".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::VOID, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_generator(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::generator::Gen[T]/GenOpt[T]/GenRes[T, E]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::GEN, scope, scope) or
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::GEN_OPT, scope, scope) or
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::GEN_RES, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_try(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::try::Try[Ok, Err]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::TRY, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_function(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::function::FunMov[Ret, Args]/FunMut[Ret, Args]/FunRef[Ret, Args]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::FUN_MOV, scope, scope) or
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::FUN_MUT, scope, scope) or
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::FUN_REF, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_recursive(
    asts::ClassPrototypeAst const &type,
    scopes::ScopeManager const &sm)
    -> std::shared_ptr<asts::TypeAst> {
    // Define the internal function to recursively search for attribute types.
    auto get_attr_types = [](this auto &&self, const asts::ClassPrototypeAst *cls_proto, const scopes::Scope *cls_scope) -> std::generator<std::pair<const asts::ClassPrototypeAst*, std::shared_ptr<asts::TypeAst>>> {
        for (const auto attr : cls_proto->impl->members | genex::views::ptr | genex::views::cast_dynamic<asts::ClassAttributeAst*>()) {
            const auto attr_type_sym = cls_scope->get_type_symbol(*attr->type, true);
            if (attr_type_sym and attr_type_sym->type != nullptr) {
                co_yield std::make_tuple(attr_type_sym->type, attr->type);
                for (auto &&tup : self(attr_type_sym->type, attr_type_sym->scope)) {
                    co_yield std::move(tup);
                }
            }
        }
    };

    // Get the attribute types recursively from the class prototype, and check for a match with the class prototype.
    for (auto [cls_proto, attr_ast] : get_attr_types(&type, sm.current_scope)) {
        if (cls_proto == &type) {
            return attr_ast;
        }
    }

    // No recursive type was found, so return nullptr.
    return nullptr;
}


auto spp::analyse::utils::type_utils::is_index_within_type_bound(
    const std::size_t index,
    asts::TypeAst const &type,
    scopes::Scope const &sm) -> bool {
    // For tuples, count the number of generic arguments.
    if (is_type_tuple(type, sm)) {
        return index < type.type_parts().back()->generic_arg_group->args.size();
    }

    // For arrays, check the size argument.
    if (is_type_array(type, sm)) {
        const auto size_arg = type.type_parts().back()->generic_arg_group->comp_at("n");
        const auto size_arg_cast = asts::ast_cast<asts::IntegerLiteralAst>(size_arg);
        return index < std::stoull(size_arg_cast->val->token_data);
    }

    std::unreachable();
}


auto spp::analyse::utils::type_utils::get_nth_type_of_indexable_type(
    const std::size_t index,
    asts::TypeAst const &type,
    scopes::Scope const &sm)
    -> std::shared_ptr<asts::TypeAst> {
    // For tuples, return the nth generic argument.
    if (is_type_tuple(type, sm)) {
        return type.type_parts().back()->generic_arg_group->get_type_args()[index]->val;
    }

    // For arrays, return the element type.
    if (is_type_array(type, sm)) {
        return type.type_parts().back()->generic_arg_group->get_type_args()[0]->val;
    }

    std::unreachable();
}


auto spp::analyse::utils::type_utils::get_generator_and_yield_type(
    asts::TypeAst const &type,
    scopes::ScopeManager const &sm,
    asts::ExpressionAst const &expr,
    std::string_view what)
    -> std::tuple<std::shared_ptr<const asts::TypeAst>, std::shared_ptr<asts::TypeAst>, bool, bool, bool, std::shared_ptr<asts::TypeAst>> {
    // Generic types are not generators, so raise an error.
    const auto type_sym = sm.current_scope->get_type_symbol(type);
    if (type_sym->scope == nullptr) {
        errors::SemanticErrorBuilder<errors::SppExpressionNotGeneratorError>().with_args(
            expr, type, what).with_scopes({sm.current_scope}).raise();
    }

    // Discover the supertypes and add the current type to it.=.
    auto sup_types = genex::views::concat(std::vector{type.shared_from_this()}, type_sym->scope->sup_types());

    // Search through the supertypes for a direct generator type.
    const auto generator_type_candidates = sup_types
        | genex::views::filter([&sm](auto &&sup_type) { return is_type_generator(*sup_type, *sm.current_scope); })
        | genex::views::to<std::vector>();

    if (generator_type_candidates.empty()) {
        errors::SemanticErrorBuilder<errors::SppExpressionNotGeneratorError>().with_args(
            expr, type, what).with_scopes({sm.current_scope}).raise();
    }
    if (generator_type_candidates.size() > 1) {
        errors::SemanticErrorBuilder<errors::SppExpressionAmbiguousGeneratorError>().with_args(
            expr, type, what).with_scopes({sm.current_scope}).raise();
    }

    // Extract the generator and yield type.
    auto generator_type = generator_type_candidates[0];
    auto yield_Type = generator_type->type_parts().back()->generic_arg_group->type_at("Yield")->val;

    // Extract the multiplicity, optionality and fallibility from the generator type.
    auto is_once = symbolic_eq(*asts::generate::common_types_precompiled::GEN_ONCE, *generator_type, *sm.current_scope, *sm.current_scope);
    auto is_optional = symbolic_eq(*asts::generate::common_types_precompiled::GEN_OPT, *generator_type, *sm.current_scope, *sm.current_scope);
    auto is_fallible = symbolic_eq(*asts::generate::common_types_precompiled::GEN_RES, *generator_type, *sm.current_scope, *sm.current_scope);

    // Get the error type if the generator is fallible.
    auto error_type = is_fallible ? generator_type->type_parts().back()->generic_arg_group->type_at("Err")->val : nullptr;

    // Return all the information about the generator type.
    return std::make_tuple(generator_type, yield_Type, is_once, is_optional, is_fallible, error_type);
}


auto spp::analyse::utils::type_utils::get_try_type(
    asts::TypeAst const &type,
    asts::ExpressionAst const &expr,
    scopes::ScopeManager const &sm)
    -> std::shared_ptr<const asts::TypeAst> {
    // Generic types are not Try types, so return nullptr.
    const auto type_sym = sm.current_scope->get_type_symbol(type);
    if (type_sym->scope == nullptr) {
        return nullptr;
    }

    // Discover the supertypes and add the current type to it.
    auto sup_types = genex::views::concat(std::vector{type.shared_from_this()}, type_sym->scope->sup_types());

    // Search through the supertypes for a direct Try type.
    const auto try_type_candidates = sup_types
        | genex::views::filter([&sm](auto &&sup_type) { return is_type_try(*sup_type, *sm.current_scope); })
        | genex::views::to<std::vector>();

    if (try_type_candidates.empty()) {
        errors::SemanticErrorBuilder<errors::SppEarlyReturnRequiresTryTypeError>().with_args(
            expr, type).with_scopes({sm.current_scope}).raise();
    }

    // Extract the Try type and return it.
    return try_type_candidates[0];
}


template <typename T>
auto spp::analyse::utils::type_utils::validate_inconsistent_types(
    std::vector<T> const &branches,
    scopes::ScopeManager const *sm,
    asts::mixins::CompilerMetaData *meta)
    -> std::tuple<std::pair<asts::Ast*, asts::TypeAst*>, std::vector<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>>> {
    // Collect type information for each branch, pairing the branch with its inferred type.
    auto branches_type_info = branches
        | genex::views::transform([sm, meta](auto &&x) { return std::make_pair(x.get(), x->infer_type(sm, meta)); })
        | genex::views::to<std::vector>();

    // Filter the branch types down to variant types for custom analysis.
    auto variant_branches_type_info = branches_type_info
        | genex::views::filter([sm](auto &&x) { return type_utils::is_type_variant(*x.second, *sm->current_scope); })
        | genex::views::to<std::vector>();

    // Set the master branch type to the first branch's type, if it exists. This is the default and may be subsequently changed.
    auto master_branch_type_info = not branches.empty()
                                       ? std::make_pair(branches_type_info[0].first, branches_type_info[0].second.get())
                                       : std::make_pair(nullptr, nullptr);

    // Override the master type if a pre-provided type (for assignment) has been given.
    if (meta->assignment_target_type != nullptr) {
        master_branch_type_info = std::make_pair(nullptr, meta->assignment_target_type.get());
    }

    // Otherwise, if there are variant branches, use the most variant type as the master branch type.
    else if (not variant_branches_type_info.empty()) {
        auto most_inner_types = 0uz;
        for (auto &&[variant_branch, variant_type] : variant_branches_type_info) {
            const auto variant_size = variant_type->type_parts().back()->generic_args->type_at("Variant")->val->type_parts().back()->generic_args->args.size();
            if (variant_size > most_inner_types) {
                master_branch_type_info = std::make_tuple(variant_branch, variant_type.get());
                most_inner_types = variant_size;
            }
        }
    }

    // Remove the master branch pointer from the list of remaining branch types and check all types match.
    auto mismatch_branches_type_info = branches_type_info
        | genex::views::remove_if([master_branch_type_info](auto &&x) { return x.first == master_branch_type_info.first; })
        | genex::views::filter([master_branch_type_info, sm](auto &&x) { return not type_utils::symbolic_eq(*master_branch_type_info.second, *x.second, *sm->current_scope, *sm->current_scope); })
        | genex::views::to<std::vector>();

    if (mismatch_branches_type_info.empty()) {
        auto [mismatch_branch, mismatch_branch_type] = std::move(mismatch_branches_type_info[0]);
        auto [master_branch, master_branch_type] = master_branch_type_info;
        analyse::errors::SemanticErrorBuilder<errors::SppTypeMismatchError>().with_args(
            *master_branch->body->final_member(), *master_branch_type, *mismatch_branch->body->final_member(), *mismatch_branch_type).scopes({sm->current_scope}).raise();
    }

    return std::make_tuple(master_branch_type_info, branches_type_info);
}


auto spp::analyse::utils::type_utils::get_all_attrs(
    asts::TypeAst const &type,
    scopes::ScopeManager const *sm)
    -> std::vector<std::pair<asts::ClassAttributeAst*, scopes::Scope*>> {
    // Get the symbol of the class type.
    const auto cls_sym = sm->current_scope->get_type_symbol(type);

    // Get the attribute information from the class type and all super types.
    auto all_scopes = cls_sym->scope->sup_scopes();
    all_scopes |= genex::actions::concat(std::vector{cls_sym->scope});
    auto all_attrs = all_scopes
        | genex::views::filter([](auto &&sup_scope) { return ast_cast<asts::ClassPrototypeAst>(sup_scope->ast) != nullptr; })
        | genex::views::transform([](auto &&sup_scope) {
            return ast_cast<asts::ClassPrototypeAst>(sup_scope->ast)->impl->members
                | genex::views::ptr
                | genex::views::cast_dynamic<asts::ClassAttributeAst*>()
                | genex::views::transform([sup_scope](auto &&x) { return std::make_pair(x, sup_scope); })
                | genex::views::to<std::vector>();
        })
        | genex::views::flatten
        | genex::views::to<std::vector>();

    return all_attrs;
}


auto spp::analyse::utils::type_utils::create_generic_cls_scope(
    asts::TypeIdentifierAst &type_part,
    scopes::TypeSymbol const &old_cls_sym,
    std::vector<scopes::Symbol*> external_generic_syms,
    const bool is_tuple,
    scopes::ScopeManager *sm,
    asts::mixins::CompilerMetaData *meta)
    -> scopes::Scope* {
    // Create a new scope and symbol for the generic substituted type.
    const auto old_cls_scope = old_cls_sym.scope;
    auto new_cls_scope = std::make_unique<scopes::Scope>(&type_part, old_cls_scope->parent, old_cls_scope->ast);
    auto new_cls_symbol = std::make_unique<scopes::TypeSymbol>(
        asts::ast_cast<asts::TypeIdentifierAst>(type_part.shared_from_this()),
        asts::ast_cast<asts::ClassPrototypeAst>(new_cls_scope->ast), new_cls_scope.get(), sm->current_scope,
        old_cls_sym.is_generic, old_cls_sym.is_copyable, old_cls_sym.visibility);

    // Configure the new scope based on the base (old) scope.
    new_cls_scope->parent->add_type_symbol(std::move(new_cls_symbol));
    new_cls_scope->table = scopes::SymbolTable(old_cls_scope->table);
    new_cls_scope->non_generic_scope = old_cls_scope;
    new_cls_scope->children.emplace_back(std::move(new_cls_scope));
    if (not std::holds_alternative<scopes::ScopeBlockName>(new_cls_scope->name)) {
        old_cls_scope->parent->children.emplace_back(std::move(new_cls_scope));
    }

    if (meta->current_stage > 7) {
        sm->attach_specific_super_scopes(*new_cls_scope, meta);
    }

    // No more checks for tuples.
    if (is_tuple) {
        return new_cls_scope.get();
    }

    // Register the generic symbols.
    auto generic_syms = external_generic_syms
        | genex::views::transform([&](auto *g) { return std::shared_ptr<scopes::Symbol>(g); })
        | genex::views::concat(type_part.generic_arg_group->args | genex::views::transform([&](auto &&g) { return create_generic_sym(*g, *sm, meta); }));

    generic_syms
        | genex::views::cast_smart_ptr<scopes::TypeSymbol>()
        | genex::views::for_each([&new_cls_scope](auto &&e) { new_cls_scope->add_type_symbol(std::move(e)); });

    generic_syms
        | genex::views::cast_smart_ptr<scopes::VariableSymbol>()
        | genex::views::for_each([&new_cls_scope](auto &&e) { new_cls_scope->add_var_symbol(std::move(e)); });

    // Run generic substitution on the symbols in the scope.
    auto tm = scopes::ScopeManager(sm->global_scope, new_cls_scope.get());
    for (auto &&scoped_sym : new_cls_scope->all_var_symbols()) {
        scoped_sym->type = scoped_sym->type->substitute_generics(type_part.generic_arg_group->args | genex::views::ptr | genex::views::to<std::vector>());
        scoped_sym->type->stage_7_analyse_semantics(&tm, meta);
    }
    for (auto attr : asts::ast_cast<asts::ClassPrototypeAst>(old_cls_scope->ast)->impl->members | genex::views::ptr | genex::views::cast_dynamic<asts::ClassAttributeAst*>()) {
        const auto new_attr = ast_clone(attr);
        new_attr->type = new_attr->type->substitute_generics(type_part.generic_arg_group->args | genex::views::ptr | genex::views::to<std::vector>());
        new_attr->type->stage_7_analyse_semantics(&tm, meta);
    }

    // Return the new class scope.
    return old_cls_scope->parent->children.back().get();
}


auto spp::analyse::utils::type_utils::create_generic_fun_scope(
    scopes::Scope const &old_fun_scope,
    asts::GenericArgumentGroupAst const &generic_args,
    std::vector<scopes::Symbol*> external_generic_syms,
    scopes::ScopeManager *sm,
    asts::mixins::CompilerMetaData *meta)
    -> scopes::Scope* {
    // Create a new scope and symbol for the generic substituted function.
    auto new_fun_scope_name = std::get<asts::IdentifierAst*>(old_fun_scope.name);
    auto new_fun_scope = std::make_unique<scopes::Scope>(new_fun_scope_name, old_fun_scope.parent, old_fun_scope.ast);
    new_fun_scope->children = old_fun_scope.children
        | genex::views::transform([](auto &&scope) { return std::make_unique<scopes::Scope>(*scope); })
        | genex::views::to<std::vector>();
    for (auto &&c : new_fun_scope->children) {
        c->parent = new_fun_scope.get();
    }

    // Register the generic symbols.
    auto generic_syms = external_generic_syms
        | genex::views::transform([&](auto &&g) { return std::shared_ptr<scopes::Symbol>(g); })
        | genex::views::concat(generic_args.args | genex::views::transform([&](auto &&g) { return create_generic_sym(*g, *sm, meta); }));

    generic_syms
        | genex::views::cast_smart_ptr<scopes::TypeSymbol>()
        | genex::views::for_each([&new_fun_scope](auto e) { new_fun_scope->add_type_symbol(std::move(e)); });

    generic_syms
        | genex::views::cast_smart_ptr<scopes::VariableSymbol>()
        | genex::views::for_each([&new_fun_scope](auto e) { new_fun_scope->add_var_symbol(std::move(e)); });

    // Return the new function scope.
    return new_fun_scope.get();
}


auto spp::analyse::utils::type_utils::create_generic_sup_scope(
    scopes::Scope &old_sup_scope,
    scopes::Scope &new_cls_scope,
    asts::GenericArgumentGroupAst const &generic_args,
    std::vector<scopes::Symbol*> external_generic_syms,
    scopes::ScopeManager *sm,
    asts::mixins::CompilerMetaData *meta)
    -> std::tuple<scopes::Scope*, scopes::Scope*> {
    // Create a new scope for the generic substituted super scope.
    auto self_type = asts::ast_name(old_sup_scope.ast)->substitute_generics(generic_args.args | genex::views::ptr | genex::views::to<std::vector>());
    auto new_sup_scope_name = old_sup_scope.name;
    auto new_sup_scope = std::make_unique<scopes::Scope>(new_sup_scope_name, old_sup_scope.parent, old_sup_scope.ast);
    new_sup_scope->table = old_sup_scope.table;
    new_sup_scope->children = old_sup_scope.children
        | genex::views::transform([](auto &&scope) { return std::make_unique<scopes::Scope>(*scope); })
        | genex::views::to<std::vector>();
    old_sup_scope.parent->children.emplace_back(std::move(new_sup_scope));

    // Register the generic symbols.
    auto generic_syms = external_generic_syms
        | genex::views::transform([](auto *g) { return std::shared_ptr<scopes::Symbol>(g); })
        | genex::views::concat(generic_args.args | genex::views::transform([&](auto &&g) { return create_generic_sym(*g, *sm, meta); }));

    generic_syms
        | genex::views::cast_smart_ptr<scopes::TypeSymbol>()
        | genex::views::for_each([&old_sup_scope](auto &&e) { old_sup_scope.parent->add_type_symbol(std::move(e)); });

    generic_syms
        | genex::views::cast_smart_ptr<scopes::VariableSymbol>()
        | genex::views::for_each([&old_sup_scope](auto &&e) { old_sup_scope.parent->add_var_symbol(std::move(e)); });

    // Add the "Self" symbol into the new scope.
    self_type->stage_7_analyse_semantics(sm, meta);
    auto old_self_sym = new_sup_scope->get_type_symbol(*self_type);
    new_sup_scope->add_type_symbol(std::make_unique<scopes::AliasSymbol>(
        std::make_unique<asts::TypeIdentifierAst>(0, "Self", nullptr), nullptr, &new_cls_scope, new_sup_scope.get(),
        old_self_sym));

    // Run generic substitution on the aliases in the new scope.
    for (auto &&scoped_sym : new_sup_scope->all_type_symbols()) {
        if (auto scoped_alias_sym = dynamic_cast<scopes::AliasSymbol*>(scoped_sym); scoped_alias_sym != nullptr) {
            auto old_type_sub = scoped_alias_sym->old_sym->fq_name();
            scoped_alias_sym->old_sym = old_sup_scope.get_type_symbol(*old_type_sub)
                                            ? old_sup_scope.get_type_symbol(*old_type_sub)
                                            : scoped_alias_sym->old_sym;
            if (genex::algorithms::contains(old_sup_scope.children | genex::views::ptr | genex::views::to<std::vector>(), scoped_alias_sym->scope)) {
                scoped_alias_sym->scope = new_sup_scope->children[genex::algorithms::position(old_sup_scope.children | genex::views::ptr, [&](auto &&x) { return x == scoped_alias_sym->scope; }) as USize].get();
            }
        }
    }

    // Run generic substitution on the constants in the new scope.
    for (auto &&scoped_sym : new_sup_scope->all_var_symbols()) {
        auto old_type_sub = scoped_sym->type->substitute_generics(generic_args.args | genex::views::ptr | genex::views::to<std::vector>());
        scoped_sym->type = std::move(old_type_sub);
    }

    // Create the scope for the new super class type. This will handle recursive sup-scope creation.
    auto super_cls_scope = static_cast<scopes::Scope*>(nullptr);;
    if (const auto ext_ast = asts::ast_cast<asts::SupPrototypeExtensionAst>(old_sup_scope.ast); ext_ast != nullptr) {
        const auto new_fq_super_type = ext_ast->super_class->substitute_generics(generic_args.args | genex::views::ptr | genex::views::to<std::vector>());
        new_fq_super_type->stage_7_analyse_semantics(sm, meta);
        super_cls_scope = new_cls_scope.get_type_symbol(*new_fq_super_type)->scope;
    }

    return std::make_tuple(&new_cls_scope, super_cls_scope);
}


auto spp::analyse::utils::type_utils::create_generic_sym(
    asts::GenericArgumentAst const &generic,
    scopes::ScopeManager &sm,
    asts::mixins::CompilerMetaData *meta,
    scopes::ScopeManager *tm)
    -> std::shared_ptr<scopes::Symbol> {
    // Handle the generic type argument => creates a type symbol.
    if (const auto type_arg = asts::ast_cast<asts::GenericArgumentTypeKeywordAst>(&generic); type_arg != nullptr) {
        const auto true_val_sym = sm.current_scope->get_type_symbol(*type_arg->val);
        auto sym = std::make_unique<scopes::TypeSymbol>(
            type_arg->name->type_parts().back(), true_val_sym ? true_val_sym->type : nullptr,
            true_val_sym ? true_val_sym->scope : nullptr, sm.current_scope, true,
            true_val_sym ? true_val_sym->is_copyable : false, asts::utils::Visibility::PUBLIC,
            type_arg->val->get_convention());
        return sym;
    }

    // Handle the generic comp argument => creates a variable symbol.
    if (const auto comp_arg = asts::ast_cast<asts::GenericArgumentCompKeywordAst>(&generic); comp_arg != nullptr) {
        auto sym = std::make_unique<scopes::VariableSymbol>(
            asts::IdentifierAst::from_type(*comp_arg->name),
            (tm ? *tm : sm).current_scope->get_type_symbol(*comp_arg->val->infer_type(tm ? tm : &sm, meta))->fq_name(),
            false, true, asts::utils::Visibility::PUBLIC);
        sym->memory_info->ast_comptime = comp_arg;
        return sym;
    }

    std::unreachable();
}


auto spp::analyse::utils::type_utils::get_type_part_symbol_with_error(
    scopes::Scope const &scope,
    asts::TypeIdentifierAst const &type_part,
    scopes::ScopeManager const &sm,
    asts::mixins::CompilerMetaData *meta,
    const bool ignore_alias)
    -> scopes::TypeSymbol* {
    // Get the type part's symbol, and raise an error if it doesn't exist.
    const auto type_sym = scope.get_type_symbol(type_part, ignore_alias, meta);
    if (type_sym == nullptr) {
        const auto alternatives = sm.current_scope->all_var_symbols()
            | genex::views::transform([](auto &&x) { return x->name->val; })
            | genex::views::remove_if([](auto &&x) { return x[0] == '$'; })
            | genex::views::to<std::vector>();

        const auto closest_match = spp::utils::strings::closest_match(type_part.name, alternatives);
        analyse::errors::SemanticErrorBuilder<errors::SppIdentifierUnknownError>().with_args(
            *type_part.without_generics(), "type", closest_match).with_scopes({sm.current_scope}).raise();
    }

    // Return the found type symbol.
    return type_sym;
}
