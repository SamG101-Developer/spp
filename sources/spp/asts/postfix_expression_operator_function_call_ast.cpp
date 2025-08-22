#include <tuple>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/func_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/convention_ref_ast.hpp>
#include <spp/asts/fold_expression_ast.hpp>
#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/function_call_argument_group_ast.hpp>
#include <spp/asts/function_call_argument_keyword_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/function_parameter_required_ast.hpp>
#include <spp/asts/function_parameter_self_ast.hpp>
#include <spp/asts/function_parameter_variadic_ast.hpp>
#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/generic_argument_type_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_runtime_member_access_ast.hpp>
#include <spp/asts/postfix_expression_operator_static_member_access_ast.hpp>
#include <spp/asts/postfix_expression_operator_function_call_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/generate/common_types.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>

#include <genex/actions/remove.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/algorithms/sorted.hpp>
#include <genex/views/address.hpp>
#include <genex/views/cast.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/drop.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/join.hpp>
#include <genex/views/map.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/to.hpp>
#include <genex/views/view.hpp>
#include <genex/operations/size.hpp>
#include <genex/views/set_algorithms.hpp>
#include <genex/views/zip.hpp>

#include "spp/asts/function_call_argument_positional_ast.hpp"


spp::asts::PostfixExpressionOperatorFunctionCallAst::PostfixExpressionOperatorFunctionCallAst(
    decltype(generic_arg_group) &&generic_arg_group,
    decltype(arg_group) &&arg_group,
    decltype(fold) &&fold) :
    PostfixExpressionOperatorAst(),
    generic_arg_group(std::move(generic_arg_group)),
    arg_group(std::move(arg_group)),
    fold(std::move(fold)) {
}


spp::asts::PostfixExpressionOperatorFunctionCallAst::~PostfixExpressionOperatorFunctionCallAst() = default;


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::pos_start() const -> std::size_t {
    return generic_arg_group->pos_start();
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::pos_end() const -> std::size_t {
    return fold ? fold->pos_end() : arg_group->pos_end();
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<PostfixExpressionOperatorFunctionCallAst>(
        ast_clone(generic_arg_group),
        ast_clone(arg_group),
        ast_clone(fold));
}


spp::asts::PostfixExpressionOperatorFunctionCallAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(generic_arg_group);
    SPP_STRING_APPEND(arg_group);
    SPP_STRING_APPEND(fold);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(generic_arg_group);
    SPP_PRINT_APPEND(arg_group);
    SPP_PRINT_APPEND(fold);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::determine_overload(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    const auto lhs = meta->postfix_expression_lhs;
    const auto [fn_owner_type, fn_owner_scope, fn_name] = analyse::utils::func_utils::get_function_owner_type_and_function_name(*lhs, *sm, meta);

    // Convert the "obj.method_call(...args)" into "Type::method_call(obj, ...args)".
    if (const auto cast_lhs = ast_cast<PostfixExpressionAst>(meta->postfix_expression_lhs); cast_lhs != nullptr and ast_cast<PostfixExpressionOperatorRuntimeMemberAccessAst>(cast_lhs->op.get())) {
        auto [transformed_lhs, transformed_fn_call] = analyse::utils::func_utils::convert_method_to_function_form(*fn_owner_type, *fn_name, *lhs, *this, *sm, meta);
        transformed_fn_call->determine_overload(sm, meta);
        m_overload_info = std::move(transformed_fn_call->m_overload_info);
        arg_group = std::move(transformed_fn_call->arg_group);
        return;
    }

    // Record the "pass" and "fail" overloads.
    auto all_overloads = analyse::utils::func_utils::get_all_function_scopes(*fn_name, *fn_owner_scope);
    auto pass_overloads = std::vector<std::tuple<analyse::scopes::Scope*, FunctionPrototypeAst*, std::vector<GenericArgumentAst*>>>{};
    auto fail_overloads = std::vector<std::tuple<analyse::scopes::Scope*, FunctionPrototypeAst*, spp::utils::errors::SemanticError*>>{};

    // Create a dummy overload for no-overload identifiers that are function types (closures etc).
    auto is_closure = false;
    if (const auto lhs_type = analyse::utils::func_utils::is_target_callable(*lhs, *sm, meta); lhs_type != nullptr and all_overloads.empty()) {
        const auto dummy_proto = analyse::utils::func_utils::create_callable_prototype(*lhs_type);
        all_overloads.emplace_back(sm->current_scope, dummy_proto.get(), nullptr);
        is_closure = true;
    }

    for (auto &&[fn_scope, fn_proto, ctx_generic_arg_group] : all_overloads) {
        auto ctx_generic_args = ctx_generic_arg_group->args | genex::views::ptr_unique | genex::views::to<std::vector>();

        // Extract generic/function parameter information from the overload.
        auto func_params = fn_proto->param_group->params | genex::views::ptr_unique | genex::views::to<std::vector>();
        auto generic_params = fn_proto->generic_param_group->params | genex::views::ptr_unique | genex::views::to<std::vector>();
        auto func_args = arg_group->args | genex::views::ptr_unique | genex::views::to<std::vector>();
        auto generic_args = generic_arg_group->args | genex::views::ptr_unique | genex::views::to<std::vector>();

        // Extract the parameter names and argument names.
        auto func_param_names = fn_proto->param_group->params
            | genex::views::map([](auto &&x) { return x->extract_name(); })
            | genex::views::to<std::vector>();

        auto func_param_names_req = fn_proto->param_group->get_required_params()
            | genex::views::map([](auto &&x) { return x->extract_name(); })
            | genex::views::to<std::vector>();

        auto func_arg_names = arg_group->get_keyword_args()
            | genex::views::map([](auto &&x) { return x->name.get(); })
            | genex::views::to<std::vector>();

        auto is_variadic_fn = fn_proto->param_group->get_variadic_param() != nullptr;

        // Use a try-except block to catch any errors as a following overload could still be valid.
        try {
            // Cannot call an abstract function.
            if (fn_proto->m_abstract_annotation != nullptr) {
                analyse::errors::SppFunctionCallAbstractFunctionError(*this, *fn_proto).scopes({fn_scope}).raise();
            }

            // Cannot call a not implemented function.
            if (fn_proto->m_no_impl_annotation != nullptr) {
                analyse::errors::SppFunctionCallNotImplFunctionError(*this, *fn_proto).scopes({fn_scope}).raise();
            }

            // Check if there are too many arguments (for a non-variadic function).
            if (func_args.size() > func_params.size() and not is_variadic_fn) {
                analyse::errors::SppFunctionCallTooManyArgumentsError(*this, *fn_proto).scopes({fn_scope}).raise();
            }

            // Remove the keyword argument names from the set of parameter names, and name the positional arguments.
            analyse::utils::func_utils::name_args(func_args, func_params, *sm);
            analyse::utils::func_utils::name_generic_args(generic_args, generic_params, *fn_proto->name, *sm);
            func_arg_names = func_args
                | genex::views::cast.operator()<FunctionCallArgumentKeywordAst>()
                | genex::views::filter([](auto &&x) { return x != nullptr; })
                | genex::views::map([](auto &&x) { return x->name.get(); })
                | genex::views::to<std::vector>();

            auto generic_infer_source = func_args
                | genex::views::map([sm, meta](auto &&x) { return std::make_pair(x->name.get(), x->val->infer_type(sm, meta)); })
                | genex::views::to<std::vector>();

            auto generic_infer_target = func_params
                | genex::views::map([](auto &&x) { return std::make_pair(x->extract_name(), x->type); })
                | genex::views::to<std::vector>();

            generic_args = analyse::utils::func_utils::infer_generic_args(
                fn_proto->generic_param_group->params,
                fn_proto->generic_param_group->get_optional_params(),
                genex::views::concat(generic_args, ctx_generic_args),
                std::map(generic_infer_source.begin(), generic_infer_source.end()),
                std::map(generic_infer_target.begin(), generic_infer_target.end()),
                lhs->infer_type(sm, meta),
                is_variadic_fn ? fn_proto->param_group->get_variadic_param()->extract_name() : nullptr,
                sm, meta);

            // For function folding, identify all tuple arguments that have non-tuple parameters.
            if (fold != nullptr) {
                // Populate the list of arguments to fold.
                for (auto &&arg : func_args | genex::views::cast.operator()<FunctionCallArgumentKeywordAst*>()) {
                    if (analyse::utils::type_utils::is_type_tuple(*arg->infer_type(sm, meta), *sm->current_scope)) {
                        if (func_params
                            | genex::views::filter([sm](auto &&x) { return not analyse::utils::type_utils::is_type_tuple(*x->type, *sm->current_scope); })
                            | genex::views::filter([arg](auto &&p) { return p->extract_name() == arg->name; })
                            | genex::operations::empty) {
                            m_folded_args.emplace_back(arg);
                        }
                    }
                }

                // Tuples being folded must all have the same element types (per tuple).
                for (auto &&arg : m_folded_args) {
                    auto first_elem_type = ast_cast<GenericArgumentTypeAst>(arg->infer_type(sm, meta)->type_parts().back()->generic_arg_group->args[0].get())->val;
                    auto mismatch = arg->infer_type(sm, meta)->type_parts().back()->generic_arg_group->args
                        | genex::views::drop(1)
                        | genex::views::filter([sm, first_elem_type](auto &&x) { return not analyse::utils::type_utils::symbolic_eq(*x->value, *first_elem_type, *sm->current_scope, *sm->current_scope); })
                        | genex::views::map([](auto &&x) { return x->val.get(); })
                        | genex::views::to<std::vector>();

                    if (not mismatch.empty()) {
                        analyse::errors::SppFunctionFoldTupleElementTypeMismatchError(
                            *first_elem_type, *mismatch[0]).scopes({sm->current_scope}).raise();
                    }
                }

                // Ensure all tuples are the same length.
                const auto first_tup_len = m_folded_args[0]->infer_type(sm, meta)->type_parts().back()->generic_arg_group->args.size();
                for (auto &&arg : m_folded_args | genex::views::drop(1)) {
                    const auto tup_len = arg->infer_type(sm, meta)->type_parts().back()->generic_arg_group->args.size();
                    if (tup_len != first_tup_len) {
                        analyse::errors::SppFunctionFoldTupleLengthMismatchError(
                            *m_folded_args[0]->val, first_tup_len, *arg->val, tup_len).scopes({sm->current_scope}).raise();
                    }
                }
            }

            // Create a new overload with the generic arguments applied.
            if (not generic_arg_group->args.empty()) {
                auto new_fn_proto = ast_clone(fn_proto);
                auto external_generics = sm->current_scope->get_extended_generic_symbols(generic_args);
                auto new_fn_scope = analyse::utils::type_utils::create_generic_fun_scope(
                    *fn_scope, GenericArgumentGroupAst(nullptr, ast_clone_vec(generic_args), nullptr),
                    external_generics, *sm, meta);

                auto tm = ScopeManager(sm->global_scope, new_fn_scope);
                new_fn_proto->generic_param_group->params.clear();

                // Substitute and analyse the function parameters and return type.
                for (auto &&p : new_fn_proto->param_group->params | genex::views::view) {
                    p->type = p->type->substitute_generics(generic_args);
                    p->type->stage_7_analyse_semantics(sm, meta);
                }
                new_fn_proto->return_type = new_fn_proto->return_type->substitute_generics(generic_args);
                new_fn_proto->return_type->stage_7_analyse_semantics(sm, meta);

                // Check he new return type isn't a borrow type.
                if (auto conv = new_fn_proto->return_type->get_convention(); conv != nullptr) {
                    analyse::errors::SppSecondClassBorrowViolationError(*new_fn_proto->return_type, *conv, "substituted function return type")
                        .scopes({sm->current_scope})
                        .raise();
                }

                // Recreate the lists of function parameters and their names.
                func_params = new_fn_proto->param_group->params | genex::views::ptr_unique | genex::views::to<std::vector>();
                func_param_names = new_fn_proto->param_group->params
                    | genex::views::map([](auto &&x) { return x->extract_name(); })
                    | genex::views::to<std::vector>();
                func_param_names_req = new_fn_proto->param_group->get_required_params()
                    | genex::views::map([](auto &&x) { return x->extract_name(); })
                    | genex::views::to<std::vector>();

                // Save the generic implementation against the base function, and update the active scope and prototype.
                fn_proto->m_generic_implementations.emplace_back(std::move(new_fn_proto));
                fn_proto = fn_proto->m_generic_implementations.back().get();
                fn_scope = new_fn_scope;
            }

            // Check any params are void, remove them
            for (auto &&p : fn_proto->param_group->params | genex::views::view) {
                if (analyse::utils::type_utils::is_type_void(*p->type, *fn_scope)) {
                    fn_proto->param_group->params |= genex::actions::remove(p);
                }
            }

            // Check for any keyword arguments that don't have a corresponding parameter.
            const auto invalid_args = func_arg_names
                | genex::views::deref
                | genex::views::set_difference(func_param_names)
                | genex::views::address
                | genex::views::to<std::vector>();
            if (not invalid_args.empty()) {
                analyse::errors::SppArgumentNameInvalidError(*func_params[0], "parameter", *invalid_args[0], "argument").scopes({sm->current_scope}).raise();
            }

            // Check for missing parameters that don't have a corresponding argument.
            const auto missing_params = func_param_names_req
                | genex::views::deref
                | genex::views::set_difference(func_arg_names)
                | genex::views::address
                | genex::views::to<std::vector>();
            if (not missing_params.empty()) {
                analyse::errors::SppArgumentMissingError(*missing_params[0], "parameter", *this, "argument").scopes({sm->current_scope}).raise();
            }

            // Type chek the arguments against the parameters.
            auto sorted_func_arguments = genex::algorithms::sorted(func_args, std::less<int>{}, [&func_param_names](auto &&a) { return func_param_names | genex::algorithms::position(a->name.get()); });
            for (auto &&[arg, param] : sorted_func_arguments | genex::views::zip(func_params)) {
                auto p_type = fn_scope->get_type_symbol(param->type).fq_name()->with_convention(param->type->get_convention());
                auto a_type = arg->infer_type(sm, meta);

                // Special case for variadic parameters.
                if (ast_cast<FunctionParameterVariadicAst>(param.get())) {
                    p_type = generate::common_types::tuple_type(param.pos_start(), std::vector(a_type->type_parts().back()->generic_arg_group->args.size(), p_type));
                    p_type->stage_7_analyse_semantics(sm, meta);
                }

                // Special case for "self" parameters.
                else if (ast_cast<FunctionParameterSelfAst>(param.get())) {
                    arg->convention = ast_clone(param->convention);
                }

                // Regular parameter with arg folding.
                else if (genex::algorithms::contains(m_folded_args, arg.get())) {
                    if (not analyse::utils::type_utils::symbolic_eq(*p_type, *a_type->type_parts().back().generic_arg_group->args[0]->val, *fn_scope, *sm->current_scope)) {
                        analyse::errors::SppTypeMismatchError(*param, *p_type, *arg, *a_type->type_parts().back()->generic_arg_group->args[0]->val).scopes({fn_scope, sm->current_scope}).raise();
                    }
                }

                // Regular parameter without arg folding.
                else if (not analyse::utils::type_utils::symbolic_eq(*p_type, *a_type, *fn_scope, *sm->current_scope)) {
                    analyse::errors::SppTypeMismatchError(*param, *p_type, *arg, *a_type).scopes({fn_scope, sm->current_scope}).raise();
                }
            }

            // Add the overload to the pass list.
            pass_overloads.emplace_back(fn_scope, fn_proto, std::move(generic_args));
        }


        catch (const analyse::errors::SppFunctionCallAbstractFunctionError &e) {
            // If the overload is abstract, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, &e);
        }

        catch (const analyse::errors::SppFunctionCallNotImplFunctionError &e) {
            // If the overload is abstract, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, &e);
        }

        catch (const analyse::errors::SppFunctionCallTooManyArgumentsError &e) {
            // If the overload has too many arguments, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, &e);
        }

        catch (const analyse::errors::SppArgumentNameInvalidError &e) {
            // If the overload has an invalid argument name, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, &e);
        }

        catch (const analyse::errors::SppArgumentMissingError &e) {
            // If the overload is missing a required argument name, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, &e);
        }

        catch (const analyse::errors::SppTypeMismatchError &e) {
            // If the overload has a type mismatch, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, &e);
        }

        catch (const analyse::errors::SppGenericParameterInferredConflictInferredError &e) {
            // If the overload has an inferred generic parameter conflict, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, &e);
        }

        catch (const analyse::errors::SppGenericParameterInferredConflictExplicitError &e) {
            // If the overload has an explicit generic parameter conflict, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, &e);
        }

        catch (const analyse::errors::SppGenericParameterNotInferredError &e) {
            // If the overload has a generic parameter that is not inferred, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, &e);
        }

        catch (const analyse::errors::SppGenericArgumentTooManyError &e) {
            // If the overload has too many generic arguments, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, &e);
        }
    }

    // Perform the return type overload selection separately here, for error reasons.
    auto return_matches = decltype(pass_overloads){};
    if (meta->return_type_overload_resolver_type != nullptr) {
        for (auto &&[fn_scope, fn_proto, fn_generics] : pass_overloads) {
            if (analyse::utils::type_utils::symbolic_eq(*fn_proto->return_type, *meta->return_type_overload_resolver_type, *fn_scope, *sm->current_scope)) {
                return_matches.emplace_back(fn_scope, fn_proto, fn_generics);
            }
        }
        if (return_matches.size() == 1) {
            pass_overloads = std::move(return_matches);
        }
    }

    // If there are no pass overloads, raise an error.
    if (pass_overloads.empty()) {
        auto failed_signatures_and_errors = fail_overloads
            | genex::views::map([](auto &&f) { return std::get<1>(f)->print_signature(std::get<0>(f)->ast != nullptr ? static_cast<std::string>(*ast_name(std::get<0>(f)->ast)) : "") + std::format(" - {}", typeid(std::get<2>(f)).name()); })
            | genex::views::join_with("\n")
            | genex::views::to<std::string>();

        auto arg_usage_signature = arg_group->args
            | genex::views::map([sm, meta](auto &&x) { return x->m_self_type == nullptr ? static_cast<std::string>(*x->infer_type(sm, meta)) : "Self"; })
            | genex::views::join_with(", ")
            | genex::views::to<std::string>();

        analyse::errors::SppFunctionCallNoValidSignaturesError(*this, failed_signatures_and_errors, arg_usage_signature).scopes({sm->current_scope}).raise();
    }

    // If there are multiple pass overloads, raise an error.
    else if (pass_overloads.size() > 1) {
        auto signatures = pass_overloads
            | genex::views::map([](auto &&x) { return std::get<1>(x)->print_signature(std::get<0>(x)->ast != nullptr ? static_cast<std::string>(*ast_name(std::get<0>(x)->ast)) : ""); })
            | genex::views::join_with("\n")
            | genex::views::to<std::string>();

        auto arg_usage_signature = arg_group->args
            | genex::views::map([sm, meta](auto &&x) { return x->m_self_type == nullptr ? static_cast<std::string>(*x->infer_type(sm, meta)) : "Self"; })
            | genex::views::join_with(", ")
            | genex::views::to<std::string>();

        analyse::errors::SppFunctionCallOverloadAmbiguousError(*this, signatures, arg_usage_signature).scopes({sm->current_scope}).raise();
    }

    // Special case for closures; apply the convention the closure name to ensure is it movable/mutable etc.
    if (is_closure) {
        const auto lhs_type = analyse::utils::func_utils::is_target_callable(*lhs, *sm, meta);
        auto dummy_self_arg = std::make_unique<FunctionCallArgumentPositionalAst>(nullptr, nullptr, ast_clone(lhs));
        if (analyse::utils::type_utils::symbolic_eq(*lhs_type->without_generics(), *generate::common_types_precompiled::FUN_MUT, *sm->current_scope, *sm->current_scope)) {
            dummy_self_arg->conv = std::make_unique<ConventionMutAst>(nullptr, nullptr);
        }
        else if (analyse::utils::type_utils::symbolic_eq(*lhs_type->without_generics(), *generate::common_types_precompiled::FUN_REF, *sm->current_scope, *sm->current_scope)) {
            dummy_self_arg->conv = std::make_unique<ConventionRefAst>(nullptr);
        }
        m_closure_dummy_arg = std::move(dummy_self_arg);
    }

    // Set the overload to the only pass overload.
    m_overload_info = std::move(pass_overloads[0]);
    if (auto self_param = std::get<1>(*m_overload_info)->param_group->get_self_param()) {
        arg_group->args[0]->conv = ast_clone(self_param->conv);
    }
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Prevent double analysis.
    if (m_overload_info.has_value()) {
        return;
    }

    meta->save();
    meta->return_type_overload_resolver_type = nullptr;
    generic_arg_group->stage_7_analyse_semantics(sm, meta);
    arg_group->stage_7_analyse_semantics(sm, meta);
    meta->restore();

    determine_overload(sm, meta);

    // Special case for GenOnce called as a coroutine => auto move into the "Yield" type.
    if (std::get<1>(*m_overload_info)->tok_fun->token_type == lex::SppTokenType::KW_COR and meta->prevent_auto_generator_resume) {
        m_is_coro_and_auto_resume = analyse::utils::type_utils::symbolic_eq(
            *generate::common_types_precompiled::GEN_ONCE, *std::get<1>(*m_overload_info)->return_type,
            *sm->current_scope, *std::get<0>(*m_overload_info));
    }
    meta->prevent_auto_generator_resume = false;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // If a fold is taking place, analyse the non-folding argument again (checks for double move).
    if (fold != nullptr and not m_folded_args.empty()) {
        auto non_folding_args = arg_group->args
            | genex::views::ptr_unique
            | genex::views::set_difference(m_folded_args)
            | genex::views::map([](auto &&x) { return ast_clone(x); })
            | genex::views::to<std::vector>();
        auto group = FunctionCallArgumentGroupAst(nullptr, std::move(non_folding_args), nullptr);
        group.stage_8_check_memory(sm, meta);
    }

    // If a closure is being called, apply memory rules to the symbolic target.
    if (m_closure_dummy_arg != nullptr) {
        auto closure_args = std::vector<std::unique_ptr<FunctionCallArgumentAst>>{};
        closure_args.emplace_back(std::move(m_closure_dummy_arg));
        auto group = FunctionCallArgumentGroupAst(nullptr, std::move(closure_args), nullptr);
        group.stage_8_check_memory(sm, meta);
    }

    // Check the argument group, now the old borrows hae been invalidated.
    generic_arg_group->stage_8_check_memory(sm, meta);

    meta->save();
    meta->target_call_function_prototype = std::get<1>(*m_overload_info);
    meta->target_call_was_function_async = m_is_async;
    arg_group->stage_8_check_memory(sm, meta);
    meta->restore();
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the function return type from the overload.
    auto ret_type = std::get<1>(*m_overload_info)->return_type;

    // If there is a scope present (non-closure), then fully qualify the return type.
    if (std::get<0>(*m_overload_info) != nullptr) {
        // Get the other generics
        auto other_generics = std::vector<GenericArgumentAst*>{};
        if (const auto cast_lhs = ast_cast<PostfixExpressionAst>(meta->postfix_expression_lhs); cast_lhs != nullptr) {
            const auto lhs_lhs_scope = sm->current_scope->get_type_symbol(*cast_lhs->lhs->infer_type(sm, meta))->scope;
            if (not analyse::utils::type_utils::is_type_tuple(*std::get<TypeIdentifierAst*>(lhs_lhs_scope->name), *sm->current_scope)) {
                other_generics = std::get<TypeIdentifierAst*>(lhs_lhs_scope->name)->type_parts().back()->generic_arg_group->args
                    | genex::views::ptr_unique
                    | genex::views::to<std::vector>();
            }

            ret_type = std::get<0>(*m_overload_info)
               ->get_type_symbol(*ret_type)->fq_name()
               ->substitute_generics(other_generics)
               ->substitute_generics(std::get<0>(*m_overload_info)->get_generics());

            auto tm = ScopeManager(sm->global_scope, std::get<0>(*m_overload_info));
            ret_type->stage_7_analyse_semantics(&tm, meta);
        }
    }

    // For GenOnce coroutines, automatically resume the coroutine and return the "Yield" type.
    if (m_is_coro_and_auto_resume and meta->prevent_auto_generator_resume) {
        auto [_, yield_type, _, _, _, _] = analyse::utils::type_utils::get_generator_and_yield_type(*ret_type, *sm, *meta->let_stmt_value, "function call");
        ret_type = yield_type;
    }

    // Return the type.
    return ret_type;
}
