module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

#define MAKE_VARIADIC_COMP_ARGS(What)                                                                          \
    What                                                                                                       \
    | genex::views::move                                                                                       \
    | genex::views::drop(i)                                                                                    \
    | genex::views::transform([](auto &&x) { return asts::AstClone(x->template To<GenericArgType>()->Val); }) \
    | genex::to<Vec>();

#define MAKE_VARIADIC_TYPE_ARGS(What)                                                                               \
    What                                                                                                            \
    | genex::views::move                                                                                            \
    | genex::views::drop(i)                                                                                         \
    | genex::views::transform([](auto &&x) { return asts::AstCloneShared(x->template To<GenericArgType>()->Val); }) \
    | genex::to<Vec>();

module spp.analyse.utils.func_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_comp_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_argument_type_positional_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_comp_optional_ast;
import spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.generic_parameter_type_optional_ast;
import spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.identifier_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.literal_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.subroutine_prototype_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.utils.uid;
import spp.utils.ptr;
import ankerl.unordered_dense;
import genex;

auto spp::analyse::utils::func_utils::GetFuncOwnerTypeAndFuncName(
    asts::ExpressionAst const &lhs,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<Shared<asts::TypeAst>, scopes::Scope const*, Shared<asts::IdentifierAst>> {
    //
    using expr_utils::RaiseMissingIdentifierAndClosestOptions;

    // Define some expression casts that are used commonly.
    const auto postfix_lhs = lhs.To<asts::PostfixExpressionAst>();
    const auto runtime_field = postfix_lhs
        ? postfix_lhs->Op->To<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>()
        : nullptr;
    const auto static_field = postfix_lhs
        ? postfix_lhs->Op->To<asts::PostfixExpressionOperatorStaticMemberAccessAst>()
        : nullptr;

    // Specific casts.
    const auto postfix_lhs_as_type = postfix_lhs ? postfix_lhs->Lhs->To<asts::TypeAst>() : nullptr;
    const auto lhs_as_ident = lhs.To<asts::IdentifierAst>();

    // If the lhs is an identifier, it must be a variable symbol, not a namespace symbol.
    if (lhs_as_ident and sm.CurrentScope->GetVarSymbol(asts::AstClone(lhs_as_ident)) == nullptr) {
        RaiseMissingIdentifierAndClosestOptions(*lhs_as_ident, sm.CurrentScope->AllVarSymbols(), {}, sm);
    }

    // Variables that will be set in each branch, and returned.
    auto fn_owner_type = Shared<asts::TypeAst>(nullptr);
    auto fn_owner_scope = static_cast<scopes::Scope const*>(nullptr);
    auto fn_name = Shared<asts::IdentifierAst>(nullptr);

    // Runtime access into an object: "object.method()".
    if (postfix_lhs != nullptr and runtime_field != nullptr) {
        fn_owner_type = postfix_lhs->Lhs->InferType(&sm, meta);
        fn_name = runtime_field->Name;
        fn_owner_scope = sm.CurrentScope->GetTypeSymbol(fn_owner_type)->LinkedScope;
    }

    // Static access into a type: "Type::method()" or "ns::Type::method()".
    else if (static_field != nullptr and postfix_lhs_as_type != nullptr) {
        fn_owner_type = asts::AstCloneShared(postfix_lhs_as_type);
        fn_name = static_field->Name;
        fn_owner_scope = sm.CurrentScope->GetTypeSymbol(fn_owner_type)->LinkedScope;
    }

    // Direct access into a namespaced free function: "std::io::print(variable)".
    else if (postfix_lhs != nullptr and static_field != nullptr) {
        fn_owner_scope = sm.CurrentScope->ConvertPostfixToNestedScope(postfix_lhs->Lhs.get());
        fn_name = static_field->Name;
        fn_owner_type = fn_owner_scope->GetVarSymbol(fn_name)->Type;
    }

    // Direct access into a non-namespaced function: "function()":
    else if (lhs_as_ident != nullptr) {
        fn_owner_type = nullptr;
        fn_name = asts::AstCloneShared(lhs_as_ident);
        fn_owner_scope = sm.CurrentScope->ParentModule();
    }

    // Non-callable AST.
    else {
        fn_owner_type = nullptr;
        fn_name = nullptr;
        fn_owner_scope = nullptr;
    }

    return std::make_tuple(fn_owner_type, fn_owner_scope, fn_name);
}

auto spp::analyse::utils::func_utils::ConvertMethodToFuncForm(
    asts::TypeAst const &function_owner_type,
    asts::IdentifierAst const &function_name,
    asts::PostfixExpressionAst const &lhs,
    asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> Pair<Unique<asts::PostfixExpressionAst>, Unique<asts::PostfixExpressionOperatorFunctionCallAst>> {
    auto self_arg_val = asts::AstClone(lhs.Lhs);

    // Create the static method access (without the function call and args).
    auto field = MakeUnique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, AstClone(&function_name));
    auto field_access = MakeUnique<asts::PostfixExpressionAst>(
        AstClone(&function_owner_type), std::move(field));

    // Create an argument for "self" and inject it into the current arguments.
    auto self_arg = MakeUnique<asts::FunctionCallArgumentPositionalAst>(
        nullptr, nullptr, std::move(self_arg_val));
    auto fn_args = std::move(fn_call.FnArgGroup->Args);
    fn_args.Insert(fn_args.begin(), std::move(self_arg));

    // Create the function call with the new arguments.
    auto new_fn_call = MakeUnique<asts::PostfixExpressionOperatorFunctionCallAst>(
        AstClone(fn_call.GnArgGroup), AstClone(fn_call.FnArgGroup), nullptr);
    new_fn_call->FnArgGroup->Args = std::move(fn_args);
    new_fn_call->FnArgGroup->Args[0]->SetSelfType(lhs.Lhs->InferType(&sm, meta));

    // Return the new ASTs.
    return MakePair(std::move(field_access), std::move(new_fn_call));
}

auto spp::analyse::utils::func_utils::GetAllFunctionScopes(
    asts::IdentifierAst const &target_fn_name,
    scopes::Scope const *target_scope,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> Vec<std::tuple<scopes::Scope const*, asts::FunctionPrototypeAst*, Unique<asts::GenericArgumentGroupAst>, Shared<asts::TypeAst>>> {
    // Todo: TIDY this function big time. I WANT TO VOMIT.
    // If the name is empty (non-symbolic call) then return "no scopes".
    // If the target scope is nullptr, then the functions are bein superimposed over a generic type.
    if (target_fn_name.Val.empty() or target_scope == nullptr) { return {}; }

    // Get the function-type name from the function: "func()" => "$Func".
    const auto mapped_name = target_fn_name.ToFuncIdentifier();
    auto overload_scopes = Vec<std::tuple<scopes::Scope const*, asts::FunctionPrototypeAst*, Unique<asts::GenericArgumentGroupAst>, Shared<asts::TypeAst>>>();

    auto is_valid_ext_scope = [mapped_name=mapped_name.get()](auto const *scope) {
        const auto ext = scope->AstNode->template To<asts::SupPrototypeExtensionAst>();
        if (ext == nullptr) { return false; }
        const auto ext_name = dynamic_shared_cast<asts::TypeIdentifierAst>(ext->Name);
        return ext_name != nullptr and ext_name->Name == mapped_name->Val;
    };

    // Check for namespaced (module-level) functions (they will have no inheritable generics).
    if (target_scope->NsSym != nullptr) {
        for (auto *ancestor_scope : target_scope->Ancestors()) {
            for (auto const *sup_scope : ancestor_scope->Children | genex::views::ptr | genex::views::filter(is_valid_ext_scope)) {
                auto generics = asts::GenericArgumentGroupAst::NewEmpty();
                auto scope = sup_scope; // not for_override ? sup_scope->children[0].get() : sup_scope;
                auto proto = asts::AstBody(sup_scope->AstNode)[0]->To<asts::FunctionPrototypeAst>();
                overload_scopes.EmplaceBack(scope, proto, std::move(generics), nullptr);
            }
        }
    }

    // Functions belonging to a type will have inheritance generics from "sup [...] Type { ... }"
    else {
        // If a class scope was provided, get all the sup scopes from it, otherwise use the specific sup scope.
        const auto sup_scopes = target_scope->AstNode->To<asts::ClassPrototypeAst>() != nullptr
            ? target_scope->SupScopes() | genex::views::transform([](auto x) -> scopes::Scope const* { return x; }) | genex::to<Vec>()
            : Vec{target_scope};

        // From the super scopes, check each one for "sup $Func ext FunXXX { ... }" super-impositions. The TypeIdentifier cast is always valid because the function types are always the target - "$Func" etc.
        // Todo: use the "is_valid_ext_scope"?
        for (auto *sup_scope : sup_scopes) {
            for (auto *sup_ast : asts::AstBody(sup_scope->AstNode) | genex::views::cast_dynamic<asts::SupPrototypeExtensionAst*>()) {
                if (sup_ast->Name->To<asts::TypeIdentifierAst>()->Name == mapped_name->Val) {
                    auto generics = MakeUnique<asts::GenericArgumentGroupAst>(nullptr, sup_scope->GetGenerics(), nullptr);
                    auto scope = sup_scope;
                    auto proto = asts::AstBody(sup_ast)[0]->To<asts::FunctionPrototypeAst>();
                    overload_scopes.EmplaceBack(scope, proto, std::move(generics), nullptr);
                }
            }
        }

        // When a derived class has overridden a method, the base method must be removed.
        for (auto &&[scope_1, fn_1, _, _] : overload_scopes) {
            for (auto &&[scope_2, fn_2, _, _] : overload_scopes) {
                if (fn_1 != fn_2 and target_scope->DepthDiff(scope_1) < target_scope->DepthDiff(scope_2)) {
                    const auto temp = fn_1->GetAstScope()->Parent->Parent;
                    fn_1->GetAstScope()->Parent->Parent = const_cast<scopes::Scope*>(scope_1);

                    auto conflict = CheckForConflictingOverride(*fn_1->GetAstScope()->Parent, scope_2, *fn_1, sm, meta);
                    if (conflict != nullptr) {
                        overload_scopes |= genex::actions::remove_if([conflict](auto &&info) { return std::get<1>(info) == conflict; });
                    }
                    fn_1->GetAstScope()->Parent->Parent = temp;
                }
            }
        }

        // Adjust the scope in the tuple to the inner function scope.
        for (auto &&[i, info] : overload_scopes | genex::views::move | genex::views::enumerate | genex::to<Vec>()) {
            auto &[scope, proto, generics, t] = info;
            scope = (scope->Children | genex::views::ptr | genex::views::filter(is_valid_ext_scope) | genex::to<Vec>())[0];
            overload_scopes[i] = std::make_tuple(scope, proto, std::move(generics), t);
        }
    }

    // Next, get scopes from "forwarding types" (ie FwdRef and FwdMut return types).
    if (target_scope->TySym != nullptr and meta->CurrentStage >= 9.0) {
        auto [fwd_ref_type, fwd_mut_type] = type_utils::GetFwdTypes(*target_scope->TySym->FqName(), sm);
        if (fwd_ref_type != nullptr) {
            const auto inner_type = fwd_ref_type->TypeParts().Back()->GnArgGroup->TypeAt("T")->Val;
            auto inner_scopes = GetAllFunctionScopes(target_fn_name, sm.CurrentScope->GetTypeSymbol(inner_type)->LinkedScope, sm, meta);
            for (auto &&i : inner_scopes) {
                std::get<3>(i) = asts::AstCloneShared(inner_type);
            }
            std::ranges::move(inner_scopes, std::back_inserter(overload_scopes));
        }
    }

    // Return all the found function scopes.
    return overload_scopes;
}

auto spp::analyse::utils::func_utils::check_for_conflicting_overload(
    scopes::Scope const &this_scope,
    scopes::Scope const *target_scope,
    asts::FunctionPrototypeAst const &new_fn,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> asts::FunctionPrototypeAst* {
    //
    using type_utils::TypeEq;

    // Get the methods that belong to this type, or any of its supertypes.
    auto existing = GetAllFunctionScopes(*new_fn.Name, target_scope, sm, meta);
    auto existing_scopes = existing | genex::views::tuple_nth<0>();
    auto existing_fns = existing | genex::views::tuple_nth<1>();

    // Check for an overload conflict with all functions of the same name.
    for (auto [old_scope, old_fn] : genex::views::zip(existing_scopes, existing_fns)) {
        // Ignore if the method is an identical match on a base class (override) or is the same object.
        if (old_fn == &new_fn) { continue; }
        if (old_fn == CheckForConflictingOverride(this_scope, old_scope, new_fn, sm, meta, old_scope)) { continue; }

        // Ignore if the return types are different.
        if (not TypeEq(*new_fn.ReturnType, *old_fn->ReturnType, this_scope, *old_scope)) { continue; }

        // Ignore if there are a different number of required generic parameters.
        if (new_fn.GnParamGroup->GetTypeParams().Len() != old_fn->GnParamGroup->GetTypeParams().Len()) { continue; }
        if (new_fn.GnParamGroup->GetCompParams().Len() != old_fn->GnParamGroup->GetCompParams().Len()) { continue; }

        // Get the two parameter lists and create copies to remove duplicate parameters from.
        auto params_new = asts::AstCloneVec(new_fn.FnParamGroup->Params);
        auto params_old = asts::AstCloneVec(old_fn->FnParamGroup->Params);

        // Remove all the required parameters on the first parameter list off of the other parameter list.
        for (auto [p, q] : genex::views::zip(new_fn.FnParamGroup->Params | genex::views::ptr, old_fn->FnParamGroup->Params | genex::views::ptr)) {
            if (TypeEq(*p->Type, *q->Type, this_scope, *old_scope)) {
                params_new |= genex::actions::remove_if([pe=p->ExtractNames()](auto &&x) { return genex::equals(x->ExtractNames(), std::move(pe), {}, genex::meta::deref, genex::meta::deref); });
                params_old |= genex::actions::remove_if([qe=q->ExtractNames()](auto &&x) { return genex::equals(x->ExtractNames(), std::move(qe), {}, genex::meta::deref, genex::meta::deref); });
            }
        }

        // If neither parameter list contains a required parameter, throw an error.
        if (genex::operations::empty(params_new
            | genex::views::ptr
            | genex::views::concat(params_old | genex::views::ptr)
            | genex::views::cast_dynamic<asts::FunctionParameterRequiredAst*>()
            | genex::to<Vec>())) {
            return old_fn;
        }
    }
    return nullptr;
}

auto spp::analyse::utils::func_utils::CheckForConflictingOverride(
    scopes::Scope const &this_scope,
    scopes::Scope const *target_scope,
    asts::FunctionPrototypeAst const &new_fn,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta,
    scopes::Scope const *exclude_scope)
    -> asts::FunctionPrototypeAst* {
    //
    using type_utils::TypeEq;

    // Helper function to get the type of the convention AST applied to the "self" parameter.
    auto hs = [](asts::FunctionPrototypeAst const *f) {
        return f->FnParamGroup->GetSelfParam() != nullptr;
    };

    // Helper function to get the type of the convention AST applied to the "self" parameter.
    auto sc = [&hs](asts::FunctionPrototypeAst const *f) {
        return hs(f) ? f->FnParamGroup->GetSelfParam()->Conv.get() : nullptr;
    };

    // Get the existing functions that belong to this type, or any of its supertypes.
    auto existing = GetAllFunctionScopes(*new_fn.Name, target_scope, sm, meta);
    auto existing_scopes = existing | genex::views::tuple_nth<0>();
    auto existing_fns = existing | genex::views::tuple_nth<1>();

    auto param_names_eq = [](auto const &a, auto const &b) {
        if (a.Len() != b.Len()) { return false; }
        for (auto const &[x, y] : genex::views::zip(a, b)) {
            if (*x != *y) { return false; }
        }
        return true;
    };

    // Check for an overload conflict with all functions of the same name.
    // Note that the "cmp" state does NOT have to match.
    for (auto [old_scope, old_fn] : genex::views::zip(existing_scopes, existing_fns)) {
        // Ignore if the method is the same object.
        if (old_fn == &new_fn) { continue; }
        if (old_scope == exclude_scope) { continue; }

        // Get the two parameter lists and create copies.
        auto params_new = new_fn.FnParamGroup->GetNonSelfParams();
        auto params_old = old_fn->FnParamGroup->GetNonSelfParams();

        // Get a list of conditions to check for conflicting functions.
        if (params_new.Len() != params_old.Len()) { continue; }

        // All parameters must have the same names.
        if (genex::any_of(
            genex::views::zip(params_new, params_old) | genex::to<Vec>(),
            [&](auto pq) { return not param_names_eq(std::get<0>(pq)->ExtractNames(), std::get<1>(pq)->ExtractNames()); })) {
            continue;
        }

        // All parameters must have the same types.
        if (genex::any_of(
            genex::views::zip(params_new, params_old) | genex::to<Vec>(),
            [&](auto pq) { return not TypeEq(*std::get<0>(pq)->Type, *std::get<1>(pq)->Type, this_scope, *old_scope, false); })) {
            continue;
        }

        // The function type (subroutine vs coroutine) must match.
        if (new_fn.TokFun->TokenType != old_fn->TokFun->TokenType) {
            continue;
        }

        // The return types must be symbolically equal.
        if (not TypeEq(*new_fn.ReturnType, *old_fn->ReturnType, this_scope, *old_scope)) {
            continue;
        }

        // Check the self parameters' conventions.
        if (hs(&new_fn) != hs(old_fn) or (sc(&new_fn) and *sc(&new_fn) != sc(old_fn)) or (not sc(&new_fn) and sc(old_fn))) {
            continue;
        }

        // The functions must have identical signatures at this point, so return the old function.
        return old_fn;
    }

    return nullptr;
}

auto spp::analyse::utils::func_utils::enforce_no_invalid_fn_args(
    Vec<asts::FunctionParameterAst*> const &params,
    Vec<asts::FunctionCallArgumentKeywordAst*> const &named_args,
    scopes::ScopeManager &sm)
    -> void {
    // Get the parameter names using the extraction method.
    const auto p_names = params
        | genex::views::transform([](auto *x) { return x->ExtractName(); })
        | genex::to<Vec>();

    // Get the argument names using the attribute.
    const auto a_names = named_args
        | genex::views::transform([](auto *x) { return x->Name; })
        | genex::to<Vec>();

    // Check for invalid argument names against parameter names.
    const auto invalid_arg_names = a_names
        | genex::views::not_in(p_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

    // Raise an error if any invalid argument names were found.
    RaiseIf<errors::SppArgumentNameInvalidError>(
        not invalid_arg_names.IsEmpty(), {sm.CurrentScope},
        ERR_ARGS(*params[0], "fn param", *invalid_arg_names[0], "fn arg"));
}

template <typename GenericArgType, typename GenericParamType>
auto spp::analyse::utils::func_utils::enforce_no_invalid_gn_args(
    Vec<asts::GenericParameterAst*> const &params,
    Vec<asts::GenericArgumentAst*> const &named_args,
    scopes::ScopeManager &sm)
    -> void {
    // Shortcut.
    if (named_args.IsEmpty()) { return; }

    // Get the parameter names using the attribute.
    const auto p_names = params
        | genex::views::cast_dynamic<GenericParamType*>()
        | genex::views::transform([](auto *x) { return x->Name; })
        | genex::to<Vec>();

    // Get the argument names using the attribute.
    const auto a_names = named_args
        | genex::views::cast_dynamic<asts::detail::make_keyword_arg_t<GenericArgType>*>()
        | genex::views::transform([](auto *x) { return x->Name; })
        | genex::to<Vec>();

    // Check for invalid argument names against parameter names.
    const auto invalid_arg_names = a_names
        | genex::views::not_in(p_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

    // Raise an error if any invalid argument names were found.
    RaiseIf<errors::SppArgumentNameInvalidError>(
        not invalid_arg_names.IsEmpty(), {sm.CurrentScope},
        ERR_ARGS(*params[0], "gn param", *invalid_arg_names[0], "gn arg"));
}

template <typename InferenceResultMap>
auto spp::analyse::utils::func_utils::enforce_no_conflicting_inferred_gn_args(
    InferenceResultMap const &inferred,
    scopes::ScopeManager &sm)
    -> void {
    //
    using type_utils::TypeEq;
    using errors::SppGenericParameterInferredConflictInferredError;

    // Check for conflicting inferred arguments.
    for (auto [a_name, a_vals] : inferred) {
        auto mismatches = a_vals
            | genex::views::drop(1)
            | genex::views::filter([&](auto &&e) { return not TypeEq(*e, *a_vals[0], *sm.CurrentScope, *sm.CurrentScope); })
            | genex::to<Vec>();

        RaiseIf<SppGenericParameterInferredConflictInferredError>(
            not mismatches.IsEmpty(), {sm.CurrentScope},
            ERR_ARGS(*a_name, *a_vals[0], *mismatches[0]));
    }
}

auto spp::analyse::utils::func_utils::enforce_no_uninferred_gn_args(
    Vec<Shared<asts::TypeIdentifierAst>> const &p_names,
    Vec<Shared<asts::TypeIdentifierAst>> const &i_names,
    scopes::Scope const &owner_scope,
    Shared<asts::Ast> const &owner,
    scopes::ScopeManager &sm)
    -> void {
    //
    using errors::SppGenericParameterNotInferredError;

    // Check for uninferred arguments.
    const auto uninferred_params = p_names
        | genex::views::not_in(i_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

    RaiseIf<SppGenericParameterNotInferredError>(
        not uninferred_params.IsEmpty(), {sm.CurrentScope, &owner_scope},
        ERR_ARGS(*uninferred_params[0], *owner));
}

auto spp::analyse::utils::func_utils::enforce_no_generic_constraint_violations(
    Vec<Shared<asts::TypeIdentifierAst>> const &p_names,
    Vec<Vec<Shared<asts::TypeAst>>> const &p_con_groups,
    Vec<asts::GenericArgumentTypeKeywordAst*> const &type_args,
    Vec<asts::GenericArgumentAst*> const &all_args,
    scopes::Scope const &owner_scope,
    scopes::ScopeManager const &sm,
    asts::meta::CompilerMetaData const &meta)
    -> void {
    // Define the cloned constraint groups.
    auto p_con_groups_cloned = Vec<Vec<Shared<asts::TypeAst>>>();
    p_con_groups_cloned.reserve(p_names.Len());

    // Cross apply inferred into constraints firstly.
    for (auto const &p_con_group : p_con_groups) {
        auto con_group_cloned = Vec<Shared<asts::TypeAst>>();
        for (auto p_con : p_con_group) {
            auto def_type_raw = p_con->WithoutGenerics();
            if (auto def_val_type_sym = owner_scope.GetTypeSymbol(def_type_raw); def_val_type_sym != nullptr and meta.CurrentStage > 4) {
                auto temp = def_val_type_sym->FqName();
                temp = temp->WithGenerics(asts::AstClone(p_con->TypeParts().Back()->GnArgGroup));
                p_con = std::move(temp);
            }

            const auto sub = p_con->SubstituteGenerics(all_args);
            // sub->Stage7_AnalyseSemantics(&sm, &meta);
            con_group_cloned.push_back(sub);
        }
        p_con_groups_cloned.push_back(std::move(con_group_cloned));
    }

    // Now check that each argument satisfied its constraints.
    const auto args = type_args;
    for (auto [i, p_name] : p_names | genex::views::enumerate) {
        const auto p_cons = p_con_groups_cloned[i];
        const auto a = args[i];
        type_utils::EnforceGenericConstraints(p_cons, *a->Val, owner_scope, *sm.CurrentScope);
    }
}

auto spp::analyse::utils::func_utils::name_fn_args(
    asts::FunctionCallArgumentGroupAst &a_group,
    asts::FunctionParameterGroupAst const &p_group,
    scopes::ScopeManager &sm)
    -> void {
    // Validate the named arguments against the paramters.
    enforce_no_invalid_fn_args(p_group.GetAllParams(), a_group.GetKeywordArgs(), sm);

    // Get the names of the keyword arguments.
    auto a_names = a_group.GetKeywordArgs()
        | genex::views::transform([](auto *x) { return x->Name; })
        | genex::to<Vec>();

    // Get the names of the leftover parameters.
    auto p_names = p_group.GetAllParams()
        | genex::views::transform([](auto *x) { return x->ExtractName(); })
        | genex::views::not_in(a_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

    // Check for the existence of a variadic parameter.
    const auto is_variadic = p_group.GetVariadicParams() != nullptr;

    for (auto [i, positional_arg] : a_group.GetPositionalArgs() | genex::views::enumerate) {
        // Create the keyword argument from the positional argument.
        auto kw_arg = MakeUnique<asts::FunctionCallArgumentKeywordAst>(
            p_names.Front(), nullptr, nullptr, nullptr);
        p_names |= genex::actions::pop_front();

        // The variadic parameter requires a tuple of the remaining arguments.
        if (p_names.IsEmpty() and is_variadic) {
            auto elems = a_group.Args
                | genex::views::move
                | genex::views::drop(i)
                | genex::views::transform([](auto &&x) { return asts::AstClone(x->Val); })
                | genex::to<Vec>();
            kw_arg->Val = MakeUnique<asts::TupleLiteralAst>(nullptr, std::move(elems), nullptr);
            a_group.Args[i] = std::move(kw_arg);
            a_group.Args |= genex::actions::take(i + 1);
            break;
        }

        // Otherwise, attach the single argument convention and value.
        kw_arg->Conv = asts::AstClone(positional_arg->Conv);
        kw_arg->SetSelfType(positional_arg->GetSelfType());
        kw_arg->Val = asts::AstClone(positional_arg->Val);
        a_group.Args[i] = std::move(kw_arg);
    }
}

auto spp::analyse::utils::func_utils::name_gn_args(
    asts::GenericArgumentGroupAst &a_group,
    asts::GenericParameterGroupAst const &p_group,
    asts::Ast const &owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta,
    const bool is_tuple_owner)
    -> void {
    // Special case for tuples to prevent an infinite recursion.
    if (is_tuple_owner) { return; }

    // Split the arguments by moving off the group (will re-add back after).
    const auto comp_args = asts::GenericArgumentGroupAst::NewEmpty();
    const auto type_args = asts::GenericArgumentGroupAst::NewEmpty();
    for (auto &arg : a_group.Args) {
        if (arg->To<asts::GenericArgumentCompAst>())
            comp_args->Args.push_back(std::move(arg));
        else
            type_args->Args.push_back(std::move(arg));
    }
    a_group.Args.Clear();

    // Copy the raw pointer vectors from the splits.
    auto comp_params_raw = p_group.GetCompParams();
    auto type_params_raw = p_group.GetTypeParams();
    const auto comp_params = Vec<asts::GenericParameterAst*>(comp_params_raw.begin(), comp_params_raw.end());
    const auto type_params = Vec<asts::GenericParameterAst*>(type_params_raw.begin(), type_params_raw.end());

    // Name the two kinds of arguments separately.
    name_gn_args_impl<asts::GenericArgumentCompAst, asts::GenericParameterCompAst, asts::GenericParameterCompVariadicAst>(*comp_args, comp_params, owner, sm, meta);
    name_gn_args_impl<asts::GenericArgumentTypeAst, asts::GenericParameterTypeAst, asts::GenericParameterTypeVariadicAst>(*type_args, type_params, owner, sm, meta);

    // Recombine the named arguments back into the original argument group.
    std::ranges::move(comp_args->Args, std::back_inserter(a_group.Args));
    std::ranges::move(type_args->Args, std::back_inserter(a_group.Args));

    // Build index map once for O(n).
    auto param_index = ankerl::unordered_dense::map<StrView, std::size_t>();
    for (auto [i, p] : p_group.GetAllParams() | genex::views::enumerate) {
        param_index[p->Name->To<asts::TypeIdentifierAst>()->Name] = i;
    }

    a_group.Args |= genex::actions::sort([&](auto const &a, auto const &b) {
        return param_index[a->ViewName()] < param_index[b->ViewName()];
    });
}

template <typename GenericArgType, typename GenericParamType, typename GenericParamTypeVariadicAst>
auto spp::analyse::utils::func_utils::name_gn_args_impl(
    asts::GenericArgumentGroupAst &a_group,
    Vec<asts::GenericParameterAst*> const &params,
    asts::Ast const &owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> void {
    //
    using asts::generate::common_types::TupleType;
    using errors::SppGenericArgumentTooManyError;

    // Validate the named arguments against the parameters.
    enforce_no_invalid_gn_args<GenericArgType, GenericParamType>(
        params, a_group.GetAllArgs(), sm);

    // Get the names of the keyword arguments.
    auto a_names = a_group.GetKeywordArgs()
        | genex::views::cast_dynamic<asts::detail::make_keyword_arg_t<GenericArgType>*>()
        | genex::views::transform([](auto *x) { return x->Name; })
        | genex::to<Vec>();

    // Get the names of the leftover parameters.
    auto p_names = params
        | genex::views::transform([](auto *x) { return x->Name; })
        | genex::views::not_in(a_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

    // Check for the existence of a variadic parameter.
    const auto is_variadic = genex::any_of(params, [](auto *p) {
        return p->template To<GenericParamTypeVariadicAst>() != nullptr;
    });

    for (auto [i, positional_arg] : a_group.GetPositionalArgs() | genex::views::enumerate) {
        RaiseIf<SppGenericArgumentTooManyError>(
            p_names.IsEmpty(), {sm.CurrentScope},
            ERR_ARGS(params.IsEmpty() ? owner : *params[0], owner, *positional_arg));

        // Create the keyword argument from the positional argument (no value yet).
        auto kw_arg = MakeUnique<asts::detail::make_keyword_arg_t<GenericArgType>>(p_names.Front(), nullptr, nullptr);
        p_names |= genex::actions::pop_front();

        // The variadic parameter requires a tuple of the remaining arguments.
        if (p_names.IsEmpty() and is_variadic) {
            // Variadic check: map arguments "func[1_u32, 1_u32]" for "func[..s]" to "func[ts = (1_u32, 1_u32)]".
            if constexpr (std::same_as<GenericParamType, asts::GenericParameterCompAst>) {
                auto elems = MAKE_VARIADIC_COMP_ARGS(a_group.Args);
                auto tuple = MakeUnique<asts::TupleLiteralAst>(nullptr, std::move(elems), nullptr);
                kw_arg->Val = std::move(tuple);
            }

            // Variadic check: map arguments "func[U32, U32]" for "func[..Ts]" to "func[Ts = (U32, U32)]".
            else {
                auto elems = MAKE_VARIADIC_TYPE_ARGS(a_group.Args);
                auto tuple = TupleType(positional_arg->PosStart(), std::move(elems));
                if (meta.CurrentStage > 5) { tuple->Stage7_AnalyseSemantics(&sm, &meta); }
                kw_arg->Val = std::move(tuple);
            }

            a_group.Args[i] = std::move(kw_arg);
            a_group.Args |= genex::actions::take(i + 1);
            break;
        }

        // Otherwise, attach the single argument convention and value.
        kw_arg->Val = asts::AstClone(positional_arg->To<GenericArgType>()->Val);
        a_group.Args[i] = std::move(kw_arg);
    }
}

auto spp::analyse::utils::func_utils::infer_gn_args(
    asts::GenericParameterGroupAst const &p_group,
    asts::GenericArgumentGroupAst &args,
    InferenceSourceMap infer_source, // Note: must be copied, ignore hints.
    InferenceTargetMap infer_target, // Note: must be copied, ignore hints.
    Shared<asts::Ast> const &owner,
    scopes::Scope const &owner_scope,
    Shared<asts::IdentifierAst> const &variadic_fn_param_name,
    const bool is_tuple_owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> void {
    // Reset.
    meta.InferSource = {};
    meta.InferTarget = {};

    // Special case for tuples to prevent an infinite recursion, or there is nothing to infer.
    if (is_tuple_owner or p_group.Params.IsEmpty()) {
        return;
    }

    // Get raw param pointer vectors — params are read-only in impl, no clone needed.
    const auto comp_params = p_group.GetCompParams();
    const auto type_params = p_group.GetTypeParams();

    auto comp_args = Vec<Unique<asts::GenericArgumentCompKeywordAst>>();
    auto type_args = Vec<Unique<asts::GenericArgumentTypeKeywordAst>>();
    auto all_args = args.GetAllArgs();
    for (auto &&a : std::move(args.Args)) {
        if (a->To<asts::GenericArgumentCompAst>())
            comp_args.push_back(dynamic_unique_cast<asts::GenericArgumentCompKeywordAst>(std::move(a)));
        else
            type_args.push_back(dynamic_unique_cast<asts::GenericArgumentTypeKeywordAst>(std::move(a)));
    }

    // Call the two individual inference functions.
    // Todo: These need to move left-to-right no matter type vs comp
    // Todo: Because of cross substitution left to right between type and comp
    // Todo: Or do type, comp, type-cross-substitution again
    auto new_type_args = infer_gn_args_impl_type(type_params, type_args, all_args, infer_source, infer_target, owner, owner_scope, variadic_fn_param_name, sm, meta);
    auto new_comp_args = infer_gn_args_impl_comp(comp_params, comp_args, all_args, infer_source, infer_target, owner, owner_scope, variadic_fn_param_name, sm, meta);

    // Build index map once for O(n) lookup during sort.
    auto param_index = ankerl::unordered_dense::map<StrView, std::size_t>();
    for (auto [i, p] : p_group.GetAllParams() | genex::views::enumerate) {
        param_index[p->Name->To<asts::TypeIdentifierAst>()->Name] = i;
    }

    // Finally, sort the arguments back into the original parameter order.
    auto final_args = Vec<Unique<asts::GenericArgumentAst>>();
    for (auto &&type_arg : new_type_args) { final_args.push_back(std::move(type_arg)); }
    for (auto &&comp_arg : new_comp_args) { final_args.push_back(std::move(comp_arg)); }
    final_args |= genex::actions::sort([&](auto const &a, auto const &b) {
        return param_index[a->ViewName()] < param_index[b->ViewName()];
    });
    args.Args = std::move(final_args);
}

auto spp::analyse::utils::func_utils::infer_gn_args_impl_comp(
    Vec<asts::GenericParameterCompAst*> const &comp_params,
    Vec<Unique<asts::GenericArgumentCompKeywordAst>> const &comp_args,
    Vec<asts::GenericArgumentAst*> &all_args,
    InferenceSourceMap const &infer_source,
    InferenceTargetMap const &infer_target,
    Shared<asts::Ast> const &owner,
    scopes::Scope const &owner_scope,
    Shared<asts::IdentifierAst> const &,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> Vec<Unique<asts::GenericArgumentCompKeywordAst>> {
    //
    using type_utils::TypeEq;

    // Get the names for ease of use.
    auto p_names = comp_params
        | genex::views::transform([](auto &&x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
        | genex::to<Vec>();
    auto a_names = comp_args
        | genex::views::transform([](auto &&x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
        | genex::to<Vec>();
    auto inferred_args = InferenceResultCompMap();

    // Infer the generic arguments from the source/target maps.
    for (auto const &p_name : p_names) {
        for (auto const &[infer_target_name, infer_target_type] : infer_target) {
            auto inferred_arg = static_cast<asts::ExpressionAst*>(nullptr);

            // Check for a direct match ("a: T" & "a: Str") or an inner match ("a: Vec[T]" & "a: Vec[Str]").
            if (infer_source.contains(infer_target_name)) {
                auto temp_gs = type_utils::GenericInferenceMap();
                type_utils::RelaxedTypeEq(
                    *infer_source.at(infer_target_name)->WithoutConvention(),
                    *infer_target_type->WithoutConvention(),
                    *sm.CurrentScope, owner_scope, temp_gs, true);

                inferred_arg = temp_gs.contains(p_name) ? temp_gs[p_name] : nullptr;
            }

            // Handle the match if it exists.
            if (inferred_arg != nullptr) {
                inferred_args[p_name].EmplaceBack(inferred_arg);
            }
        }
    }

    // Load the pre-existing generic arguments into the inference map.
    for (auto const &arg : comp_args) {
        auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(arg->Name);
        inferred_args[cast_name].EmplaceBack(arg->Val.get());
    }

    // Add the missing arguments that have optional values.
    auto i_names = inferred_args | genex::views::keys | genex::to<Vec>();
    for (auto *opt_param : comp_params | genex::views::cast_dynamic<asts::GenericParameterCompOptionalAst*>()) {
        const auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(opt_param->Name);
        if (genex::contains(i_names, *cast_name, genex::meta::deref)) { continue; }
        inferred_args[cast_name].EmplaceBack(opt_param->DefaultVal.get());
        i_names = inferred_args | genex::views::keys | genex::to<Vec>();
    }

    // Check each generic argument name only has one unique inferred type. "cmp n: S32" cannot infer to "1" and "2".
    enforce_no_conflicting_inferred_gn_args(inferred_args, sm);
    enforce_no_uninferred_gn_args(p_names, i_names, owner_scope, owner, sm);

    // At this point, all conflicts have been checked, so it is safe to only use the first inferred value.
    auto formatted_args = InferenceFinalCompMap();
    for (auto [arg_name, inferred_vals] : inferred_args) {
        formatted_args[arg_name] = inferred_vals[0];
    }

    // Cross-apply generic arguments. This allows "Vec[T, A=Alloc[T]]" to substitute the new value of "T" into "Alloc".
    for (auto const &arg_name : inferred_args | genex::views::keys | genex::to<Vec>()) {
        auto filtered_formatted_args = formatted_args;
        filtered_formatted_args.erase(arg_name);
        auto other_args_group = asts::GenericArgumentGroupAst::FromMap(std::move(filtered_formatted_args));
        auto other_args_vec = other_args_group->Args | genex::views::ptr | genex::to<Vec>();
    }

    // Convert the inferred types into new generic arguments.
    auto final_args = std::remove_cvref_t<decltype(comp_args)>();
    for (auto const &[key, val] : formatted_args) {
        if (const auto val_as_type = val->To<asts::TypeAst>(); val_as_type != nullptr) {
            auto temp_val = asts::IdentifierAst::FromType(*val_as_type);
            auto temp_arg = MakeUnique<asts::GenericArgumentCompKeywordAst>(AstClone(key), nullptr, std::move(temp_val));
            final_args.EmplaceBack(std::move(temp_arg));
        }
        else {
            auto temp_val = AstClone(val);
            auto temp_arg = MakeUnique<asts::GenericArgumentCompKeywordAst>(AstClone(key), nullptr, std::move(temp_val));
            final_args.EmplaceBack(std::move(temp_arg));
        }
    }

    // Type-check the "comp" args. Only do this at the semantic analysis stage.
    if (meta.CurrentStage <= 7) { return final_args; }

    auto p_name_index_comp = ankerl::unordered_dense::map<StrView, std::size_t>();
    for (auto [i, p] : p_names | genex::views::enumerate) {
        p_name_index_comp[p->Name] = i;
    }
    final_args |= genex::actions::sort([&](auto const &a, auto const &b) {
        return p_name_index_comp[a->ViewName()] < p_name_index_comp[b->ViewName()];
    });

    for (auto &&[param, arg] : std::ranges::views::zip(comp_params, final_args)) {
        // Not convinced owner_scope mapping is correct here (see scopes for equality below)
        auto a_type = owner_scope.GetTypeSymbol(arg->Val->InferType(&sm, &meta))->FqName();
        auto p_type = param->Type->SubstituteGenerics(all_args);
        // p_type->Stage7_AnalyseSemantics(&sm, meta); // TODO: Needed?

        // For a variadic comp argument, check every element of the args tuple.
        if (param->To<asts::GenericParameterCompVariadicAst>()) {
            auto variadic_types = a_type->TypeParts().Back()->GnArgGroup->Args
                | genex::views::ptr
                | genex::views::cast_dynamic<asts::GenericArgumentTypePositionalAst*>()
                | genex::views::transform([](auto &&g) { return g->Val; })
                | genex::to<Vec>();

            for (auto const &a_type_inner : variadic_types) {
                RaiseIf<errors::SppTypeMismatchError>(
                    not TypeEq(*p_type, *a_type_inner, owner_scope, *sm.CurrentScope),
                    {&owner_scope, sm.CurrentScope}, ERR_ARGS(*param, *p_type, *arg, *a_type_inner));
            }
            break;
        }

        // Otherwise, just check the argument type against the parameter type.
        RaiseIf<errors::SppTypeMismatchError>(
            not TypeEq(*p_type, *a_type, owner_scope, *sm.CurrentScope),
            {&owner_scope, sm.CurrentScope}, ERR_ARGS(*param, *p_type, *arg, *a_type));
    }

    return final_args;
}

auto spp::analyse::utils::func_utils::infer_gn_args_impl_type(
    Vec<asts::GenericParameterTypeAst*> const &type_params,
    Vec<Unique<asts::GenericArgumentTypeKeywordAst>> const &type_args,
    Vec<asts::GenericArgumentAst*> &all_args,
    InferenceSourceMap const &infer_source,
    InferenceTargetMap const &infer_target,
    Shared<asts::Ast> const &owner,
    scopes::Scope const &owner_scope,
    Shared<asts::IdentifierAst> const &variadic_fn_param_name,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> Vec<Unique<asts::GenericArgumentTypeKeywordAst>> {
    // Get the names for ease of use.
    auto p_names = type_params
        | genex::views::transform([](auto &&x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
        | genex::to<Vec>();
    auto p_con_groups = type_params
        | genex::views::transform([](auto &&x) { return x->Constraints->Constraints; })
        | genex::to<Vec>();
    auto a_names = type_args
        | genex::views::transform([](auto &&x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
        | genex::to<Vec>();
    auto inferred_args = InferenceResultTypeMap();

    // Infer the generic arguments from the source/target maps.
    for (auto const &p_name : p_names) {
        for (auto const &[infer_target_name, infer_target_type] : infer_target) {
            auto inferred_arg = Shared<asts::TypeAst>(nullptr);

            // Check for a direct match ("a: T" & "a: Str") or an inner match ("a: Vec[T]" & "a: Vec[Str]").
            if (infer_source.contains(infer_target_name)) {
                auto temp_gs = type_utils::GenericInferenceMap();
                type_utils::RelaxedTypeEq(
                    *infer_source.at(infer_target_name)->WithoutConvention(),
                    *infer_target_type->WithoutConvention(),
                    *sm.CurrentScope, owner_scope, temp_gs, true);
                auto inferred_arg_raw = temp_gs.contains(p_name) ? temp_gs[p_name]->To<asts::TypeAst>() : nullptr;
                inferred_arg = inferred_arg_raw ? inferred_arg_raw->shared_from_this() : nullptr;
            }

            // Handle the match if it exists.
            if (inferred_arg != nullptr) {
                inferred_args[p_name].EmplaceBack(inferred_arg);
            }

            // Handle the variadic parameter if it exists.
            if (variadic_fn_param_name != nullptr and *infer_target_name == *variadic_fn_param_name) {
                auto &temp1 = inferred_args[p_name].Back()->TypeParts().Back()->GnArgGroup->Args[0];
                auto &temp2 = temp1->To<asts::GenericArgumentTypeAst>()->Val;
                inferred_args[p_name].PopBack();
                inferred_args[p_name].EmplaceBack(temp2);
            }
        }
    }

    // Load the pre-existing generic arguments into the inference map.
    for (auto const &arg : type_args) {
        auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(arg->Name);
        inferred_args[cast_name].EmplaceBack(arg->Val);
    }

    // Add the missing arguments that have optional values.
    auto i_names = inferred_args | genex::views::keys | genex::to<Vec>();
    for (auto *opt_param : type_params | genex::views::cast_dynamic<asts::GenericParameterTypeOptionalAst*>()) {
        const auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(opt_param->Name);
        if (genex::contains(i_names, *cast_name, genex::meta::deref)) { continue; }

        auto def_type = opt_param->DefaultVal;
        auto def_type_raw = def_type->WithoutGenerics();
        if (auto def_val_type_sym = owner_scope.GetTypeSymbol(def_type_raw); def_val_type_sym != nullptr and meta.CurrentStage > 4) {
            auto temp = def_val_type_sym->FqName()->WithConvention(asts::AstClone(def_type->GetConvention()));
            temp = temp->WithGenerics(asts::AstClone(def_type->TypeParts().Back()->GnArgGroup));
            def_type = std::move(temp);
        }
        inferred_args[cast_name].EmplaceBack(def_type);
        i_names = inferred_args | genex::views::keys | genex::to<Vec>();
    }

    // Check each generic argument name only has one unique inferred type. "T" cannot infer to "Str" and "U32".
    enforce_no_conflicting_inferred_gn_args(inferred_args, sm);
    enforce_no_uninferred_gn_args(p_names, i_names, owner_scope, owner, sm);

    // At this point, all conflicts have been checked, so it is safe to only use the first inferred value.
    auto formatted_args = InferenceFinalTypeMap();
    for (auto [arg_name, inferred_vals] : inferred_args) {
        formatted_args[arg_name] = inferred_vals[0];
    }

    // Cross-apply generic arguments. This allows "Vec[T, A=Alloc[T]]" to substitute the new value of "T" into "Alloc".
    // Todo: This needs to be left to right explicitly?
    for (auto const &arg_name : i_names) {
        if (genex::contains(a_names, *arg_name, genex::meta::deref)) { continue; }

        auto filtered_formatted_args = formatted_args;
        filtered_formatted_args.erase(arg_name);
        auto other_args_group = asts::GenericArgumentGroupAst::FromMap(std::move(filtered_formatted_args));
        auto other_args_vec = other_args_group->Args | genex::views::ptr | genex::to<Vec>();

        auto t = formatted_args[arg_name]->SubstituteGenerics(other_args_vec);
        t->Stage7_AnalyseSemantics(&sm, &meta);
        formatted_args[arg_name] = t;
    }

    // Convert the inferred types into new generic arguments.
    auto final_args = std::remove_cvref_t<decltype(type_args)>();
    for (auto &&[key, val] : formatted_args) {
        auto temp_arg = MakeUnique<asts::GenericArgumentTypeKeywordAst>(key, nullptr, val);
        final_args.EmplaceBack(std::move(temp_arg));
    }

    auto p_name_index_type = ankerl::unordered_dense::map<StrView, std::size_t>();
    for (auto [i, p] : p_names | genex::views::enumerate) {
        p_name_index_type[p->Name] = i;
    }
    final_args |= genex::actions::sort([&](auto const &a, auto const &b) {
        return p_name_index_type[a->ViewName()] < p_name_index_type[b->ViewName()];
    });

    // Finally, enforce the constraints on the inferred generic arguments.
    if (meta.CurrentStage < 9) { return final_args; }
    const auto type_args_raw = final_args | genex::views::ptr | genex::to<Vec>();
    enforce_no_generic_constraint_violations(p_names, p_con_groups, type_args_raw, all_args, owner_scope, sm, meta);
    return final_args;
}

auto spp::analyse::utils::func_utils::IsTargetCallable(
    asts::ExpressionAst &expr,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> Shared<asts::TypeAst> {
    // Get the type of the expression.
    auto expr_type = expr.InferType(&sm, meta);
    const auto is_type_functional = type_utils::IsTypeFunc(
        *expr_type, *sm.CurrentScope);
    return is_type_functional ? std::move(expr_type) : nullptr;
}

auto spp::analyse::utils::func_utils::CreateCallablePrototype(
    asts::TypeAst const &expr_type)
    -> Unique<asts::FunctionPrototypeAst> {
    // Extract the parameter and return types from the expression type.
    auto dummy_return_type = expr_type.TypeParts().Back()->GnArgGroup->TypeAt("Out")->Val;
    auto dummy_param_types = expr_type.TypeParts().Back()->GnArgGroup->TypeAt("Args")->Val->TypeParts().Back()->GnArgGroup->GetTypeArgs()
        | genex::views::transform([](auto *g) { return MakeUnique<asts::FunctionParameterRequiredAst>(nullptr, nullptr, g->Val); })
        | genex::views::cast_smart<asts::FunctionParameterAst>()
        | genex::to<Vec>();

    // Create a function prototype based off of the parameter and return type.
    // Todo: When might it be a coroutine, not a subroutine?
    // Todo: Do we set "cmp" here for the subroutine ever?
    auto dummy_param_group = MakeUnique<asts::FunctionParameterGroupAst>(nullptr, std::move(dummy_param_types), nullptr);
    auto dummy_name = MakeUnique<asts::IdentifierAst>(0uz, "<anonymous>");
    auto dummy_overload = MakeUnique<asts::SubroutinePrototypeAst>(
        SPP_NO_ANNOTATIONS, nullptr, nullptr, std::move(dummy_name), nullptr, std::move(dummy_param_group), nullptr,
        std::move(dummy_return_type), nullptr);

    // Return the function prototype.
    return dummy_overload;
}

auto spp::analyse::utils::func_utils::get_overload_types(
    asts::TypeAst const &overload_set_type,
    scopes::Scope const &scope)
    -> Vec<Shared<asts::TypeAst>> {
    // Extract the overload types from the overload set type and are functional.
    return scope.GetTypeSymbol(overload_set_type.shared_from_this())->LinkedScope->SupTypes()
        | genex::views::filter([&](auto &&t) { return type_utils::IsTypeFunc(*t, scope); })
        | genex::to<Vec>();
}

template auto spp::analyse::utils::func_utils::name_gn_args_impl<
    spp::asts::GenericArgumentCompAst,
    spp::asts::GenericParameterCompAst,
    spp::asts::GenericParameterCompVariadicAst>(
    asts::GenericArgumentGroupAst &a_group,
    Vec<asts::GenericParameterAst*> const &params,
    asts::Ast const &owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta) -> void;

template auto spp::analyse::utils::func_utils::name_gn_args_impl<
    spp::asts::GenericArgumentTypeAst,
    spp::asts::GenericParameterTypeAst,
    spp::asts::GenericParameterTypeVariadicAst>(
    asts::GenericArgumentGroupAst &a_group,
    Vec<asts::GenericParameterAst*> const &params,
    asts::Ast const &owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta) -> void;
