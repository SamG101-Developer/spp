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
import spp.utils.algorithms;
import spp.utils.ptr;
import spp.utils.types;
import spp.utils.uid;
import ankerl;
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
    auto field = MakeUnique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(
        nullptr, AstClone(&function_name));
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
    new_fn_call->Source.OriginalExpr = fn_call.Source.OriginalExpr;

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
                // This depth difference checker ensures the derived version is kept.
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

auto spp::analyse::utils::func_utils::CheckForConflictingOverload(
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
        auto tmp = genex::views::concat(params_new | genex::views::ptr, params_old | genex::views::ptr) | genex::to<Vec>();
        if (genex::operations::empty(tmp
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

auto spp::analyse::utils::func_utils::EnforceNoInvalidFnArgs(
    Vec<asts::FunctionParameterAst*> const &params,
    Vec<asts::FunctionCallArgumentKeywordAst*> const &named_args,
    scopes::ScopeManager &sm)
    -> void {
    //
    using errors::SppArgumentNameInvalidError;

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
    RaiseIf<SppArgumentNameInvalidError>(
        not invalid_arg_names.IsEmpty(), {sm.CurrentScope},
        ERR_ARGS(*params[0], "fn param", *invalid_arg_names[0], "fn arg"));
}

template <typename GenericArgType, typename GenericParamType>
auto spp::analyse::utils::func_utils::EnforceNoInvalidGnArgs(
    Vec<asts::GenericParameterAst*> const &params,
    Vec<asts::GenericArgumentAst*> const &named_args,
    scopes::ScopeManager &sm)
    -> void {
    // Shortcut.
    using errors::SppArgumentNameInvalidError;
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
    RaiseIf<SppArgumentNameInvalidError>(
        not invalid_arg_names.IsEmpty(), {sm.CurrentScope},
        ERR_ARGS(*params[0], "gn param", *invalid_arg_names[0], "gn arg"));
}

template <typename InferenceResultMap>
auto spp::analyse::utils::func_utils::EnforceNoConflictingInferredGnArgs(
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

auto spp::analyse::utils::func_utils::EnforceNoUninferredGnArgs(
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

auto spp::analyse::utils::func_utils::EnforceGenericConstraintsAllArgs(
    asts::GenericParameterGroupAst const &p_group,
    asts::GenericArgumentGroupAst const &a_group,
    scopes::Scope const &owner_scope,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> void {
    using errors::SppGenericConstraintError;

    // Extract important information.
    auto p_names = p_group.GetTypeParams()
        | genex::views::transform([](auto &&x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
        | genex::to<Vec>();
    auto p_con_groups = p_group.GetTypeParams()
        | genex::views::transform([](auto &&x) { return x->Constraints->Constraints; })
        | genex::to<Vec>();
    const auto all_args = a_group.GetAllArgs();
    const auto type_args = a_group.GetTypeArgs();

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
            sub->Stage7_AnalyseSemantics(&sm, &meta);
            con_group_cloned.push_back(sub);
        }
        p_con_groups_cloned.push_back(std::move(con_group_cloned));
    }

    // Now check that each argument satisfied its constraints.
    // Look up each arg by name rather than by position: type_args may also contain
    // class-level generic args that are absent from p_group, so a positional loop
    // would produce index offsets and check the wrong arg against the wrong constraint.
    for (auto [i, p_name] : p_names | genex::views::enumerate) {
        const auto p_cons = p_con_groups_cloned[i];
        auto matching = type_args
            | genex::views::filter([&](auto const *a) { return a->ViewName() == p_name->Name; })
            | genex::to<Vec>();
        if (matching.IsEmpty()) { continue; }

        // Raise an error if any constraint of this argument is not satisfied.
        const auto unsatisfied = type_utils::EnforceGenericConstraintsOneArg(p_cons, *matching[0]->Val, owner_scope, *sm.CurrentScope);
        RaiseIf<SppGenericConstraintError>(
            unsatisfied != nullptr, {&owner_scope, sm.CurrentScope},
            ERR_ARGS(*unsatisfied, *matching[0]->Val));
    }
}

auto spp::analyse::utils::func_utils::NameFnArgs(
    asts::FunctionCallArgumentGroupAst &a_group,
    asts::FunctionParameterGroupAst const &p_group,
    scopes::ScopeManager &sm)
    -> void {
    // Validate the named arguments against the parameters.
    EnforceNoInvalidFnArgs(p_group.GetAllParams(), a_group.GetKeywordArgs(), sm);

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
        kw_arg->Conv = std::move(positional_arg->Conv);
        kw_arg->SetSelfType(positional_arg->GetSelfType());
        kw_arg->Val = std::move(positional_arg->Val);
        a_group.Args[i] = std::move(kw_arg);
    }
}

auto spp::analyse::utils::func_utils::NameGnArgs(
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
    NameGnArgsImpl<asts::GenericArgumentCompAst, asts::GenericParameterCompAst, asts::GenericParameterCompVariadicAst>(*comp_args, comp_params, owner, sm, meta);
    NameGnArgsImpl<asts::GenericArgumentTypeAst, asts::GenericParameterTypeAst, asts::GenericParameterTypeVariadicAst>(*type_args, type_params, owner, sm, meta);

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
auto spp::analyse::utils::func_utils::NameGnArgsImpl(
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
    EnforceNoInvalidGnArgs<GenericArgType, GenericParamType>(
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

    meta.Save();
    meta.TypeAnalysisTypeScope = nullptr;

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
        if (meta.CurrentStage > 5) { kw_arg->Val->Stage7_AnalyseSemantics(&sm, &meta); }
        a_group.Args[i] = std::move(kw_arg);
    }

    meta.Restore();
}

// Run RelaxedTypeEq on one (source, target) pair and distribute results by param kind.
static auto CollectDirectInferences(
    spp::Shared<spp::asts::TypeAst> const &source_type,
    spp::Shared<spp::asts::TypeAst> const &target_type,
    spp::Shared<spp::asts::IdentifierAst> const &target_name,
    spp::Vec<spp::Shared<spp::asts::TypeIdentifierAst>> const &type_p_names,
    spp::Vec<spp::Shared<spp::asts::TypeIdentifierAst>> const &comp_p_names,
    spp::Shared<spp::asts::IdentifierAst> const &variadic_fn_param_name,
    spp::analyse::scopes::Scope const &owner_scope,
    spp::analyse::scopes::ScopeManager &sm,
    spp::analyse::utils::func_utils::InferenceResultTypeMap &type_inferred,
    spp::analyse::utils::func_utils::InferenceResultCompMap &comp_inferred)
    -> void {
    //
    auto temp_gs = spp::analyse::utils::type_utils::GenericInferenceMap();
    spp::analyse::utils::type_utils::RelaxedTypeEq(
        *source_type->WithoutConvention(),
        *target_type->WithoutConvention(),
        *sm.CurrentScope, owner_scope, temp_gs, true);

    //
    for (auto const &[inferred_name, inferred_val] : temp_gs) {
        if (genex::contains(type_p_names, *inferred_name, genex::meta::deref)) {
            auto *typed = inferred_val->To<spp::asts::TypeAst>();
            if (typed == nullptr) { continue; }
            auto shared = typed->shared_from_this();
            // Variadic unwrap: inferred value is Tup[T]; extract the inner type.
            if (variadic_fn_param_name != nullptr and target_name != nullptr and *target_name == *variadic_fn_param_name) {
                auto &inner = shared->TypeParts().Back()->GnArgGroup->Args[0];
                shared = inner->To<spp::asts::GenericArgumentTypeAst>()->Val;
            }
            type_inferred[inferred_name].EmplaceBack(std::move(shared));
        }
        else if (genex::contains(comp_p_names, *inferred_name, genex::meta::deref)) {
            comp_inferred[inferred_name].EmplaceBack(inferred_val);
        }
    }
}

auto spp::analyse::utils::func_utils::InferGnArgs(
    asts::GenericParameterGroupAst const &p_group,
    asts::GenericArgumentGroupAst &a_group,
    InferenceSourceMap infer_source,
    InferenceTargetMap infer_target,
    Shared<asts::Ast> const &owner,
    scopes::Scope const &owner_scope,
    Shared<asts::IdentifierAst> const &variadic_fn_param_name,
    const bool is_tuple_owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> void {
    using errors::SppGenericConstraintError;
    using errors::SppTypeMismatchError;
    using type_utils::TypeEq;

    meta.InferSource = {};
    meta.InferTarget = {};

    if (is_tuple_owner or p_group.Params.IsEmpty()) { return; }

    // Separate param lists and extract names and constraint groups.
    const auto type_params = p_group.GetTypeParams();
    const auto comp_params = p_group.GetCompParams();

    auto type_p_names = type_params
        | genex::views::transform([](auto *x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
        | genex::to<Vec>();
    auto comp_p_names = comp_params
        | genex::views::transform([](auto *x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
        | genex::to<Vec>();

    // Split explicit args into type and comp args.
    auto explicit_type_args = Vec<Unique<asts::GenericArgumentTypeKeywordAst>>();
    auto explicit_comp_args = Vec<Unique<asts::GenericArgumentCompKeywordAst>>();
    for (auto &&a : std::move(a_group.Args)) {
        if (a->To<asts::GenericArgumentCompAst>())
            explicit_comp_args.push_back(dynamic_unique_cast<asts::GenericArgumentCompKeywordAst>(std::move(a)));
        else
            explicit_type_args.push_back(dynamic_unique_cast<asts::GenericArgumentTypeKeywordAst>(std::move(a)));
    }
    auto type_a_names = explicit_type_args
        | genex::views::transform([](auto const &x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
        | genex::to<Vec>();

    // Build the 2 inference maps at the same time for future cross application.
    auto type_inferred = InferenceResultTypeMap();
    auto comp_inferred = InferenceResultCompMap();

    for (auto const &arg : explicit_type_args) {
        type_inferred[dynamic_shared_cast<asts::TypeIdentifierAst>(arg->Name)].EmplaceBack(arg->Val);
    }
    for (auto const &arg : explicit_comp_args) {
        comp_inferred[dynamic_shared_cast<asts::TypeIdentifierAst>(arg->Name)].EmplaceBack(arg->Val.get());
    }

    // First inference comes from the infer source and target maps.
    for (auto const &[target_name, target_type] : infer_target) {
        if (not infer_source.contains(target_name)) { continue; }
        CollectDirectInferences(
            infer_source.at(target_name), target_type, target_name, type_p_names, comp_p_names, variadic_fn_param_name,
            owner_scope, sm, type_inferred, comp_inferred);
    }

    // Next is constraint based inference, where for example [U, F: FunRef[(), U]] can infer U from the return type of
    // whatever subtype of F is FunRef, and U is extractable as a generic argument.
    {
        auto type_i_names_seen = type_inferred | genex::views::keys | genex::to<Vec>();

        for (auto *param : type_params) {
            if (param->Constraints->Constraints.IsEmpty()) { continue; }
            const auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(param->Name);

            // Find the inferred concrete type for this param.
            Shared<asts::TypeAst> inferred_type;
            for (auto const &[k, vals] : type_inferred) {
                if (*k == *cast_name and not vals.IsEmpty()) {
                    inferred_type = vals[0];
                    break;
                }
            }
            if (inferred_type == nullptr) { continue; }

            // Build the candidate list from the type and all subtypes.
            const auto concrete_sym = sm.CurrentScope->GetTypeSymbol(inferred_type);
            auto candidates = Vec<Shared<asts::TypeAst>>{};
            if (concrete_sym != nullptr and not concrete_sym->IsGeneric) {
                candidates.EmplaceBack(concrete_sym->FqName());
                if (concrete_sym->LinkedScope != nullptr) {
                    for (auto const *sup_scope : concrete_sym->LinkedScope->SupScopes()) {
                        if (sup_scope->AstNode->To<asts::ClassPrototypeAst>() == nullptr) { continue; }
                        candidates.EmplaceBack(sup_scope->TySym->FqName());
                    }
                }
            }

            for (auto const &constraint : param->Constraints->Constraints) {
                // Try each candidate in order and stop at the first match.
                auto temp_gs = type_utils::GenericInferenceMap();
                auto matched = false;
                for (auto const &candidate : candidates) {
                    temp_gs.clear();
                    if (type_utils::RelaxedTypeEq(
                        *candidate->WithoutConvention(),
                        *constraint->WithoutConvention(),
                        *sm.CurrentScope, owner_scope, temp_gs, true, false)) {
                        matched = true;
                        break;
                    }
                }

                // Niche constraint error that needs to be added here, otherwise we get misleading errors from
                // fallthrough. The parameter had a concrete inferred type (so candidates were available), but none of
                // them satisfied this constraint. If the constraint is what other generic parameters are inferred
                // through (eg the "U" in "P: FunMov[(T,), Opt[U]]"), then the supplied argument simply does not fit the
                // constraint's shape. Surface that as a constraint error now, rather than letting the dependent
                // parameter fall through and fail later with a misleading "generic parameter not inferred" error that
                // hides the real cause. Constraints that reference no other generics (eg "P: Copy") are left to the
                // authoritative TypeEq-based EnforceGenericConstraintsAllArgs check, to avoid any RelaxedTypeEq
                // false-negative rejecting a valid call here.
                if (not candidates.IsEmpty() and not matched) {
                    const auto constraint_drives_inference = genex::any_of(
                        type_params, [&](auto const *other) { return constraint->ContainsGenerics(*other); });
                    RaiseIf<SppGenericConstraintError>(
                        constraint_drives_inference,
                        {sm.CurrentScope, &owner_scope}, ERR_ARGS(*constraint, *inferred_type));
                }

                for (auto const &[inferred_name, inferred_val] : temp_gs) {
                    // Skip names already present in the map to avoid duplicate entries.
                    if (genex::contains(type_i_names_seen, *inferred_name, genex::meta::deref)) { continue; }

                    if (genex::contains(type_p_names, *inferred_name, genex::meta::deref)) {
                        auto *typed = inferred_val->To<asts::TypeAst>();
                        if (typed == nullptr) { continue; }
                        type_inferred[inferred_name].EmplaceBack(typed->shared_from_this());
                        type_i_names_seen = type_inferred | genex::views::keys | genex::to<Vec>();
                    }
                    else if (genex::contains(comp_p_names, *inferred_name, genex::meta::deref)) {
                        comp_inferred[inferred_name].EmplaceBack(inferred_val);
                    }
                }
            }
        }
    }

    // Apply optional defaults for still unknown type params. Type params may need qualification.
    auto type_i_names = type_inferred | genex::views::keys | genex::to<Vec>();
    for (auto *opt_param : type_params | genex::views::cast_dynamic<asts::GenericParameterTypeOptionalAst*>()) {
        const auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(opt_param->Name);
        if (genex::contains(type_i_names, *cast_name, genex::meta::deref)) { continue; }
        auto def_type = opt_param->DefaultVal;
        auto def_type_raw = def_type->WithoutGenerics();
        if (auto def_sym = owner_scope.GetTypeSymbol(def_type_raw); def_sym != nullptr and meta.CurrentStage > 4) {
            auto temp = def_sym->FqName()->WithConvention(asts::AstClone(def_type->GetConvention()));
            if (not type_utils::IsTypeSelf(*def_type)) {
                temp = temp->WithGenerics(asts::AstClone(def_type->TypeParts().Back()->GnArgGroup));
            }
            def_type = std::move(temp);
        }
        type_inferred[cast_name].EmplaceBack(def_type);
        type_i_names = type_inferred | genex::views::keys | genex::to<Vec>();
    }

    // Apply optional defaults for still unknown comp params.
    auto comp_i_names = comp_inferred | genex::views::keys | genex::to<Vec>();
    for (auto *opt_param : comp_params | genex::views::cast_dynamic<asts::GenericParameterCompOptionalAst*>()) {
        const auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(opt_param->Name);
        if (genex::contains(comp_i_names, *cast_name, genex::meta::deref)) { continue; }
        comp_inferred[cast_name].EmplaceBack(opt_param->DefaultVal.get());
        comp_i_names = comp_inferred | genex::views::keys | genex::to<Vec>();
    }

    // Validate there are no conflicting candidates or uninferred required params.
    EnforceNoConflictingInferredGnArgs(type_inferred, sm);
    EnforceNoConflictingInferredGnArgs(comp_inferred, sm);
    EnforceNoUninferredGnArgs(type_p_names, type_i_names, owner_scope, owner, sm);
    EnforceNoUninferredGnArgs(comp_p_names, comp_i_names, owner_scope, owner, sm);

    // Collapse the inferences to "single winner" maps (all are matching).
    auto type_formatted = InferenceFinalTypeMap();
    for (auto [name, vals] : type_inferred) { type_formatted[name] = vals[0]; }
    auto comp_formatted = InferenceFinalCompMap();
    for (auto [name, vals] : comp_inferred) { comp_formatted[name] = vals[0]; }

    // Cross-apply: substitute all known values (type + comp together) into each type param's resolved type. Handles
    // "Vec[T, A=Alloc[T]]" style defaults and type<->comp cross-substitution.
    for (auto const &type_name : type_i_names) {
        if (genex::contains(type_a_names, *type_name, genex::meta::deref)) { continue; }

        // Build unified "all other" substitution, skipping current name to avoid cycles.
        auto other_unified = type_utils::GenericInferenceMap();
        for (auto const &[n, v] : type_formatted) {
            if (*n != *type_name) { other_unified.emplace(n, v.get()); }
        }
        for (auto const &[n, v] : comp_formatted) { other_unified.emplace(n, v); }
        auto other_group = asts::GenericArgumentGroupAst::FromMap(other_unified);
        auto other_args = other_group->GetAllArgs();

        auto t = type_formatted[type_name]->SubstituteGenerics(other_args);
        t->Stage7_AnalyseSemantics(&sm, &meta);
        type_formatted[type_name] = t;
    }

    // Emit the final arg list sorted into parameter declaration order.
    auto param_index = ankerl::unordered_dense::map<StrView, std::size_t>();
    for (auto [i, p] : p_group.GetAllParams() | genex::views::enumerate) {
        param_index[p->Name->To<asts::TypeIdentifierAst>()->Name] = i;
    }

    auto final_args = Vec<Unique<asts::GenericArgumentAst>>();
    for (auto const &[key, val] : type_formatted) {
        final_args.push_back(MakeUnique<asts::GenericArgumentTypeKeywordAst>(key, nullptr, val));
    }
    for (auto const &[key, val] : comp_formatted) {
        if (const auto *val_as_type = val->To<asts::TypeAst>(); val_as_type != nullptr) {
            final_args.push_back(MakeUnique<asts::GenericArgumentCompKeywordAst>(
                asts::AstClone(key), nullptr, asts::IdentifierAst::FromType(*val_as_type)));
        }
        else {
            final_args.push_back(MakeUnique<asts::GenericArgumentCompKeywordAst>(
                asts::AstClone(key), nullptr, asts::AstClone(val)));
        }
    }
    final_args |= genex::actions::sort([&](auto const &a, auto const &b) {
        return param_index[a->ViewName()] < param_index[b->ViewName()];
    });
    a_group.Args = std::move(final_args);

    // Cmp argument type-checking (semantic stage only). Done after args are restored onto a_group so that the following
    // issue is solved: when analysing SizedIntegerSigned[32_u32], 32_u32 is inferred and checked. But as it is
    // inferred, the generics were missing, because this function temporarily removes them. So we only analyse AFTER
    // they are re-added having been checked.
    if (meta.CurrentStage > 7) {
        auto all_final_unified = type_utils::GenericInferenceMap();
        for (auto const &[n, v] : type_formatted) { all_final_unified.emplace(n, v.get()); }
        for (auto const &[n, v] : comp_formatted) { all_final_unified.emplace(n, v); }
        auto all_final_group = asts::GenericArgumentGroupAst::FromMap(all_final_unified);
        auto all_final_args = all_final_group->GetAllArgs();

        auto comp_p_name_index = ankerl::unordered_dense::map<StrView, std::size_t>();
        for (auto [i, p] : comp_p_names | genex::views::enumerate) { comp_p_name_index[p->Name] = i; }

        auto sorted_comp = comp_formatted | genex::to<Vec>();
        sorted_comp |= genex::actions::sort([&](auto const &a, auto const &b) {
            return comp_p_name_index[a.first->Name] < comp_p_name_index[b.first->Name];
        });

        for (auto &&[kv, param] : genex::views::zip(sorted_comp, comp_params)) {
            auto *inferred_val = kv.second;
            auto a_type = owner_scope.GetTypeSymbol(inferred_val->InferType(&sm, &meta))->FqName();
            auto p_type = param->Type->SubstituteGenerics(all_final_args);

            if (param->To<asts::GenericParameterCompVariadicAst>()) {
                for (auto const &inner : a_type->TypeParts().Back()->GnArgGroup->Args
                     | genex::views::ptr
                     | genex::views::cast_dynamic<asts::GenericArgumentTypePositionalAst*>()
                     | genex::views::transform([](auto *g) { return g->Val; })
                     | genex::to<Vec>()) {
                    RaiseIf<SppTypeMismatchError>(
                        not TypeEq(*p_type, *inner, owner_scope, *sm.CurrentScope),
                        {&owner_scope, sm.CurrentScope}, ERR_ARGS(*param, *p_type, *inferred_val, *inner));
                }
                break;
            }
            auto raw_a_type = inferred_val->InferType(&sm, &meta);
            RaiseIf<SppTypeMismatchError>(
                not TypeEq(*p_type, *a_type, owner_scope, *sm.CurrentScope),
                {&owner_scope, sm.CurrentScope}, ERR_ARGS(*param, *p_type, *inferred_val, *raw_a_type));
        }
    }
}

auto spp::analyse::utils::func_utils::IsTargetCallable(
    asts::ExpressionAst &expr,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> Shared<const asts::TypeAst> {
    // Get the type of the expression, then find its functional
    // type.
    using type_utils::GetFunctionalType;
    auto expr_type = expr.InferType(&sm, meta);
    auto func_type = GetFunctionalType(*expr_type, *sm.CurrentScope);

    // Return the expr_type unless its generic, in which case
    // return the "func_type" -> got from constraints.
    const auto is_generic = sm.CurrentScope->GetTypeSymbol(expr_type)->IsGeneric;
    return is_generic ? func_type : expr_type;
}

auto spp::analyse::utils::func_utils::CreateCallablePrototype(
    asts::TypeAst const &expr_type)
    -> Unique<asts::FunctionPrototypeAst> {
    // Extract the parameter and return types from the expression type.
    auto dummy_return_type = expr_type.TypeParts().Back()->GnArgGroup->TypeAt("Out")->Val;
    auto dummy_param_types = expr_type.TypeParts().Back()->GnArgGroup->TypeAt("Args")->Val->TypeParts().Back()->GnArgGroup->GetTypeArgs()
        | genex::views::transform([](auto *g) { return MakeUnique<asts::FunctionParameterRequiredAst>(nullptr, nullptr, g->Val); })
        | spp::views::cast_unique<asts::FunctionParameterAst>();

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

auto spp::analyse::utils::func_utils::GetOverloadTypes(
    asts::TypeAst const &overload_set_type,
    scopes::Scope const &scope)
    -> Vec<Shared<asts::TypeAst>> {
    // Extract the overload types from the overload set type and are functional.
    return scope.GetTypeSymbol(overload_set_type.shared_from_this())->LinkedScope->SupTypes()
        | genex::views::filter([&](auto &&t) { return type_utils::IsTypeFunc(*t, scope); })
        | genex::to<Vec>();
}

template auto spp::analyse::utils::func_utils::NameGnArgsImpl<
    spp::asts::GenericArgumentCompAst,
    spp::asts::GenericParameterCompAst,
    spp::asts::GenericParameterCompVariadicAst>(
    asts::GenericArgumentGroupAst &a_group,
    Vec<asts::GenericParameterAst*> const &params,
    asts::Ast const &owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta) -> void;

template auto spp::analyse::utils::func_utils::NameGnArgsImpl<
    spp::asts::GenericArgumentTypeAst,
    spp::asts::GenericParameterTypeAst,
    spp::asts::GenericParameterTypeVariadicAst>(
    asts::GenericArgumentGroupAst &a_group,
    Vec<asts::GenericParameterAst*> const &params,
    asts::Ast const &owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta) -> void;
