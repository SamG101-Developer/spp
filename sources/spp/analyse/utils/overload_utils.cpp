module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <opex/macros.hpp>

module spp.analyse.utils.overload_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.type_utils;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import genex;
import opex.cast;


// The variadic arg is the different excpeiton types that need to get "caught".
#define SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(error_type, message)           \
    catch (error_type const &e) {                                            \
        fail_overloads.emplace_back(fn_scope, fn_proto, e.clone(), message); \
    }


auto spp::analyse::utils::overload_utils::determine_overload(
    asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::pair<PassOverloadInfo, bool> {
    // Extract metadata about the target function's overloads (owner, scope, etc).
    const auto lhs = meta->postfix_expression_lhs;
    const auto [fn_owner_type, fn_owner_scope, fn_name] = func_utils::get_function_owner_type_and_function_name(
        *lhs, *sm, meta);

    // If we are resolving a method, then use the free function equivalent.
    const auto is_postfix = meta->postfix_expression_lhs->to<asts::PostfixExpressionAst>();
    const auto is_runtime = is_postfix ? is_postfix->op->to<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>() : nullptr;
    if (is_runtime != nullptr) {
        return propagate_method_to_function(fn_call, *fn_owner_type, *fn_name, *is_postfix, sm, meta);
    }

    // Get all the overloads to deail with, and handle closure mechanics.
    auto [is_closure, closure_proto, all_overloads] = retrieve_all_overloads(fn_name.get(), *fn_owner_scope, sm, meta);
    auto pass_overloads = std::vector<PassOverloadInfo>{};
    auto fail_overloads = std::vector<FailOverloadInfo>{};

    // Check each provided overload for a complete match.
    for (auto &&[fn_scope, fn_proto, sup_generic_arg_group, fwd_type] : all_overloads) {
        // Extract generic/function parameter information from the overload.
        const auto fn_params = fn_proto->param_group.get();
        const auto gn_params = fn_proto->generic_param_group.get();
        auto fn_args = asts::ast_clone(fn_call.arg_group);
        auto gn_args = asts::ast_clone(fn_call.generic_arg_group);

        // Get the implicit generic arguments for the function call.
        auto implicit_gn_args = retrieve_implicit_generic_args_for_call(
            fwd_type, std::move(sup_generic_arg_group->args), meta);

        // Extract the parameter names and argument names.
        auto fn_param_names = fn_proto->param_group->get_all_params()
            | genex::views::transform([](auto *param) { return param->extract_name(); });
        auto fn_param_names_req = fn_proto->param_group->get_required_params()
            | genex::views::transform([](auto *param) { return param->extract_name(); });
        const auto is_variadic_fn = fn_proto->param_group->get_variadic_param() != nullptr;

        try {
            // Cannot check for "too few" arguments here because of potential "T=Void" + "x: T" removal.
            // Check if there are too many arguments (for a non-variadic function).
            raise_if<errors::SppFunctionCallTooManyArgumentsError>(
                fn_args->args.size() > fn_params->params.size() and not is_variadic_fn,
                {fn_scope}, ERR_ARGS(*fn_proto, fn_call));

            infer_all_generics(
                *fn_proto, *fn_params, *gn_params, *fn_args, *gn_args, *implicit_gn_args, is_variadic_fn, fn_scope, sm, meta);
            std::tie(fn_proto, fn_scope) = generate_generic_substituted_prototype(
                fn_proto, fn_scope, *implicit_gn_args, *gn_args, sm, meta);

            validate_args_match_params(fn_call, *fn_proto, fn_scope, *fn_args, sm, meta);
            pass_overloads.emplace_back(fn_scope, fn_proto, std::move(fn_args), gn_args->get_all_args());
        }

        SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(analyse::errors::SppFunctionCallAbstractFunctionError, "calling an abstract function")
        SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(analyse::errors::SppFunctionCallNotImplFunctionError, "calling a function that is not implemented")
        SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(analyse::errors::SppFunctionCallTooManyArgumentsError, "too many arguments")
        SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(analyse::errors::SppArgumentNameInvalidError, "invalid argument name")
        SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(analyse::errors::SppArgumentMissingError, "missing required argument")
        SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(analyse::errors::SppTypeMismatchError, "type mismatch")
        SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(analyse::errors::SppGenericParameterInferredConflictInferredError, "inferred generic parameter conflict")
        SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(analyse::errors::SppGenericParameterNotInferredError, "generic parameter not inferred")
        SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(analyse::errors::SppGenericArgumentTooManyError, "too many generic arguments")
        SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(analyse::errors::SppGenericConstraintError, "generic constraint not satisfied")
    }
    // Perform the return type overload selection separately here, for error reasons.
    if (meta->return_type_overload_resolver_type != nullptr) {
        auto return_matches = std::vector<PassOverloadInfo>();
        for (auto &&[fn_scope, fn_proto, fn_args, fn_generics] : pass_overloads) {
            if (type_utils::symbolic_eq(*fn_proto->return_type, *meta->return_type_overload_resolver_type, *fn_scope, *sm->current_scope)) {
                return_matches.emplace_back(fn_scope, fn_proto, std::move(fn_args), std::move(fn_generics));
            }
        }

        // If there is only one return-type match, select it into the pass overloads.
        if (return_matches.size() == 1) {
            pass_overloads = std::move(return_matches);
        }
    }

    manage_matched_overloads(fn_call, pass_overloads, fail_overloads, *fn_call.arg_group, sm, meta);
    if (closure_proto) {
        fn_call.closure_dummy_proto = std::move(closure_proto);
    }
    return std::make_pair(std::move(pass_overloads[0]), is_closure);
}


auto spp::analyse::utils::overload_utils::propagate_method_to_function(
    asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
    asts::TypeAst const &fn_owner_type,
    asts::IdentifierAst const &fn_name,
    asts::PostfixExpressionAst const &cast_lhs,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::pair<PassOverloadInfo, bool> {
    // Get the function conversion of the method (free function with self argument).
    auto [transformed_lhs, transformed_fn_call] = func_utils::convert_method_to_function_form(
        fn_owner_type, fn_name, cast_lhs, fn_call, *sm, meta);

    // Determine the overload based off the function (uniform system).
    meta->save();
    meta->postfix_expression_lhs = transformed_lhs.get();
    auto info = determine_overload(*transformed_fn_call, sm, meta);
    meta->restore();

    // Get the argument group with the "self" injection, and bind it to the function call.
    fn_call.arg_group = std::move(transformed_fn_call->arg_group);
    return info;
}


auto spp::analyse::utils::overload_utils::retrieve_all_overloads(
    asts::IdentifierAst const *fn_name,
    scopes::Scope const &fn_owner_scope,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<bool, std::unique_ptr<asts::FunctionPrototypeAst>, std::vector<OverloadInfo>> {
    // For named functions (ie non-closures), get all the function overload implementation scopes.
    auto all_overloads = func_utils::get_all_function_scopes(*fn_name, &fn_owner_scope, *sm, meta);
    if (not all_overloads.empty()) { return std::make_tuple(false, nullptr, std::move(all_overloads)); };

    // If there are no scopes, assume that this is a closure (do functional type check).
    const auto closure_fn_type = func_utils::is_target_callable(*meta->postfix_expression_lhs, *sm, meta);
    if (closure_fn_type != nullptr) {
        auto closure_fn_proto = func_utils::create_callable_prototype(*closure_fn_type);
        all_overloads.emplace_back(sm->current_scope, closure_fn_proto.get(), asts::GenericArgumentGroupAst::new_empty(), nullptr);
        return std::make_tuple(true, std::move(closure_fn_proto), std::move(all_overloads));
    }

    // Otherwise, there are no scopes (handled in caller).
    return std::make_tuple(false, nullptr, std::vector<OverloadInfo>{});
}


auto spp::analyse::utils::overload_utils::retrieve_implicit_generic_args_for_call(
    std::shared_ptr<asts::TypeAst> const &fwd_type,
    std::vector<std::unique_ptr<asts::GenericArgumentAst>> &&sup_gn_args,
    asts::meta::CompilerMetaData const *meta)
    -> std::unique_ptr<asts::GenericArgumentGroupAst> {
    // Get the generic arguments belonging to the owning type of the method.
    auto gn_args = asts::GenericArgumentGroupAst::new_empty();
    const auto is_postfix = meta->postfix_expression_lhs->to<asts::PostfixExpressionAst>();
    const auto is_type = is_postfix ? asts::ast_clone_shared(is_postfix->lhs->to<asts::TypeAst>()) : nullptr;

    // If we are using a forwarding type, inspect that type's generics.
    if (fwd_type != nullptr) {
        auto fwd_generics = std::move(fwd_type->type_parts().back()->generic_arg_group->args);
        gn_args->merge_generics(std::move(fwd_generics));
    }

    // Otherwise, take the generics from the owning type (given it isn't a module-level function).
    else if (is_type != nullptr) {
        auto lhs_generics = std::move(is_type->type_parts().back()->generic_arg_group->args);
        gn_args->merge_generics(std::move(lhs_generics));
    }

    // Merge the "sup [...] Type" generics into the generic argument group for this call.
    gn_args->merge_generics(std::move(sup_gn_args));
    return gn_args;
}


auto spp::analyse::utils::overload_utils::infer_all_generics(
    asts::FunctionPrototypeAst const &fn_proto,
    asts::FunctionParameterGroupAst const &fn_params,
    asts::GenericParameterGroupAst const &gn_params,
    asts::FunctionCallArgumentGroupAst &fn_args,
    asts::GenericArgumentGroupAst &gn_args,
    asts::GenericArgumentGroupAst const &implicit_generic_args,
    const bool is_variadic_fn,
    scopes::Scope const *fn_scope,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    // Name the positional function and generic arguments.
    func_utils::name_fn_args(fn_args, fn_params, *sm);
    func_utils::name_gn_args(gn_args, gn_params, *std::dynamic_pointer_cast<asts::Ast>(fn_proto.name), *sm, *meta);

    // The inference source is all of the function arguments (except for "self")
    auto generic_infer_source = fn_args.get_keyword_args()
        | genex::views::remove_if([](auto const &a) { return a->name->val == "self"; })
        | genex::views::transform([sm, meta](auto const &x) { return std::make_pair(x->name, x->val->infer_type(sm, meta)); })
        | genex::to<std::vector>();

    // The inference target is all of the function parameters (except for "self").
    auto generic_infer_target = fn_params.get_non_self_params()
        | genex::views::transform([](auto *x) { return std::make_pair(x->extract_name(), x->type); })
        | genex::to<std::vector>();

    // Infer all of the generics from the function arguments and parameters.
    func_utils::infer_gn_args(
        gn_args,
        *fn_proto.generic_param_group,
        genex::views::concat(gn_args.get_all_args(), implicit_generic_args.get_all_args()) | genex::to<std::vector>(),
        {generic_infer_source.begin(), generic_infer_source.end()},
        {generic_infer_target.begin(), generic_infer_target.end()},
        meta->postfix_expression_lhs->infer_type(sm, meta),
        *fn_scope,
        is_variadic_fn ? fn_proto.param_group->get_variadic_param()->extract_name() : nullptr,
        false, *sm, *meta);
}


auto spp::analyse::utils::overload_utils::generate_generic_substituted_prototype(
    asts::FunctionPrototypeAst *fn_proto,
    scopes::Scope const *fn_scope,
    asts::GenericArgumentGroupAst &implicit_generic_args,
    asts::GenericArgumentGroupAst &explicit_generic_args,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::pair<asts::FunctionPrototypeAst*, scopes::Scope const*> {
    // Merge the implicit and explicit generic arguments into one group.
    const auto combined_generics = asts::GenericArgumentGroupAst::new_empty();
    combined_generics->merge_generics(std::move(explicit_generic_args.args));
    combined_generics->merge_generics(std::move(implicit_generic_args.args));

    // Consider if we need to create a generic substituted function prototype.
    if (not combined_generics->args.empty()) {
        auto new_fn_proto = asts::ast_clone(fn_proto);
        new_fn_proto->mark_non_generic_impl(fn_proto);

        // Create the new function scope for the generic implementation.
        const auto generic_syms = sm->current_scope->get_extended_generic_symbols(combined_generics->get_all_args());
        const auto new_fn_scope = type_utils::create_generic_fun_scope(
            *fn_scope, asts::GenericArgumentGroupAst(nullptr, ast_clone_vec(combined_generics->args), nullptr),
            generic_syms, sm, meta);
        auto tm = scopes::ScopeManager(sm->global_scope, new_fn_scope);
        new_fn_proto->generic_param_group->params = decltype(new_fn_proto->generic_param_group->params){};

        // Substitute and analyse the function parameters and return type.
        for (auto *p : new_fn_proto->param_group->get_non_self_params()) {
            p->type = p->type->substitute_generics(combined_generics->get_all_args());
            p->type->stage_7_analyse_semantics(&tm, meta);
        }
        new_fn_proto->return_type = new_fn_proto->return_type->substitute_generics(combined_generics->get_all_args());
        new_fn_proto->return_type->stage_7_analyse_semantics(&tm, meta);

        // Check the new return type isn't a borrow type.
        SPP_ENFORCE_SECOND_CLASS_BORROW_VIOLATION(
            new_fn_proto->return_type, new_fn_proto->return_type, tm, "substituted function return type");

        // Save the generic implementation against the base function, and update the active scope and prototype.
        const auto new_fn_proto_ptr = new_fn_proto.get();
        fn_proto->registered_generic_substitutions().back().second = std::move(new_fn_proto);

        // Todo: Remove this once re-using prototypes is done.
        if (meta->current_stage == 12) {
            fn_proto->m_generate_llvm_declaration(&tm, meta, meta->llvm_ctx);
        }

        return std::make_pair(new_fn_proto_ptr, new_fn_scope);
    }

    return std::make_pair(fn_proto, fn_scope);
}


auto spp::analyse::utils::overload_utils::manage_matched_overloads(
    asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
    std::vector<PassOverloadInfo> const &pass_overloads,
    std::vector<FailOverloadInfo> const &fail_overloads,
    asts::FunctionCallArgumentGroupAst const &arg_group,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    // If there are no pass overloads, raise an error.
    using namespace std::string_literals;
    if (pass_overloads.empty()) {
        auto failed_signatures_and_errors = "\n" + (fail_overloads
            | genex::views::transform([](auto const &f) { return "    - "s + std::get<1>(f)->print_signature("") + ": "s + std::get<3>(f); })
            | genex::views::intersperse("\n"s)
            | genex::views::join
            | genex::to<std::string>());

        auto arg_usage_signature = arg_group.args
            | genex::views::transform([sm, meta](auto const &x) { return x->injected_self_type == nullptr ? static_cast<std::string>(*x->infer_type(sm, meta)) : "Self"; })
            | genex::views::intersperse(", "s)
            | genex::views::join
            | genex::to<std::string>();

        raise<errors::SppFunctionCallNoValidSignaturesError>(
            {sm->current_scope}, ERR_ARGS(fn_call, failed_signatures_and_errors, arg_usage_signature));
    }

    // If there are multiple pass overloads, raise an error.
    // std::get<0>(x)->ast != nullptr ? static_cast<std::string>(*ast_name(std::get<0>(x)->ast)) : ""
    if (pass_overloads.size() > 1) {
        auto signatures = "\n" + (pass_overloads
            | genex::views::transform([](auto const &x) { return "    - "s + std::get<1>(x)->print_signature(""); })
            | genex::views::intersperse("\n"s)
            | genex::views::join
            | genex::to<std::string>());

        auto arg_usage_signature = arg_group.args
            | genex::views::transform([sm, meta](auto const &x) { return x->injected_self_type == nullptr ? static_cast<std::string>(*x->infer_type(sm, meta)) : "Self"; })
            | genex::views::intersperse(", "s)
            | genex::views::join
            | genex::to<std::string>();

        raise<errors::SppFunctionCallOverloadAmbiguousError>(
            {sm->current_scope}, ERR_ARGS(fn_call, signatures, arg_usage_signature));
    }
}


auto spp::analyse::utils::overload_utils::validate_args_match_params(
    asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
    asts::FunctionPrototypeAst const &fn_proto,
    scopes::Scope const *fn_scope,
    asts::FunctionCallArgumentGroupAst const &func_args,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    // Check any params are void, pop them (indexes because of unique pointers).
    for (auto &&i : genex::views::iota(0uz, fn_proto.param_group->params.size())) {
        if (type_utils::is_type_void(*fn_proto.param_group->params[i]->type, *fn_scope)) {
            genex::actions::erase(fn_proto.param_group->params, fn_proto.param_group->params.begin() + (i as SSize));
        }
    }

    // Recreate the lists of function parameters, and their names (Void removed, generics etc).
    const auto func_params = fn_proto.param_group.get();
    const auto func_param_names = fn_proto.param_group->params
        | genex::views::transform([](auto &&x) { return x->extract_name(); })
        | genex::to<std::vector>();
    const auto func_param_names_req = fn_proto.param_group->get_required_params()
        | genex::views::transform([](auto &&x) { return x->extract_name(); })
        | genex::to<std::vector>();
    auto func_arg_names = func_args.get_keyword_args()
        | genex::views::transform([](auto const &x) { return x->name.get(); })
        | genex::to<std::vector>();

    // Check for any keyword arguments that don't have a corresponding parameter.
    // Todo: Can we use: "analyse::utils::func_utils::enforce_no_invalid_fn_args()"?
    const auto invalid_args = func_arg_names
        | genex::views::not_in(func_param_names, genex::meta::deref, genex::meta::deref)
        | genex::to<std::vector>();
    raise_if<errors::SppArgumentNameInvalidError>(
        not invalid_args.empty(), {sm->current_scope},
        ERR_ARGS(*func_params->params[0], "parameter", *invalid_args[0], "argument"));

    // Check for missing parameters that don't have a corresponding argument.
    const auto missing_params = func_param_names_req
        | genex::views::not_in(func_arg_names, genex::meta::deref, genex::meta::deref)
        | genex::to<std::vector>();
    raise_if<errors::SppArgumentMissingError>(
        not missing_params.empty(), {sm->current_scope},
        ERR_ARGS(*missing_params[0], "parameter", fn_call, "argument"));

    // Type check the arguments against the parameters. Sort the arguments into parameter order first.
    auto sorted_func_arguments = func_args.get_keyword_args();
    genex::actions::sort(
        sorted_func_arguments,
        {}, [&func_param_names](asts::FunctionCallArgumentKeywordAst *arg) { return genex::position(func_param_names, [&arg](auto const &param) { return *arg->name == *param; }); });

    for (auto [arg, param] : genex::views::zip(sorted_func_arguments, func_params->get_all_params())) {
        auto p_type = fn_scope->get_type_symbol(param->type)->fq_name()->with_convention(ast_clone(param->type->get_convention()));
        auto a_type = arg->infer_type(sm, meta);
        arg->infer_type(sm, meta);
        auto temp = type_utils::GenericInferenceMap();

        // Special case for variadic parameters (updates p_type so don't follow with "else if").
        if (param->to<asts::FunctionParameterVariadicAst>()) {
            auto ts = std::vector(a_type->type_parts().back()->generic_arg_group->args.size(), p_type);
            p_type = asts::generate::common_types::tuple_type(param->pos_start(), std::move(ts));
            p_type->stage_7_analyse_semantics(sm, meta);
        }

        // Special case for "self" parameters.
        if (const auto self_param = param->to<asts::FunctionParameterSelfAst>(); self_param != nullptr) {
            arg->conv = asts::ast_clone(self_param->conv);
        }

        // Regular parameter without arg folding. The double check is required for generics applied to the
        // superclass in sup-ext that cannot be substituted because they can be anything, so reverse type check
        // them with the "relaxed" variation. This is the only place this is required. Check testing with type
        // aliases to be sure.
        // Todo: This is an issue bcoz prototypes still contain generics if they are from the sup block.
        // Todo: Only an issue in final codegen.
        else if (not type_utils::symbolic_eq(*p_type, *a_type, *fn_scope, *sm->current_scope)) {
            raise_if<errors::SppTypeMismatchError>(
                not type_utils::relaxed_symbolic_eq(*a_type, *p_type, *sm->current_scope, *fn_scope, temp),
                {fn_scope, sm->current_scope}, ERR_ARGS(*param, *p_type, *arg, *a_type));
        }
    }
}
