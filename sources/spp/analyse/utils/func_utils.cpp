module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.func_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
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
import ankerl;
import genex;


auto spp::analyse::utils::func_utils::get_function_owner_type_and_function_name(
    asts::ExpressionAst const &lhs,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<std::shared_ptr<asts::TypeAst>, scopes::Scope const*, std::shared_ptr<asts::IdentifierAst>> {
    // Define some expression casts that are used commonly.
    const auto postfix_lhs = lhs.to<asts::PostfixExpressionAst>();
    const auto runtime_field = postfix_lhs
        ? postfix_lhs->op->to<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>()
        : nullptr;
    const auto static_field = postfix_lhs
        ? postfix_lhs->op->to<asts::PostfixExpressionOperatorStaticMemberAccessAst>()
        : nullptr;

    // Specific casts.
    const auto postfix_lhs_as_type = postfix_lhs ? postfix_lhs->lhs->to<asts::TypeAst>() : nullptr;
    const auto lhs_as_ident = lhs.to<asts::IdentifierAst>();

    // Variables that will be set in each branch, and returned.
    auto fn_owner_type = std::shared_ptr<asts::TypeAst>(nullptr);
    auto fn_owner_scope = static_cast<scopes::Scope const*>(nullptr);
    auto fn_name = std::shared_ptr<asts::IdentifierAst>(nullptr);

    // Runtime access into an object: "object.method()".
    if (postfix_lhs != nullptr and runtime_field != nullptr) {
        fn_owner_type = postfix_lhs->lhs->infer_type(&sm, meta);
        fn_name = runtime_field->name;
        fn_owner_scope = sm.current_scope->get_type_symbol(fn_owner_type)->scope;
    }

    // Static access into a type: "Type::method()" or "ns::Type::method()".
    else if (static_field != nullptr and postfix_lhs_as_type != nullptr) {
        fn_owner_type = asts::ast_clone(postfix_lhs_as_type);
        fn_name = static_field->name;
        fn_owner_scope = sm.current_scope->get_type_symbol(fn_owner_type)->scope;
    }

    // Direct access into a namespaced free function: "std::io::print(variable)".
    else if (postfix_lhs != nullptr and static_field != nullptr) {
        fn_owner_scope = sm.current_scope->convert_postfix_to_nested_scope(postfix_lhs->lhs.get());
        fn_name = static_field->name;
        fn_owner_type = fn_owner_scope->get_var_symbol(fn_name)->type;
    }

    // Direct access into a non-namespaced function: "function()":
    else if (lhs_as_ident != nullptr) {
        fn_owner_type = nullptr;
        fn_name = asts::ast_clone(lhs_as_ident);
        fn_owner_scope = sm.current_scope->parent_module();
    }

    // Non-callable AST.
    else {
        fn_owner_type = nullptr;
        fn_name = nullptr;
        fn_owner_scope = nullptr;
    }

    return std::make_tuple(fn_owner_type, fn_owner_scope, fn_name);
}


auto spp::analyse::utils::func_utils::convert_method_to_function_form(
    asts::TypeAst const &function_owner_type,
    asts::IdentifierAst const &function_name,
    asts::PostfixExpressionAst const &lhs,
    asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> std::pair<
        std::unique_ptr<asts::PostfixExpressionAst>,
        std::unique_ptr<asts::PostfixExpressionOperatorFunctionCallAst>> {
    /* REFACTOR TO OWN FUNCTION */
    // The "self" argument will be the lhs.lhs if is symbolic, otherwise a materialization.
    auto self_arg_val = std::unique_ptr<asts::ExpressionAst>(nullptr);
    if (const auto o = sm.current_scope->get_var_symbol_outermost(*lhs.lhs).first; o != nullptr) {
        self_arg_val = asts::ast_clone(lhs.lhs);
    }
    else if (lhs.lhs->to<asts::LiteralAst>() == nullptr) {
        // Use object initializer mock object.
        auto mock = std::make_unique<asts::ObjectInitializerAst>(lhs.lhs->infer_type(&sm, meta), nullptr);
        self_arg_val = std::move(mock);
    }
    else {
        // Create a "let" statement, then use the identifier.
        auto var_name = std::make_shared<asts::IdentifierAst>(0, spp::utils::generate_uid());
        const auto let_stmt = ({
            auto var = std::make_unique<asts::LocalVariableSingleIdentifierAst>(nullptr, var_name, nullptr);
            std::make_unique<asts::LetStatementInitializedAst>(
                nullptr, std::move(var), nullptr, nullptr, asts::ast_clone(lhs.lhs));
        });
        let_stmt->stage_7_analyse_semantics(&sm, meta);
        sm.current_scope->get_var_symbol(var_name)->comptime_value = asts::ast_clone(lhs.lhs);
        self_arg_val = asts::ast_clone(var_name);
    }

    // Create the static method access (without the function call and args)
    auto field = std::make_unique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, ast_clone(&function_name));
    auto field_access = std::make_unique<asts::PostfixExpressionAst>(ast_clone(&function_owner_type), std::move(field));

    // Create an argument for "self" and inject it into the current arguments.
    auto self_arg = std::make_unique<asts::FunctionCallArgumentPositionalAst>(
        nullptr, nullptr, std::move(self_arg_val));
    auto fn_args = std::move(fn_call.arg_group->args);
    fn_args.insert(fn_args.begin(), std::move(self_arg));

    // Create the function call with the new arguments.
    auto new_fn_call = std::make_unique<asts::PostfixExpressionOperatorFunctionCallAst>(
        ast_clone(fn_call.generic_arg_group), ast_clone(fn_call.arg_group), nullptr);
    new_fn_call->arg_group->args = std::move(fn_args);
    new_fn_call->arg_group->args[0]->set_self_type(lhs.lhs->infer_type(&sm, meta));

    // Return the new ASTs.
    return std::make_pair(std::move(field_access), std::move(new_fn_call));
}


auto spp::analyse::utils::func_utils::get_all_function_scopes(
    asts::IdentifierAst const &target_fn_name,
    scopes::Scope const *target_scope,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> std::vector<std::tuple<scopes::Scope const*, asts::FunctionPrototypeAst*, std::unique_ptr<asts::GenericArgumentGroupAst>, std::shared_ptr<asts::TypeAst>>> {
    // Todo: TIDY this function big time. I WANT TO VOMIT.
    // If the name is empty (non-symbolic call) then return "no scopes".
    // If the target scope is nullptr, then the functions are bein superimposed over a generic type.
    if (target_fn_name.val.empty() or target_scope == nullptr) { return {}; }

    // Get the function-type name from the function: "func()" => "$Func".
    const auto mapped_name = target_fn_name.to_function_identifier();
    auto overload_scopes = std::vector<std::tuple<scopes::Scope const*, asts::FunctionPrototypeAst*, std::unique_ptr<asts::GenericArgumentGroupAst>, std::shared_ptr<asts::TypeAst>>>();

    auto is_valid_ext_scope = [mapped_name=mapped_name.get()](auto const *scope) {
        const auto ext = scope->ast->template to<asts::SupPrototypeExtensionAst>();
        if (ext == nullptr) { return false; }
        const auto ext_name = std::dynamic_pointer_cast<asts::TypeIdentifierAst>(ext->name);
        return ext_name != nullptr and ext_name->name == mapped_name->val;
    };

    // Check for namespaced (module-level) functions (they will have no inheritable generics).
    if (target_scope->ns_sym != nullptr) {
        for (auto *ancestor_scope : target_scope->ancestors()) {
            for (auto const *sup_scope : ancestor_scope->children | genex::views::ptr | genex::views::filter(is_valid_ext_scope)) {
                auto generics = asts::GenericArgumentGroupAst::new_empty();
                auto scope = sup_scope; // not for_override ? sup_scope->children[0].get() : sup_scope;
                auto proto = asts::ast_body(sup_scope->ast)[0]->to<asts::FunctionPrototypeAst>();
                overload_scopes.emplace_back(scope, proto, std::move(generics), nullptr);
            }
        }
    }

    // Functions belonging to a type will have inheritance generics from "sup [...] Type { ... }"
    else {
        // If a class scope was provided, get all the sup scopes from it, otherwise use the specific sup scope.
        const auto sup_scopes = target_scope->ast->to<asts::ClassPrototypeAst>() != nullptr
            ? target_scope->sup_scopes() | genex::views::transform([](auto x) -> scopes::Scope const* { return x; }) | genex::to<std::vector>()
            : std::vector{target_scope};

        // From the super scopes, check each one for "sup $Func ext FunXXX { ... }" super-impositions. The TypeIdentifier cast is always valid because the function types are always the target - "$Func" etc.
        // Todo: use the "is_valid_ext_scope"?
        for (auto *sup_scope : sup_scopes) {
            for (auto *sup_ast : asts::ast_body(sup_scope->ast) | genex::views::cast_dynamic<asts::SupPrototypeExtensionAst*>()) {
                if (sup_ast->name->to<asts::TypeIdentifierAst>()->name == mapped_name->val) {
                    auto generics = std::make_unique<asts::GenericArgumentGroupAst>(nullptr, sup_scope->get_generics(), nullptr);
                    auto scope = sup_scope;
                    auto proto = asts::ast_body(sup_ast)[0]->to<asts::FunctionPrototypeAst>();
                    overload_scopes.emplace_back(scope, proto, std::move(generics), nullptr);
                }
            }
        }

        // When a derived class has overridden a method, the base method must be removed.
        for (auto &&[scope_1, fn_1, _, _] : overload_scopes) {
            for (auto &&[scope_2, fn_2, _, _] : overload_scopes) {
                if (fn_1 != fn_2 and target_scope->depth_difference(scope_1) < target_scope->depth_difference(scope_2)) {
                    auto conflict = check_for_conflicting_override(*scope_1, scope_2, *fn_1, sm, meta);
                    if (conflict != nullptr) {
                        overload_scopes |= genex::actions::remove_if([conflict](auto &&info) { return std::get<1>(info) == conflict; });
                    }
                }
            }
        }

        // Adjust the scope in the tuple to the inner function scope.
        for (auto &&[i, info] : overload_scopes | genex::views::move | genex::views::enumerate | genex::to<std::vector>()) {
            auto &[scope, proto, generics, t] = info;
            scope = (scope->children | genex::views::ptr | genex::views::filter(is_valid_ext_scope) | genex::to<std::vector>())[0];
            overload_scopes[i] = std::make_tuple(scope, proto, std::move(generics), t);
        }
    }

    // Next, get scopes from "forwarding types" (ie FwdRef and FwdMut return types).
    if (target_scope->ty_sym != nullptr and meta->current_stage >= 9.0) {
        auto [fwd_ref_type, fwd_mut_type] = type_utils::get_fwd_types(*target_scope->ty_sym->fq_name(), sm);
        if (fwd_ref_type != nullptr) {
            const auto inner_type = fwd_ref_type->type_parts().back()->generic_arg_group->type_at("T")->val;
            auto inner_scopes = get_all_function_scopes(target_fn_name, sm.current_scope->get_type_symbol(inner_type)->scope, sm, meta);
            for (auto &&i : inner_scopes) {
                std::get<3>(i) = asts::ast_clone(inner_type);
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
    // Get the methods that belong to this type, or any of its supertypes.
    auto existing = get_all_function_scopes(*new_fn.orig_name, target_scope, sm, meta);
    auto existing_scopes = existing | genex::views::tuple_nth<0>();
    auto existing_fns = existing | genex::views::tuple_nth<1>();

    // Check for an overload conflict with all functions of the same name.
    for (auto [old_scope, old_fn] : genex::views::zip(existing_scopes, existing_fns)) {
        // Ignore if the method is an identical match on a base class (override) or is the same object.
        if (old_fn == &new_fn) { continue; }
        if (old_fn == check_for_conflicting_override(this_scope, old_scope, new_fn, sm, meta, old_scope)) { continue; }

        // Ignore if the return types are different.
        if (not type_utils::symbolic_eq(*new_fn.return_type, *old_fn->return_type, this_scope, *old_scope)) { continue; }

        // Ignore if there are a different number of required generic parameters.
        if (new_fn.generic_param_group->get_type_params().size() != old_fn->generic_param_group->get_type_params().size()) { continue; }
        if (new_fn.generic_param_group->get_comp_params().size() != old_fn->generic_param_group->get_comp_params().size()) { continue; }

        // Get the two parameter lists and create copies to remove duplicate parameters from.
        auto params_new = asts::ast_clone_vec(new_fn.param_group->params);
        auto params_old = asts::ast_clone_vec(old_fn->param_group->params);

        // Remove all the required parameters on the first parameter list off of the other parameter list.
        for (auto [p, q] : genex::views::zip(new_fn.param_group->params | genex::views::ptr, old_fn->param_group->params | genex::views::ptr)) {
            if (type_utils::symbolic_eq(*p->type, *q->type, this_scope, *old_scope)) {
                params_new |= genex::actions::remove_if([pe=p->extract_names()](auto &&x) { return genex::equals(x->extract_names(), std::move(pe), {}, genex::meta::deref, genex::meta::deref); });
                params_old |= genex::actions::remove_if([qe=q->extract_names()](auto &&x) { return genex::equals(x->extract_names(), std::move(qe), {}, genex::meta::deref, genex::meta::deref); });
            }
        }

        // If neither parameter list contains a required parameter, throw an error.
        if (genex::operations::empty(params_new
            | genex::views::ptr
            | genex::views::concat(params_old | genex::views::ptr)
            | genex::views::cast_dynamic<asts::FunctionParameterRequiredAst*>()
            | genex::to<std::vector>())) {
            return old_fn;
        }
    }
    return nullptr;
}


auto spp::analyse::utils::func_utils::check_for_conflicting_override(
    scopes::Scope const &this_scope,
    scopes::Scope const *target_scope,
    asts::FunctionPrototypeAst const &new_fn,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta,
    scopes::Scope const *exclude_scope)
    -> asts::FunctionPrototypeAst* {
    // Helper function to get the type of the convention AST applied to the "self" parameter.
    auto hs = [](asts::FunctionPrototypeAst const *f) {
        return f->param_group->get_self_param() != nullptr;
    };

    // Helper function to get the type of the convention AST applied to the "self" parameter.
    auto sc = [&hs](asts::FunctionPrototypeAst const *f) {
        return hs(f) ? f->param_group->get_self_param()->conv.get() : nullptr;
    };

    // Get the existing functions that belong to this type, or any of its supertypes.
    auto existing = get_all_function_scopes(*new_fn.orig_name, target_scope, sm, meta);
    auto existing_scopes = existing | genex::views::tuple_nth<0>();
    auto existing_fns = existing | genex::views::tuple_nth<1>();

    auto param_names_eq = [](auto const &a, auto const &b) {
        if (a.size() != b.size()) { return false; }
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
        auto params_new = new_fn.param_group->get_non_self_params();
        auto params_old = old_fn->param_group->get_non_self_params();

        // Get a list of conditions to check for conflicting functions.
        if (params_new.size() != params_old.size()) { continue; }

        // All parameters must have the same names.
        if (genex::any_of(
            genex::views::zip(params_new, params_old) | genex::to<std::vector>(),
            [&](auto pq) { return not param_names_eq(std::get<0>(pq)->extract_names(), std::get<1>(pq)->extract_names()); })) {
            continue;
        }

        // All parameters must have the same types.
        if (genex::any_of(
            genex::views::zip(params_new, params_old) | genex::to<std::vector>(),
            [&](auto pq) { return not type_utils::symbolic_eq(*std::get<0>(pq)->type, *std::get<1>(pq)->type, this_scope, *old_scope, false); })) {
            continue;
        }

        // The function type (subroutine vs coroutine) must match.
        if (new_fn.tok_fun->token_type != old_fn->tok_fun->token_type) {
            continue;
        }

        // The return types must be symbolically equal.
        if (not type_utils::symbolic_eq(*new_fn.return_type, *old_fn->return_type, this_scope, *old_scope)) {
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
    std::vector<asts::FunctionParameterAst*> const &params,
    std::vector<asts::FunctionCallArgumentKeywordAst*> const &named_args,
    scopes::ScopeManager &sm)
    -> void {
    // Get the parameter names using the extraction method.
    const auto p_names = params
        | genex::views::transform([](auto *x) { return x->extract_name(); })
        | genex::to<std::vector>();

    // Get the argument names using the attribute.
    const auto a_names = named_args
        | genex::views::transform([](auto *x) { return x->name; })
        | genex::to<std::vector>();

    // Check for invalid argument names against parameter names.
    const auto invalid_arg_names = a_names
        | genex::views::not_in(p_names, genex::meta::deref, genex::meta::deref)
        | genex::to<std::vector>();

    // Raise an error if any invalid argument names were found.
    raise_if<errors::SppArgumentNameInvalidError>(
        not invalid_arg_names.empty(), {sm.current_scope},
        ERR_ARGS(*params[0], "fn param", *invalid_arg_names[0], "fn arg"));
}


template <typename GenericArgType, typename GenericParamType>
auto spp::analyse::utils::func_utils::enforce_no_invalid_gn_args(
    std::vector<asts::GenericParameterAst*> const &params,
    std::vector<asts::GenericArgumentAst*> const &named_args,
    scopes::ScopeManager &sm)
    -> void {
    // Get the parameter names using the attribute.
    const auto p_names = params
        | genex::views::cast_dynamic<GenericParamType*>()
        | genex::views::transform([](auto *x) { return x->name; })
        | genex::to<std::vector>();

    // Get the argument names using the attribute.
    const auto a_names = named_args
        | genex::views::cast_dynamic<asts::detail::make_keyword_arg_t<GenericArgType>*>()
        | genex::views::transform([](auto *x) { return x->name; })
        | genex::to<std::vector>();

    // Check for invalid argument names against parameter names.
    const auto invalid_arg_names = a_names
        | genex::views::not_in(p_names, genex::meta::deref, genex::meta::deref)
        | genex::to<std::vector>();

    // Raise an error if any invalid argument names were found.
    raise_if<errors::SppArgumentNameInvalidError>(
        not invalid_arg_names.empty(), {sm.current_scope},
        ERR_ARGS(*params[0], "gn param", *invalid_arg_names[0], "gn arg"));
}


template <typename InferenceResultMap>
auto spp::analyse::utils::func_utils::enforce_no_conflicting_inferred_gn_args(
    InferenceResultMap const &inferred,
    scopes::ScopeManager &sm)
    -> void {
    // Check for conflicting inferred arguments.
    for (auto [a_name, a_vals] : inferred) {
        auto mismatches = a_vals
            | genex::views::drop(1)
            | genex::views::filter([&](auto &&e) { return not type_utils::symbolic_eq(*e, *a_vals[0], *sm.current_scope, *sm.current_scope); })
            | genex::to<std::vector>();

        raise_if<errors::SppGenericParameterInferredConflictInferredError>(
            not mismatches.empty(), {sm.current_scope},
            ERR_ARGS(*a_name, *a_vals[0], *mismatches[0]));
    }
}


auto spp::analyse::utils::func_utils::enforce_no_uninferred_gn_args(
    std::vector<std::shared_ptr<asts::TypeIdentifierAst>> const &p_names,
    std::vector<std::shared_ptr<asts::TypeIdentifierAst>> const &i_names,
    scopes::Scope const &owner_scope,
    std::shared_ptr<asts::Ast> const &owner,
    scopes::ScopeManager &sm)
    -> void {
    // Check for uninferred arguments.
    const auto uninferred_params = p_names
        | genex::views::not_in(i_names, genex::meta::deref, genex::meta::deref)
        | genex::to<std::vector>();

    raise_if<errors::SppGenericParameterNotInferredError>(
        not uninferred_params.empty(), {sm.current_scope, &owner_scope},
        ERR_ARGS(*uninferred_params[0], *owner));
}


auto spp::analyse::utils::func_utils::name_fn_args(
    asts::FunctionCallArgumentGroupAst &a_group,
    asts::FunctionParameterGroupAst const &p_group,
    scopes::ScopeManager &sm)
    -> void {
    // Validate the named arguments against the paramters.
    enforce_no_invalid_fn_args(p_group.get_all_params(), a_group.get_keyword_args(), sm);

    // Get the names of the keyword arguments.
    auto a_names = a_group.get_keyword_args()
        | genex::views::transform([](auto *x) { return x->name; })
        | genex::to<std::vector>();

    // Get the names of the leftover parameters.
    auto p_names = p_group.get_all_params()
        | genex::views::transform([](auto *x) { return x->extract_name(); })
        | genex::views::not_in(a_names, genex::meta::deref, genex::meta::deref)
        | genex::to<std::vector>();

    // Check for the existence of a variadic parameter.
    const auto is_variadic = p_group.get_variadic_param() != nullptr;

    for (auto [i, positional_arg] : a_group.get_positional_args() | genex::views::enumerate) {
        // Create the keyword argument from the positional argument.
        auto kw_arg = std::make_unique<asts::FunctionCallArgumentKeywordAst>(
            p_names.front(), nullptr, nullptr, nullptr);
        p_names |= genex::actions::pop_front();

        // The variadic parameter requires a tuple of the remaining arguments.
        if (p_names.empty() and is_variadic) {
            auto elems = a_group.args
                | genex::views::move
                | genex::views::drop(i)
                | genex::views::transform([](auto &&x) { return asts::ast_clone(x->val); })
                | genex::to<std::vector>();
            kw_arg->val = std::make_unique<asts::TupleLiteralAst>(nullptr, std::move(elems), nullptr);
            a_group.args[i] = std::move(kw_arg);
            a_group.args |= genex::actions::take(i + 1);
            break;
        }

        // Otherwise, attach the single argument convention and value.
        kw_arg->conv = asts::ast_clone(positional_arg->conv);
        kw_arg->set_self_type(positional_arg->get_self_type());
        kw_arg->val = asts::ast_clone(positional_arg->val);
        a_group.args[i] = std::move(kw_arg);
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

    // Create temporary argument and parameter groups for the two kinds of generics.
    const auto comp_args = asts::GenericArgumentGroupAst::new_empty();
    const auto type_args = asts::GenericArgumentGroupAst::new_empty();
    const auto comp_params = asts::GenericParameterGroupAst::new_empty();
    const auto type_params = asts::GenericParameterGroupAst::new_empty();

    // Split the arguments and parameters into the two kinds.
    comp_args->args = ast_clone_vec<asts::GenericArgumentAst>(a_group.get_comp_args());
    type_args->args = ast_clone_vec<asts::GenericArgumentAst>(a_group.get_type_args());
    comp_params->params = ast_clone_vec<asts::GenericParameterAst>(p_group.get_comp_params());
    type_params->params = ast_clone_vec<asts::GenericParameterAst>(p_group.get_type_params());

    // Name the two kinds of arguments separately.
    name_gn_args_impl<asts::GenericArgumentCompAst, asts::GenericParameterCompAst>(*comp_args, *comp_params, owner, sm, meta);
    name_gn_args_impl<asts::GenericArgumentTypeAst, asts::GenericParameterTypeAst>(*type_args, *type_params, owner, sm, meta);

    // Recombine the named arguments back into the original argument group.
    a_group.args.clear();
    std::ranges::move(comp_args->args, std::back_inserter(a_group.args));
    std::ranges::move(type_args->args, std::back_inserter(a_group.args));

    // Finally, sort the arguments back into the original parameter order.
    a_group.args |= genex::actions::sort([&](auto const &a, auto const &b) {
        // Find the index of the first argument's parameter.
        const auto a_index = genex::position(p_group.get_all_params(), [&](auto *p) {
            return *p->name == *(a->template to<asts::GenericArgumentTypeKeywordAst>()
                ? a->template to<asts::GenericArgumentTypeKeywordAst>()->name
                : a->template to<asts::GenericArgumentCompKeywordAst>()->name);
        });

        // Find the index of the second argument's parameter.
        const auto b_index = genex::position(p_group.get_all_params(), [&](auto *p) {
            return *p->name == *(b->template to<asts::GenericArgumentTypeKeywordAst>()
                ? b->template to<asts::GenericArgumentTypeKeywordAst>()->name
                : b->template to<asts::GenericArgumentCompKeywordAst>()->name);
        });

        // Compare the two indices.
        return a_index < b_index;
    });
}


template <typename GenericArgType, typename GenericParamType>
auto spp::analyse::utils::func_utils::name_gn_args_impl(
    asts::GenericArgumentGroupAst &a_group,
    asts::GenericParameterGroupAst const &p_group,
    asts::Ast const &owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> void {

    // Validate the named arguments against the paramters.
    enforce_no_invalid_gn_args<GenericArgType, GenericParamType>(
        p_group.get_all_params(), a_group.get_all_args(), sm);

    // Get the names of the keyword arguments.
    auto a_names = a_group.get_keyword_args()
        | genex::views::cast_dynamic<asts::detail::make_keyword_arg_t<GenericArgType>*>()
        | genex::views::transform([](auto *x) { return x->name; })
        | genex::to<std::vector>();

    // Get the names of the leftover parameters.
    auto p_names = p_group.get_all_params()
        | genex::views::transform([](auto *x) { return x->name; })
        | genex::views::not_in(a_names, genex::meta::deref, genex::meta::deref)
        | genex::to<std::vector>();

    // Check for the existence of a variadic parameter.
    const auto is_variadic = p_group.get_variadic_param() != nullptr;

    for (auto [i, positional_arg] : a_group.get_positional_args() | genex::views::enumerate) {
        raise_if<errors::SppGenericArgumentTooManyError>(
            p_names.empty(), {sm.current_scope},
            ERR_ARGS(p_group.params.empty() ? owner : *p_group.params[0], owner, *positional_arg));

        // Create the keyword argument from the positional argument.
        auto kw_arg = std::make_unique<asts::detail::make_keyword_arg_t<GenericArgType>>(
            p_names.front(), nullptr, nullptr);
        p_names |= genex::actions::pop_front();

        // The variadic parameter requires a tuple of the remaining arguments.
        if (p_names.empty() and is_variadic) {
            if constexpr (std::same_as<GenericParamType, asts::GenericParameterCompAst>) {
                auto elems = a_group.args
                    | genex::views::move
                    | genex::views::drop(i)
                    | genex::views::transform([](auto &&x) { return asts::ast_clone(x->template to<GenericArgType>()->val); })
                    | genex::to<std::vector>();

                // Variadic check: map arguments "func[1_u32, 1_u32]" for "func[..s]" to "func[ts = (1_u32, 1_u32)]".
                auto tup_lit = std::make_unique<asts::TupleLiteralAst>(nullptr, std::move(elems), nullptr);
                kw_arg->val = std::move(tup_lit);
            }
            else {
                auto elems = a_group.args
                    | genex::views::move
                    | genex::views::drop(i)
                    | genex::views::transform([](auto &&x) { return std::shared_ptr(asts::ast_clone(x->template to<GenericArgType>()->val)); })
                    | genex::to<std::vector>();

                // Variadic check: map arguments "func[U32, U32]" for "func[..Ts]" to "func[Ts = (U32, U32)]".
                auto tup_type = asts::generate::common_types::tuple_type(positional_arg->pos_start(), std::move(elems));
                tup_type->stage_7_analyse_semantics(&sm, &meta);
                kw_arg->val = tup_type;
            }

            a_group.args[i] = std::move(kw_arg);
            a_group.args |= genex::actions::take(i + 1);
            break;
        }

        // Otherwise, attach the single argument convention and value.
        kw_arg->val = asts::ast_clone(positional_arg->to<GenericArgType>()->val);
        a_group.args[i] = std::move(kw_arg);
    }
}


auto spp::analyse::utils::func_utils::infer_gn_args(
    asts::GenericArgumentGroupAst &a_group,
    asts::GenericParameterGroupAst const &p_group,
    std::vector<asts::GenericArgumentAst*> const &explicit_args,
    InferenceSourceMap const &infer_source,
    InferenceTargetMap const &infer_target,
    std::shared_ptr<asts::Ast> const &owner,
    scopes::Scope const &owner_scope,
    std::shared_ptr<asts::IdentifierAst> const &variadic_fn_param_name,
    const bool is_tuple_owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta)
    -> void {
    // Special case for tuples to prevent an infinite recursion, or there is nothing to infer.
    if (is_tuple_owner or p_group.params.empty()) {
        const auto i = a_group.args.size();
        auto clone = asts::ast_clone_vec(explicit_args);
        std::ranges::move(clone, std::back_inserter(a_group.args));
        if (i > 0) { a_group.args |= genex::actions::drop(i); }
        return;
    }

    // Create temporary argument and parameter groups for the two kinds of generics.
    const auto comp_args = asts::GenericArgumentGroupAst::new_empty();
    const auto type_args = asts::GenericArgumentGroupAst::new_empty();
    const auto comp_params = asts::GenericParameterGroupAst::new_empty();
    const auto type_params = asts::GenericParameterGroupAst::new_empty();

    // Split the arguments and parameters into the two kinds.
    comp_args->args = ast_clone_vec<asts::GenericArgumentAst>(a_group.get_comp_args());
    type_args->args = ast_clone_vec<asts::GenericArgumentAst>(a_group.get_type_args());
    comp_params->params = ast_clone_vec<asts::GenericParameterAst>(p_group.get_comp_params());
    type_params->params = ast_clone_vec<asts::GenericParameterAst>(p_group.get_type_params());

    const auto comp_explicit_args = explicit_args
        | genex::views::cast_dynamic<asts::GenericArgumentCompKeywordAst*>()
        | genex::to<std::vector>();

    const auto type_explicit_args = explicit_args
        | genex::views::cast_dynamic<asts::GenericArgumentTypeKeywordAst*>()
        | genex::to<std::vector>();

    // Call the two individual inference functions.
    // Todo: These need to move left-to-right no matter type vs comp
    // Todo: Because of cross substitution left to right between type and comp
    // Todo: Or do type, comp, type-cross-substitution again
    infer_gn_args_impl_type(*type_args, *type_params, type_explicit_args, infer_source, infer_target, owner, owner_scope, variadic_fn_param_name, sm, meta);
    infer_gn_args_impl_comp(*comp_args, *comp_params, comp_explicit_args, infer_source, infer_target, owner, owner_scope, variadic_fn_param_name, sm, meta);

    // Recombine the named arguments back into the original argument group.
    a_group.args.clear();
    std::ranges::move(comp_args->args, std::back_inserter(a_group.args));
    std::ranges::move(type_args->args, std::back_inserter(a_group.args));

    // Finally, sort the arguments back into the original parameter order.
    a_group.args |= genex::actions::sort([&](auto const &a, auto const &b) {
        // Find the index of the first argument's parameter.
        const auto a_index = genex::position(p_group.get_all_params(), [&](auto *p) {
            return *p->name == *(a->template to<asts::GenericArgumentTypeKeywordAst>()
                ? a->template to<asts::GenericArgumentTypeKeywordAst>()->name
                : a->template to<asts::GenericArgumentCompKeywordAst>()->name);
        });

        // Find the index of the second argument's parameter.
        const auto b_index = genex::position(p_group.get_all_params(), [&](auto *p) {
            return *p->name == *(b->template to<asts::GenericArgumentTypeKeywordAst>()
                ? b->template to<asts::GenericArgumentTypeKeywordAst>()->name
                : b->template to<asts::GenericArgumentCompKeywordAst>()->name);
        });

        // Compare the two indices.
        return a_index < b_index;
    });
}



auto spp::analyse::utils::func_utils::infer_gn_args_impl_comp(
    asts::GenericArgumentGroupAst &a_group,
    asts::GenericParameterGroupAst const &p_group,
    std::vector<asts::GenericArgumentCompKeywordAst*> const &explicit_args,
    InferenceSourceMap const &infer_source,
    InferenceTargetMap const &infer_target,
    std::shared_ptr<asts::Ast> const &,
    scopes::Scope const &owner_scope,
    std::shared_ptr<asts::IdentifierAst> const &owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta) -> void {
    // Get the names for ease of use.
    auto p_names = p_group.params
        | genex::views::transform([](auto &&x) { return std::dynamic_pointer_cast<asts::TypeIdentifierAst>(x->name); })
        | genex::to<std::vector>();
    auto ea_names = explicit_args
        | genex::views::transform([](auto &&x) { return std::dynamic_pointer_cast<asts::TypeIdentifierAst>(x->name); })
        | genex::to<std::vector>();
    auto inferred_args = InferenceResultCompMap();

    // Preload the explicit generic arguments into the inference map.
    for (auto *arg : explicit_args) {
        inferred_args[std::dynamic_pointer_cast<asts::TypeIdentifierAst>(arg->name)].emplace_back(arg->val.get());
    }

    // Infer the generic arguments from the source/target maps.
    for (auto const &param_name : p_names) {
        for (auto const &[infer_target_name, infer_target_type] : infer_target) {
            auto inferred_arg = static_cast<asts::ExpressionAst const*>(nullptr);

            // Check for a direct match ("a: T" & "a: Str") or an inner match ("a: Vec[T]" & "a: Vec[Str]").
            if (infer_source.contains(infer_target_name)) {
                auto temp_gs = type_utils::GenericInferenceMap();
                type_utils::relaxed_symbolic_eq(
                    *infer_source.at(infer_target_name)->without_convention(),
                    *infer_target_type->without_convention(),
                    sm.current_scope, &owner_scope, temp_gs, true);
                inferred_arg = temp_gs[param_name];
            }

            // Handle the match if it exists.
            if (inferred_arg != nullptr) {
                inferred_args[param_name].emplace_back(inferred_arg);
            }
        }
    }

    // Fully qualify and type arguments (replaced within the inference map).
    auto i_names = inferred_args | genex::views::keys | genex::to<std::vector>();
    for (auto *opt_param : p_group.get_optional_params() | genex::views::cast_dynamic<asts::GenericParameterCompOptionalAst*>()) {
        const auto cast_name = std::dynamic_pointer_cast<asts::TypeIdentifierAst>(opt_param->name);
        if (genex::contains(i_names, *cast_name, genex::meta::deref)) { continue; }
        inferred_args[cast_name].emplace_back(opt_param->default_val.get());
        i_names = inferred_args | genex::views::keys | genex::to<std::vector>();
    }

    // Check each generic argument name only has one unique inferred type. "cmp n: S32" cannot infer to "1" and "2".
    enforce_no_conflicting_inferred_gn_args(inferred_args, sm);
    enforce_no_uninferred_gn_args(p_names, i_names, owner_scope, owner, sm);

    // At this point, all conflicts have been checked, so it is safe to only use the first inferred value.
    auto formatted_generic_arguments = std::map<std::shared_ptr<asts::TypeAst>, asts::ExpressionAst const*>();
    for (auto [arg_name, inferred_vals] : inferred_args) {
        formatted_generic_arguments[arg_name] = inferred_vals[0];
    }

    // todo: something is needed here, ie "[T, cmp x: Vec[T]]" needs to substitute the value for "T" into "Vec[T]".
    // Cross-apply generic arguments. This allows "Vec[T, A=Alloc[T]]" to substitute the new value of "T" into "Alloc".
    // for (auto const &arg_name : inferred_args | genex::views::keys | genex::to<std::vector>()) {
    //     if (genex::contains(explicit_arg_names, *arg_name, genex::meta::deref)) {
    //         continue;
    //     }
    //
    //     auto other_args = formatted_generic_arguments
    //         | genex::views::filter([&](auto &&p) { return *p.first != *arg_name; })
    //         | genex::to<std::vector>();
    //
    //     auto other_args_group = asts::GenericArgumentGroupAst::from_map(std::map(other_args.begin(), other_args.end()));
    //     auto other_args_vec = other_args_group->args | genex::views::ptr | genex::to<std::vector>();
    // }

    // Convert the inferred types into new generic arguments.
    auto i = a_group.args.size();
    for (auto const &[key, val] : formatted_generic_arguments) {
        const auto matching_param = genex::find(p_names, *key->to<asts::TypeIdentifierAst>(), genex::meta::deref);
        if (matching_param == p_names.end()) { continue; }

        if (const auto val_as_type = val->to<asts::TypeAst>(); val_as_type != nullptr) {
            auto temp_val = asts::IdentifierAst::from_type(*val_as_type);
            auto temp_arg = std::make_unique<asts::GenericArgumentCompKeywordAst>(ast_clone(key), nullptr, std::move(temp_val));
            a_group.args.emplace_back(std::move(temp_arg));
        }
        else {
            auto temp_val = ast_clone(val);
            auto temp_arg = std::make_unique<asts::GenericArgumentCompKeywordAst>(ast_clone(key), nullptr, std::move(temp_val));
            a_group.args.emplace_back(std::move(temp_arg));
        }
    }
    if (i > 0) { a_group.args |= genex::actions::drop(i); }

    // Type-check the "comp" args. Only do this at the semantic analysis stage.
    if (meta.current_stage <= 7) { return; }

    a_group.args |= genex::actions::sort([&](auto const &a, auto const &b) {
        const auto a_index = genex::position(p_names, [&](auto const &p) { return *p == *a->template to<asts::GenericArgumentCompKeywordAst>()->name; });
        const auto b_index = genex::position(p_names, [&](auto const &p) { return *p == *b->template to<asts::GenericArgumentCompKeywordAst>()->name; });
        return a_index < b_index;
    });

    for (auto &&[param, arg] : genex::views::zip(p_group.get_all_params(), a_group.get_all_args())) {
        auto comp_arg = arg->to<asts::GenericArgumentCompKeywordAst>();
        auto comp_param = param->to<asts::GenericParameterCompAst>();
        if (comp_arg == nullptr) { continue; }

        // Not convinces owner_scope mapping is correct here (see scopes for equality below)
        auto a_type = owner_scope.get_type_symbol(comp_arg->val->infer_type(&sm, &meta))->fq_name();
        auto p_type = comp_param->type->substitute_generics(a_group.get_all_args());
        // p_type->stage_7_analyse_semantics(&sm, meta); // TODO: Needed?

        // For a variadic comp argument, check every element of the args tuple.
        if (comp_param->to<asts::GenericParameterCompVariadicAst>()) {
            auto variadic_types = a_type->type_parts().back()->generic_arg_group->args
                | genex::views::ptr
                | genex::views::cast_dynamic<asts::GenericArgumentTypePositionalAst*>()
                | genex::views::transform([](auto &&g) { return g->val; })
                | genex::to<std::vector>();

            for (auto const &a_type_inner : variadic_types) {
                raise_if<errors::SppTypeMismatchError>(
                    not type_utils::symbolic_eq(*p_type, *a_type_inner, owner_scope, *sm.current_scope),
                    {&owner_scope, sm.current_scope}, ERR_ARGS(*comp_param, *p_type, *comp_arg, *a_type_inner));
            }
            break;
        }

        // Otherwise, just check the argument type against the parameter type.
        raise_if<errors::SppTypeMismatchError>(
            not type_utils::symbolic_eq(*p_type, *a_type, owner_scope, *sm.current_scope),
            {&owner_scope, sm.current_scope}, ERR_ARGS(*comp_param, *p_type, *comp_arg, *a_type));
    }
}


auto spp::analyse::utils::func_utils::infer_gn_args_impl_type(
    asts::GenericArgumentGroupAst &a_group,
    asts::GenericParameterGroupAst const &p_group,
    std::vector<asts::GenericArgumentTypeKeywordAst*> const &explicit_args,
    InferenceSourceMap const &infer_source,
    InferenceTargetMap const &infer_target,
    std::shared_ptr<asts::Ast> const &owner,
    scopes::Scope const &owner_scope,
    std::shared_ptr<asts::IdentifierAst> const &variadic_fn_param_name,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta) -> void {
    // Get the names for ease of use.
    auto p_names = p_group.params
        | genex::views::transform([](auto &&x) { return std::dynamic_pointer_cast<asts::TypeIdentifierAst>(x->name); })
        | genex::to<std::vector>();
    auto ea_names = explicit_args
        | genex::views::transform([](auto &&x) { return std::dynamic_pointer_cast<asts::TypeIdentifierAst>(x->name); })
        | genex::to<std::vector>();
    auto inferred_args = InferenceResultTypeMap();

    // Preload the explicit generic arguments into the inference map, as the consistency of these arguments needs
    // checking too.
    for (auto *arg : explicit_args) {
        auto cast_name = std::dynamic_pointer_cast<asts::TypeIdentifierAst>(arg->name);
        inferred_args[cast_name].emplace_back(arg->val);
    }

    // Infer the generic arguments from the source/target maps.
    for (auto const &param_name : p_names) {
        for (auto const &[infer_target_name, infer_target_type] : infer_target) {
            auto inferred_arg = std::shared_ptr<const asts::TypeAst>(nullptr);

            // Check for a direct match ("a: T" & "a: Str") or an inner match ("a: Vec[T]" & "a: Vec[Str]").
            if (infer_source.contains(infer_target_name)) {
                auto temp_gs = type_utils::GenericInferenceMap();
                type_utils::relaxed_symbolic_eq(
                    *infer_source.at(infer_target_name)->without_convention(),
                    *infer_target_type->without_convention(),
                    sm.current_scope, &owner_scope, temp_gs, true);
                auto inferred_arg_raw = temp_gs.contains(param_name) ? temp_gs[param_name]->to<asts::TypeAst>() : nullptr;
                inferred_arg = inferred_arg_raw ? inferred_arg_raw->shared_from_this() : nullptr;
            }

            // Handle the match if it exists.
            if (inferred_arg != nullptr) {
                inferred_args[param_name].emplace_back(inferred_arg);
            }

            // Handle the variadic parameter if it exists.
            if (variadic_fn_param_name != nullptr and *infer_target_name == *variadic_fn_param_name) {
                auto temp1 = ast_clone(inferred_args[param_name].back()->type_parts().back()->generic_arg_group->args[0]);
                auto temp2 = std::move(temp1)->to<asts::GenericArgumentTypeAst>()->val;
                inferred_args[param_name].pop_back();
                inferred_args[param_name].emplace_back(temp2);
            }
        }
    }

    // Fully qualify and type arguments (replaced within the inference map).
    auto i_names = inferred_args | genex::views::keys | genex::to<std::vector>();
    for (auto *opt_param : p_group.get_optional_params() | genex::views::cast_dynamic<asts::GenericParameterTypeOptionalAst*>()) {
        const auto cast_name = std::dynamic_pointer_cast<asts::TypeIdentifierAst>(opt_param->name);
        if (genex::contains(i_names, *cast_name, genex::meta::deref)) { continue; }

        auto def_type = opt_param->default_val;
        auto def_type_raw = def_type->without_generics();
        if (auto def_val_type_sym = owner_scope.get_type_symbol(def_type_raw); def_val_type_sym != nullptr and meta.current_stage > 4) {
            auto temp = def_val_type_sym->fq_name()->with_convention(asts::ast_clone(def_type->get_convention()));
            temp = temp->with_generics(asts::ast_clone(def_type->type_parts().back()->generic_arg_group));
            def_type = std::move(temp);
        }
        inferred_args[cast_name].emplace_back(def_type);
        i_names = inferred_args | genex::views::keys | genex::to<std::vector>();
    }

    // Check each generic argument name only has one unique inferred type. "T" cannot infer to "Str" and "U32".
    enforce_no_conflicting_inferred_gn_args(inferred_args, sm);
    enforce_no_uninferred_gn_args(p_names, i_names, owner_scope, owner, sm);

    // At this point, all conflicts have been checked, so it is safe to only use the first inferred value.
    auto formatted_args = std::map<std::shared_ptr<asts::TypeAst>, std::shared_ptr<const asts::TypeAst>>();
    for (auto [arg_name, inferred_vals] : inferred_args) {
        formatted_args[arg_name] = inferred_vals[0];
    }

    // Cross-apply generic arguments. This allows "Vec[T, A=Alloc[T]]" to substitute the new value of "T" into "Alloc".
    // Todo: This needs to be left to right explicitly?
    for (auto const &arg_name : i_names) {
        if (genex::contains(ea_names, *arg_name, genex::meta::deref)) {
            continue;
        }

        auto other_args = formatted_args
            | genex::views::filter([&](auto const &p) { return *p.first != *arg_name; })
            | genex::views::transform([](auto const &p) { return std::make_pair(std::dynamic_pointer_cast<asts::TypeIdentifierAst>(p.first), p.second); })
            | genex::to<std::vector>();
        auto other_args_map = ankerl::unordered_dense::map<std::shared_ptr<asts::TypeIdentifierAst>, std::shared_ptr<const asts::TypeAst>>(other_args.begin(), other_args.end());
        auto other_args_group = asts::GenericArgumentGroupAst::from_map(std::move(other_args_map));
        auto other_args_vec = other_args_group->args | genex::views::ptr | genex::to<std::vector>();

        auto t = formatted_args[arg_name]->substitute_generics(other_args_vec);
        formatted_args[arg_name] = t;
    }

    // Convert the inferred types into new generic arguments.
    auto i = a_group.args.size();
    for (auto const &[key, val] : formatted_args) {
        const auto matching_param = genex::find(p_names, *key->to<asts::TypeIdentifierAst>(), genex::meta::deref);
        if (matching_param == p_names.end()) { continue; }
        auto temp_arg = std::make_unique<asts::GenericArgumentTypeKeywordAst>(ast_clone(key), nullptr, ast_clone(val));
        a_group.args.emplace_back(std::move(temp_arg));
    }

    if (i > 0) {
        a_group.args |= genex::actions::drop(i);
    }

    a_group.args |= genex::actions::sort([&](auto const &a, auto const &b) {
        const auto a_index = genex::position(p_names, [&](auto const &p) { return *p == *a->template to<asts::GenericArgumentTypeKeywordAst>()->name; });
        const auto b_index = genex::position(p_names, [&](auto const &p) { return *p == *b->template to<asts::GenericArgumentTypeKeywordAst>()->name; });
        return a_index < b_index;
    });
}


auto spp::analyse::utils::func_utils::is_target_callable(
    asts::ExpressionAst &expr,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> std::shared_ptr<asts::TypeAst> {
    // Get the type of the expression.
    auto expr_type = expr.infer_type(&sm, meta);
    const auto is_type_functional = type_utils::is_type_function(
        *expr_type, *sm.current_scope);
    return is_type_functional ? std::move(expr_type) : nullptr;
}


auto spp::analyse::utils::func_utils::create_callable_prototype(
    asts::TypeAst const &expr_type)
    -> std::unique_ptr<asts::FunctionPrototypeAst> {
    // Extract the parameter and return types from the expression type.
    auto dummy_return_type = expr_type.type_parts().back()->generic_arg_group->type_at("Out")->val;
    auto dummy_param_types = expr_type.type_parts().back()->generic_arg_group->type_at("Args")->val->type_parts().back()->generic_arg_group->get_type_args()
        | genex::views::transform([](auto *g) { return std::make_unique<asts::FunctionParameterRequiredAst>(nullptr, nullptr, g->val); })
        | genex::views::cast_smart<asts::FunctionParameterAst>()
        | genex::to<std::vector>();

    // Create a function prototype based off of the parameter and return type.
    // Todo: When might it be a coroutine, not a subroutine?
    // Todo: Do we set "cmp" here for the usbroutine ever?
    auto dummy_param_group = std::make_unique<asts::FunctionParameterGroupAst>(nullptr, std::move(dummy_param_types), nullptr);
    auto dummy_name = std::make_unique<asts::IdentifierAst>(0, "<anonymous>");
    auto dummy_overload = std::make_unique<asts::SubroutinePrototypeAst>(
        SPP_NO_ANNOTATIONS, nullptr, nullptr, std::move(dummy_name), nullptr, std::move(dummy_param_group), nullptr,
        std::move(dummy_return_type), nullptr);

    // Return the function prototype.
    return dummy_overload;
}


template auto spp::analyse::utils::func_utils::name_gn_args_impl<
    spp::asts::GenericArgumentCompAst,
    spp::asts::GenericParameterCompAst>(
    asts::GenericArgumentGroupAst &a_group,
    asts::GenericParameterGroupAst const &p_group,
    asts::Ast const &owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta) -> void;


template auto spp::analyse::utils::func_utils::name_gn_args_impl<
    spp::asts::GenericArgumentTypeAst,
    spp::asts::GenericParameterTypeAst>(
    asts::GenericArgumentGroupAst &a_group,
    asts::GenericParameterGroupAst const &p_group,
    asts::Ast const &owner,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData &meta) -> void;
