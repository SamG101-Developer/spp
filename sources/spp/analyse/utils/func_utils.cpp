#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/func_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/function_call_argument_keyword_ast.hpp>
#include <spp/asts/function_call_argument_positional_ast.hpp>
#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/function_parameter_required_ast.hpp>
#include <spp/asts/function_parameter_self_ast.hpp>
#include <spp/asts/function_parameter_variadic_ast.hpp>
#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/fold_expression_ast.hpp>
#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_comp_keyword_ast.hpp>
#include <spp/asts/generic_argument_comp_positional_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/generic_argument_type_positional_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/generic_parameter_comp_ast.hpp>
#include <spp/asts/generic_parameter_comp_variadic_ast.hpp>
#include <spp/asts/generic_parameter_type_ast.hpp>
#include <spp/asts/generic_parameter_type_optional_ast.hpp>
#include <spp/asts/generic_parameter_type_variadic_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_runtime_member_access_ast.hpp>
#include <spp/asts/postfix_expression_operator_static_member_access_ast.hpp>
#include <spp/asts/postfix_expression_operator_function_call_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/tuple_literal_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/generate/common_types.hpp>
#include <spp/macros.hpp>

#include <genex/actions/clear.hpp>
#include <genex/actions/concat.hpp>
#include <genex/actions/pop_front.hpp>
#include <genex/actions/remove_if.hpp>
#include <genex/actions/replace.hpp>
#include <genex/actions/sort.hpp>
#include <genex/actions/take.hpp>
#include <genex/algorithms/any_of.hpp>
#include <genex/algorithms/any_of.hpp>
#include <genex/algorithms/contains.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/operations/size.hpp>
#include <genex/views/address.hpp>
#include <genex/views/cast.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/drop.hpp>
#include <genex/views/enumerate.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/indirect.hpp>
#include <genex/views/map.hpp>
#include <genex/views/materialize.hpp>
#include <genex/views/move.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/set_algorithms.hpp>
#include <genex/views/transform.hpp>
#include <genex/views/tuple_n.hpp>
#include <genex/views/view.hpp>
#include <genex/views/zip.hpp>

#include "spp/asts/annotation_ast.hpp"


auto spp::analyse::utils::func_utils::get_function_owner_type_and_function_name(
    asts::ExpressionAst const &lhs,
    scopes::ScopeManager &sm,
    asts::mixins::CompilerMetaData *meta)
    -> std::tuple<asts::TypeAst*, scopes::Scope const*, asts::IdentifierAst*> {
    // Define some expression casts that are used commonly.
    const auto postfix_lhs = asts::ast_cast<asts::PostfixExpressionAst>(&lhs);
    const auto runtime_field = asts::ast_cast<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(postfix_lhs->op.get());
    const auto static_field = asts::ast_cast<asts::PostfixExpressionOperatorStaticMemberAccessAst>(postfix_lhs->op.get());

    // Variables that will be set in each branch, and returned.
    auto function_owner_type = std::shared_ptr<asts::TypeAst>(nullptr);
    auto function_owner_scope = static_cast<scopes::Scope const*>(nullptr);
    auto function_name = std::shared_ptr<asts::IdentifierAst>(nullptr);

    // Runtime access into an object: "object.method()".
    if (postfix_lhs != nullptr and runtime_field != nullptr) {
        function_owner_type = postfix_lhs->lhs->infer_type(&sm, meta);
        function_name = runtime_field->name;
        function_owner_scope = sm.current_scope->get_type_symbol(*function_owner_type)->scope;
    }

    // Static access into a type: "Type::method()" or "ns::Type::method()".
    else if (auto type_lhs = asts::ast_cast<asts::TypeAst>(postfix_lhs->lhs.get()); postfix_lhs != nullptr and static_field != nullptr and type_lhs != nullptr) {
        function_owner_type = ast_clone(type_lhs);
        function_name = static_field->name;
        function_owner_scope = sm.current_scope->get_type_symbol(*function_owner_type)->scope;
    }

    // Direct access into a namespaced free function: "std::io::print(variable)".
    else if (postfix_lhs != nullptr and static_field != nullptr) {
        function_owner_scope = sm.current_scope->get_ns_symbol(*static_field->name)->scope;
        function_name = static_field->name;
        function_owner_type = function_owner_scope->get_var_symbol(*function_name)->type;
    }

    // Direct access into a non-namespaced function: "function()":
    else if (auto id_lhs = asts::ast_cast<asts::IdentifierAst>(const_cast<asts::ExpressionAst*>(&lhs)); id_lhs != nullptr) {
        function_owner_type = nullptr;
        function_name = ast_clone(id_lhs);
        function_owner_scope = sm.current_scope->parent_module();
    }

    // Non-callable AST
    else {
        function_owner_type = nullptr;
        function_name = nullptr;
        function_owner_scope = nullptr;
    }

    return std::make_tuple(function_owner_type.get(), function_owner_scope, function_name.get());
}


auto spp::analyse::utils::func_utils::convert_method_to_function_form(
    asts::TypeAst const &function_owner_type,
    asts::IdentifierAst const &function_name,
    asts::PostfixExpressionAst const &lhs,
    asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
    scopes::ScopeManager &sm,
    asts::mixins::CompilerMetaData *meta)
    -> std::pair<std::unique_ptr<asts::PostfixExpressionAst>, std::unique_ptr<asts::PostfixExpressionOperatorFunctionCallAst>> {
    // Create an argument for "self" and inject it into the current arguments.
    auto self_arg = std::make_unique<asts::FunctionCallArgumentPositionalAst>(nullptr, nullptr, ast_clone(lhs.lhs));
    auto fn_args = std::move(fn_call.arg_group->args);
    fn_args.insert(fn_args.begin(), std::move(self_arg));

    // Create the static method access (without the function call and args)
    auto field = std::make_unique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, ast_clone(&function_name));
    auto field_access = std::make_unique<asts::PostfixExpressionAst>(ast_clone(&function_owner_type), std::move(field));

    // Create the function call with the new arguments.
    auto new_fn_call = std::make_unique<asts::PostfixExpressionOperatorFunctionCallAst>(ast_clone(fn_call.generic_arg_group), ast_clone(fn_call.arg_group), nullptr);
    new_fn_call->arg_group->args = std::move(fn_args);
    new_fn_call->arg_group->args[0]->set_self_type(lhs.lhs->infer_type(&sm, meta));

    // Return the new ASTs.
    return std::make_pair(std::move(field_access), std::move(new_fn_call));
}


auto spp::analyse::utils::func_utils::get_all_function_scopes(
    asts::IdentifierAst const &target_fn_name,
    scopes::Scope const *target_scope,
    const bool for_override)
    -> std::vector<std::tuple<scopes::Scope const*, asts::FunctionPrototypeAst*, std::unique_ptr<asts::GenericArgumentGroupAst>>> {
    // If the name is empty (non-symbolic call) then return "no scopes".
    // If the target scope is nullptr, then the functions are bein superimposed over a generic type.
    if (target_fn_name.val == "" or target_scope == nullptr) { return {}; }

    // Get the function-type name from the function: "func()" => "$Func".
    const auto mapped_name = target_fn_name.to_function_identifier();
    auto overload_scopes = std::vector<std::tuple<scopes::Scope const*, asts::FunctionPrototypeAst*, std::unique_ptr<asts::GenericArgumentGroupAst>>>();
    auto is_ext_scope = [](auto *scope) { return asts::ast_cast<asts::SupPrototypeExtensionAst>(scope->ast); };

    // Check for namespaced (module-level) functions (they will have no inheritable generics).
    if (target_scope->ns_sym != nullptr) {
        for (auto *ancestor_scope : target_scope->ancestors()) {
            for (const auto *sup_scope : ancestor_scope->children | genex::views::ptr | genex::views::filter(is_ext_scope)) {
                auto generics = nullptr;
                auto scope = sup_scope->children[0].get();
                auto proto = asts::ast_cast<asts::FunctionPrototypeAst>(ast_body(sup_scope->ast)[0]);
                overload_scopes.emplace_back(scope, proto, generics);
            }
        }
    }

    // Functions belonging to a type ill have inheritance generics from "sup [...] Type { ... }"
    else {
        // If a class scope was provided, get all the sup scopes from it, otherwise use the specific sup scope.
        const auto sup_scopes = asts::ast_cast<asts::ClassPrototypeAst>(target_scope->ast) != nullptr
                                    ? target_scope->direct_sup_scopes() | genex::views::transform([](auto x) -> const scopes::Scope* { return x; }) | genex::views::to<std::vector>()
                                    : std::vector{target_scope};

        // From the super scopes, check each one for "sup $Func ext FunXXX { ... }" super-impositions.
        for (auto *sup_scope : sup_scopes) {
            for (auto *sup_ast : asts::ast_body(sup_scope->ast) | genex::views::cast_dynamic<asts::SupPrototypeExtensionAst*>() | genex::views::to<std::vector>()) {
                if (asts::ast_cast<asts::TypeIdentifierAst>(sup_ast->name)->name == mapped_name->val) {
                    auto generics = std::make_unique<asts::GenericArgumentGroupAst>(nullptr, sup_scope->get_generics(), nullptr);
                    auto scope = sup_scope;
                    auto proto = asts::ast_cast<asts::FunctionPrototypeAst>(asts::ast_body(sup_ast)[0]);
                    overload_scopes.emplace_back(scope, proto, std::move(generics));
                }
            }
        }

        // When a derived class has overridden a method, the base method must be removed.
        for (auto &&[scope_1, fn_1, _] : overload_scopes) {
            for (auto &&[scope_2, fn_2, _] : overload_scopes) {
                if (fn_1 != fn_2 and target_scope->depth_difference(scope_1) < target_scope->depth_difference(scope_2)) {
                    auto conflict = check_for_conflicting_override(*scope_1, scope_2, *fn_1);
                    if (conflict != nullptr) {
                        overload_scopes |= genex::actions::remove_if([conflict](auto &&info) { return std::get<1>(info) == conflict; });
                    }
                }
            }
        }

        // Adjust the scope in the tuple to the inner function scope.
        if (not for_override) {
            for (auto &&[i, info] : overload_scopes | genex::views::move | genex::views::enumerate | genex::views::to<std::vector>()) {
                auto &[scope, proto, generics] = info;
                scope = scope->children[0].get();
                overload_scopes[i] = std::make_tuple(scope, proto, std::move(generics));
            }
        }
    }

    // Return all the found function scopes.
    return overload_scopes;
}


auto spp::analyse::utils::func_utils::check_for_conflicting_overload(
    scopes::Scope const &this_scope,
    scopes::Scope const *target_scope,
    asts::FunctionPrototypeAst const &new_fn)
    -> asts::FunctionPrototypeAst* {
    // Get the methods that belong to this type, or any of its supertypes.
    auto existing = get_all_function_scopes(*new_fn.orig_name, target_scope);
    auto existing_scopes = existing | genex::views::tuple_element<0>() | genex::views::to<std::vector>();
    auto existing_fns = existing | genex::views::tuple_element<1>() | genex::views::to<std::vector>();

    // Check for an overload conflict with all functions of the same name.
    for (auto [old_scope, old_fn] : genex::views::zip(existing_scopes, existing_fns) | genex::views::to<std::vector>()) {
        // Ignore if the method is an identical match on a base class (override) or is the same object.
        if (old_fn == &new_fn) { continue; }
        if (old_fn == check_for_conflicting_override(this_scope, old_scope, new_fn, old_scope)) { continue; }

        // Ignore if the return types are different.
        if (not type_utils::symbolic_eq(*new_fn.return_type, *old_fn->return_type, this_scope, *old_scope)) { continue; }

        // Ignore if there are a different number of required generic parameters.
        if (new_fn.generic_param_group->get_type_params().size() != old_fn->generic_param_group->get_type_params().size()) { continue; }
        if (new_fn.generic_param_group->get_comp_params().size() != old_fn->generic_param_group->get_comp_params().size()) { continue; }

        // Get the two parameter lists and create copies to remove duplicate parameters from.
        auto params_new = ast_clone_vec(new_fn.param_group->params);
        auto params_old = ast_clone_vec(old_fn->param_group->params);

        // Remove all the required parameters on the first parameter list off of the other parameter list.
        for (auto [p, q] : genex::views::zip(new_fn.param_group->params | genex::views::ptr, old_fn->param_group->params | genex::views::ptr)) {
            if (type_utils::symbolic_eq(*p->type, *q->type, this_scope, *old_scope)) {
                genex::actions::remove_if([p](auto &&x) { return x.extract_names() == p->extract_names(); });
                params_new |= genex::actions::remove_if([pe=p->extract_names()](auto &&x) { return x->extract_names() == std::move(pe); });
                params_old |= genex::actions::remove_if([qe=q->extract_names()](auto &&x) { return x->extract_names() == std::move(qe); });
            }
        }

        // If neither parameter list contains a required parameter, throw an error.
        if (genex::operations::empty(params_new
            | genex::views::ptr
            | genex::views::concat(params_old | genex::views::ptr)
            | genex::views::cast_dynamic<asts::FunctionParameterRequiredAst*>()
            | genex::views::to<std::vector>())) {
            return old_fn;
        }
    }
    return nullptr;
}


auto spp::analyse::utils::func_utils::check_for_conflicting_override(
    scopes::Scope const &this_scope,
    scopes::Scope const *target_scope,
    asts::FunctionPrototypeAst const &new_fn,
    scopes::Scope const *exclude_scope)
    -> asts::FunctionPrototypeAst* {
    // Helper function to get the type of the convention AST applied to the "self" parameter.
    auto hs = [](asts::FunctionPrototypeAst const *f) {
        return f->param_group->get_self_param() != nullptr;
    };

    // Helper function to get the type of the convention AST applied to the "self" parameter.
    auto sc = [&hs](asts::FunctionPrototypeAst const *f) {
        return hs(f) ? f->param_group->get_self_param()->conv->tag : asts::ConventionAst::ConventionTag::MOV;
    };

    // Get the existing functions that belong to this type, or any of its supertypes.
    auto existing = get_all_function_scopes(*new_fn.orig_name, target_scope, true);
    auto existing_scopes = existing | genex::views::tuple_element<0>() | genex::views::to<std::vector>();
    auto existing_fns = existing | genex::views::tuple_element<1>() | genex::views::to<std::vector>();

    // Check for an overload conflict with all functions of the same name.
    for (auto [old_scope, old_fn] : genex::views::zip(existing_scopes, existing_fns) | genex::views::to<std::vector>()) {
        // Ignore if the method is the same object.
        if (old_fn == &new_fn) { continue; }
        if (old_scope == exclude_scope) { continue; }

        // Get the two parameter lists and create copies.
        auto params_new = new_fn.param_group->get_non_self_params();
        auto params_old = old_fn->param_group->get_non_self_params();

        // Get a list of conditions to check for conflicting functions.
        if (params_new.size() != params_old.size()) { continue; }

        // All parameters must have the same names.
        if (genex::algorithms::any_of(
            genex::views::zip(params_new, params_old) | genex::views::materialize(),
            [](auto pq) { return std::get<0>(pq)->extract_names() != std::get<1>(pq)->extract_names(); })) {
            continue;
        }

        // All parameters must have the same types.
        if (genex::algorithms::any_of(
            genex::views::zip(params_new, params_old) | genex::views::materialize(),
            [this_scope, old_scope](auto pq) { return not type_utils::symbolic_eq(*std::get<0>(pq)->type, *std::get<1>(pq)->type, this_scope, *old_scope, false); })) {
            continue;
        }

        // The function type (subroutines vs coroutine) must match.
        if (new_fn.tok_fun->token_type != old_fn->tok_fun->token_type) { continue; }

        // The return types must be the same type.
        if (not type_utils::symbolic_eq(*new_fn.return_type, *old_fn->return_type, this_scope, *old_scope)) { continue; }

        // Check the self parameters' conventions.
        if (hs(&new_fn) != hs(old_fn) or sc(&new_fn) != sc(old_fn)) { continue; }

        // The functions must have identical signatures at this point, so return the old function.
        return old_fn;
    }
    return nullptr;
}


auto spp::analyse::utils::func_utils::name_args(
    std::vector<std::unique_ptr<asts::FunctionCallArgumentAst>> &args,
    std::vector<asts::FunctionParameterAst*> params,
    scopes::ScopeManager const &sm)
    -> void {
    // Get the argument names and parameter names, and check for the existence of a variadic parameter.
    auto arg_names = args
        | genex::views::ptr
        | genex::views::cast_dynamic<asts::FunctionCallArgumentKeywordAst*>()
        | genex::views::transform([](auto &&x) { return x->name; })
        | genex::views::to<std::vector>();

    auto param_names = params
        | genex::views::transform([](auto &&x) { return x->extract_name(); })
        | genex::views::to<std::vector>();

    const auto is_variadic =
        not params.empty() and asts::ast_cast<asts::FunctionParameterVariadicAst>(params.back()) != nullptr;

    // Check for invalid argument names against parameter names, then remove the valid ones.
    auto invalid_arg_names = arg_names
        | genex::views::set_difference_unsorted(param_names, SPP_INSTANT_INDIRECT, SPP_INSTANT_INDIRECT)
        | genex::views::to<std::vector>();

    if (not invalid_arg_names.empty()) {
        errors::SemanticErrorBuilder<errors::SppArgumentNameInvalidError>().with_args(
            *params[0], "function parameter", *invalid_arg_names[0], "function argument").with_scopes({sm.current_scope}).raise();
    }

    // Name all the positional arguments with leftover parameter names.
    auto positional_args = args
        | genex::views::ptr
        | genex::views::cast_dynamic<asts::FunctionCallArgumentPositionalAst*>();

    for (auto [i, positional_arg] : positional_args | genex::views::enumerate | genex::views::to<std::vector>()) {
        auto param_name = param_names.front();
        param_names |= genex::actions::pop_front();
        auto keyword_arg = std::make_unique<asts::FunctionCallArgumentKeywordAst>(std::move(param_name), nullptr, std::move(positional_arg->conv), nullptr);

        // The variadic parameter requires a tuple of the remaining arguments.
        if (param_names.empty() and is_variadic) {
            auto elems = args
                | genex::views::move
                | genex::views::drop(i)
                | genex::views::transform([](auto &&x) { return std::move(x->val); })
                | genex::views::to<std::vector>();
            keyword_arg->val = std::make_unique<asts::TupleLiteralAst>(nullptr, std::move(elems), nullptr);
            args[i] = std::move(keyword_arg);
            args |= genex::actions::take(i + 1);
            break;
        }

        keyword_arg->conv = std::move(positional_arg->conv);
        keyword_arg->set_self_type(positional_arg->get_self_type());
        keyword_arg->val = std::move(positional_arg->val);
        args[i] = std::move(keyword_arg);
    }
}


auto spp::analyse::utils::func_utils::name_generic_args(
    std::vector<std::unique_ptr<asts::GenericArgumentAst>> &args,
    std::vector<asts::GenericParameterAst*> params,
    asts::Ast const &owner,
    scopes::ScopeManager const &sm,
    const bool is_tuple_owner)
    -> void {
    // Special case for tuples to prevent an infinite recursion.
    if (is_tuple_owner) {
        return;
    }

    // Split into "comp" and "type" arguments.
    auto comp_args = args
        | genex::views::cast_smart_ptr<asts::GenericArgumentCompAst>()
        | genex::views::to<std::vector>();

    auto type_args = args
        | genex::views::cast_smart_ptr<asts::GenericArgumentTypeAst>()
        | genex::views::to<std::vector>();

    // Split into "comp" and "type" parameters.
    const auto comp_params = params
        | genex::views::cast_dynamic<asts::GenericParameterCompAst*>()
        | genex::views::to<std::vector>();

    const auto type_params = params
        | genex::views::cast_dynamic<asts::GenericParameterTypeAst*>()
        | genex::views::to<std::vector>();

    // Call the two individual naming functions.
    name_generic_args_impl(comp_args, comp_params, owner, sm);
    name_generic_args_impl(type_args, type_params, owner, sm);

    // Recombine the arguments (into the "args" vector).
    args |= genex::actions::clear();
    args |= genex::actions::concat(comp_args | genex::views::cast_smart_ptr<asts::GenericArgumentAst>());
    args |= genex::actions::concat(type_args | genex::views::cast_smart_ptr<asts::GenericArgumentAst>());
}


template <typename GenericArgType, typename GenericParamType>
auto spp::analyse::utils::func_utils::name_generic_args_impl(
    std::vector<std::unique_ptr<GenericArgType>> &args,
    std::vector<GenericParamType*> params,
    asts::Ast const &owner,
    scopes::ScopeManager const &sm)
    -> void {
    // Get the argument names and parameter names, and check for the existence of a variadic parameter.
    auto arg_names = args
        | genex::views::ptr
        | genex::views::cast_dynamic<asts::detail::make_keyword_arg_t<GenericArgType>*>()
        | genex::views::transform([](auto &&x) { return x->name; })
        | genex::views::to<std::vector>();

    auto param_names = params
        | genex::views::transform([](auto &&x) { return x->name; })
        | genex::views::to<std::vector>();

    const auto is_variadic =
        not params.empty() and asts::ast_cast<asts::detail::make_variadic_param_t<GenericParamType>>(params.back()) != nullptr;

    // Check for invalid argument names against parameter names, then remove the valid ones.
    auto invalid_arg_names = arg_names
        | genex::views::set_difference_unsorted(param_names, SPP_INSTANT_INDIRECT, SPP_INSTANT_INDIRECT)
        | genex::views::to<std::vector>();

    if (not invalid_arg_names.empty()) {
        errors::SemanticErrorBuilder<errors::SppArgumentNameInvalidError>().with_args(
            *params[0], "function parameter", *invalid_arg_names[0], "function argument").with_scopes({sm.current_scope}).raise();
    }

    // Name all the positional arguments with leftover parameter names.
    auto positional_args = args
        | genex::views::ptr
        | genex::views::cast_dynamic<asts::detail::make_positional_arg_t<GenericArgType>*>();

    for (auto [i, positional_arg] : positional_args | genex::views::enumerate | genex::views::to<std::vector>()) {
        // Error if there are too many generic arguments.
        if (param_names.empty()) {
            errors::SemanticErrorBuilder<errors::SppGenericArgumentTooManyError>().with_args(
                *params[0], owner, *positional_arg).with_scopes({sm.current_scope}).raise();
        }

        // Name the argument based on the parameter names available.
        auto param_name = param_names.front();
        param_names |= genex::actions::pop_front();
        auto keyword_arg = std::make_unique<asts::detail::make_keyword_arg_t<GenericArgType>>(std::move(param_name), nullptr, nullptr);

        if (param_names.empty() and is_variadic) {
            if constexpr (std::same_as<GenericParamType, asts::GenericParameterCompAst>) {
                // Variadic check: map arguments "func[1_u32, 1_u32]" for "func[..s]" to "func[ts = (1_u32, 1_u32)]".
                auto vals = args | genex::views::ptr | genex::views::drop(i) | genex::views::transform([](auto &&x) { return std::move(x->val); }) | genex::views::to<std::vector>();
                auto tup_lit = std::make_unique<asts::TupleLiteralAst>(nullptr, std::move(vals), nullptr);
                keyword_arg->val = std::move(tup_lit);
            }
            else {
                // Variadic check: map arguments "func[U32, U32]" for "func[..Ts]" to "func[Ts = (U32, U32)]".
                auto types = args | genex::views::ptr | genex::views::drop(i) | genex::views::transform([](auto &&x) { return std::move(x->val); }) | genex::views::to<std::vector>();
                auto tup_type = asts::generate::common_types::tuple_type(positional_arg->pos_start(), std::move(types));
                keyword_arg->val = std::move(tup_type);
            }

            args[i] = std::move(keyword_arg);
            args |= genex::actions::take(i + 1);
            break;
        }

        // Standard naming of the argument.
        keyword_arg->val = std::move(positional_arg->val);
        args[i] = std::move(keyword_arg);
    }
}


auto spp::analyse::utils::func_utils::infer_generic_args(
    std::vector<std::unique_ptr<asts::GenericArgumentAst>> &args,
    std::vector<asts::GenericParameterAst*> params,
    std::vector<asts::GenericParameterAst*> opt_params,
    std::vector<asts::GenericArgumentAst*> explicit_args,
    std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>> const &infer_source,
    std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>> const &infer_target,
    std::shared_ptr<asts::Ast> owner,
    scopes::Scope const *owner_scope,
    std::shared_ptr<asts::IdentifierAst> variadic_param_identifier,
    const bool is_tuple_owner,
    scopes::ScopeManager &sm,
    asts::mixins::CompilerMetaData *meta)
    -> void {
    // Special case for tuples to prevent an infinite recursion, or there is nothing to infer.
    if (is_tuple_owner or params.empty()) {
        args |= genex::actions::clear();
        args |= genex::actions::concat(explicit_args | genex::views::transform([](auto &&x) { return ast_clone(x); }));
        return;
    }

    // Split into "comp" and "type" arguments.
    auto comp_args = args
        | genex::views::cast_smart_ptr<asts::GenericArgumentCompKeywordAst>()
        | genex::views::to<std::vector>();

    const auto comp_explicit_args = explicit_args
        | genex::views::cast_dynamic<asts::GenericArgumentCompKeywordAst*>()
        | genex::views::to<std::vector>();

    auto type_args = args
        | genex::views::cast_smart_ptr<asts::GenericArgumentTypeKeywordAst>()
        | genex::views::to<std::vector>();

    const auto type_explicit_args = explicit_args
        | genex::views::cast_dynamic<asts::GenericArgumentTypeKeywordAst*>()
        | genex::views::to<std::vector>();

    // Split into "comp" and "type" parameters.
    const auto comp_params = params
        | genex::views::cast_dynamic<asts::GenericParameterCompAst*>()
        | genex::views::to<std::vector>();

    const auto comp_opt_params = opt_params
        | genex::views::cast_dynamic<asts::GenericParameterCompAst*>()
        | genex::views::to<std::vector>();

    const auto type_params = params
        | genex::views::cast_dynamic<asts::GenericParameterTypeAst*>()
        | genex::views::to<std::vector>();

    const auto type_opt_params = opt_params
        | genex::views::cast_dynamic<asts::GenericParameterTypeAst*>()
        | genex::views::to<std::vector>();

    // Call the two individual inference functions.
    infer_generic_args_impl(comp_args, comp_params, comp_opt_params, comp_explicit_args, infer_source, infer_target, owner, owner_scope, variadic_param_identifier, sm, meta);
    infer_generic_args_impl(type_args, type_params, type_opt_params, type_explicit_args, infer_source, infer_target, owner, owner_scope, variadic_param_identifier, sm, meta);

    // Recombine the arguments (into the "args" vector).
    args |= genex::actions::clear();
    args |= genex::actions::concat(comp_args | genex::views::cast_smart_ptr<asts::GenericArgumentAst>());
    args |= genex::actions::concat(type_args | genex::views::cast_smart_ptr<asts::GenericArgumentAst>());
}


template <typename GenericArgType, typename GenericParamType>
auto spp::analyse::utils::func_utils::infer_generic_args_impl(
    std::vector<std::unique_ptr<GenericArgType>> &args,
    std::vector<GenericParamType*> params,
    std::vector<GenericParamType*> opt_params,
    std::vector<GenericArgType*> explicit_args,
    std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>> const &infer_source,
    std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>> const &infer_target,
    std::shared_ptr<asts::Ast> owner,
    scopes::Scope const *owner_scope,
    std::shared_ptr<asts::IdentifierAst> variadic_param_identifier,
    scopes::ScopeManager &sm,
    asts::mixins::CompilerMetaData *meta)
    -> void {
    // Get the parameter names for ease of use.
    auto param_names = params
        | genex::views::transform([](auto &&x) { return asts::ast_cast<asts::TypeIdentifierAst>(x->name); })
        | genex::views::to<std::vector>();

    auto explicit_arg_names = explicit_args
        | genex::views::transform([](auto &&x) { return asts::ast_cast<asts::TypeIdentifierAst>(x->name.get()); })
        | genex::views::to<std::vector>();

    // The inferred generic map is of the structure: {TypeAst: [ExpressionAst]} for the different types/constants that
    // each generic is inferred as.
    auto inferred_args = std::map<std::shared_ptr<asts::TypeAst>, std::vector<asts::ExpressionAst const*>>();

    // Preload the explicit generic arguments into the inference map, as the consistency of these arguments needs
    // checking too.
    for (auto &&arg : explicit_args) {
        inferred_args[arg->name].emplace_back(arg->val.get());
    }

    // Infer the generic arguments from the source/target maps.
    for (auto param_name : param_names) {
        for (auto [infer_target_name, infer_target_type] : infer_target) {
            auto inferred_arg = static_cast<asts::ExpressionAst const*>(nullptr);

            // Check for a direct match ("a: T" & "a: Str") or an inner match ("a: Vec[T]" & "a: Vec[Str]").
            if (infer_source.contains(infer_target_name)) {
                inferred_arg = infer_source.at(infer_target_name)->without_generics()->match_generic(*infer_target_type->without_convention(), *param_name);
            }

            // Handle the match if it exists.
            if (inferred_arg != nullptr) {
                inferred_args.at(param_name).emplace_back(inferred_arg);
            }

            // Handle the variadic parameter if it exists.
            if constexpr (std::same_as<GenericParamType, asts::GenericParameterTypeAst>) {
                if (variadic_param_identifier != nullptr and *infer_target_name == *variadic_param_identifier) {
                    auto temp1 = std::move(asts::ast_cast<asts::TypeAst>(inferred_args.at(param_name).back())->type_parts().back()->generic_arg_group->args[0]);
                    auto temp2 = asts::ast_cast<asts::GenericArgumentTypeAst>(std::move(temp1))->val;
                    inferred_args.at(param_name).pop_back();
                    inferred_args.at(param_name).emplace_back(temp2.get());
                }
            }
        }
    }

    // Fully qualify and type arguments (replaced within the inference map).
    if constexpr (std::same_as<GenericParamType, asts::GenericParameterTypeAst>) {
        if (const auto owner_sym = sm.current_scope->get_type_symbol(*asts::ast_cast<asts::TypeAst>(owner)); owner_sym != nullptr) {
            for (auto &&opt_param : opt_params | genex::views::cast_dynamic<asts::detail::make_optional_param_t<GenericParamType>*>()) {
                if (not genex::algorithms::contains(inferred_args | genex::views::keys | genex::views::to<std::vector>(), *opt_param->name, [](auto x) -> asts::TypeAst& { return *x; })) {
                    auto def_type = owner_scope->get_type_symbol(*opt_param->default_val)->fq_name();
                    inferred_args[opt_param->name].emplace_back(def_type.get());
                }
            }
        }
    }

    // Check each generic argument name only has one unique inferred type. "T" cannot infer to "Str" and "U32".
    for (auto [arg_name, inferred_vals] : inferred_args) {
        auto mismatches = inferred_vals
            | genex::views::drop(1)
            | genex::views::filter([&](auto &&e) { return not type_utils::symbolic_eq(*e, *inferred_vals[0], *sm.current_scope, *sm.current_scope); })
            | genex::views::to<std::vector>();

        if (not mismatches.empty()) {
            errors::SemanticErrorBuilder<errors::SppGenericParameterInferredConflictInferredError>().with_args(
                *arg_name, *inferred_vals[0], *mismatches[0]).with_scopes({sm.current_scope}).raise();
        }
    }

    // Check all the generic arguments have been inferred.
    auto not_inferred_args = param_names
        | genex::views::set_difference(inferred_args | genex::views::keys | genex::views::indirect, {}, [](auto &&x) { return *x; })
        | genex::views::to<std::vector>();
    if (not not_inferred_args.empty()) {
        errors::SemanticErrorBuilder<errors::SppGenericParameterNotInferredError>().with_args(
            *not_inferred_args[0], *owner).with_scopes({sm.current_scope, owner_scope}).raise();
    }

    // At this point, all conflicts have been checked, so it is safe to only use the first inferred value.
    auto formatted_generic_arguments = std::map<std::shared_ptr<asts::TypeAst>, asts::ExpressionAst const*>();
    for (auto [arg_name, inferred_vals] : inferred_args) {
        formatted_generic_arguments[arg_name] = inferred_vals[0];
    }

    // Cross-apply generic arguments. This allows "Vec[T, A=Alloc[T]]" to substitute the new value of "T" into "Alloc".
    for (auto &&arg_name : inferred_args | genex::views::keys | genex::views::to<std::vector>()) {
        if (genex::algorithms::contains(explicit_arg_names, *arg_name, [](auto *x) { return *x; })) {
            continue;
        }

        auto other_args = formatted_generic_arguments
            | genex::views::filter([&](auto &&p) { return *p.first != *arg_name; })
            | genex::views::to<std::vector>();

        auto other_args_group = asts::GenericArgumentGroupAst::from_map(std::map(other_args.begin(), other_args.end()));
        auto other_args_vec = other_args_group->args | genex::views::ptr | genex::views::to<std::vector>();

        auto t = asts::ast_cast<asts::TypeAst>(formatted_generic_arguments[arg_name])->substitute_generics(other_args_vec);
        t->stage_7_analyse_semantics(&sm, meta);
        formatted_generic_arguments[arg_name] = sm.current_scope->get_type_symbol(*t)->fq_name().get();
    }

    // Convert the inferred types into new generic arguments.
    args |= genex::actions::clear();
    for (auto const &[key, val] : formatted_generic_arguments) {
        auto matching_param = genex::algorithms::find_if(params, [&](auto *p) { return *p->name == *key; });
        if (matching_param == params.end()) { continue; }

        if (asts::ast_cast<asts::GenericParameterCompAst>(*matching_param) and asts::ast_cast<asts::TypeAst>(val)) {
            auto temp_val = asts::IdentifierAst::from_type(*asts::ast_cast<asts::TypeAst>(val));
            auto temp_arg = std::make_unique<asts::GenericArgumentCompKeywordAst>(ast_clone(key), nullptr, std::move(temp_val));
            args.emplace_back(asts::ast_cast<GenericArgType>(std::move(temp_arg)));
        }
        else if (asts::ast_cast<asts::GenericParameterCompAst>(*matching_param)) {
            auto temp_val = ast_clone(val);
            auto temp_arg = std::make_unique<asts::GenericArgumentCompKeywordAst>(ast_clone(key), nullptr, std::move(temp_val));
            args.emplace_back(asts::ast_cast<GenericArgType>(std::move(temp_arg)));
        }
        else {
            auto temp_val = ast_clone(asts::ast_cast<asts::TypeAst>(val));
            auto temp_arg = std::make_unique<asts::GenericArgumentTypeKeywordAst>(ast_clone(key), nullptr, std::move(temp_val));
            args.emplace_back(asts::ast_cast<GenericArgType>(std::move(temp_arg)));
        }
    }

    // Sort the arguments to match the parameter order.
    args |= genex::actions::sort([&](auto &&ident_a, auto &&ident_b) {
        auto a = genex::algorithms::position(param_names, [&](auto &&x) { return *x == *ident_a->name; });
        auto b = genex::algorithms::position(param_names, [&](auto &&x) { return *x == *ident_b->name; });
        return a < b;
    });

    // Type-check the "comp" args. Only do this at the semantic analysis stage.
    if (meta->current_stage <= 5) {
        return;
    }
    if constexpr (std::same_as<GenericParamType, asts::GenericParameterTypeAst>) {
        return;
    }

    for (auto &&[param, arg] : genex::views::zip(params, args | genex::views::ptr) | genex::views::to<std::vector>()) {
        auto comp_arg = asts::ast_cast<asts::GenericArgumentCompKeywordAst>(arg);
        auto comp_param = asts::ast_cast<asts::GenericParameterCompAst>(param);
        if (comp_arg == nullptr) { continue; }

        auto a_type = comp_arg->val->infer_type(&sm, meta);
        auto p_type = comp_param->type->substitute_generics(args | genex::views::ptr | genex::views::cast_dynamic<asts::GenericArgumentAst*>() | genex::views::to<std::vector>());
        p_type->stage_7_analyse_semantics(&sm, meta);

        // For a variadic comp argument, check every element of the args tuple.
        if (asts::ast_cast<asts::GenericParameterCompVariadicAst>(comp_param)) {
            auto variadic_types = a_type->type_parts().back()->generic_arg_group->args
                | genex::views::ptr
                | genex::views::cast_dynamic<asts::GenericArgumentTypePositionalAst*>()
                | genex::views::transform([](auto &&g) { return g->val; })
                | genex::views::to<std::vector>();

            for (auto a_type_inner : variadic_types) {
                if (not type_utils::symbolic_eq(*p_type, *a_type_inner, *owner_scope, *sm.current_scope)) {
                    errors::SemanticErrorBuilder<errors::SppTypeMismatchError>().with_args(
                        *comp_param, *p_type, *comp_arg, *a_type_inner).with_scopes({owner_scope, sm.current_scope}).raise();
                }
            }
            break;
        }

        // Otherwise, just check the argument type against the parameter type.
        if (not type_utils::symbolic_eq(*p_type, *a_type, *owner_scope, *sm.current_scope)) {
            errors::SemanticErrorBuilder<errors::SppTypeMismatchError>().with_args(
                *comp_param, *p_type, *comp_arg, *a_type).with_scopes({owner_scope, sm.current_scope}).raise();
        }
    }
}


auto spp::analyse::utils::func_utils::is_target_callable(
    asts::ExpressionAst &expr,
    scopes::ScopeManager &sm,
    asts::mixins::CompilerMetaData *meta)
    -> std::shared_ptr<asts::TypeAst> {
    // Get the type of the expression.
    auto expr_type = expr.infer_type(&sm, meta);
    const auto is_type_functional = type_utils::is_type_function(*expr_type, *sm.current_scope);
    return is_type_functional ? std::move(expr_type) : nullptr;
}


auto spp::analyse::utils::func_utils::create_callable_prototype(
    asts::TypeAst const &expr_type)
    -> std::unique_ptr<asts::FunctionPrototypeAst> {
    // Extract the parameter and return types from the expression type.
    auto dummy_return_type = expr_type.type_parts().back()->generic_arg_group->type_at("Out")->val;
    auto dummy_param_types = expr_type.type_parts().back()->generic_arg_group->type_at("Args")->val->type_parts().back()->generic_arg_group->get_type_args()
        | genex::views::transform([](auto *g) { return std::make_unique<asts::FunctionParameterRequiredAst>(nullptr, nullptr, g->val); })
        | genex::views::cast_smart_ptr<asts::FunctionParameterAst>()
        | genex::views::to<std::vector>();

    // Create a function prototype based off of the parameter and return type.
    auto dummy_param_group = std::make_unique<asts::FunctionParameterGroupAst>(nullptr, std::move(dummy_param_types), nullptr);
    auto dummy_overload = std::make_unique<asts::FunctionPrototypeAst>(
        SPP_NO_ANNOTATIONS, nullptr, nullptr, nullptr, std::move(dummy_param_group), nullptr,
        std::move(dummy_return_type), nullptr);

    // Return the function prototype.
    return dummy_overload;
}


template auto spp::analyse::utils::func_utils::name_generic_args_impl<spp::asts::GenericArgumentCompAst, spp::asts::GenericParameterCompAst>(
    std::vector<std::unique_ptr<asts::GenericArgumentCompAst>> &args,
    std::vector<asts::GenericParameterCompAst*> params,
    asts::Ast const &owner,
    scopes::ScopeManager const &sm) -> void;


template auto spp::analyse::utils::func_utils::name_generic_args_impl<spp::asts::GenericArgumentTypeAst, spp::asts::GenericParameterTypeAst>(
    std::vector<std::unique_ptr<asts::GenericArgumentTypeAst>> &args,
    std::vector<asts::GenericParameterTypeAst*> params,
    asts::Ast const &owner,
    scopes::ScopeManager const &sm) -> void;


template auto spp::analyse::utils::func_utils::infer_generic_args_impl<spp::asts::GenericArgumentCompKeywordAst, spp::asts::GenericParameterCompAst>(
    std::vector<std::unique_ptr<asts::GenericArgumentCompKeywordAst>> &args,
    std::vector<asts::GenericParameterCompAst*> params,
    std::vector<asts::GenericParameterCompAst*> opt_params,
    std::vector<asts::GenericArgumentCompKeywordAst*> explicit_args,
    std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>> const &infer_source,
    std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>> const &infer_target,
    std::shared_ptr<asts::Ast> owner,
    scopes::Scope const *owner_scope,
    std::shared_ptr<asts::IdentifierAst> variadic_param_identifier,
    scopes::ScopeManager &sm,
    asts::mixins::CompilerMetaData *meta) -> void;


template auto spp::analyse::utils::func_utils::infer_generic_args_impl<spp::asts::GenericArgumentTypeKeywordAst, spp::asts::GenericParameterTypeAst>(
    std::vector<std::unique_ptr<asts::GenericArgumentTypeKeywordAst>> &args,
    std::vector<asts::GenericParameterTypeAst*> params,
    std::vector<asts::GenericParameterTypeAst*> opt_params,
    std::vector<asts::GenericArgumentTypeKeywordAst*> explicit_args,
    std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>> const &infer_source,
    std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>> const &infer_target,
    std::shared_ptr<asts::Ast> owner,
    scopes::Scope const *owner_scope,
    std::shared_ptr<asts::IdentifierAst> variadic_param_identifier,
    scopes::ScopeManager &sm,
    asts::mixins::CompilerMetaData *meta) -> void;
