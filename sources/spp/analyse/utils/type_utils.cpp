#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/case_expression_branch_ast.hpp>
#include <spp/asts/class_attribute_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/function_parameter_variadic_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_comp_ast.hpp>
#include <spp/asts/generic_argument_comp_keyword_ast.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/generic_argument_type_positional_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/integer_literal_ast.hpp>
#include <spp/asts/iter_expression_branch_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>
#include <spp/asts/mixins/compiler_stages.hpp>
#include <spp/utils/strings.hpp>

#include <genex/actions/concat.hpp>
#include <genex/algorithms/any_of.hpp>
#include <genex/algorithms/contains.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/views/cast.hpp>
#include <genex/views/chunk.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/flatten.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/remove.hpp>
#include <genex/views/transform.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/remove_if.hpp>
#include <genex/views/to.hpp>
#include <opex/cast.hpp>

#include "spp/asts/function_prototype_ast.hpp"
#include "spp/lex/lexer.hpp"
#include "spp/parse/parser_spp.hpp"


auto spp::analyse::utils::type_utils::symbolic_eq(
    asts::TypeAst const &lhs_type,
    asts::TypeAst const &rhs_type,
    scopes::Scope const &lhs_scope,
    scopes::Scope const &rhs_scope,
    const bool check_variant,
    const bool lhs_ignore_alias)
    -> bool {
    // Special case for the "!" never type.
    if (rhs_type.is_never_type()) { return true; }
    if (lhs_type.is_never_type()) { return rhs_type.is_never_type(); }

    // Do a convention check, and allow "&mut" to coerce to "&".
    const auto lhs_conv = lhs_type.get_convention();
    const auto rhs_conv = rhs_type.get_convention();
    if ((lhs_conv and *lhs_conv != rhs_conv) or (not lhs_conv and rhs_conv)) {
        if (not((lhs_conv and *lhs_conv == asts::ConventionAst::ConventionTag::REF) and (rhs_conv and *rhs_conv == asts::ConventionAst::ConventionTag::MUT))) {
            return false;
        }
    }

    // Strip the generics from the types.
    const auto stripped_lhs = lhs_type.without_generics();
    const auto stripped_rhs = rhs_type.without_generics();

    // Get the non-generic symbols.
    const auto stripped_lhs_sym = lhs_scope.get_type_symbol(stripped_lhs, false, lhs_ignore_alias);
    const auto stripped_rhs_sym = rhs_scope.get_type_symbol(stripped_rhs, false);

    // If the left-hand-side is a "Variant" type, check the composite types first.
    if (check_variant and symbolic_eq(*stripped_lhs_sym->fq_name()->without_generics(), *asts::generate::common_types_precompiled::VAR, lhs_scope, lhs_scope, false)) {
        auto lhs_composite_types = deduplicate_variant_inner_types(*lhs_scope.get_type_symbol(lhs_type.shared_from_this())->fq_name(), lhs_scope);
        if (genex::algorithms::any_of(lhs_composite_types, [&](auto &&lhs_composite_type) { return symbolic_eq(*lhs_composite_type, rhs_type, lhs_scope, rhs_scope); })) {
            return true;
        }
    }

    // If the stripped types are not equal, return false (comparing by address is fine -- ClassPrototypeAst* nodes).
    if (stripped_lhs_sym->type != stripped_rhs_sym->type) {
        return false;
    }

    // Get the generic arguments for both types.
    const auto lhs_type_fq = lhs_scope.get_type_symbol(lhs_type.shared_from_this(), false, lhs_ignore_alias)->fq_name();
    const auto rhs_type_fq = rhs_scope.get_type_symbol(rhs_type.shared_from_this(), false)->fq_name();

    auto &lhs_generics = lhs_type_fq->type_parts().back()->generic_arg_group->args;
    auto &rhs_generics = rhs_type_fq->type_parts().back()->generic_arg_group->args;

    // Special case for variadic parameter types.
    const auto temp_type_proto = lhs_scope.get_type_symbol(lhs_type.shared_from_this())->type;
    if (temp_type_proto and not temp_type_proto->generic_param_group->params.empty()) {
        if (asts::ast_cast<asts::FunctionParameterVariadicAst>(temp_type_proto->generic_param_group->params.back().get()) != nullptr) {
            if (lhs_generics.size() != rhs_generics.size()) {
                return false;
            }
        }
    }

    // Ensure each generic argument is symbolically equal to the other.
    for (auto [lhs_generic, rhs_generic] : genex::views::zip(lhs_generics | genex::views::ptr, rhs_generics | genex::views::ptr) | genex::views::to<std::vector>()) {
        if (asts::ast_cast<asts::GenericArgumentTypeAst>(lhs_generic)) {
            const auto lhs_generic_part = asts::ast_cast<asts::GenericArgumentTypeAst>(lhs_generic);
            const auto rhs_generic_part = asts::ast_cast<asts::GenericArgumentTypeAst>(rhs_generic);
            if (not symbolic_eq(*lhs_generic_part->val, *rhs_generic_part->val, lhs_scope, rhs_scope)) {
                return false;
            }
        }
        else {
            const auto lhs_generic_part = asts::ast_cast<asts::GenericArgumentCompAst>(lhs_generic);
            const auto rhs_generic_part = asts::ast_cast<asts::GenericArgumentCompAst>(rhs_generic);
            if (not symbolic_eq(*lhs_generic_part->val, *rhs_generic_part->val, lhs_scope, rhs_scope)) {
                return false;
            }
        }
    }

    // If all the generic arguments are symbolically equal, return true.
    return true;
}


auto spp::analyse::utils::type_utils::symbolic_eq(
    asts::ExpressionAst const &lhs_expr,
    asts::ExpressionAst const &rhs_expr,
    scopes::Scope const &,
    scopes::Scope const &)
    -> bool {
    // Simple equality between the expressions.
    return lhs_expr == rhs_expr;
}


auto spp::analyse::utils::type_utils::relaxed_symbolic_eq(
    asts::TypeAst const &lhs_type,
    asts::TypeAst const &rhs_type,
    scopes::Scope const *lhs_scope,
    scopes::Scope const *rhs_scope,
    std::map<std::shared_ptr<asts::TypeIdentifierAst>, asts::ExpressionAst const*, spp::utils::SymNameCmp<std::shared_ptr<asts::TypeIdentifierAst>>> &generic_args,
    const bool check_variant) -> bool {
    // Todo: Convention check?
    // If the right-hand-side scope is nullptr, the scope is generic so auto-match it.
    if (rhs_scope == nullptr) {
        return true;
    }

    // Strip the generics from the right-hand-side type (possible generic).
    const auto stripped_lhs = lhs_type.without_generics();
    const auto stripped_rhs = rhs_type.without_generics();

    // If the right-hand-side is generic, then return a match: "sup[T] T { ... }" matches all types.
    const auto stripped_lhs_sym = lhs_scope->get_type_symbol(stripped_lhs);
    const auto stripped_rhs_sym = rhs_scope->get_type_symbol(stripped_rhs);
    if (stripped_rhs_sym->is_generic) {
        generic_args[std::dynamic_pointer_cast<asts::TypeIdentifierAst>(stripped_rhs)] = &lhs_type;
        return true;
    }

    // If the left-hand-side is a "Variant" type, check the composite types first.
    // Todo: on the failure of a variant match in "any_of", does the generic map need rolling back?
    if (check_variant and symbolic_eq(*asts::generate::common_types_precompiled::VAR, *stripped_rhs_sym->fq_name()->without_generics(), *rhs_scope, *rhs_scope)) {
        auto rhs_composite_types = deduplicate_variant_inner_types(*rhs_scope->get_type_symbol(rhs_type.shared_from_this())->fq_name(), *rhs_scope);
        if (genex::algorithms::any_of(rhs_composite_types, [&](auto &&rhs_composite_type) { return relaxed_symbolic_eq(lhs_type, *rhs_composite_type, lhs_scope, rhs_scope, generic_args); })) {
            return true;
        }
    }

    // If the stripped types aren't equal, then return false.
    if (stripped_lhs_sym->type != stripped_rhs_sym->type) {
        return false;
    }

    // The next step is to get the generic arguments for both types.
    const auto lhs_type_fq = lhs_scope->get_type_symbol(lhs_type.shared_from_this())->fq_name();
    const auto rhs_type_fq = rhs_scope->get_type_symbol(rhs_type.shared_from_this())->fq_name();

    auto &lhs_generics = lhs_type_fq->type_parts().back()->generic_arg_group->args;
    auto &rhs_generics = rhs_type_fq->type_parts().back()->generic_arg_group->args;

    // Special case for variadic parameter types.
    const auto temp_type_proto = lhs_scope->get_type_symbol(lhs_type.shared_from_this())->type;
    if (temp_type_proto and not temp_type_proto->generic_param_group->params.empty()) {
        if (asts::ast_cast<asts::FunctionParameterVariadicAst>(temp_type_proto->generic_param_group->params.back().get()) != nullptr) {
            if (lhs_generics.size() != rhs_generics.size()) {
                return false;
            }
        }
    }

    // Ensure each generic argument is symbolically equal to the other.
    for (auto [lhs_generic, rhs_generic] : genex::views::zip(lhs_generics | genex::views::ptr, rhs_generics | genex::views::ptr) | genex::views::to<std::vector>()) {
        if (asts::ast_cast<asts::GenericArgumentTypeAst>(rhs_generic)) {
            const auto lhs_generic_part = asts::ast_cast<asts::GenericArgumentTypeAst>(lhs_generic);
            const auto rhs_generic_part = asts::ast_cast<asts::GenericArgumentTypeAst>(rhs_generic);
            if (not relaxed_symbolic_eq(*lhs_generic_part->val, *rhs_generic_part->val, lhs_scope, rhs_scope, generic_args)) {
                return false;
            }
        }
        else {
            const auto lhs_generic_part = asts::ast_cast<asts::GenericArgumentCompAst>(lhs_generic);
            const auto rhs_generic_part = asts::ast_cast<asts::GenericArgumentCompAst>(rhs_generic);
            if (not relaxed_symbolic_eq(*lhs_generic_part->val, *rhs_generic_part->val, *lhs_scope, *rhs_scope, generic_args)) {
                return false;
            }
        }
    }

    // If all the generic arguments are symbolically equal, return true.
    return true;
}


auto spp::analyse::utils::type_utils::relaxed_symbolic_eq(
    asts::ExpressionAst const &lhs_expr,
    asts::ExpressionAst const &rhs_expr,
    scopes::Scope const &,
    scopes::Scope const &,
    std::map<std::shared_ptr<asts::TypeIdentifierAst>, asts::ExpressionAst const*, spp::utils::SymNameCmp<std::shared_ptr<asts::TypeIdentifierAst>>> &generic_args)
    -> bool {
    // Simple equality between the expressions, with generic matching.
    if (const auto rhs_expr_as_identifier = asts::ast_cast<asts::IdentifierAst>(&rhs_expr)) {
        generic_args[asts::TypeIdentifierAst::from_identifier(*rhs_expr_as_identifier)] = &lhs_expr;
        return true;
    }
    return lhs_expr == rhs_expr;
}


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
    // Get the attribute types recursively from the class prototype, and check for a match with the class prototype.
    for (auto [cls_proto, attr_ast] : get_attr_types(&type, sm.current_scope)) {
        if (cls_proto == &type) {
            return attr_ast;
        }
    }

    // No recursive type was found, so return nullptr.
    return nullptr;
}


auto spp::analyse::utils::type_utils::get_attr_types(
    const asts::ClassPrototypeAst *cls_proto,
    const scopes::Scope *cls_scope)
    -> std::generator<std::pair<const asts::ClassPrototypeAst*, std::shared_ptr<asts::TypeAst>>> {
    // Define the internal function to recursively search for attribute types.
    for (const auto attr : cls_proto->impl->members | genex::views::ptr | genex::views::cast_dynamic<asts::ClassAttributeAst*>()) {
        const auto attr_type_sym = cls_scope->get_type_symbol(attr->type, true);
        if (attr_type_sym and attr_type_sym->type != nullptr) {
            co_yield std::make_tuple(attr_type_sym->type, attr->type);
            for (auto &&tup : get_attr_types(attr_type_sym->type, attr_type_sym->scope)) {
                co_yield std::move(tup);
            }
        }
    }
}


auto spp::analyse::utils::type_utils::is_index_within_type_bound(
    const std::size_t index,
    asts::TypeAst const &type,
    scopes::Scope const &sm)
    -> bool {
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
    const auto type_sym = sm.current_scope->get_type_symbol(type.shared_from_this());
    if (type_sym->scope == nullptr) {
        errors::SemanticErrorBuilder<errors::SppExpressionNotGeneratorError>().with_args(
            expr, type, what).with_scopes({sm.current_scope}).raise();
    }

    // Discover the supertypes and add the current type to it.=.
    auto sup_types = std::vector{type.shared_from_this()}
        | genex::views::concat(type_sym->scope->sup_types())
        | genex::views::to<std::vector>();

    // Search through the supertypes for a direct generator type.
    const auto generator_type_candidates = sup_types
        | genex::views::filter([&sm](auto const &sup_type) { return is_type_generator(*sup_type, *sm.current_scope); })
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
    const auto type_sym = sm.current_scope->get_type_symbol(type.shared_from_this());
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
    scopes::ScopeManager *sm,
    asts::mixins::CompilerMetaData *meta)
    -> std::tuple<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>, std::vector<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>>> {
    // Collect type information for each branch, pairing the branch with its inferred type.
    auto branches_type_info = branches
        | genex::views::transform([sm, meta](auto *x) { return std::make_pair(x, x->infer_type(sm, meta)); })
        | genex::views::to<std::vector>();

    // Filter the branch types down to variant types for custom analysis.
    auto variant_branches_type_info = branches_type_info
        | genex::views::filter([sm](auto &&x) { return type_utils::is_type_variant(*x.second, *sm->current_scope); })
        | genex::views::to<std::vector>();

    // Set the master branch type to the first branch's type, if it exists. This is the default and may be subsequently changed.
    auto master_branch_type_info = not branches.empty()
                                       ? std::make_pair(branches_type_info[0].first, branches_type_info[0].second)
                                       : std::make_pair(nullptr, nullptr);

    // Override the master type if a pre-provided type (for assignment) has been given.
    if (meta->assignment_target_type != nullptr) {
        master_branch_type_info = std::make_pair(nullptr, meta->assignment_target_type);
    }

    // Otherwise, if there are variant branches, use the most variant type as the master branch type.
    else if (not variant_branches_type_info.empty()) {
        auto most_inner_types = 0uz;
        for (auto &&[variant_branch, variant_type] : variant_branches_type_info) {
            const auto variant_size = variant_type->type_parts().back()->generic_arg_group->type_at("Variant")->val->type_parts().back()->generic_arg_group->args.size();
            if (variant_size > most_inner_types) {
                master_branch_type_info = std::make_tuple(variant_branch, variant_type);
                most_inner_types = variant_size;
            }
        }
    }

    // Remove the master branch pointer from the list of remaining branch types and check all types match.
    auto mismatch_branches_type_info = branches_type_info
        | genex::views::remove_if([master_branch_type_info](auto const &x) { return x.first == master_branch_type_info.first; })
        | genex::views::filter([master_branch_type_info, sm](auto const &x) { return not type_utils::symbolic_eq(*master_branch_type_info.second, *x.second, *sm->current_scope, *sm->current_scope); })
        | genex::views::to<std::vector>();

    if (not mismatch_branches_type_info.empty()) {
        auto [mismatch_branch, mismatch_branch_type] = std::move(mismatch_branches_type_info[0]);
        auto [master_branch, master_branch_type] = master_branch_type_info;
        analyse::errors::SemanticErrorBuilder<errors::SppTypeMismatchError>().with_args(
            *master_branch->body->final_member(), *master_branch_type, *mismatch_branch->body->final_member(), *mismatch_branch_type).with_scopes({sm->current_scope}).raise();
    }

    // Cast to common AST nodes and return with the types.
    auto cast_master_branch_type_info = std::make_pair(asts::ast_cast<asts::Ast>(master_branch_type_info.first), master_branch_type_info.second);
    auto cast_branches_type_info = branches_type_info
        | genex::views::transform([](auto &&x) { return std::make_pair(asts::ast_cast<asts::Ast>(x.first), x.second); })
        | genex::views::to<std::vector>();
    return std::make_tuple(cast_master_branch_type_info, cast_branches_type_info);
}


auto spp::analyse::utils::type_utils::get_all_attrs(
    asts::TypeAst const &type,
    scopes::ScopeManager const *sm)
    -> std::vector<std::pair<asts::ClassAttributeAst*, scopes::Scope*>> {
    // Get the symbol of the class type.
    const auto cls_sym = sm->current_scope->get_type_symbol(type.shared_from_this());

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
    std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
    const bool is_tuple,
    const bool is_alias,
    scopes::ScopeManager *sm,
    asts::mixins::CompilerMetaData *meta)
    -> scopes::Scope* {
    // Create a new scope and symbol for the generic substituted type.
    const auto old_cls_scope = old_cls_sym.scope;
    auto new_cls_scope = std::make_unique<scopes::Scope>(
        std::dynamic_pointer_cast<asts::TypeIdentifierAst>(type_part.shared_from_this()),
        old_cls_scope->parent, old_cls_scope->ast);
    const auto new_cls_symbol = std::make_shared<scopes::TypeSymbol>(
        ast_clone(&type_part),
        asts::ast_cast<asts::ClassPrototypeAst>(new_cls_scope->ast), new_cls_scope.get(), sm->current_scope,
        old_cls_sym.is_generic, old_cls_sym.is_directly_copyable, old_cls_sym.visibility);
    new_cls_symbol->is_copyable = [&old_cls_sym] { return old_cls_sym.is_copyable(); };

    // Configure the new scope based on the base (old) scope.
    if (not is_alias) {
        new_cls_scope->parent->add_type_symbol(new_cls_symbol);
    }
    new_cls_scope->ty_sym = new_cls_symbol;
    new_cls_scope->table = old_cls_scope->table;
    new_cls_scope->non_generic_scope = old_cls_scope;
    auto new_cls_scope_ptr = new_cls_scope.get();
    if (not is_alias) {
        old_cls_scope->parent->children.emplace_back(std::move(new_cls_scope));
    }
    else {
        old_cls_scope->parent->temp_scopes.emplace_back(std::move(new_cls_scope));
    }

    if (meta->current_stage > 7) {
        sm->attach_specific_super_scopes(*new_cls_scope_ptr, meta);
    }

    // No more checks for tuples.
    if (is_tuple) {
        return new_cls_scope_ptr;
    }

    // Register the generic symbols.
    register_generic_syms(external_generic_syms, type_part.generic_arg_group->args, new_cls_scope_ptr, sm, meta);

    // Run generic substitution on the symbols in the scope.
    const auto substitution_generics = type_part.generic_arg_group->args
        | genex::views::ptr
        | genex::views::to<std::vector>();

    for (auto const &scoped_sym : new_cls_scope_ptr->all_var_symbols(true)) {
        scoped_sym->type = scoped_sym->type->substitute_generics(substitution_generics);
    }

    for (const auto *attr : asts::ast_cast<asts::ClassPrototypeAst>(old_cls_scope->ast)->impl->members | genex::views::ptr | genex::views::cast_dynamic<asts::ClassAttributeAst*>() | genex::views::to<std::vector>()) {
        const auto new_attr = ast_clone(attr);
        new_attr->type = new_attr->type->substitute_generics(substitution_generics);
        new_attr->stage_7_analyse_semantics(sm, meta);
    }

    // Return the new class scope.
    return new_cls_scope_ptr;
}


auto spp::analyse::utils::type_utils::create_generic_fun_scope(
    scopes::Scope const &old_fun_scope,
    asts::GenericArgumentGroupAst const &generic_args,
    std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
    scopes::ScopeManager *sm,
    asts::mixins::CompilerMetaData *meta)
    -> scopes::Scope* {
    // Create a new scope and symbol for the generic substituted function.
    auto new_fun_scope_name = std::get<scopes::ScopeBlockName>(old_fun_scope.name);
    auto new_fun_scope = std::make_unique<scopes::Scope>(new_fun_scope_name, old_fun_scope.parent, old_fun_scope.ast);
    new_fun_scope->children = old_fun_scope.children
        | genex::views::transform([](auto &&scope) { return std::make_unique<scopes::Scope>(*scope); })
        | genex::views::to<std::vector>();
    for (auto const &c : new_fun_scope->children) {
        c->parent = new_fun_scope.get();
    }

    auto new_fun_scope_ptr = new_fun_scope.get();
    const auto old_fn_proto = asts::ast_cast<asts::FunctionPrototypeAst>(asts::ast_body(old_fun_scope.ast)[0]);
    old_fn_proto->register_generic_substituted_scope(std::move(new_fun_scope));

    // Todo: Save this scope (unique pointer) against the base FunctionPrototype's, so that all versions of the function
    //  can be generated by the code generation stage.

    register_generic_syms(external_generic_syms, generic_args.args, new_fun_scope_ptr, sm, meta);

    // Return the new function scope.
    return new_fun_scope_ptr;
}


auto spp::analyse::utils::type_utils::create_generic_sup_scope(
    scopes::Scope &old_sup_scope,
    scopes::Scope &new_cls_scope,
    asts::GenericArgumentGroupAst const &generic_args,
    std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
    scopes::ScopeManager const *sm,
    asts::mixins::CompilerMetaData *meta)
    -> std::tuple<scopes::Scope*, scopes::Scope*> {
    // Create a new scope for the generic substituted super scope.
    const auto self_type = asts::ast_name(old_sup_scope.ast)->substitute_generics(generic_args.args | genex::views::ptr | genex::views::to<std::vector>());
    auto new_sup_scope_name = old_sup_scope.name;
    auto new_sup_scope = std::make_unique<scopes::Scope>(new_sup_scope_name, old_sup_scope.parent, old_sup_scope.ast);
    new_sup_scope->table = old_sup_scope.table;
    new_sup_scope->children = old_sup_scope.children
        | genex::views::transform([](auto &&scope) { return std::make_unique<scopes::Scope>(*scope); })
        | genex::views::to<std::vector>();
    auto new_sup_scope_ptr = new_sup_scope.get();
    old_sup_scope.parent->children.emplace_back(std::move(new_sup_scope));
    auto tm = scopes::ScopeManager(sm->global_scope, new_sup_scope_ptr);

    std::get<scopes::ScopeBlockName>(new_sup_scope_ptr->name).name =
        substitute_sup_scope_name(std::get<scopes::ScopeBlockName>(new_sup_scope_ptr->name).name, generic_args);

    // Register the generic symbols.
    register_generic_syms(external_generic_syms, generic_args.args, new_sup_scope_ptr, &tm, meta);

    // Add the "Self" symbol into the new scope.
    self_type->stage_7_analyse_semantics(&tm, meta);
    auto old_self_sym = new_sup_scope_ptr->get_type_symbol(self_type);
    new_sup_scope_ptr->add_type_symbol(std::make_unique<scopes::AliasSymbol>(
        std::make_unique<asts::TypeIdentifierAst>(0, "Self", nullptr),
        nullptr, &new_cls_scope, new_sup_scope_ptr, old_self_sym));

    // Run generic substitution on the aliases in the new scope.
    for (auto &&scoped_sym : new_sup_scope_ptr->all_type_symbols(true) | genex::views::to<std::vector>()) {
        if (auto scoped_alias_sym = std::dynamic_pointer_cast<scopes::AliasSymbol>(scoped_sym); scoped_alias_sym != nullptr and not scoped_sym->is_generic) {
            auto old_type_sub = scoped_alias_sym->old_sym->fq_name()->substitute_generics(generic_args.args | genex::views::ptr | genex::views::to<std::vector>());

            auto restore = scoped_alias_sym->old_sym;
            scoped_alias_sym->old_sym = old_sup_scope.get_type_symbol(old_type_sub);
            scoped_alias_sym->old_sym = scoped_alias_sym->old_sym ? scoped_alias_sym->old_sym : restore;

            auto old_sup_scope_children = old_sup_scope.children | genex::views::ptr | genex::views::to<std::vector>();
            if (genex::algorithms::contains(old_sup_scope_children, scoped_alias_sym->scope)) {
                scoped_alias_sym->scope = new_sup_scope_ptr->children[genex::algorithms::position(old_sup_scope_children, [&](auto &&x) { return x == scoped_alias_sym->scope; }) as USize].get();
            }
        }
    }

    // Run generic substitution on the constants in the new scope.
    for (auto const &scoped_sym : new_sup_scope_ptr->all_var_symbols(true)) {
        auto old_type_sub = scoped_sym->type->substitute_generics(generic_args.args | genex::views::ptr | genex::views::to<std::vector>());
        scoped_sym->type = std::move(old_type_sub);
    }

    // Create the scope for the new super class type. This will handle recursive sup-scope creation.
    auto super_cls_scope = static_cast<scopes::Scope*>(nullptr);;
    if (const auto ext_ast = asts::ast_cast<asts::SupPrototypeExtensionAst>(old_sup_scope.ast); ext_ast != nullptr) {
        const auto new_fq_super_type = ext_ast->super_class->substitute_generics(generic_args.args | genex::views::ptr | genex::views::to<std::vector>());
        new_fq_super_type->stage_7_analyse_semantics(&tm, meta);
        super_cls_scope = new_cls_scope.get_type_symbol(new_fq_super_type)->scope;
    }

    return std::make_tuple(new_sup_scope_ptr, super_cls_scope);
}


auto spp::analyse::utils::type_utils::create_generic_sym(
    asts::GenericArgumentAst const &generic,
    scopes::ScopeManager &sm,
    asts::mixins::CompilerMetaData *meta,
    scopes::ScopeManager *tm)
    -> std::shared_ptr<scopes::Symbol> {
    // Intercept creating "Out = Bool".

    // Handle the generic type argument => creates a type symbol.
    if (const auto type_arg = asts::ast_cast<asts::GenericArgumentTypeKeywordAst>(&generic); type_arg != nullptr) {
        const auto true_val_sym = sm.current_scope->get_type_symbol(type_arg->val);

        auto sym = std::make_unique<scopes::TypeSymbol>(
            type_arg->name->type_parts().back(), true_val_sym ? true_val_sym->type : nullptr,
            true_val_sym ? true_val_sym->scope : nullptr, sm.current_scope, true,
            true_val_sym ? true_val_sym->is_directly_copyable : false, asts::utils::Visibility::PUBLIC,
            ast_clone(type_arg->val->get_convention()));
        return sym;
    }

    // Handle the generic comp argument => creates a variable symbol.
    if (const auto comp_arg = asts::ast_cast<asts::GenericArgumentCompKeywordAst>(&generic); comp_arg != nullptr) {
        auto sym = std::make_unique<scopes::VariableSymbol>(
            asts::IdentifierAst::from_type(*comp_arg->name),
            (tm ? *tm : sm).current_scope->get_type_symbol(comp_arg->val->infer_type(tm ? tm : &sm, meta))->fq_name(),
            false, true, asts::utils::Visibility::PUBLIC);
        sym->memory_info->ast_comptime = ast_clone(comp_arg);
        return sym;
    }

    std::unreachable();
}


auto spp::analyse::utils::type_utils::register_generic_syms(
    std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
    std::vector<std::unique_ptr<asts::GenericArgumentAst>> const &generic_args,
    scopes::Scope *scope,
    scopes::ScopeManager *sm,
    asts::mixins::CompilerMetaData *meta)
    -> void {
    external_generic_syms
        | genex::views::cast_smart_ptr<scopes::TypeSymbol>()
        | genex::views::for_each([&](auto const &e) { scope->add_type_symbol(e); });

    external_generic_syms
        | genex::views::cast_smart_ptr<scopes::VariableSymbol>()
        | genex::views::for_each([&](auto const &e) { scope->add_var_symbol(e); });

    auto generic_syms = generic_args
        | genex::views::transform([&](auto const &g) { return create_generic_sym(*g, *sm, meta); })
        | genex::views::to<std::vector>();

    generic_syms
        | genex::views::cast_smart_ptr<scopes::TypeSymbol>()
        | genex::views::for_each([&](auto const &e) { scope->add_type_symbol(e); });

    generic_syms
        | genex::views::cast_smart_ptr<scopes::VariableSymbol>()
        | genex::views::for_each([&](auto const &e) { scope->add_var_symbol(e); });
}


auto spp::analyse::utils::type_utils::get_type_part_symbol_with_error(
    scopes::Scope const &scope,
    asts::TypeIdentifierAst const &type_part,
    scopes::ScopeManager const &sm,
    asts::mixins::CompilerMetaData *,
    const bool ignore_alias)
    -> scopes::TypeSymbol* {
    // Get the type part's symbol, and raise an error if it doesn't exist.
    const auto type_sym = scope.get_type_symbol(type_part.shared_from_this(), false, ignore_alias);
    if (type_sym == nullptr) {
        const auto alternatives = sm.current_scope->all_type_symbols()
            | genex::views::transform([](auto const &x) { return x->name->name; })
            | genex::views::remove_if([](auto const &x) { return x[0] == '$'; })
            | genex::views::to<std::vector>();

        const auto closest_match = spp::utils::strings::closest_match(type_part.name, alternatives);
        analyse::errors::SemanticErrorBuilder<errors::SppIdentifierUnknownError>().with_args(
            *type_part.without_generics(), "type", closest_match).with_scopes({sm.current_scope}).raise();
    }

    // Return the found type symbol.
    return type_sym.get();
}


auto spp::analyse::utils::type_utils::get_namespaced_scope_with_error(
    scopes::ScopeManager const &sm,
    asts::IdentifierAst const &ns)
    -> scopes::Scope* {
    // If the namespace does not exist, raise an error.
    const auto ns_scope = sm.get_namespaced_scope({&ns});
    if (ns_scope == nullptr) {
        const auto alternatives = sm.current_scope->all_var_symbols()
            | genex::views::transform([](auto const &x) { return x->name->val; })
            | genex::views::to<std::vector>();

        const auto closest_match = spp::utils::strings::closest_match(ns.val, alternatives);
        analyse::errors::SemanticErrorBuilder<errors::SppIdentifierUnknownError>().with_args(
            ns, "namespace", closest_match).with_scopes({sm.current_scope}).raise();
    }

    // Return the found namespace scope.
    return ns_scope;
}


auto spp::analyse::utils::type_utils::deduplicate_variant_inner_types(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> std::vector<std::shared_ptr<asts::TypeAst>> {
    // Create the list of types.
    auto out = std::vector<std::shared_ptr<asts::TypeAst>>();
    if (type.type_parts().back()->generic_arg_group->args.empty()) {
        return out;
    }

    for (auto &&generic_arg : type.type_parts().back()->generic_arg_group->get_type_args()[0]->val->type_parts().back()->generic_arg_group->get_type_args()) {
        // Inspect inner variant types by extending the composite type list.
        if (is_type_variant(*generic_arg->val->without_generics(), scope)) {
            out.append_range(deduplicate_variant_inner_types(*generic_arg->val, scope));
        }

        // Ensure there are no borrowed types inside the variant type.
        else if (const auto conv = generic_arg->val->get_convention(); conv != nullptr) {
            errors::SemanticErrorBuilder<errors::SppSecondClassBorrowViolationError>().with_args(
                type, *conv, "variant type argument").with_scopes({&scope}).raise();
        }

        // Inspect a non-variant type, and if it hasn't beem added to the list, add it.
        else if (not genex::algorithms::any_of(out, [&](auto x) { return symbolic_eq(*generic_arg->val, *x, scope, scope); })) {
            out.emplace_back(generic_arg->val);
        }
    }

    // Return the deduplicated list of types.
    return out;
}


auto spp::analyse::utils::type_utils::substitute_sup_scope_name(
    std::string const &old_sup_scope_name,
    asts::GenericArgumentGroupAst const &generic_args)
    -> std::string {
    const auto parts = old_sup_scope_name
        | genex::views::chunk('#')
        | genex::views::to<std::vector>()
        | genex::views::transform([](auto &&x) { return std::string(x.begin(), x.end()); })
        | genex::views::to<std::vector>();

    if (not parts[1].contains(" ext ")) {
        const auto t = parse::ParserSpp(lex::Lexer(parts[1]).lex()).parse_type_expression()->substitute_generics(generic_args.get_all_args());
        return parts[0] + "#" + static_cast<std::string>(*t) + "#" + parts[2];
    }
    const auto t = parse::ParserSpp(lex::Lexer(parts[1].substr(0, parts[1].find(" ext "))).lex()).parse_type_expression()->substitute_generics(generic_args.get_all_args());
    const auto u = parse::ParserSpp(lex::Lexer(parts[1].substr(parts[1].find(" ext ") + 5)).lex()).parse_type_expression()->substitute_generics(generic_args.get_all_args());
    return parts[0] + "#" + static_cast<std::string>(*t) + " ext " + static_cast<std::string>(*u) + "#" + parts[2];
}


template auto spp::analyse::utils::type_utils::validate_inconsistent_types<spp::asts::CaseExpressionBranchAst*>(
    std::vector<asts::CaseExpressionBranchAst*> const &,
    scopes::ScopeManager *,
    asts::mixins::CompilerMetaData *)
    -> std::tuple<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>, std::vector<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>>>;


template auto spp::analyse::utils::type_utils::validate_inconsistent_types<spp::asts::IterExpressionBranchAst*>(
    std::vector<asts::IterExpressionBranchAst*> const &,
    scopes::ScopeManager *,
    asts::mixins::CompilerMetaData *)
    -> std::tuple<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>, std::vector<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>>>;
