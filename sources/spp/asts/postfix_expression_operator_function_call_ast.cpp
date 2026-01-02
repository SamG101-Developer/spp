module;
#include <opex/macros.hpp>
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_function_call_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.type_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.fold_expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;
import genex;
import opex.cast;


spp::asts::PostfixExpressionOperatorFunctionCallAst::PostfixExpressionOperatorFunctionCallAst(
    decltype(generic_arg_group) &&generic_arg_group,
    decltype(arg_group) &&arg_group,
    decltype(fold) &&fold) :
    PostfixExpressionOperatorAst(),
    m_overload_info(std::nullopt),
    m_is_async(nullptr),
    m_folded_args(),
    m_closure_dummy_arg(nullptr),
    m_closure_dummy_proto(nullptr),
    m_is_coro_and_auto_resume(false),
    generic_arg_group(std::move(generic_arg_group)),
    arg_group(std::move(arg_group)),
    fold(std::move(fold)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->generic_arg_group);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->arg_group);
}


spp::asts::PostfixExpressionOperatorFunctionCallAst::~PostfixExpressionOperatorFunctionCallAst() = default;


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::pos_start() const
    -> std::size_t {
    return generic_arg_group->pos_start();
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::pos_end() const
    -> std::size_t {
    return fold ? fold->pos_end() : arg_group->pos_end();
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<PostfixExpressionOperatorFunctionCallAst>(
        ast_clone(generic_arg_group),
        ast_clone(arg_group),
        ast_clone(fold));
    ast->m_overload_info = m_overload_info;
    ast->m_is_async = m_is_async;
    ast->m_folded_args = m_folded_args;
    ast->m_folded_arg_group = ast_clone(m_folded_arg_group);
    ast->m_closure_dummy_arg_group = ast_clone(m_closure_dummy_arg_group);
    ast->m_closure_dummy_arg = ast_clone(m_closure_dummy_arg);
    ast->m_closure_dummy_proto = ast_clone(m_closure_dummy_proto);
    ast->m_is_coro_and_auto_resume = m_is_coro_and_auto_resume;
    return ast;
}


spp::asts::PostfixExpressionOperatorFunctionCallAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(generic_arg_group);
    SPP_STRING_APPEND(arg_group);
    SPP_STRING_APPEND(fold);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(generic_arg_group);
    SPP_PRINT_APPEND(arg_group);
    SPP_PRINT_APPEND(fold);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::determine_overload(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    const auto lhs = meta->postfix_expression_lhs;
    const auto [fn_owner_type, fn_owner_scope, fn_name] = analyse::utils::func_utils::get_function_owner_type_and_function_name(*lhs, *sm, meta);

    // Convert the "obj.method_call(...args)" into "Type::method_call(obj, ...args)".
    if (const auto cast_lhs = meta->postfix_expression_lhs->to<PostfixExpressionAst>(); cast_lhs != nullptr and cast_lhs->op->to<PostfixExpressionOperatorRuntimeMemberAccessAst>()) {
        auto [transformed_lhs, transformed_fn_call] = analyse::utils::func_utils::convert_method_to_function_form(
            *fn_owner_type, *fn_name, *cast_lhs, *this, *sm, meta);
        meta->save();
        meta->postfix_expression_lhs = transformed_lhs.get();
        transformed_fn_call->determine_overload(sm, meta);
        meta->restore();
        m_overload_info = std::move(transformed_fn_call->m_overload_info);
        arg_group = std::move(transformed_fn_call->arg_group);
        return;
    }

    // Record the "pass" and "fail" overloads.
    auto all_overloads = std::vector<std::tuple<analyse::scopes::Scope const*, FunctionPrototypeAst*, std::unique_ptr<GenericArgumentGroupAst>, std::shared_ptr<asts::TypeAst>>>{};
    if (fn_name != nullptr) {
        all_overloads = analyse::utils::func_utils::get_all_function_scopes(*fn_name, fn_owner_scope, *sm, meta);
    }
    auto pass_overloads = std::vector<std::tuple<analyse::scopes::Scope const*, FunctionPrototypeAst*, std::vector<GenericArgumentAst*>>>();
    auto fail_overloads = std::vector<std::tuple<analyse::scopes::Scope const*, FunctionPrototypeAst*, std::unique_ptr<analyse::errors::SemanticError>>>();

    // Create a dummy overload for no-overload identifiers that are function types (closures etc).
    auto is_closure = false;
    if (all_overloads.empty()) {
        if (const auto lhs_type = analyse::utils::func_utils::is_target_callable(*lhs, *sm, meta); lhs_type != nullptr) {
            m_closure_dummy_proto = analyse::utils::func_utils::create_callable_prototype(*lhs_type);
            all_overloads.emplace_back(sm->current_scope, m_closure_dummy_proto.get(), GenericArgumentGroupAst::new_empty(), nullptr);
            is_closure = true;
        }
    }

    for (auto &&[fn_scope, fn_proto, ctx_generic_arg_group, fwd_type] : all_overloads) {
        auto lhs_arg_group = GenericArgumentGroupAst::new_empty();
        if (fwd_type == nullptr) {
            if (auto p = meta->postfix_expression_lhs->to<PostfixExpressionAst>(); p != nullptr) {
                if (auto pp = p->lhs->to<TypeAst>(); pp != nullptr) {
                    auto args = std::move(std::shared_ptr(ast_clone(pp))->type_parts().back()->generic_arg_group->args);
                    lhs_arg_group->merge_generics(std::move(args));
                }
            }
        }
        else {
            lhs_arg_group->merge_generics(std::move(fwd_type->type_parts().back()->generic_arg_group->args));
        }
        lhs_arg_group->merge_generics(std::move(ctx_generic_arg_group->args));
        auto ctx_generic_args = lhs_arg_group->get_all_args();

        // Extract generic/function parameter information from the overload.
        auto func_params = fn_proto->param_group->get_all_params();
        auto generic_params = fn_proto->generic_param_group->get_all_params();
        auto func_args = ast_clone_vec(arg_group->args);
        auto generic_args = ast_clone_vec(generic_arg_group->args);

        // Extract the parameter names and argument names.
        auto func_param_names = fn_proto->param_group->params
            | genex::views::transform([](auto const &x) { return x->extract_name(); })
            | genex::to<std::vector>();

        auto func_param_names_req = fn_proto->param_group->get_required_params()
            | genex::views::transform([](auto const &x) { return x->extract_name(); })
            | genex::to<std::vector>();

        auto func_arg_names = arg_group->get_keyword_args()
            | genex::views::transform([](auto const &x) { return x->name.get(); })
            | genex::to<std::vector>();

        auto is_variadic_fn = fn_proto->param_group->get_variadic_param() != nullptr;

        // Use a try-except block to catch any errors as a following overload could still be valid.
        try {
            // Cannot call an abstract function.
            // if (fn_proto->abstract_annotation != nullptr) {
            //     analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionCallAbstractFunctionError>()
            //         .with_args(*fn_proto, *this)
            //         .raises_from(fn_scope);
            // }

            // Check if there are too many arguments (for a non-variadic function).
            if (func_args.size() > func_params.size() and not is_variadic_fn) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionCallTooManyArgumentsError>()
                    .with_args(*fn_proto, *this)
                    .raises_from(fn_scope);
            }

            // Remove the keyword argument names from the set of parameter names, and name the positional arguments.
            analyse::utils::func_utils::name_args(func_args, func_params, *sm);
            analyse::utils::func_utils::name_generic_args(generic_args, generic_params, *std::dynamic_pointer_cast<Ast>(fn_proto->name), *sm, meta);
            func_arg_names = func_args
                | genex::views::ptr
                | genex::views::cast_dynamic<FunctionCallArgumentKeywordAst*>()
                | genex::views::transform([](auto const &x) { return x->name.get(); })
                | genex::to<std::vector>();

            auto generic_infer_source = func_args
                | genex::views::ptr
                | genex::views::cast_dynamic<FunctionCallArgumentKeywordAst*>()
                | genex::views::transform([sm, meta](auto const &x) { return std::make_pair(x->name, x->val->infer_type(sm, meta)); })
                | genex::to<std::vector>();

            auto generic_infer_target = func_params
                | genex::views::transform([](auto const &x) { return std::make_pair(x->extract_name(), x->type); })
                | genex::to<std::vector>();

            analyse::utils::func_utils::infer_generic_args(
                generic_args,
                fn_proto->generic_param_group->get_all_params(),
                fn_proto->generic_param_group->get_optional_params(),
                genex::views::concat(generic_args | genex::views::ptr, ctx_generic_args) | genex::to<std::vector>(),
                {generic_infer_source.begin(), generic_infer_source.end()},
                {generic_infer_target.begin(), generic_infer_target.end()},
                lhs->infer_type(sm, meta),
                fn_scope,
                is_variadic_fn ? fn_proto->param_group->get_variadic_param()->extract_name() : nullptr,
                false, *sm, meta);

            // For function folding, identify all tuple arguments that have non-tuple parameters.
            if (fold != nullptr) {
                // Populate the list of arguments to fold.
                for (auto &&arg : func_args | genex::views::ptr | genex::views::cast_dynamic<FunctionCallArgumentKeywordAst*>()) {
                    if (analyse::utils::type_utils::is_type_tuple(*arg->infer_type(sm, meta), *sm->current_scope)) {
                        func_params
                            | genex::views::filter([arg](auto &&x) { return *x->extract_name() == *arg->name; })
                            | genex::views::filter([sm](auto &&x) { return not analyse::utils::type_utils::is_type_tuple(*x->type, *sm->current_scope); })
                            | genex::views::for_each([this, arg](auto &&) { m_folded_args.emplace_back(arg); });
                    }
                }

                // Tuples being folded must all have the same element types (per tuple).
                for (auto &&arg : m_folded_args) {
                    auto first_elem_type = arg->infer_type(sm, meta)->type_parts().back()->generic_arg_group->args[0]->to<GenericArgumentTypeAst>()->val;
                    auto mismatch = arg->infer_type(sm, meta)->type_parts().back()->generic_arg_group->get_type_args()
                        | genex::views::drop(1)
                        | genex::views::filter([sm, first_elem_type](auto &&x) { return not analyse::utils::type_utils::symbolic_eq(*x->val, *first_elem_type, *sm->current_scope, *sm->current_scope); })
                        | genex::views::transform([](auto &&x) { return x->val.get(); })
                        | genex::to<std::vector>();

                    if (not mismatch.empty()) {
                        analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionFoldTupleElementTypeMismatchError>()
                            .with_args(*first_elem_type, *mismatch[0])
                            .raises_from(sm->current_scope);
                    }
                }

                // Ensure all tuples are the same length.
                const auto first_tup_len = m_folded_args[0]->infer_type(sm, meta)->type_parts().back()->generic_arg_group->args.size();
                for (auto &&arg : m_folded_args | genex::views::drop(1)) {
                    const auto tup_len = arg->infer_type(sm, meta)->type_parts().back()->generic_arg_group->args.size();
                    if (tup_len != first_tup_len) {
                        analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionFoldTupleLengthMismatchError>()
                            .with_args(*m_folded_args[0]->val, first_tup_len, *arg->val, tup_len)
                            .raises_from(sm->current_scope);
                    }
                }
            }

            // Create a new overload with the generic arguments applied.
            // Todo: should re-use generic substituted prototypes when available. Then remove the manual generation.
            auto combined_generics = GenericArgumentGroupAst::new_empty();
            combined_generics->merge_generics(std::move(generic_args));
            combined_generics->merge_generics(std::move(lhs_arg_group->args));
            auto generic_args_raw = combined_generics->get_all_args();
            if (not generic_args_raw.empty()) {
                auto new_fn_proto = ast_clone(fn_proto); // Todo: if the fn_proto hasn't been analysed -> issues.
                auto external_generics = sm->current_scope->get_extended_generic_symbols(generic_args_raw);
                auto new_fn_scope = analyse::utils::type_utils::create_generic_fun_scope(
                    *fn_scope, GenericArgumentGroupAst(nullptr, ast_clone_vec(combined_generics->args), nullptr),
                    external_generics, sm, meta);

                auto tm = ScopeManager(sm->global_scope, new_fn_scope);
                new_fn_proto->generic_param_group->params = decltype(GenericParameterGroupAst::params)();

                // Substitute and analyse the function parameters and return type.
                for (auto *p : new_fn_proto->param_group->get_non_self_params()) {
                    p->type = p->type->substitute_generics(generic_args_raw);
                    p->type->stage_7_analyse_semantics(&tm, meta);
                }
                new_fn_proto->return_type = new_fn_proto->return_type->substitute_generics(generic_args_raw);
                new_fn_proto->return_type->stage_7_analyse_semantics(&tm, meta);

                // Check the new return type isn't a borrow type.
                if (auto conv = new_fn_proto->return_type->get_convention(); conv != nullptr) {
                    analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>()
                        .with_args(*new_fn_proto->return_type, *conv, "substituted function return type")
                        .raises_from(sm->current_scope);
                }

                // Save the generic implementation against the base function, and update the active scope and prototype.
                auto new_fn_proto_ptr = new_fn_proto.get();
                fn_proto->registered_generic_substitutions().back().second = std::move(new_fn_proto);
                fn_proto = new_fn_proto_ptr;
                fn_scope = new_fn_scope;

                // Todo: Remove this once re-using prototypes is done.
                if (meta->current_stage == 12) {
                    fn_proto->m_generate_llvm_declaration(&tm, meta, meta->llvm_ctx);
                }
            }

            // Check any params are void, pop them (indexes because of unique pointers).
            for (auto &&i : genex::views::iota(0uz, fn_proto->param_group->params.size())) {
                if (analyse::utils::type_utils::is_type_void(*fn_proto->param_group->params[i]->type, *fn_scope)) {
                    genex::actions::erase(fn_proto->param_group->params, fn_proto->param_group->params.begin() + (i as SSize));
                }
            }

            // Recreate the lists of function parameters, and their names (Void removed, generics etc).
            func_params = fn_proto->param_group->params | genex::views::ptr | genex::to<std::vector>();
            func_param_names = fn_proto->param_group->params
                | genex::views::transform([](auto &&x) { return x->extract_name(); })
                | genex::to<std::vector>();
            func_param_names_req = fn_proto->param_group->get_required_params()
                | genex::views::transform([](auto &&x) { return x->extract_name(); })
                | genex::to<std::vector>();

            // Check for any keyword arguments that don't have a corresponding parameter.
            const auto invalid_args = func_arg_names
                | genex::views::set_difference_unsorted(func_param_names, SPP_INSTANT_INDIRECT, SPP_INSTANT_INDIRECT)
                | genex::to<std::vector>();
            if (not invalid_args.empty()) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppArgumentNameInvalidError>()
                    .with_args(*func_params[0], "parameter", *invalid_args[0], "argument")
                    .raises_from(sm->current_scope);
            }

            // Check for missing parameters that don't have a corresponding argument.
            const auto missing_params = func_param_names_req
                | genex::views::set_difference_unsorted(func_arg_names, SPP_INSTANT_INDIRECT, SPP_INSTANT_INDIRECT)
                | genex::to<std::vector>();
            if (not missing_params.empty()) {
                analyse::errors::SemanticErrorBuilder<analyse::errors::SppArgumentMissingError>()
                    .with_args(*missing_params[0], "parameter", *this, "argument")
                    .raises_from(sm->current_scope);
            }

            // Type check the arguments against the parameters. Sort the arguments into parameter order first.
            auto sorted_func_arguments = func_args | genex::views::ptr | genex::views::cast_dynamic<FunctionCallArgumentKeywordAst*>() | genex::to<std::vector>();
            genex::actions::sort(
                sorted_func_arguments,
                {}, [&func_param_names](FunctionCallArgumentKeywordAst *arg) { return genex::position(func_param_names, [&arg](auto const &param) { return *arg->name == *param; }); });

            for (auto &&[arg, param] : sorted_func_arguments | genex::views::zip(func_params)) {
                auto p_type = fn_scope->get_type_symbol(param->type)->fq_name()->with_convention(ast_clone(param->type->get_convention()));
                auto a_type = arg->infer_type(sm, meta);
                auto temp = analyse::utils::type_utils::GenericInferenceMap();

                // Special case for variadic parameters (updates p_type so don't follow with "else if").
                if (param->to<FunctionParameterVariadicAst>()) {
                    auto ts = std::vector(a_type->type_parts().back()->generic_arg_group->args.size(), p_type);
                    p_type = generate::common_types::tuple_type(param->pos_start(), std::move(ts));
                    p_type->stage_7_analyse_semantics(sm, meta);
                }

                // Special case for "self" parameters.
                if (auto self_param = param->to<FunctionParameterSelfAst>(); self_param != nullptr) {
                    arg->conv = ast_clone(self_param->conv);
                }

                // Regular parameter with arg folding.
                else if (genex::contains(m_folded_args, arg)) {
                    if (not analyse::utils::type_utils::symbolic_eq(*p_type, *a_type->type_parts().back()->generic_arg_group->get_type_args()[0]->val, *fn_scope, *sm->current_scope)) {
                        analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>()
                            .with_args(*param, *p_type, *arg, *a_type->type_parts().back()->generic_arg_group->get_type_args()[0]->val)
                            .raises_from(fn_scope, sm->current_scope);
                    }
                }

                // Regular parameter without arg folding. The double check is required for generics applied to the
                // superclass in sup-ext that cannot be substituted because they can be anything, so reverse type check
                // them with the "relaxed" variation. This is the only place this is required. Check testing with type
                // aliases to be sure.
                // Todo: Is this needed for the arg folding variation?
                else if (not analyse::utils::type_utils::symbolic_eq(*p_type, *a_type, *fn_scope, *sm->current_scope)) {
                    if (not analyse::utils::type_utils::relaxed_symbolic_eq(*a_type, *p_type, sm->current_scope, fn_scope, temp)) {
                        analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>()
                            .with_args(*param, *p_type, *arg, *a_type)
                            .raises_from(fn_scope, sm->current_scope);
                    }
                }
            }

            // Add the overload to the pass list.
            pass_overloads.emplace_back(fn_scope, fn_proto, std::move(generic_args_raw));
        }

        catch (const analyse::errors::SppFunctionCallAbstractFunctionError &e) {
            // If the overload is abstract, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, e.clone());
        }

        catch (const analyse::errors::SppFunctionCallNotImplFunctionError &e) {
            // If the overload is abstract, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, e.clone());
        }

        catch (const analyse::errors::SppFunctionCallTooManyArgumentsError &e) {
            // If the overload has too many arguments, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, e.clone());
        }

        catch (const analyse::errors::SppArgumentNameInvalidError &e) {
            // If the overload has an invalid argument name, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, e.clone());
        }

        catch (const analyse::errors::SppArgumentMissingError &e) {
            // If the overload is missing a required argument name, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, e.clone());
        }

        catch (const analyse::errors::SppTypeMismatchError &e) {
            // If the overload has a type mismatch, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, e.clone());
        }

        catch (const analyse::errors::SppGenericParameterInferredConflictInferredError &e) {
            // If the overload has an inferred generic parameter conflict, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, e.clone());
        }

        catch (const analyse::errors::SppGenericParameterNotInferredError &e) {
            // If the overload has a generic parameter that is not inferred, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, e.clone());
        }

        catch (const analyse::errors::SppGenericArgumentTooManyError &e) {
            // If the overload has too many generic arguments, we cannot use it.
            fail_overloads.emplace_back(fn_scope, fn_proto, e.clone());
        }
    }

    // Perform the return type overload selection separately here, for error reasons.
    auto return_matches = decltype(pass_overloads)();
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
    using namespace std::string_literals;
    if (pass_overloads.empty()) {
        auto failed_signatures_and_errors = "\n" + (fail_overloads
            | genex::views::transform([](auto const &f) { return "    - "s + std::get<1>(f)->print_signature(""); })
            | genex::views::intersperse("\n"s)
            | genex::views::join
            | genex::to<std::string>());

        auto arg_usage_signature = arg_group->args
            | genex::views::transform([sm, meta](auto const &x) { return x->injected_self_type == nullptr ? static_cast<std::string>(*x->infer_type(sm, meta)) : "Self"; })
            | genex::views::intersperse(", "s)
            | genex::views::join
            | genex::to<std::string>();

        analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionCallNoValidSignaturesError>()
            .with_args(*this, failed_signatures_and_errors, arg_usage_signature)
            .raises_from(sm->current_scope);
    }

    // If there are multiple pass overloads, raise an error.
    // std::get<0>(x)->ast != nullptr ? static_cast<std::string>(*ast_name(std::get<0>(x)->ast)) : ""
    if (pass_overloads.size() > 1) {
        auto signatures = "\n" + (pass_overloads
            | genex::views::transform([](auto const &x) { return "    - "s + std::get<1>(x)->print_signature(""); })
            | genex::views::intersperse("\n"s)
            | genex::views::join
            | genex::to<std::string>());

        auto arg_usage_signature = arg_group->args
            | genex::views::transform([sm, meta](auto const &x) { return x->injected_self_type == nullptr ? static_cast<std::string>(*x->infer_type(sm, meta)) : "Self"; })
            | genex::views::intersperse(", "s)
            | genex::views::join
            | genex::to<std::string>();

        analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionCallOverloadAmbiguousError>()
            .with_args(*this, signatures, arg_usage_signature)
            .raises_from(sm->current_scope);
    }

    // Special case for closures; apply the convention the closure name to ensure is it movable/mutable etc.
    // Todo: move this from overload selection to semantic_analysis?
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
    if (const auto self_param = std::get<1>(*m_overload_info)->param_group->get_self_param()) {
        arg_group->args[0]->conv = ast_clone(self_param->conv);
    }
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Prevent double analysis.
    if (m_overload_info.has_value()) { return; }

    meta->save();
    meta->return_type_overload_resolver_type = nullptr;
    generic_arg_group->stage_7_analyse_semantics(sm, meta);
    arg_group->stage_7_analyse_semantics(sm, meta);
    meta->restore();

    determine_overload(sm, meta);

    // Special case for GenOnce called as a coroutine => auto move into the "Yield" type.
    if (std::get<1>(*m_overload_info)->tok_fun->token_type == lex::SppTokenType::KW_COR and not meta->prevent_auto_generator_resume) {
        m_is_coro_and_auto_resume = analyse::utils::type_utils::symbolic_eq(
            *generate::common_types_precompiled::GEN_ONCE, *std::get<1>(*m_overload_info)->return_type->without_generics(),
            *sm->current_scope, *std::get<0>(*m_overload_info));
    }
    meta->prevent_auto_generator_resume = false;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If a fold is taking place, analyse the non-folding argument again (checks for double move).
    if (fold != nullptr and not m_folded_args.empty()) {
        auto non_folding_args = arg_group->args
            | genex::views::ptr
            | genex::views::materialize
            | genex::views::set_difference_unsorted(m_folded_args)
            | genex::views::transform([](auto &&x) { return ast_clone(x); })
            | genex::to<std::vector>();
        m_folded_arg_group = std::make_unique<FunctionCallArgumentGroupAst>(nullptr, std::move(non_folding_args), nullptr);
        m_folded_arg_group->stage_7_analyse_semantics(sm, meta);
        m_folded_arg_group->stage_8_check_memory(sm, meta);
    }

    // If a closure is being called, apply memory rules to the symbolic target.
    if (m_closure_dummy_arg != nullptr) {
        auto closure_args = std::vector<std::unique_ptr<FunctionCallArgumentAst>>();
        closure_args.emplace_back(std::move(m_closure_dummy_arg));
        m_closure_dummy_arg_group = std::make_unique<FunctionCallArgumentGroupAst>(nullptr, std::move(closure_args), nullptr);
        m_closure_dummy_arg_group->stage_7_analyse_semantics(sm, meta);
        m_closure_dummy_arg_group->stage_8_check_memory(sm, meta);
    }

    // Check the argument group, now the old borrows hae been invalidated.
    generic_arg_group->stage_8_check_memory(sm, meta);

    meta->save();
    meta->target_call_function_prototype = std::get<1>(*m_overload_info);
    meta->target_call_was_function_async = m_is_async;
    arg_group->stage_8_check_memory(sm, meta);
    meta->restore();
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) -> llvm::Value* {
    // Get the llvm function target, and generate the argument values.
    const auto uid = spp::utils::generate_uid(this);
    const auto llvm_func = std::get<1>(*m_overload_info)->llvm_func;
    const auto llvm_func_args = arg_group->args
        | genex::views::transform([sm, meta, ctx](auto const &x) { return x->stage_10_code_gen_2(sm, meta, ctx); })
        | genex::to<std::vector>();
    SPP_ASSERT(llvm_func != nullptr);
    SPP_ASSERT(not ctx->builder.GetInsertBlock()->getTerminator());

    // Create the call instruction.
    const auto llvm_call = ctx->builder.CreateCall(llvm_func, llvm_func_args, "call" + uid);
    return llvm_call;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the function return type from the overload.
    auto ret_type = std::get<1>(*m_overload_info)->return_type;

    // If there is a scope present (non-closure), then fully qualify the return type.
    if (std::get<0>(*m_overload_info) != nullptr) {
        ret_type = std::get<0>(*m_overload_info)->get_type_symbol(ret_type)->fq_name();
    }

    // For GenOnce coroutines, automatically resume the coroutine and return the "Yield" type.
    if (m_is_coro_and_auto_resume and not meta->prevent_auto_generator_resume) {
        auto [_, yield_type, _, _, _, _] = analyse::utils::type_utils::get_generator_and_yield_type(
            *ret_type, *sm->current_scope, *meta->postfix_expression_lhs, "function call");
        ret_type = yield_type;
    }

    // Return the type.
    return ret_type;
}


auto spp::asts::PostfixExpressionOperatorFunctionCallAst::mark_as_async(
    Ast *async_token)
    -> void {
    m_is_async = async_token;
}
