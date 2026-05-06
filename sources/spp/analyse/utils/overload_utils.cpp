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
import spp.asts.expression_ast;
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
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import genex;
import opex.cast;


// The variadic arg is the different exception types that need to get "caught".
#define SPP_OVERLOAD_RESOLUTION_ERROR_HANDLER(error_type, message)           \
    catch (error_type const &e) {                                            \
        fail_overloads.EmplaceBack(fn_scope, fn_proto, e.Clone(), message); \
    }


auto spp::analyse::utils::overload_utils::DetermineOverload(
    asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> Pair<PassOverloadInfo, bool> {
    // Extract metadata about the target function's overloads (owner, scope, etc).
    using type_utils::TypeEq;
    using func_utils::GetFuncOwnerTypeAndFuncName;
    auto lhs = meta->PostfixExpressionLhs;

    // Todo: Workaround for aliased variable symbols being used as function targets, due
    //  to scope lookup.
    auto temp = Shared<asts::ExpressionAst>(nullptr);
    if (const auto id = lhs->To<asts::IdentifierAst>()) {
        const auto x = sm->CurrentScope->GetVarSymbol(asts::AstCloneShared(id));
        if (x and x->MemInfo->AstCompTime) {
            temp = x->FqName();
            lhs = temp.get();
        }
    }

    const auto [fn_owner_type, fn_owner_scope, fn_name] = GetFuncOwnerTypeAndFuncName(
        *lhs, *sm, meta);

    // If we are resolving a method, then use the free function equivalent.
    const auto is_postfix = meta->PostfixExpressionLhs->To<asts::PostfixExpressionAst>();
    const auto is_runtime = is_postfix ? is_postfix->Op->To<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>() : nullptr;
    if (is_runtime != nullptr) {
        auto [overload_info, is_closure, pf] = PropagateMethodToFunction(
            fn_call, *fn_owner_type, *fn_name, *is_postfix, sm, meta);
        fn_call.SetTransformedAst(std::move(pf));
        return MakePair(std::move(overload_info), is_closure);
    }

    // Get all the overloads to deal with, and handle closure mechanics.
    auto [is_closure, closure_proto, all_overloads] = RetrieveAllOverloads(fn_name.get(), *fn_owner_scope, sm, meta);
    auto pass_overloads = Vec<PassOverloadInfo>{};
    auto fail_overloads = Vec<FailOverloadInfo>{};

    // Check each provided overload for a complete match.
    for (auto &&[fn_scope, fn_proto, sup_generic_arg_group, fwd_type] : all_overloads) {
        // Extract generic/function parameter information from the overload.
        const auto fn_params = fn_proto->FnParamGroup.get();
        const auto gn_params = fn_proto->GnParamGroup.get();
        auto fn_args = asts::AstClone(fn_call.FnArgGroup);
        auto gn_args = asts::AstClone(fn_call.GnArgGroup);

        // Get the implicit generic arguments for the function call.
        auto implicit_gn_args = RetrieveImplicitGenericArgsForCall(
            fwd_type, std::move(sup_generic_arg_group->Args), meta);

        // Extract the parameter names and argument names.
        auto fn_param_names = fn_proto->FnParamGroup->GetAllParams()
            | genex::views::transform([](auto *param) { return param->ExtractName(); });
        auto fn_param_names_req = fn_proto->FnParamGroup->GetRequiredParams()
            | genex::views::transform([](auto *param) { return param->ExtractName(); });
        const auto is_variadic_fn = fn_proto->FnParamGroup->GetVariadicParams() != nullptr;

        try {
            // Cannot check for "too few" arguments here because of potential "T=Void" + "x: T" removal.
            // Check if there are too many arguments (for a non-variadic function).
            RaiseIf<errors::SppFunctionCallTooManyArgumentsError>(
                fn_args->Args.Len() > fn_params->Params.Len() and not is_variadic_fn,
                {fn_scope}, ERR_ARGS(*fn_proto, fn_call));

            InferAllGenerics(
                *fn_proto, *fn_params, *gn_params, *fn_args, *gn_args, *implicit_gn_args, is_variadic_fn, fn_scope, sm, meta);
            std::tie(fn_proto, fn_scope) = GenerateGenericSubstitutedPrototype(
                fn_proto, fn_scope, *implicit_gn_args, *gn_args, sm, meta);

            ValidateArgsMatchParams(fn_call, *fn_proto, fn_scope, *fn_args, sm, meta);
            pass_overloads.EmplaceBack(fn_scope, fn_proto, std::move(fn_args), gn_args->GetAllArgs());
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
    if (meta->ReturnTypeOverloadResolverType != nullptr) {
        auto return_matches = Vec<PassOverloadInfo>();
        for (auto &&[fn_scope, fn_proto, fn_args, fn_generics] : pass_overloads) {
            if (TypeEq(*fn_proto->ReturnType, *meta->ReturnTypeOverloadResolverType, *fn_scope, *sm->CurrentScope)) {
                return_matches.EmplaceBack(fn_scope, fn_proto, std::move(fn_args), std::move(fn_generics));
            }
        }

        // If there is only one return-type match, select it into the pass overloads.
        if (return_matches.Len() == 1) {
            pass_overloads = std::move(return_matches);
        }
    }

    ManageMatchedOverloads(fn_call, pass_overloads, fail_overloads, *fn_call.FnArgGroup, sm, meta);
    if (closure_proto) {
        fn_call.SetClosureDummyProto(std::move(closure_proto));
    }
    return MakePair(std::move(pass_overloads[0]), is_closure);
}


auto spp::analyse::utils::overload_utils::PropagateMethodToFunction(
    asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
    asts::TypeAst const &fn_owner_type,
    asts::IdentifierAst const &fn_name,
    asts::PostfixExpressionAst const &cast_lhs,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<PassOverloadInfo, bool, Unique<asts::PostfixExpressionAst>> {
    //
    using func_utils::ConvertMethodToFuncForm;

    // Get the function conversion of the method (free function with self argument).
    auto [transformed_lhs, transformed_fn_call] = ConvertMethodToFuncForm(
        fn_owner_type, fn_name, cast_lhs, fn_call, *sm, meta);

    // Determine the overload based off the function (uniform system).
    meta->Save();
    meta->PostfixExpressionLhs = transformed_lhs.get();
    auto [overload_info, is_closure] = DetermineOverload(*transformed_fn_call, sm, meta);
    meta->Restore();

    // Get the argument group with the "self" injection, and bind it to the function call.
    fn_call.FnArgGroup = asts::AstClone(transformed_fn_call->FnArgGroup);

    // Create a mock postfix based on the transformation.
    auto pf = MakeUnique<asts::PostfixExpressionAst>(
        std::move(transformed_lhs), std::move(transformed_fn_call));
    return std::make_tuple(std::move(overload_info), is_closure, std::move(pf));
}


auto spp::analyse::utils::overload_utils::RetrieveAllOverloads(
    asts::IdentifierAst const *fn_name,
    scopes::Scope const &fn_owner_scope,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<bool, Unique<asts::FunctionPrototypeAst>, Vec<OverloadInfo>> {
    //
    using func_utils::IsTargetCallable;
    using func_utils::CreateCallablePrototype;

    // For named functions (ie non-closures), get all the function overload implementation scopes.
    auto all_overloads = fn_name
        ? func_utils::GetAllFunctionScopes(*fn_name, &fn_owner_scope, *sm, meta)
        : Vec<std::tuple<scopes::Scope const*, asts::FunctionPrototypeAst*, Unique<asts::GenericArgumentGroupAst>, Shared<asts::TypeAst>>>{};
    if (not all_overloads.IsEmpty()) { return std::make_tuple(false, nullptr, std::move(all_overloads)); };

    // If there are no scopes, assume that this is a closure (do functional type check).
    const auto closure_fn_type = IsTargetCallable(*meta->PostfixExpressionLhs, *sm, meta);
    if (closure_fn_type != nullptr) {
        auto closure_fn_proto = CreateCallablePrototype(*closure_fn_type);
        all_overloads.EmplaceBack(sm->CurrentScope, closure_fn_proto.get(), asts::GenericArgumentGroupAst::NewEmpty(), nullptr);
        return std::make_tuple(true, std::move(closure_fn_proto), std::move(all_overloads));
    }

    // Otherwise, there are no scopes (handled in caller).
    return std::make_tuple(false, nullptr, Vec<OverloadInfo>{});
}


auto spp::analyse::utils::overload_utils::RetrieveImplicitGenericArgsForCall(
    Shared<asts::TypeAst> const &fwd_type,
    Vec<Unique<asts::GenericArgumentAst>> &&sup_gn_args,
    asts::meta::CompilerMetaData const *meta)
    -> Unique<asts::GenericArgumentGroupAst> {
    // Get the generic arguments belonging to the owning type of the method.
    auto gn_args = asts::GenericArgumentGroupAst::NewEmpty();
    const auto is_postfix = meta->PostfixExpressionLhs->To<asts::PostfixExpressionAst>();
    const auto is_type = is_postfix ? asts::AstCloneShared(is_postfix->Lhs->To<asts::TypeAst>()) : nullptr;

    // If we are using a forwarding type, inspect that type's generics.
    if (fwd_type != nullptr) {
        auto fwd_generics = std::move(fwd_type->TypeParts().Back()->GnArgGroup->Args);
        gn_args->MergeGenerics(std::move(fwd_generics));
    }

    // Otherwise, take the generics from the owning type (given it isn't a module-level function).
    else if (is_type != nullptr) {
        auto lhs_generics = std::move(is_type->TypeParts().Back()->GnArgGroup->Args);
        gn_args->MergeGenerics(std::move(lhs_generics));
    }

    // Merge the "sup [...] Type" generics into the generic argument group for this call.
    gn_args->MergeGenerics(std::move(sup_gn_args));
    return gn_args;
}


auto spp::analyse::utils::overload_utils::InferAllGenerics(
    asts::FunctionPrototypeAst const &fn_proto,
    asts::FunctionParameterGroupAst const &fn_params,
    asts::GenericParameterGroupAst const &gn_params,
    asts::FunctionCallArgumentGroupAst &fn_args,
    asts::GenericArgumentGroupAst &explicit_gn_args,
    asts::GenericArgumentGroupAst const &implicit_gn_args,
    const bool is_variadic_fn,
    scopes::Scope const *fn_scope,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    // Name the positional function and generic arguments.
    func_utils::name_fn_args(fn_args, fn_params, *sm);
    func_utils::name_gn_args(explicit_gn_args, gn_params, *dynamic_shared_cast<asts::Ast>(fn_proto.Name), *sm, *meta);

    // The inference source is all of the function arguments (except for "self")
    auto generic_infer_source = fn_args.GetKeywordArgs()
        | genex::views::remove_if([](auto const &a) { return a->Name->Val == "self"; })
        | genex::views::transform([sm, meta](auto const &x) { return MakePair(x->Name, x->Val->InferType(sm, meta)); })
        | genex::to<Vec>();

    // The inference target is all of the function parameters (except for "self").
    auto generic_infer_target = fn_params.GetNonSelfParams()
        | genex::views::transform([](auto *x) { return MakePair(x->ExtractName(), x->Type); })
        | genex::to<Vec>();

    // Infer all of the generics from the function arguments and parameters.
    const auto temp_arg_group = asts::GenericArgumentGroupAst::NewEmpty();
    *temp_arg_group += explicit_gn_args;
    *temp_arg_group += implicit_gn_args;

    func_utils::infer_gn_args(
        *fn_proto.GnParamGroup,
        *temp_arg_group,
        {generic_infer_source.begin(), generic_infer_source.end()},
        {generic_infer_target.begin(), generic_infer_target.end()},
        meta->PostfixExpressionLhs->InferType(sm, meta),
        *fn_scope,
        is_variadic_fn ? fn_proto.FnParamGroup->GetVariadicParams()->ExtractName() : nullptr,
        false, *sm, *meta);
    explicit_gn_args.Args = std::move(temp_arg_group->Args);
}


auto spp::analyse::utils::overload_utils::GenerateGenericSubstitutedPrototype(
    asts::FunctionPrototypeAst *fn_proto,
    scopes::Scope const *fn_scope,
    asts::GenericArgumentGroupAst &implicit_generic_args,
    asts::GenericArgumentGroupAst &explicit_generic_args,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<asts::FunctionPrototypeAst*, scopes::Scope const*> {
    //
    using type_utils::IsTypeBorrowed;
    using errors::SppSecondClassBorrowViolationError;

    // Merge the implicit and explicit generic arguments into one group.
    const auto combined_generics = asts::GenericArgumentGroupAst::NewEmpty();
    combined_generics->MergeGenerics(std::move(explicit_generic_args.Args));
    combined_generics->MergeGenerics(std::move(implicit_generic_args.Args));

    // Consider if we need to create a generic substituted function prototype.
    if (not combined_generics->Args.IsEmpty()) {
        auto new_fn_proto = asts::AstClone(fn_proto);
        new_fn_proto->SetNonGenericImpl(fn_proto);

        // Create the new function scope for the generic implementation.
        const auto generic_syms = sm->CurrentScope->GetExtendedGenericSymbols(combined_generics->GetAllArgs());
        const auto new_fn_scope = type_utils::CreateGenericFunScope(
            *fn_scope, asts::GenericArgumentGroupAst(nullptr, AstCloneVec(combined_generics->Args), nullptr),
            generic_syms, sm, meta);
        auto tm = scopes::ScopeManager(sm->GlobalScope, new_fn_scope);
        new_fn_proto->GnParamGroup->Params = decltype(new_fn_proto->GnParamGroup->Params){};

        // Substitute and analyse the function parameters and return type.
        for (auto *p : new_fn_proto->FnParamGroup->GetNonSelfParams()) {
            p->Type = p->Type->SubstituteGenerics(combined_generics->GetAllArgs());
            p->Type->Stage7_AnalyseSemantics(&tm, meta);
        }
        new_fn_proto->ReturnType = new_fn_proto->ReturnType->SubstituteGenerics(combined_generics->GetAllArgs());
        new_fn_proto->ReturnType->Stage7_AnalyseSemantics(&tm, meta);

        // Check the new return type isn't a borrow type.
        RaiseIf<SppSecondClassBorrowViolationError>(
            IsTypeBorrowed(*new_fn_proto->ReturnType, tm),
            {sm->CurrentScope}, ERR_ARGS(*new_fn_proto->ReturnType, *new_fn_proto->ReturnType, "substituted function return type"));

        // Save the generic implementation against the base function, and update the active scope and prototype.
        const auto new_fn_proto_ptr = new_fn_proto.get();
        fn_proto->RegisteredGenericSubstitutions().back().Second = std::move(new_fn_proto);
        return std::make_tuple(new_fn_proto_ptr, new_fn_scope);
    }

    return std::make_tuple(fn_proto, fn_scope);
}


auto spp::analyse::utils::overload_utils::ManageMatchedOverloads(
    asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
    Vec<PassOverloadInfo> const &pass_overloads,
    Vec<FailOverloadInfo> const &fail_overloads,
    asts::FunctionCallArgumentGroupAst const &arg_group,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    // If there are no pass overloads, raise an error.
    using namespace std::string_literals;
    if (pass_overloads.IsEmpty()) {
        auto failed_signatures_and_errors = "\n" + (fail_overloads
            | genex::views::transform([](auto const &f) { return "    - "s + std::get<1>(f)->PrintSignature("") + ": "s + std::get<3>(f); })
            | genex::views::intersperse("\n"s)
            | genex::views::join
            | genex::to<Str>());

        auto arg_usage_signature = arg_group.Args
            | genex::views::transform([sm, meta](auto const &x) { return x->GetSelfType() == nullptr ? x->InferType(sm, meta)->ToString() : "Self"; })
            | genex::views::intersperse(", "s)
            | genex::views::join
            | genex::to<Str>();

        Raise<errors::SppFunctionCallNoValidSignaturesError>(
            {sm->CurrentScope}, ERR_ARGS(fn_call, failed_signatures_and_errors, arg_usage_signature));
    }

    // If there are multiple pass overloads, raise an error.
    // std::get<0>(x)->ast != nullptr ? static_cast<Str>(*ast_name(std::get<0>(x)->AstNode)) : ""
    if (pass_overloads.Len() > 1) {
        auto signatures = "\n" + (pass_overloads
            | genex::views::transform([](auto const &x) { return "    - "s + std::get<1>(x)->PrintSignature(""); })
            | genex::views::intersperse("\n"s)
            | genex::views::join
            | genex::to<Str>());

        auto arg_usage_signature = arg_group.Args
            | genex::views::transform([sm, meta](auto const &x) { return x->GetSelfType() == nullptr ? x->InferType(sm, meta)->ToString() : "Self"; })
            | genex::views::intersperse(", "s)
            | genex::views::join
            | genex::to<Str>();

        Raise<errors::SppFunctionCallOverloadAmbiguousError>(
            {sm->CurrentScope}, ERR_ARGS(fn_call, signatures, arg_usage_signature));
    }
}


auto spp::analyse::utils::overload_utils::ValidateArgsMatchParams(
    asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
    asts::FunctionPrototypeAst const &fn_proto,
    scopes::Scope const *fn_scope,
    asts::FunctionCallArgumentGroupAst const &func_args,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void {
    //
    using errors::SppArgumentNameInvalidError;
    using errors::SppArgumentMissingError;
    using errors::SppTypeMismatchError;
    using type_utils::TypeEq;
    using type_utils::RelaxedTypeEq;

    // Check any params are void, pop them (indexes because of unique pointers).
    for (auto &&i : genex::views::iota(0uz, fn_proto.FnParamGroup->Params.Len())) {
        if (type_utils::IsTypeVoid(*fn_proto.FnParamGroup->Params[i]->Type, *fn_scope)) {
            genex::actions::erase(fn_proto.FnParamGroup->Params, fn_proto.FnParamGroup->Params.begin() + (i as SSize));
        }
    }

    // Recreate the lists of function parameters, and their names (Void removed, generics etc).
    const auto func_params = fn_proto.FnParamGroup.get();
    const auto func_param_names = fn_proto.FnParamGroup->Params
        | genex::views::transform([](auto &&x) { return x->ExtractName(); })
        | genex::to<Vec>();
    const auto func_param_names_req = fn_proto.FnParamGroup->GetRequiredParams()
        | genex::views::transform([](auto &&x) { return x->ExtractName(); })
        | genex::to<Vec>();
    auto func_arg_names = func_args.GetKeywordArgs()
        | genex::views::transform([](auto const &x) { return x->Name.get(); })
        | genex::to<Vec>();

    // Check for any keyword arguments that don't have a corresponding parameter.
    // Todo: Can we use: "analyse::utils::func_utils::enforce_no_invalid_fn_args()"?
    const auto invalid_args = func_arg_names
        | genex::views::not_in(func_param_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();
    RaiseIf<SppArgumentNameInvalidError>(
        not invalid_args.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*func_params->Params[0], "parameter", *invalid_args[0], "argument"));

    // Check for missing parameters that don't have a corresponding argument.
    const auto missing_params = func_param_names_req
        | genex::views::not_in(func_arg_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();
    RaiseIf<SppArgumentMissingError>(
        not missing_params.IsEmpty(), {sm->CurrentScope},
        ERR_ARGS(*missing_params[0], "parameter", fn_call, "argument"));

    // Type check the arguments against the parameters. Sort the arguments into parameter order first.
    auto sorted_func_arguments = func_args.GetKeywordArgs();
    genex::actions::sort(
        sorted_func_arguments,
        {}, [&](asts::FunctionCallArgumentKeywordAst *arg) { return genex::position(func_param_names, [&arg](auto const &param) { return *arg->Name == *param; }); });

    for (auto [arg, param] : genex::views::zip(sorted_func_arguments, func_params->GetAllParams())) {
        auto p_type = fn_scope->GetTypeSymbol(param->Type)->FqName()->WithConvention(AstClone(param->Type->GetConvention()));
        auto a_type = arg->InferType(sm, meta);
        auto temp = type_utils::GenericInferenceMap();

        // Special case for variadic parameters (updates p_type so don't follow with "else if").
        if (param->To<asts::FunctionParameterVariadicAst>()) {
            auto ts = Vec(a_type->TypeParts().Back()->GnArgGroup->Args.Len(), p_type);
            p_type = asts::generate::common_types::TupleType(param->PosStart(), std::move(ts));
            p_type->Stage7_AnalyseSemantics(sm, meta);
        }

        // Special case for "self" parameters.
        if (const auto self_param = param->To<asts::FunctionParameterSelfAst>(); self_param != nullptr) {
            arg->Conv = asts::AstClone(self_param->Conv);
        }

        // Regular parameter without arg folding. The double check is required for generics applied to the
        // superclass in sup-ext that cannot be substituted because they can be anything, so reverse type check
        // them with the "relaxed" variation. This is the only place this is required. Check testing with type
        // aliases to be sure.
        // Todo: This is an issue bcoz prototypes still contain generics if they are from the sup block.
        // Todo: Only an issue in final codegen.
        else if (not TypeEq(*p_type, *a_type, *fn_scope, *sm->CurrentScope)) {
            RaiseIf<SppTypeMismatchError>(
                not RelaxedTypeEq(*a_type, *p_type, *sm->CurrentScope, *fn_scope, temp),
                {fn_scope, sm->CurrentScope}, ERR_ARGS(*param, *p_type, *arg, *a_type));
        }
    }
}
