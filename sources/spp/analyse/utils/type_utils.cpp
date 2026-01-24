module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.analyse.utils.type_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_member_ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_optional_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.integer_literal_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.utils.ptr;
import spp.utils.strings;
import genex;


auto spp::analyse::utils::type_utils::convention_eq(
    asts::TypeAst const &lhs_type,
    asts::TypeAst const &rhs_type)
    -> bool {
    // Extract the conventions.
    const auto lhs_conv = lhs_type.get_convention();
    const auto rhs_conv = rhs_type.get_convention();

    // If the conventions are not equal, return false (allow "&mut" to coerce to "&").
    if ((lhs_conv and *lhs_conv != rhs_conv) or (not lhs_conv and rhs_conv)) {
        if (not((lhs_conv and *lhs_conv == asts::ConventionTag::REF) and (rhs_conv and *rhs_conv == asts::ConventionTag::MUT))) {
            return false;
        }
    }
    return true;
}


auto spp::analyse::utils::type_utils::symbolic_eq(
    asts::TypeAst const &lhs_type,
    asts::TypeAst const &rhs_type,
    scopes::Scope const &lhs_scope,
    scopes::Scope const &rhs_scope,
    const bool check_variant)
    -> bool {
    // Special case for the "!" never type.
    if (rhs_type.is_never_type()) { return true; }
    if (lhs_type.is_never_type()) { return rhs_type.is_never_type(); }

    // Strip the generics from the types.
    const auto stripped_lhs = lhs_type.without_generics();
    const auto stripped_rhs = rhs_type.without_generics();

    // Get the non-generic symbols.
    const auto stripped_lhs_sym = lhs_scope.get_type_symbol(stripped_lhs, false);
    const auto stripped_rhs_sym = rhs_scope.get_type_symbol(stripped_rhs, false);

    // If the left-hand-side is a "Variant" type, check the composite types first.
    if (check_variant and symbolic_eq(*stripped_lhs_sym->fq_name()->without_generics(), *asts::generate::common_types_precompiled::VAR, lhs_scope, lhs_scope, false)) {
        auto lhs_composite_types = deduplicate_variant_inner_types(*lhs_scope.get_type_symbol(lhs_type.shared_from_this())->fq_name(), lhs_scope);
        if (genex::any_of(lhs_composite_types, [&](auto &&lhs_composite_type) { return symbolic_eq(*lhs_composite_type, rhs_type, lhs_scope, rhs_scope); })) {
            return true;
        }
    }

    if (not convention_eq(lhs_type, rhs_type)) { return false; }

    // If the stripped types are not equal, return false (comparing by address is fine -- ClassPrototypeAst* nodes).
    if (stripped_lhs_sym->type != stripped_rhs_sym->type) { return false; }

    // Get the generic arguments for both types.
    const auto lhs_type_fq = lhs_scope.get_type_symbol(lhs_type.shared_from_this(), false)->fq_name();
    const auto rhs_type_fq = rhs_scope.get_type_symbol(rhs_type.shared_from_this(), false)->fq_name();

    auto &lhs_generics = lhs_type_fq->type_parts().back()->generic_arg_group->args;
    auto &rhs_generics = rhs_type_fq->type_parts().back()->generic_arg_group->args;

    // Special case for variadic parameter types.
    const auto temp_type_proto = lhs_scope.get_type_symbol(lhs_type.shared_from_this())->type;
    if (temp_type_proto and not temp_type_proto->generic_param_group->params.empty()) {
        if (temp_type_proto->generic_param_group->params.back()->to<asts::FunctionParameterVariadicAst>() != nullptr) {
            if (lhs_generics.size() != rhs_generics.size()) {
                return false;
            }
        }
    }

    // Ensure each generic argument is symbolically equal to the other. Todo: why genex broke here?
    for (auto const &[lhs_generic, rhs_generic] : std::ranges::views::zip(lhs_generics, rhs_generics)) {
        if (lhs_generic->to<asts::GenericArgumentTypeAst>()) {
            const auto lhs_generic_part = lhs_generic->to<asts::GenericArgumentTypeAst>();
            const auto rhs_generic_part = rhs_generic->to<asts::GenericArgumentTypeAst>();
            if (not symbolic_eq(*lhs_generic_part->val, *rhs_generic_part->val, lhs_scope, rhs_scope)) { return false; }
        }
        else {
            const auto lhs_generic_part = lhs_generic->to<asts::GenericArgumentCompAst>();
            const auto rhs_generic_part = rhs_generic->to<asts::GenericArgumentCompAst>();
            if (not symbolic_eq(*lhs_generic_part->val, *rhs_generic_part->val, lhs_scope, rhs_scope)) { return false; }
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
    scopes::Scope const &lhs_scope,
    scopes::Scope const &rhs_scope,
    GenericInferenceMap &generic_args,
    const bool check_variant) -> bool {
    // Strip the generics from the right-hand-side type (possible generic).
    const auto stripped_lhs = std::const_pointer_cast<asts::TypeAst>(lhs_type.without_generics()->without_convention());
    const auto stripped_rhs = std::const_pointer_cast<asts::TypeAst>(rhs_type.without_generics()->without_convention());

    // If the right-hand-side is generic, then return a match: "sup[T] T { ... }" matches all types.
    const auto stripped_rhs_sym = rhs_scope.get_type_symbol(stripped_rhs);
    if (stripped_rhs_sym->is_generic) {
        const auto t = std::dynamic_pointer_cast<asts::TypeIdentifierAst>(stripped_rhs);
        generic_args.insert({t, &lhs_type});
        return true;
    }

    if (not convention_eq(lhs_type, rhs_type)) { return false; }

    const auto stripped_lhs_sym = lhs_scope.get_type_symbol(stripped_lhs);
    if (stripped_lhs_sym->is_generic) {
        const auto t = std::dynamic_pointer_cast<asts::TypeIdentifierAst>(stripped_lhs);
        generic_args.insert({t, &rhs_type});
        return true;
    }

    // If the left-hand-side is a "Variant" type, check the composite types first.
    // Todo: on the failure of a variant match in "any_of", does the generic map need rolling back?
    if (check_variant and symbolic_eq(*asts::generate::common_types_precompiled::VAR, *stripped_rhs_sym->fq_name()->without_generics(), rhs_scope, rhs_scope)) {
        auto rhs_composite_types = deduplicate_variant_inner_types(*rhs_scope.get_type_symbol(rhs_type.shared_from_this())->fq_name(), rhs_scope);
        if (genex::any_of(rhs_composite_types, [&](auto &&rhs_composite_type) { return relaxed_symbolic_eq(lhs_type, *rhs_composite_type, lhs_scope, rhs_scope, generic_args); })) {
            return true;
        }
    }

    // If the stripped types aren't equal, then return false.
    if (stripped_lhs_sym->type != stripped_rhs_sym->type) {
        return false;
    }

    // The next step is to get the generic arguments for both types.
    const auto lhs_type_fq = lhs_scope.get_type_symbol(lhs_type.shared_from_this())->fq_name();
    const auto rhs_type_fq = rhs_scope.get_type_symbol(rhs_type.shared_from_this())->fq_name();

    auto &lhs_generics = lhs_type_fq->type_parts().back()->generic_arg_group->args;
    auto &rhs_generics = rhs_type_fq->type_parts().back()->generic_arg_group->args;

    // Special case for variadic parameter types.
    const auto temp_type_proto = lhs_scope.get_type_symbol(lhs_type.shared_from_this())->type;
    if (temp_type_proto and not temp_type_proto->generic_param_group->params.empty()) {
        if (temp_type_proto->generic_param_group->params.back()->to<asts::FunctionParameterVariadicAst>() != nullptr) {
            if (lhs_generics.size() != rhs_generics.size()) {
                return false;
            }
        }
    }

    // Ensure each generic argument is symbolically equal to the other. Todo: why genex broke here?
    for (auto [lhs_generic, rhs_generic] : std::ranges::views::zip(lhs_generics, rhs_generics)) {
        if (rhs_generic->to<asts::GenericArgumentTypeAst>()) {
            const auto lhs_generic_part = lhs_generic->to<asts::GenericArgumentTypeAst>();
            const auto rhs_generic_part = rhs_generic->to<asts::GenericArgumentTypeAst>();
            if (not relaxed_symbolic_eq(*lhs_generic_part->val, *rhs_generic_part->val, lhs_scope, rhs_scope, generic_args)) {
                return false;
            }
        }
        else {
            const auto lhs_generic_part = lhs_generic->to<asts::GenericArgumentCompAst>();
            const auto rhs_generic_part = rhs_generic->to<asts::GenericArgumentCompAst>();
            if (not relaxed_symbolic_eq(*lhs_generic_part->val, *rhs_generic_part->val, lhs_scope, rhs_scope, generic_args)) {
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
    GenericInferenceMap &generic_args)
    -> bool {
    // Simple equality between the expressions, with generic matching.
    if (const auto rhs_expr_as_identifier = rhs_expr.to<asts::IdentifierAst>()) {
        generic_args[asts::TypeIdentifierAst::from_identifier(*rhs_expr_as_identifier)] = &lhs_expr;
        return true;
    }
    return lhs_expr == rhs_expr;
}


auto spp::analyse::utils::type_utils::is_type_comptime_indexable(
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


auto spp::analyse::utils::type_utils::is_type_never(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::void::Void".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::NEVER, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_generator(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::generator::Gen[T]/GenOpt[T]/GenRes[T, E]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::GEN, scope, scope) or
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::GEN_ONCE, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_runtime_indexable(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Test for the type against "std::iter::IndexRef[T]/IndexMut[T]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::INDEX_REF, scope, scope) or
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::INDEX_MUT, scope, scope);
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
    auto attr_info = std::vector<std::pair<std::shared_ptr<scopes::TypeSymbol>, asts::ClassAttributeAst*>>{};
    get_attr_types(&type, sm.current_scope, attr_info);
    for (auto const &[attr_type_sym, attr_ast] : attr_info) {
        if (attr_type_sym == type.get_cls_sym()) {
            return attr_ast->type;
        }
    }

    // No recursive type was found, so return nullptr.
    return nullptr;
}


auto spp::analyse::utils::type_utils::is_type_borrowed(
    asts::TypeAst const &type,
    scopes::ScopeManager const &sm,
    const bool deep)
    -> bool {
    // Check that either this type, or any inner types for variants, are "&" or "&mut".
    if (type.get_convention() != nullptr) { return true; }

    // Check the inner types for variant types.
    const auto variant_type = asts::generate::common_types_precompiled::VAR;
    if (deep and symbolic_eq(*type.without_generics(), *variant_type, *sm.current_scope, *sm.current_scope, false)) {
        for (auto const &inner_type_arg : type.type_parts().back()->generic_arg_group->get_type_args()) {
            if (is_type_borrowed(*inner_type_arg->val, sm)) { return true; }
        }
    }
    return false;
}


auto spp::analyse::utils::type_utils::get_attr_types(
    const asts::ClassPrototypeAst *cls_proto,
    const scopes::Scope *cls_scope,
    std::vector<std::pair<std::shared_ptr<scopes::TypeSymbol>, asts::ClassAttributeAst*>> &attr_symbols)
    -> void {
    // Get all attribute types, without recursion errors (this will be handled elsewhere).
    for (auto const &member : cls_proto->impl->members | genex::views::ptr | genex::views::cast_dynamic<asts::ClassAttributeAst*>) {
        auto type_sym = cls_scope->get_type_symbol(member->type);
        if (genex::contains(attr_symbols, type_sym, genex::get<0>)) { continue; }
        if (type_sym->scope == nullptr) { continue; }

        attr_symbols.emplace_back(type_sym, member);
        get_attr_types(type_sym->type, type_sym->scope, attr_symbols);
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
        const auto size_arg_cast = size_arg->val->to<asts::IntegerLiteralAst>();
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
    scopes::Scope const &scope,
    asts::ExpressionAst const &expr,
    std::string_view what)
    -> std::tuple<std::shared_ptr<const asts::TypeAst>, std::shared_ptr<asts::TypeAst>, bool> {
    // Generic types are not generators, so raise an error.
    const auto type_sym = scope.get_type_symbol(type.shared_from_this());
    raise_if<errors::SppExpressionNotGeneratorError>(
        type_sym->scope == nullptr, {&scope}, ERR_ARGS(expr, type, what));

    // Discover the supertypes and add the current type to it.=.
    auto sup_types = std::vector{type.shared_from_this()};
    sup_types.append_range(type_sym->scope->sup_types());

    // Search through the supertypes for a direct generator type.
    const auto generator_type_candidates = sup_types
        | genex::views::filter([&scope](auto const &sup_type) { return is_type_generator(*sup_type, scope); })
        | genex::to<std::vector>();

    raise_if<errors::SppExpressionNotGeneratorError>(
        generator_type_candidates.empty(), {&scope}, ERR_ARGS(expr, type, what));

    raise_if<errors::SppExpressionAmbiguousGeneratorError>(
        generator_type_candidates.size() > 1, {&scope}, ERR_ARGS(expr, type, what));

    // Extract the generator and yield type.
    auto generator_type = generator_type_candidates[0];
    auto yield_type = generator_type->type_parts().back()->generic_arg_group->type_at("Yield")->val;

    // Extract the multiplicity, optionality and fallibility from the generator type.
    auto is_once = symbolic_eq(
        *asts::generate::common_types_precompiled::GEN_ONCE, *generator_type->without_generics(), scope, scope);

    // Return all the information about the generator type.
    return std::make_tuple(generator_type, yield_type, is_once);
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
    auto sup_types = std::vector{type.shared_from_this()};
    sup_types.append_range(type_sym->scope->sup_types());

    // Search through the supertypes for a direct Try type.
    const auto try_type_candidates = sup_types
        | genex::views::filter([&sm](auto &&sup_type) { return is_type_try(*sup_type, *sm.current_scope); })
        | genex::to<std::vector>();

    raise_if<errors::SppEarlyReturnRequiresTryTypeError>(
        try_type_candidates.empty(), {sm.current_scope}, ERR_ARGS(expr, type));

    // Extract the Try type and return it.
    return try_type_candidates[0];
}


auto spp::analyse::utils::type_utils::get_fwd_types(
    asts::TypeAst const &type,
    scopes::ScopeManager const &sm)
    -> std::pair<std::shared_ptr<const asts::TypeAst>, std::shared_ptr<const asts::TypeAst>> {
    // Generic types do not have forward types, so return nullptr.
    const auto type_sym = sm.current_scope->get_type_symbol(type.shared_from_this());
    if (type_sym->scope == nullptr) {
        return {nullptr, nullptr};
    }

    // Discover the supertypes and add the current type to it.
    auto sup_types = std::vector{type.shared_from_this()};
    sup_types.append_range(type_sym->scope->sup_types());

    // Search through the supertypes for a direct FwdRef type.
    const auto fwd_ref_type_candidates = sup_types
        | genex::views::filter([&sm](auto &&sup_type) { return symbolic_eq(*sup_type, *asts::generate::common_types_precompiled::FWD_REF, *sm.current_scope, *sm.current_scope); })
        | genex::to<std::vector>();

    // Search through the supertypes for a direct FwdMut type.
    const auto fwd_mut_type_candidates = sup_types
        | genex::views::filter([&sm](auto &&sup_type) { return symbolic_eq(*sup_type, *asts::generate::common_types_precompiled::FWD_MUT, *sm.current_scope, *sm.current_scope); })
        | genex::to<std::vector>();

    // No error raised here; just return the pair of types (nullptr if not found).
    auto fwd_ref_type = fwd_ref_type_candidates.empty() ? nullptr : fwd_ref_type_candidates[0];
    auto fwd_mut_type = fwd_mut_type_candidates.empty() ? nullptr : fwd_mut_type_candidates[0];
    return std::pair{fwd_ref_type, fwd_mut_type};
}


auto spp::analyse::utils::type_utils::validate_inconsistent_types(
    std::vector<asts::CaseExpressionBranchAst*> const &branches,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>, std::vector<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>>> {
    // Collect type information for each branch, pairing the branch with its inferred type.
    auto branches_type_info = branches
        | genex::views::transform([sm, meta](auto *x) { return std::make_pair(x, x->infer_type(sm, meta)); })
        | genex::to<std::vector>();

    // Filter the branch types down to variant types for custom analysis.
    auto variant_branches_type_info = branches_type_info
        | genex::views::filter([sm](auto &&x) { return type_utils::is_type_variant(*x.second, *sm->current_scope); })
        | genex::to<std::vector>();

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
    // Todo: Shouldn't need to auto-remove "!" type, because symbolic_eq handles it?
    auto mismatch_branches_type_info = branches_type_info
        | genex::views::remove_if([&](auto const &x) { return symbolic_eq(*asts::generate::common_types_precompiled::NEVER, *x.second, *sm->current_scope, *sm->current_scope); })
        | genex::views::remove_if([&](auto const &x) { return x.first == master_branch_type_info.first; })
        | genex::views::remove_if([&](auto const &x) { return type_utils::symbolic_eq(*master_branch_type_info.second, *x.second, *sm->current_scope, *sm->current_scope); })
        | genex::to<std::vector>();

    if (not mismatch_branches_type_info.empty()) {
        const auto [mismatch_branch, mismatch_branch_type] = std::move(mismatch_branches_type_info[0]);
        const auto [master_branch, master_branch_type] = master_branch_type_info;
        const auto final_member = master_branch ? master_branch->body->final_member() : meta->assignment_target.get();
        raise<errors::SppTypeMismatchError>(
            {sm->current_scope},
            ERR_ARGS(*final_member, *master_branch_type, *mismatch_branch->body->final_member(), *mismatch_branch_type));
    }

    // Cast to common AST nodes and return with the types.
    auto cast_master_branch_type_info = std::make_pair(master_branch_type_info.first->template to<asts::Ast>(), master_branch_type_info.second);
    auto cast_branches_type_info = branches_type_info
        | genex::views::transform([](auto &&x) { return std::make_pair(x.first->template to<asts::Ast>(), x.second); })
        | genex::to<std::vector>();
    return std::make_tuple(cast_master_branch_type_info, cast_branches_type_info);
}


auto spp::analyse::utils::type_utils::get_all_attrs(
    asts::TypeAst const &type,
    scopes::ScopeManager const *sm)
    -> std::vector<std::pair<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<scopes::TypeSymbol>>> {
    // Get the symbol of the class type.
    const auto cls_sym = sm->current_scope->get_type_symbol(type.shared_from_this());

    // Get the attribute information from the class type and all super types.
    auto all_scopes = std::vector{cls_sym->scope};
    all_scopes.append_range(cls_sym->scope->sup_scopes());

    const auto all_syms = all_scopes
        | genex::views::filter([](auto &&sup_scope) { return sup_scope->ast->template to<asts::ClassPrototypeAst>() != nullptr; })
        | genex::views::transform([](auto &&sup_scope) { return std::make_pair(sup_scope, sup_scope->all_var_symbols(true)); })
        | genex::to<std::vector>();

    auto extended_syms = std::vector<std::pair<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<scopes::TypeSymbol>>>{};
    for (auto const &[sup_scope, syms] : all_syms) {
        for (auto const &sym : syms) {
            const auto sym_type = sup_scope->get_type_symbol(sym->type);
            const auto ext_type = sym_type->fq_name();
            extended_syms.emplace_back(sym->name, sym_type);
        }
    }

    return extended_syms;
}


auto spp::analyse::utils::type_utils::create_generic_cls_scope(
    asts::TypeIdentifierAst &type_part,
    scopes::TypeSymbol const &old_cls_sym,
    std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
    const bool is_tuple,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> scopes::Scope* {
    // Create a new scope and symbol for the generic substituted type.
    const auto old_cls_scope = old_cls_sym.scope ? : old_cls_sym.scope_defined_in;
    auto new_cls_scope = std::make_unique<scopes::Scope>(
        std::dynamic_pointer_cast<asts::TypeIdentifierAst>(type_part.shared_from_this()),
        old_cls_scope->parent, old_cls_scope->ast);

    // Note there is no LLVM type propagation: handled separately before stage 10.
    const auto new_cls_sym = std::make_shared<scopes::TypeSymbol>(
        spp::utils::ptr::shared_cast<asts::TypeIdentifierAst>(type_part.shared_from_this()),
        new_cls_scope->ast->to<asts::ClassPrototypeAst>(), new_cls_scope.get(), sm->current_scope,
        old_cls_scope->parent, old_cls_sym.is_generic, old_cls_sym.is_directly_copyable, old_cls_sym.visibility);
    const auto new_cls_scope_ptr = new_cls_scope.get();

    new_cls_sym->is_copyable = [&old_cls_sym] { return old_cls_sym.is_copyable(); };
    auto new_alias_stmt = asts::ast_clone(old_cls_sym.alias_stmt);
    if (new_alias_stmt) {
        new_alias_stmt->old_type = new_alias_stmt->old_type->substitute_generics(type_part.generic_arg_group->get_all_args());
        new_alias_stmt->old_type->stage_7_analyse_semantics(sm, meta);

        const auto target_scope = new_alias_stmt->get_ast_scope()->parent;
        target_scope->add_type_symbol(new_cls_sym);
        new_alias_stmt->m_temp_scope_1->add_type_symbol(new_cls_sym);
        new_alias_stmt->m_temp_scope_1->children.emplace_back(std::move(new_cls_scope));
        new_cls_sym->alias_stmt = std::move(new_alias_stmt);
    }

    // Configure the new scope based on the base (old) scope.
    else {
        new_cls_scope->parent->add_type_symbol(new_cls_sym);
        new_cls_scope->parent->children.emplace_back(std::move(new_cls_scope));
    }
    new_cls_scope_ptr->ty_sym = new_cls_sym;
    new_cls_scope_ptr->table = old_cls_scope->table;
    new_cls_scope_ptr->non_generic_scope = old_cls_scope;

    if (meta->current_stage > 7) {
        sm->attach_specific_super_scopes(*new_cls_scope_ptr, meta);
    }

    // No more checks for tuples.
    auto new_ast = asts::ast_clone(old_cls_scope->ast->to<asts::ClassPrototypeAst>());
    new_ast->set_ast_scope(new_cls_scope_ptr);
    new_ast->generic_param_group->params.clear();
    const auto new_ast_ptr = new_ast.get();

    old_cls_sym.type->register_generic_substitution(new_cls_scope_ptr, std::move(new_ast));
    if (is_tuple) {
        return new_cls_scope_ptr;
    }

    // Register the generic symbols.
    register_generic_syms(external_generic_syms, type_part.generic_arg_group->args, new_cls_scope_ptr, sm, meta);

    // Run generic substitution on the symbols in the scope.
    const auto fq_type = new_cls_sym->fq_name();
    auto substitution_generics = fq_type->type_parts().back()->generic_arg_group->args | genex::views::ptr | genex::to<std::vector>();
    substitution_generics.append_range(type_part.generic_arg_group->args | genex::views::ptr | genex::to<std::vector>());

    auto tm = scopes::ScopeManager(sm->global_scope, new_alias_stmt ? sm->current_scope->get_type_symbol(new_alias_stmt->old_type)->scope : new_cls_scope_ptr);
    for (auto const &scoped_sym : new_cls_scope_ptr->all_var_symbols(true)) {
        scoped_sym->type = scoped_sym->type->substitute_generics(substitution_generics);
        if (meta->current_stage > 5) {
            scoped_sym->type->stage_7_analyse_semantics(&tm, meta);
        }
    }

    for (auto *attr : new_ast_ptr->impl->members | genex::views::ptr | genex::views::cast_dynamic<asts::ClassAttributeAst*>()) {
        attr->type = attr->type->substitute_generics(substitution_generics);
        if (meta->current_stage > 5) {
            attr->stage_7_analyse_semantics(&tm, meta);
        }

        // Remove void attributes from the class.
        if (is_type_void(*attr->type, *new_cls_scope_ptr)) {
            // new_ast_ptr->impl |= genex::actions::remove_if([&](auto &&x) { return x == attr; }, [](auto &&x) { return x.get(); });
            new_ast_ptr->impl->members |= genex::actions::remove_if([&](auto &&x) { return x.get() == attr; });
            new_cls_scope_ptr->rem_var_symbol(attr->name);
        }
    }

    // Return the new class scope.
    return new_cls_scope_ptr;
}


auto spp::analyse::utils::type_utils::create_generic_fun_scope(
    scopes::Scope const &old_fun_scope,
    asts::GenericArgumentGroupAst const &generic_args,
    std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> scopes::Scope* {
    // Create a new scope and symbol for the generic substituted function.
    auto new_fun_scope = std::make_unique<scopes::Scope>(old_fun_scope);
    const auto new_fun_scope_ptr = new_fun_scope.get();
    const auto old_fn_proto = asts::ast_body(old_fun_scope.ast)[0]->to<asts::FunctionPrototypeAst>();
    old_fn_proto->register_generic_substitution(std::move(new_fun_scope), nullptr);

    auto tm = scopes::ScopeManager(sm->global_scope, new_fun_scope_ptr);
    register_generic_syms(external_generic_syms, generic_args.args, new_fun_scope_ptr, &tm, meta);

    // Return the new function scope.
    return new_fun_scope_ptr;
}


auto spp::analyse::utils::type_utils::create_generic_sup_scope(
    scopes::Scope &old_sup_scope,
    scopes::Scope &new_cls_scope,
    asts::GenericArgumentGroupAst const &generic_args,
    std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
    scopes::ScopeManager const *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<scopes::Scope*, scopes::Scope*> {
    // Create a new scope for the generic substituted super scope.
    const auto self_type = asts::ast_name(old_sup_scope.ast)->substitute_generics(generic_args.args | genex::views::ptr | genex::to<std::vector>());
    auto new_sup_scope = std::make_unique<scopes::Scope>(old_sup_scope);
    auto new_sup_scope_ptr = new_sup_scope.get();
    old_sup_scope.parent->children.emplace_back(std::move(new_sup_scope));

    // std::get<scopes::ScopeBlockName>(new_sup_scope_ptr->name).name =
    //     substitute_sup_scope_name(std::get<scopes::ScopeBlockName>(new_sup_scope_ptr->name).name, generic_args);

    // Register the generic symbols.
    auto tm = scopes::ScopeManager(sm->global_scope, new_sup_scope_ptr);
    register_generic_syms(external_generic_syms, generic_args.args, new_sup_scope_ptr, &tm, meta);

    // Add the "Self" symbol into the new scope.
    self_type->stage_7_analyse_semantics(&tm, meta);
    const auto old_self_sym = new_sup_scope_ptr->get_type_symbol(self_type);
    const auto new_self_sym = std::make_shared<scopes::TypeSymbol>(
        std::make_unique<asts::TypeIdentifierAst>(0, "Self", nullptr), new_cls_scope.ty_sym->type, &new_cls_scope, new_sup_scope_ptr);
    new_self_sym->alias_stmt = std::make_unique<asts::TypeStatementAst>(
        SPP_NO_ANNOTATIONS, nullptr, asts::TypeIdentifierAst::from_string("Self"), nullptr, nullptr, self_type);
    old_self_sym->aliased_by_symbols.emplace_back(new_self_sym);
    new_sup_scope_ptr->add_type_symbol(new_self_sym);

    // Run generic substitution on the aliases in the new scope.
    for (auto const &scoped_sym : new_sup_scope_ptr->all_type_symbols(true)) {
        if (scoped_sym->alias_stmt != nullptr) {
            auto old_type_sub = scoped_sym->alias_stmt->old_type->substitute_generics(generic_args.args | genex::views::ptr | genex::to<std::vector>());
            // old_type_sub->stage_7_analyse_semantics(&tm, meta);  Todo: Why is this commented?
            const auto old_type_sub_sym = new_sup_scope_ptr->get_type_symbol(old_type_sub);

            scoped_sym->alias_stmt->old_type = std::move(old_type_sub);
            if (old_type_sub_sym != nullptr) {
                old_type_sub_sym->aliased_by_symbols.push_back(scoped_sym);
                scoped_sym->type = old_type_sub_sym->type;
                scoped_sym->scope = old_type_sub_sym->scope;
            }
        }
    }

    // Run generic substitution on the constants in the new scope.
    for (auto const &scoped_sym : new_sup_scope_ptr->all_var_symbols(true)) {
        auto old_type_sub = scoped_sym->type->substitute_generics(generic_args.args | genex::views::ptr | genex::to<std::vector>());
        scoped_sym->type = std::move(old_type_sub);
    }

    // Create the scope for the new super class type. This will handle recursive sup-scope creation.
    auto super_cls_scope = static_cast<scopes::Scope*>(nullptr);
    if (const auto ext_ast = old_sup_scope.ast->to<asts::SupPrototypeExtensionAst>(); ext_ast != nullptr) {
        const auto new_fq_super_type = ext_ast->super_class->substitute_generics(generic_args.args | genex::views::ptr | genex::to<std::vector>());
        new_fq_super_type->stage_7_analyse_semantics(&tm, meta);
        super_cls_scope = new_cls_scope.get_type_symbol(new_fq_super_type)->scope;
    }

    return std::make_tuple(new_sup_scope_ptr, super_cls_scope);
}


auto spp::analyse::utils::type_utils::create_generic_sym(
    asts::GenericArgumentAst const &generic,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta,
    scopes::ScopeManager *tm)
    -> std::shared_ptr<scopes::Symbol> {
    // Handle the generic type argument => creates a type symbol.
    if (const auto type_arg = generic.to<asts::GenericArgumentTypeKeywordAst>(); type_arg != nullptr) {
        const auto true_val_sym = sm.current_scope->get_type_symbol(type_arg->val);

        auto sym = std::make_unique<scopes::TypeSymbol>(
            type_arg->name->type_parts().back(), true_val_sym ? true_val_sym->type : nullptr,
            true_val_sym ? true_val_sym->scope : nullptr, sm.current_scope, sm.current_scope->parent_module(), true,
            true_val_sym ? true_val_sym->is_directly_copyable : false, asts::utils::Visibility::PUBLIC,
            asts::ast_clone(type_arg->val->get_convention()));
        return sym;
    }

    // Handle the generic comp argument => creates a variable symbol.
    if (const auto comp_arg = generic.to<asts::GenericArgumentCompKeywordAst>(); comp_arg != nullptr) {
        auto sym = std::make_unique<scopes::VariableSymbol>(
            asts::IdentifierAst::from_type(*comp_arg->name),
            (tm ? *tm : sm).current_scope->get_type_symbol(comp_arg->val->infer_type(tm ? tm : &sm, meta))->fq_name(),
            false, true, asts::utils::Visibility::PUBLIC);
        sym->memory_info->ast_comptime = asts::ast_clone(comp_arg);
        return sym;
    }

    std::unreachable();
}


auto spp::analyse::utils::type_utils::register_generic_syms(
    std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
    std::vector<std::unique_ptr<asts::GenericArgumentAst>> const &generic_args,
    scopes::Scope *scope,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    // Register the type symbols to the scope.
    for (auto const &e : external_generic_syms | genex::views::cast_smart<scopes::TypeSymbol>()) {
        scope->add_type_symbol(e);
    }

    // Register the variable symbols to the scope.
    for (auto const &e : external_generic_syms | genex::views::cast_smart<scopes::VariableSymbol>()) {
        scope->add_var_symbol(e);
    }

    // Convert the generic arguments into symbols.
    auto generic_syms = generic_args
        | genex::views::transform([&](auto const &g) { return create_generic_sym(*g, *sm, meta); })
        | genex::to<std::vector>();

    // Register the created generic symbols to the scope.
    for (auto const &e : generic_syms | genex::views::cast_smart<scopes::TypeSymbol>()) {
        scope->add_type_symbol(e);
    }

    // Register the created generic symbols to the scope.
    for (auto const &e : generic_syms | genex::views::cast_smart<scopes::VariableSymbol>()) {
        scope->add_var_symbol(e);
    }
}


auto spp::analyse::utils::type_utils::get_type_sym_or_error(
    scopes::Scope const &scope,
    asts::TypeIdentifierAst const &type_part,
    scopes::ScopeManager const &sm,
    asts::meta::CompilerMetaData *)
    -> scopes::TypeSymbol* {
    // Get the type part's symbol, and raise an error if it doesn't exist.
    const auto type_sym = scope.get_type_symbol(type_part.shared_from_this(), false);
    if (type_sym == nullptr) {
        const auto alternatives = sm.current_scope->all_type_symbols()
            | genex::views::transform([](auto const &x) { return x->name->name; })
            | genex::views::remove_if([](auto const &x) { return x[0] == '$'; })
            | genex::to<std::vector>();

        const auto closest_match = spp::utils::strings::closest_match(type_part.name, alternatives);
        raise<errors::SppIdentifierUnknownError>(
            {sm.current_scope}, ERR_ARGS(*type_part.without_generics(), "type", closest_match));
    }

    // Return the found type symbol.
    return type_sym.get();
}


auto spp::analyse::utils::type_utils::get_ns_scope_or_error(
    scopes::Scope const &scope,
    asts::IdentifierAst const &ns,
    scopes::ScopeManager const &sm)
    -> scopes::Scope* {
    // If the namespace does not exist, raise an error.
    const auto ns_sym = scope.get_ns_symbol(ns.shared_from_this());
    if (ns_sym == nullptr) {
        const auto alternatives = sm.current_scope->all_var_symbols()
            | genex::views::transform([](auto const &x) { return x->name->val; })
            | genex::to<std::vector>();

        const auto closest_match = spp::utils::strings::closest_match(ns.val, alternatives);
        raise<errors::SppIdentifierUnknownError>(
            {sm.current_scope}, ERR_ARGS(ns, "namespace", closest_match));
    }

    // Return the found namespace scope.
    return ns_sym->scope;
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

        // Inspect a non-variant type, and if it hasn't beem added to the list, add it.
        else if (not genex::any_of(out, [&](auto x) { return symbolic_eq(*generic_arg->val, *x, scope, scope); })) {
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
        | genex::views::split('#')
        | genex::to<std::vector>()
        | genex::views::transform([](auto &&x) { return std::string(x.begin(), x.end()); })
        | genex::to<std::vector>();

    if (not parts[1].contains(" ext ")) {
        const auto t = INJECT_CODE(parts[1], parse_type_expression)->substitute_generics(generic_args.get_all_args());
        return parts[0] + "#" + static_cast<std::string>(*t) + "#" + parts[2];
    }
    const auto t = INJECT_CODE(parts[1].substr(0, parts[1].find(" ext ")), parse_type_expression)->substitute_generics(generic_args.get_all_args());
    const auto u = INJECT_CODE(parts[1].substr(parts[1].find(" ext ") + 5), parse_type_expression)->substitute_generics(generic_args.get_all_args());
    return parts[0] + "#" + static_cast<std::string>(*t) + " ext " + static_cast<std::string>(*u) + "#" + parts[2];
}


auto spp::analyse::utils::type_utils::recursive_alias_search(
    asts::TypeStatementAst const &alias_stmt,
    scopes::Scope *tracking_scope,
    std::shared_ptr<asts::TypeAst> actual_old_type,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<std::shared_ptr<asts::TypeAst>, std::shared_ptr<asts::GenericParameterGroupAst>, scopes::Scope*, scopes::Scope*> {
    // Todo: Contender for worst function is the program, second to "func_utils::get_all_function_scopes"
    // Todo: Detect cycles to prevent infinite loops of type aliasing.
    // Create lists for tracking chains.
    auto type_list = std::vector<std::shared_ptr<asts::TypeAst>>{};
    auto scope_list = std::vector<scopes::Scope*>{};
    auto alias_list = std::vector<asts::TypeStatementAst*>{};
    auto generic_list = std::vector<std::unique_ptr<asts::GenericParameterGroupAst>>{};
    auto sym_list = std::vector<scopes::TypeSymbol*>{};

    auto cls_proto = static_cast<asts::ClassPrototypeAst*>(nullptr);
    auto ts_proto = static_cast<asts::TypeStatementAst*>(nullptr);

    while (true) {
        auto tm = scopes::ScopeManager(sm->global_scope, tracking_scope);
        meta->save();
        meta->skip_type_analysis_generic_checks = true;
        actual_old_type->without_generics()->stage_7_analyse_semantics(&tm, meta);
        meta->restore();
        const auto sym = tracking_scope->get_type_symbol(actual_old_type->without_generics());

        type_list.emplace_back(actual_old_type);
        scope_list.emplace_back(tracking_scope);
        alias_list.emplace_back(sym->alias_stmt.get());
        sym_list.emplace_back(sym.get());

        // Always check for alias first, because type might have been set by prev alias analysis.
        if (sym->alias_stmt != nullptr) {
            generic_list.emplace_back(asts::ast_clone(sym->alias_stmt->generic_param_group));
            actual_old_type = sym->alias_stmt->old_type;
            tracking_scope = sym->scope_defined_in;
            const auto new_sym = tracking_scope->get_type_symbol(actual_old_type->without_generics());
            ts_proto = ts_proto ? : sym->alias_stmt.get(); // always override, so last one is gotten.
        }

        // See if we have found a non-alias type (concrete class definition).
        else if (sym->type != nullptr) {
            cls_proto = sym->type;
            generic_list.emplace_back(asts::ast_clone(cls_proto->generic_param_group));
            break;
        }

        // If the aliased type is generic, then just return it as-is.
        else {
            return {actual_old_type, nullptr, scope_list.back(), sym_list.back()->scope};
        }
    }

    // For the last alias, which maps to a class, we need to attach generics. So for example, the vector class is
    // defined as: "cls Vec[T, A] { ... }", so for "use std::vector::Vec" => "type Vec = std::vector::Vec" =>
    // "type Vec[T, A] = std::vector::Vec[T, A]". This only needs to happen for the lowest level alias, which maps to a
    // class (see the immediate "break")
    for (auto layer = type_list.size() - 1; layer > 0; --layer) {
        auto generics = generic_list[layer] ? std::move(generic_list[layer]) : asts::GenericParameterGroupAst::new_empty();
        const auto alias = alias_list[layer - 1];

        if (alias->old_type->type_parts().back()->generic_arg_group->args.empty()) {
            alias->generic_param_group = std::move(generics);
            alias->old_type->type_parts().back()->generic_arg_group = asts::GenericArgumentGroupAst::from_params(*alias->generic_param_group);
        }
    }

    // At this point, we have the lowest level type that the type alias has mapped to, potentially via other aliases.
    // Next, we need to move back up the chain to re-apply generics.
    for (auto layer = 0uz; layer < type_list.size() - 1; ++layer) {
        // We substitute the current generics into the current type, then update the current type.
        const auto args = asts::ast_clone(type_list[layer]->type_parts().back()->generic_arg_group);
        const auto params = alias_list[layer]->generic_param_group.get();
        func_utils::name_gn_args(*args, *params, *type_list[layer], *sm, *meta);
        auto tm = scopes::ScopeManager(sm->global_scope, scope_list[layer + 1]);

        meta->save();
        meta->skip_type_analysis_generic_checks = true;
        type_list[layer + 1]->stage_7_analyse_semantics(&tm, meta);
        meta->restore();

        type_list[layer + 1] = type_list[layer + 1]->substitute_generics(args->get_all_args());

        meta->save();
        meta->skip_type_analysis_generic_checks = true;
        type_list[layer + 1]->stage_7_analyse_semantics(&tm, meta);
        meta->restore();

        for (auto p : params->get_optional_params() | genex::views::cast_dynamic<asts::GenericParameterTypeOptionalAst*>()) {
            p->default_val = p->default_val->substitute_generics(args->get_all_args());
        }
    }

    auto strip_params = [](asts::GenericParameterGroupAst &params, asts::GenericArgumentGroupAst const &args) {
        // Remove type parameters who have been given arguments in the type part's generic argument group.
        for (auto *p : params.get_type_params()) {
            for (auto const *q : args.get_type_args() | genex::views::cast_dynamic<asts::GenericArgumentTypeKeywordAst*>()) {
                if (*p->name == *q->name and *p->name != *q->val) {
                    params.params |= genex::actions::remove_if([&p](auto &&x) { return x.get() == p; });
                    break;
                }
            }
        }

        // Remove comp parameters who have been given arguments in the type part's generic argument group.
        for (auto *p : params.get_comp_params()) {
            for (auto const *q : args.get_comp_args() | genex::views::cast_dynamic<asts::GenericArgumentCompKeywordAst*>()) {
                if (*p->name == *q->name and *asts::IdentifierAst::from_type(*p->name) != *q->val) {
                    params.params |= genex::actions::remove_if([&p](auto &&x) { return x.get() == p; });
                    break;
                }
            }
        }
    };

    if (ts_proto != nullptr) {
        auto params = asts::ast_clone(ts_proto->generic_param_group);
        auto args = asts::ast_clone(type_list.front()->type_parts().back()->generic_arg_group);
        func_utils::name_gn_args(*args, *params, *ts_proto->old_type, *sm, *meta);

        strip_params(*params, *args);
        return {type_list.back(), std::move(params), scope_list.back(), sym_list.back()->scope};
    }

    if (cls_proto != nullptr) {
        auto params = asts::ast_clone(cls_proto->generic_param_group);
        auto args = asts::ast_clone(type_list.back()->type_parts().back()->generic_arg_group);
        func_utils::name_gn_args(*args, *params, *cls_proto->name, *sm, *meta);
        for (auto *p : params->get_all_params() | genex::views::cast_dynamic<asts::GenericParameterTypeOptionalAst*>()) {
            p->default_val = p->default_val->substitute_generics(args->get_all_args());
        }
        // todo: comp param's types need substituting?

        strip_params(*params, *args);
        auto base_args = asts::GenericArgumentGroupAst::from_params(*cls_proto->generic_param_group);
        params = *asts::ast_clone(alias_stmt.generic_param_group) + *params;

        const auto t = type_list.back()->with_generics(std::move(base_args))->substitute_generics(args->get_all_args());
        return {t, std::move(params), scope_list.back(), sym_list.back()->scope};
    }
    return {type_list.back(), nullptr, scope_list.back(), sym_list.back()->scope};
}


auto spp::analyse::utils::type_utils::get_field_index_in_type(
    asts::TypeAst const &type_sym,
    asts::IdentifierAst const &field_name,
    scopes::ScopeManager *sm)
    -> std::size_t {
    // Get all the attributes on the type.
    const auto all_attrs = get_all_attrs(type_sym, sm);

    // Find the field index.
    for (auto index = 0uz; index < all_attrs.size(); ++index) {
        if (*all_attrs[index].first == field_name) {
            return index;
        }
    }

    return all_attrs.size();

    // return genex::position(all_attrs, genex::operations::eq_fixed(field_name), [](auto &&attr) -> decltype(auto) { return *attr.first->name; });
}
