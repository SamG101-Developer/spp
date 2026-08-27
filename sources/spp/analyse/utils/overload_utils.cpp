module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.overload_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.generic_bindings;
import spp.analyse.utils.monomorphization_utils;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.generic_parameter_type_optional_ast;
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
import std;
import sys;

#define SPP_FN_RES_ERR_WRAPPER(error_type, message)                          \
  catch (error_type const &e) {                                              \
    fail_overloads.EmplaceBack(FailedOverload{fn_proto, e.what(), message}); \
    while (meta->Depth() > original_meta_depth) { meta->Restore(); }         \
  }

namespace {
  auto NamedArgsOnly(
    spp::Vec<spp::Unique<spp::asts::GenericArgumentAst>> &&args)
    -> spp::Vec<spp::Unique<spp::asts::GenericArgumentAst>> {
    //
    using namespace spp::asts;
    auto out = spp::Vec<spp::Unique<GenericArgumentAst>>();
    for (auto &&arg : args) {
      const auto named = arg->To<GenericArgumentTypeKeywordAst>() != nullptr
        or arg->To<GenericArgumentCompKeywordAst>() != nullptr;
      if (named) { out.EmplaceBack(std::move(arg)); }
    }
    return out;
  }

  /**
   * Determine whether a (stripped) parameter type refers to a generic that is "rigid" at the call site: ie a
   * generic parameter belonging to a scope that encloses the caller, and so is already fixed rather than being
   * inferred/substituted for this particular call.
   *
   * When a parameter's type is such a rigid generic (eg calling @code slice_ref(from: I, into: I)@endcode from within
   * a method of @code sup [V, I] SliceRef[V, I]@endcode), the argument must match that generic exactly. This is
   * different from the "matches anything" behaviour of @code RelaxedTypeEq@endcode, which is only appropriate when a
   * generic is genuinely free to be inferred for the call (eg a non-substitutable superclass generic in a sup-ext
   * block, which is not visible as a generic from the caller's scope).
   */
  auto IsRigidGenericAtCaller(
    spp::asts::TypeAst const &param_type,
    spp::analyse::scopes::Scope const &caller_scope)
    -> bool {
    const auto stripped = param_type.WithoutGenerics()->WithoutConvention();
    const auto sym = caller_scope.GetTypeSymbol(stripped.get());
    return sym != nullptr and sym->IsGeneric;
  }

  /**
   * Whether a name that @c RelaxedTypeEq had to bind in order to match is one the caller cannot choose. The same
   * rigidity test as @c IsRigidGenericAtCaller, applied to the bindings the relaxed match produced rather than to the
   * parameter's head type, so that a generic appearing *inside* a parameter type is covered too: the "w" of
   * @code that: &SizedInteger[w=w, signed=false]@endcode is fixed by whoever instantiated the enclosing block, even
   * though @c SizedInteger itself is not generic and the head-type test therefore says nothing about it.
   *
   * A generic that is genuinely free for the call - the callee's own parameter, or a superclass generic in a sup-ext
   * block that is not visible from the caller - is not found as a generic here, so the relaxed match keeps working
   * for the cases it exists to serve.
   */
  auto IsRigidBindingAtCaller(
    spp::asts::TypeIdentifierAst const &bound_name,
    spp::analyse::scopes::Scope const &caller_scope)
    -> bool {
    if (const auto type_sym = caller_scope.GetTypeSymbol(&bound_name); type_sym != nullptr) {
      return type_sym->IsGeneric;
    }
    const auto as_id = spp::asts::IdentifierAst::FromType(bound_name);
    const auto comp_sym = caller_scope.GetVarSymbol(as_id.get());
    return comp_sym != nullptr and comp_sym->IsGeneric;
  }

  /**
   * Whether a prototype's signature is written in terms of @c Self , and so reads differently per implementer.
   */
  auto SignatureNamesSelf(
    spp::asts::FunctionPrototypeAst const &fn_proto)
    -> bool {
    const auto names_self = [](spp::asts::TypeAst const &type) {
      return genex::any_of(type.Iterator(), [](auto const &part) { return part->Name == "Self"; });
    };
    return names_self(*fn_proto.ReturnType)
      or genex::any_of(fn_proto.FnParamGroup->GetNonSelfParams(), [&](auto const *p) { return names_self(*p->Type); });
  }
}

auto spp::analyse::utils::overload_utils::DetermineOverload(
  asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> Pair<PassedOverload, bool> {
  //
  using scopes::ScopeManager;
  using errors::SppFunctionCallTooManyArgumentsError;
  using type_utils::TypeEq;
  using type_utils::ResolveAndSubstituteSelfType;
  using func_utils::GetFuncOwnerTypeAndFuncName;

  auto lhs = meta->PostfixExpressionLhs;

  // Todo: Workaround for aliased variable symbols being used
  //  as function targets, due to scope lookup.
  auto temp = Shared<asts::ExpressionAst>(nullptr);
  if (const auto id = lhs->To<asts::IdentifierAst>()) {
    const auto x = sm->CurrentScope->GetVarSymbol(id);
    if (x and x->MemInfo->AstCompTime) {
      temp = x->FqName();
      lhs = temp.get();
    }
  }

  // Extract metadata about the target function's overloads
  // such as the function's owner and scope.
  const auto [fn_owner_type, fn_owner_scope, fn_name] = GetFuncOwnerTypeAndFuncName(
    *lhs, *sm, meta);

  const auto is_postfix = meta->PostfixExpressionLhs->To<asts::PostfixExpressionAst>();
  const auto is_runtime = is_postfix
    ? is_postfix->Op->To<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>()
    : nullptr;

  // If we are resolving a method, then use the free function
  // equivalent. For example, convert "1.add(2)" to
  // "S32::add(1, 2)".
  if (is_runtime != nullptr) {
    auto propagated = PropagateMethodToFunction(
      fn_call, *fn_owner_type, *fn_name, *is_postfix, sm, meta);
    fn_call.SetTransformedAst(std::move(propagated.TransformedAst));
    return {std::move(propagated.Overload), propagated.IsClosure};
  }

  // Get all the overloads to deal with, and handle closure
  // mechanics.
  auto candidates = RetrieveAllOverloads(
    fn_name.get(), *fn_owner_scope, sm, meta);
  auto pass_overloads = Vec<PassedOverload>{};
  auto fail_overloads = Vec<FailedOverload>{};
  auto original_meta_depth = meta->Depth();

  // Check each provided overload for a complete match.
  for (auto &candidate : candidates.Overloads) {
    auto *&fn_proto = candidate.Proto;
    auto const *&fn_scope = candidate.FnScope;

    // Extract generic/function parameter information from the
    // overload.
    const auto fn_params = fn_proto->FnParamGroup.get();
    const auto gn_params = fn_proto->GnParamGroup.get();
    auto fn_args = asts::AstClone(fn_call.FnArgGroup);
    auto gn_args = asts::AstClone(fn_call.GnArgGroup);
    const auto is_variadic_fn =
      fn_proto->FnParamGroup->GetVariadicParams() != nullptr;

    try {
      // Cannot check for "too few" arguments here because of
      // potential "T=Void" + "x: T" removal. Check if there
      // are too many arguments (for a non-variadic function).
      RaiseIf<SppFunctionCallTooManyArgumentsError>(
        fn_args->Args.Len() > fn_params->Params.Len() and not is_variadic_fn, {fn_scope},
        ERR_ARGS(*fn_proto, fn_proto->FnParamGroup->Params.Len(), fn_call, fn_call.FnArgGroup->Args.Len()));

      // Every generic argument this call is resolved with, merged
      // once, here. Precedence runs highest first: what the call
      // wrote, then what the owner type (or the forwarding type
      // it was reached through) pins, then what the enclosing "sup"
      // block declares - the first binding offered for a name wins.
      generic_bindings::NameGnArgs(*gn_args, *gn_params, *fn_proto->Name, *sm, *meta);
      gn_args->MergeGenerics(RetrieveOwnerGenericArgs(candidate.FwdType, meta));
      gn_args->MergeGenerics(std::move(candidate.SupGenerics->Args));

      // Todo: Heavily document this, but effectively, it allows the Writer::write_all() to be used, calling
      //  self.write(), but using the *implementers* write() method, not the abstract one on Write.
      const auto declared_self = fn_scope->GetEnclosingSelfType(*meta);
      const auto declared_self_sym = declared_self != nullptr
        ? fn_scope->GetTypeSymbol(declared_self.get())
        : nullptr;
      const auto declared_on_abstract = declared_self_sym != nullptr and declared_self_sym->LinkedScope != nullptr
        and not type_utils::GetUnimplementedAbstractMethods(*declared_self_sym->LinkedScope).IsEmpty();

      auto self_pin = Shared<asts::TypeAst>(nullptr);
      if (declared_self != nullptr and (SignatureNamesSelf(*fn_proto) or declared_on_abstract)) {
        auto receiver = fn_owner_type->WithConvention(nullptr);

        // "Self" only stands for the receiver when the receiver
        // really is an implementer of the class the method was
        // declared on. For example, avoid forwarding types
        // incorrectly triggering this.
        const auto receiver_sym = sm->CurrentScope->GetTypeSymbol(receiver->WithoutGenerics().get());
        const auto receiver_implements_declarer = receiver_sym != nullptr and receiver_sym->LinkedScope != nullptr
          and genex::any_of(receiver_sym->LinkedScope->SupTypes(), [&](auto const &sup) {
            return TypeEq(*declared_self, *sup, *fn_scope, *receiver_sym->LinkedScope);
          });

        if (receiver_implements_declarer and not TypeEq(*declared_self, *receiver, *fn_scope, *sm->CurrentScope)) {
          if (not receiver->IsSelfType() and not receiver_sym->IsGeneric) {
            self_pin = receiver;
          }
          auto self_arg = Vec<Unique<asts::GenericArgumentAst>>();
          self_arg.EmplaceBack(MakeUnique<asts::GenericArgumentTypeKeywordAst>(
            asts::generate::common_types::SelfType(0), nullptr, std::move(receiver)));
          gn_args->MergeGenerics(std::move(self_arg));
        }
      }

      InferAllGenerics(
        *fn_proto, *fn_params, *fn_args, *gn_args, is_variadic_fn, fn_scope, sm, meta);

      // Inference rebuilds the argument list from the prototype's
      // generic parameters, so the pin above - whose name is not
      // one of them - is dropped on the way out. Put it back.
      if (self_pin != nullptr) {
        auto self_arg = Vec<Unique<asts::GenericArgumentAst>>();
        self_arg.EmplaceBack(MakeUnique<asts::GenericArgumentTypeKeywordAst>(
          asts::generate::common_types::SelfType(0), nullptr, std::move(self_pin)));
        gn_args->MergeGenerics(std::move(self_arg));
      }

      // "InferAllGenerics" has run "NameFnArgs", so the trailing
      // arguments of a variadic call are already collapsed into one
      // tuple-valued argument. Its type is what the callee's variadic
      // parameter actually receives.
      auto variadic_pack_type = Shared<asts::TypeAst>(nullptr);
      if (is_variadic_fn) {
        const auto variadic_name = fn_proto->FnParamGroup->GetVariadicParams()->ExtractName();
        for (auto const &a : fn_args->GetKeywordArgs()) {
          if (a->Name->Val != variadic_name->Val) { continue; }
          variadic_pack_type = a->Val->InferType(sm, meta);
          break;
        }
      }

      std::tie(fn_proto, fn_scope) = PotentiallyGenerateGenericSubstitutedPrototype(
        fn_proto, fn_scope, *gn_args, variadic_pack_type, sm, meta);
      ValidateArgsMatchParams(
        fn_call, *fn_proto, fn_scope, *fn_args, sm, meta);
      pass_overloads.EmplaceBack(
        PassedOverload{fn_scope, fn_proto, std::move(fn_args)});
    }

    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppFunctionCallAbstractFunctionError, "calling an abstract function")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppFunctionCallTooManyArgumentsError, "too many arguments")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppArgumentNameInvalidError, "invalid argument name")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppArgumentMissingError, "missing required argument")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppTypeMismatchError, "type mismatch")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppGenericParameterConflictError, "inferred generic parameter conflict")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppGenericParameterNotInferredError, "generic parameter not inferred")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppGenericArgumentTooManyError, "too many generic arguments")
    SPP_FN_RES_ERR_WRAPPER(analyse::errors::SppGenericConstraintError, "generic constraint not satisfied")
  }

  // Perform the return type overload selection separately
  // here, for error reasons.
  if (meta->ReturnTypeOverloadResolverType != nullptr) {
    auto return_matches = Vec<PassedOverload>();
    for (auto &&matched : pass_overloads) {
      auto ret = asts::AstCloneShared(matched.Proto->ReturnType);
      auto tm = ScopeManager(sm->GlobalScope, const_cast<scopes::Scope*>(matched.FnScope));
      ret = ResolveAndSubstituteSelfType(*ret, *matched.FnScope, tm, *meta);

      if (TypeEq(*ret, *meta->ReturnTypeOverloadResolverType, *matched.FnScope, *sm->CurrentScope)) {
        return_matches.EmplaceBack(std::move(matched));
      }
    }

    // If there is only one return-type match, select it
    // into the pass overloads.
    if (return_matches.Len() == 1) {
      pass_overloads = std::move(return_matches);
    }
  }

  ManageMatchedOverloads(
    fn_call, pass_overloads, fail_overloads,
    *fn_call.FnArgGroup, sm, meta);

  // Store the closure if it was generated as part of the
  // overload resolution.
  if (candidates.ClosureProto) {
    fn_call.SetClosureDummyProto(std::move(candidates.ClosureProto));
  }
  return {std::move(pass_overloads[0]), candidates.IsClosure};
}

auto spp::analyse::utils::overload_utils::PropagateMethodToFunction(
  asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
  asts::TypeAst const &fn_owner_type,
  asts::IdentifierAst const &fn_name,
  asts::PostfixExpressionAst const &cast_lhs,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> PropagatedMethodCall {
  //
  using func_utils::ConvertMethodToFuncForm;

  // Get the function conversion of the method (free
  // function with self argument).
  auto [transformed_lhs, transformed_fn_call] = ConvertMethodToFuncForm(
    fn_owner_type, fn_name, cast_lhs, fn_call, *sm, meta);

  // Determine the overload based off the function
  // (uniform system).
  meta->Save();
  meta->PostfixExpressionLhs = transformed_lhs.get();
  auto [overload, is_closure] = DetermineOverload(
    *transformed_fn_call, sm, meta);
  meta->Restore();

  // Get the argument group with the "self" injection,
  // and bind it to the function call.
  fn_call.FnArgGroup = asts::AstClone(transformed_fn_call->FnArgGroup);

  // Create a mock postfix based on the transformation.
  transformed_lhs->Stage7_AnalyseSemantics(sm, meta);
  auto pf = MakeUnique<asts::PostfixExpressionAst>(
    std::move(transformed_lhs), std::move(transformed_fn_call));
  return PropagatedMethodCall{std::move(overload), is_closure, std::move(pf)};
}

auto spp::analyse::utils::overload_utils::RetrieveAllOverloads(
  asts::IdentifierAst const *fn_name,
  scopes::Scope const &fn_owner_scope,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> OverloadCandidates {
  //
  using func_utils::IsTargetCallable;
  using func_utils::CreateCallablePrototype;

  // For named functions (ie non-closures), get all the
  // function overload implementation scopes.
  auto all_overloads = fn_name
    ? func_utils::GetAllFunctionScopes(*fn_name, &fn_owner_scope, *sm, meta)
    : Vec<func_utils::FunctionOverload>{};
  if (not all_overloads.IsEmpty()) {
    return OverloadCandidates{false, nullptr, std::move(all_overloads)};
  }

  // If there are no scopes, assume that this is a closure
  // (do functional type check).
  const auto closure_fn_type = IsTargetCallable(*meta->PostfixExpressionLhs, *sm, meta);
  if (closure_fn_type != nullptr) {
    auto closure_fn_proto = CreateCallablePrototype(*closure_fn_type);
    all_overloads.EmplaceBack(func_utils::FunctionOverload{
      .FnScope = sm->CurrentScope,
      .Proto = closure_fn_proto.get(),
      .SupGenerics = asts::GenericArgumentGroupAst::NewEmpty(),
      .FwdType = nullptr
    });
    return OverloadCandidates{true, std::move(closure_fn_proto), std::move(all_overloads)};
  }

  // Otherwise, there are no scopes (handled in caller).
  return OverloadCandidates{false, nullptr, {}};
}

auto spp::analyse::utils::overload_utils::RetrieveOwnerGenericArgs(
  Shared<asts::TypeAst> const &fwd_type,
  asts::meta::CompilerMetaData const *meta)
  -> Vec<Unique<asts::GenericArgumentAst>> {
  // A forwarding type stands in for the owner, so its
  // generics are the ones that count.
  if (fwd_type != nullptr) {
    return NamedArgsOnly(std::move(fwd_type->LastTypePart()->GnArgGroup->Args));
  }

  // Otherwise take them from the type the call was made
  // on, if it was made on one at all - a module-level
  // function has no owner to inherit from.
  const auto is_postfix = meta->PostfixExpressionLhs->To<asts::PostfixExpressionAst>();
  const auto is_type = is_postfix ? asts::AstCloneShared(is_postfix->Lhs->To<asts::TypeAst>()) : nullptr;
  if (is_type != nullptr) {
    return NamedArgsOnly(std::move(is_type->LastTypePart()->GnArgGroup->Args));
  }

  return {};
}

auto spp::analyse::utils::overload_utils::InferAllGenerics(
  asts::FunctionPrototypeAst const &fn_proto,
  asts::FunctionParameterGroupAst const &fn_params,
  asts::FunctionCallArgumentGroupAst &fn_args,
  asts::GenericArgumentGroupAst &gn_args,
  const bool is_variadic_fn,
  scopes::Scope const *fn_scope,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> void {
  //
  using generic_bindings::EnforceGenericConstraintsAllArgs;
  using generic_bindings::InferGnArgs;
  using func_utils::NameFnArgs;

  // Name the positional function arguments. The generic arguments
  // were named by the caller, which has to do it before it merges
  // the owner's and the "sup" block's arguments in.
  NameFnArgs(fn_args, fn_params, *sm, gn_args.GetAllArgs());

  // The inference source is all the function arguments (except for
  // "self")
  auto generic_infer_source = fn_args.GetKeywordArgs()
    | genex::views::remove_if([](auto const &a) { return a->Name->Val == "self"; })
    | genex::views::transform([&](auto const &x) { return MakePair(x->Name, x->Val->InferType(sm, meta)); })
    | genex::to<Vec>();

  // The inference target is all of the function parameters (except
  // for "self").
  auto generic_infer_target = fn_params.GetNonSelfParams()
    | genex::views::transform([](auto *x) { return MakePair(x->ExtractName(), x->Type); })
    | genex::to<Vec>();

  // Infer all of the generics from the function arguments and
  // parameters.
  InferGnArgs(
    *fn_proto.GnParamGroup, gn_args,
    MakeShared<generic_bindings::InferenceSourceMap>(
      generic_infer_source.begin(), generic_infer_source.end()),
    MakeShared<generic_bindings::InferenceTargetMap>(
      generic_infer_target.begin(), generic_infer_target.end()),
    meta->PostfixExpressionLhs->InferType(sm, meta),
    *fn_scope,
    is_variadic_fn ? fn_proto.FnParamGroup->GetVariadicParams()->ExtractName() : nullptr,
    false, *sm, *meta);

  EnforceGenericConstraintsAllArgs(
    *fn_proto.GnParamGroup, gn_args, *fn_scope, *sm, *meta);
}

auto spp::analyse::utils::overload_utils::PotentiallyGenerateGenericSubstitutedPrototype(
  asts::FunctionPrototypeAst *fn_proto,
  scopes::Scope const *fn_scope,
  asts::GenericArgumentGroupAst &generic_args,
  Shared<asts::TypeAst> const &variadic_pack_type,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> Tup<asts::FunctionPrototypeAst*, scopes::Scope const*> {
  //
  using errors::SppSecondClassBorrowViolationError;
  using monomorphization_utils::CreateGenericFunScope;
  using type_utils::IsTypeBorrowed;

  // Inference has already produced a binding for every one of
  // this prototype's generic parameters, including the ones it
  // inherited from the enclosing "sup" block.
  auto &combined_generics = generic_args;

  // An unbound parameter is what it looks like and is left alone:
  // its symbol carries no class prototype, which is what separates
  // it from a parameter bound to a real type.
  for (auto *arg : combined_generics.Args
       | genex::views::ptr
       | genex::views::cast_dynamic<asts::GenericArgumentTypeKeywordAst*>()) {
    const auto val_sym = sm->CurrentScope->GetTypeSymbol(arg->Val.get());
    if (val_sym == nullptr or not val_sym->IsGeneric or val_sym->Type == nullptr) { continue; }
    if (val_sym->LinkedScope == nullptr or val_sym->LinkedScope->TySym == nullptr) { continue; }
    arg->Val = val_sym->LinkedScope->TySym->FqName();
  }

  // The same for a comp-time argument naming a bound comp generic.
  // A binding is a variable symbol carrying the argument it was
  // bound from, so what the name resolves to is read back off that;
  // an unbound parameter carries nothing and is left alone.
  for (auto *arg : combined_generics.Args
       | genex::views::ptr
       | genex::views::cast_dynamic<asts::GenericArgumentCompKeywordAst*>()) {
    const auto val_ident = arg->Val->To<asts::IdentifierAst>();
    if (val_ident == nullptr) { continue; }

    const auto val_sym = sm->CurrentScope->GetVarSymbol(val_ident);
    if (val_sym == nullptr or val_sym->MemInfo->AstCompTime == nullptr) { continue; }

    const auto bound_arg = val_sym->MemInfo->AstCompTime->To<asts::GenericArgumentCompKeywordAst>();
    if (bound_arg == nullptr or bound_arg->Val == nullptr) { continue; }
    arg->Val = asts::AstClone(bound_arg->Val);
  }

  // Drop the arguments that only restate their parameter. What
  // is left is what this instantiation actually pins, and if
  // that is nothing then there is no instantiation to make.
  combined_generics.Args |= genex::actions::remove_if(
    [](auto const &a) { return generic_bindings::BindsToItself(*a); });

  // Separate variadic instantiation by the types going into the
  // variadic function parameter.
  if (variadic_pack_type != nullptr) {
    auto pack_name = MakeUnique<asts::TypeIdentifierAst>(
      variadic_pack_type->PosStart(),
      "VariadicPackOf" + fn_proto->FnParamGroup->GetVariadicParams()->ExtractName()->Val,
      nullptr);
    combined_generics.Args.EmplaceBack(MakeUnique<asts::GenericArgumentTypeKeywordAst>(
      std::move(pack_name), nullptr, asts::AstClone(variadic_pack_type)));
  }

  // Consider if we need to create a generic substituted
  // function prototype.
  if (not combined_generics.Args.IsEmpty()) {
    // Reuse the instantiation for these exact arguments if one
    // already exists.
    if (auto [existing_scope, existing_proto] = fn_proto->FindGenericSubstitution(combined_generics);
      existing_proto != nullptr) {
      return {existing_proto, existing_scope};
    }

    auto new_fn_proto = asts::AstClone(fn_proto);
    new_fn_proto->SetNonGenericImpl(fn_proto);
    new_fn_proto->DetachLlvmFuncSlot();

    // Create the new function scope for the generic implementation.
    const auto generic_syms = sm->CurrentScope->GetExtendedGenericSymbols(combined_generics.GetAllArgs());
    const auto new_fn_scope = CreateGenericFunScope(
      *fn_scope, asts::GenericArgumentGroupAst(nullptr, AstCloneVec(combined_generics.Args), nullptr),
      generic_syms, sm, meta);
    auto tm = scopes::ScopeManager(sm->GlobalScope, new_fn_scope);

    // Drop only the parameters this substitution actually bound.
    new_fn_proto->GnParamGroup->Params |= genex::actions::remove_if([&](auto const &param) {
      return genex::any_of(combined_generics.Args, [&](auto const &arg) {
        return arg->ViewName() == param->Name->ToString();
      });
    });

    auto &generic_sub_slot = asts::AstBody(
      fn_scope->AstNode)[0]->To<asts::FunctionPrototypeAst>()->RegisteredGenericSubstitutions().back();

    // Substitute and analyse the function parameters and return
    // type.
    for (auto *p : new_fn_proto->FnParamGroup->GetNonSelfParams()) {
      p->Type = p->Type->SubstituteGenerics(combined_generics.GetAllArgs());
      p->Type->Stage7_AnalyseSemantics(&tm, meta);
    }

    // A parameter declared against a generic keeps being callable
    // through whatever its constraint promised, even though its type
    // has just been rewritten to the argument. "F: FunMov" says the
    // body may consume it once, and a "FunMut" satisfies that while
    // also being callable through a borrow - so reading the substituted
    // type would make this body consume the value here and borrow it
    // in the next instantiation, which linear ownership cannot account
    // for. Recorded against the symbol because the constraint lives on
    // the template's generic parameter, which nothing in the
    // instantiation refers to any more.
    for (auto *p : new_fn_proto->FnParamGroup->GetNonSelfParams()) {
      const auto declared = p->Source.OriginalType;
      if (declared == nullptr) { continue; }

      const auto gn_param = genex::find_if(
        fn_proto->GnParamGroup->Params, [&](auto const &g) { return *g->Name == *declared; });
      if (gn_param == fn_proto->GnParamGroup->Params.end()) { continue; }

      const auto constraints = (*gn_param)->To<asts::GenericParameterTypeAst>();
      if (constraints == nullptr or constraints->Constraints == nullptr) { continue; }

      for (auto const &c : constraints->Constraints->Constraints) {
        if (not type_utils::IsTypeFunc(*c, *new_fn_scope)) { continue; }
        const auto sym = new_fn_scope->Children[0]->GetVarSymbol(p->ExtractName().get(), true);
        if (sym == nullptr) { break; }

        // Substituted the same way the parameter's own type is:
        // the constraint is written in the template's terms
        // ("FunMov[(T,), U]"), and what the call needs is this
        // instantiation's argument and return types.
        auto callable = c->SubstituteGenerics(combined_generics.GetAllArgs());
        callable->Stage7_AnalyseSemantics(&tm, meta);
        sym->CallableAsType = std::move(callable);
        break;
      }
    }

    // "self" is typed as "Self", so only a substitution that pins
    // "Self" to the receiver has anything to rewrite here. The
    // symbol bound from the type has to be rewritten with it: this
    // clone inherited the template's, and Stage6, which is what sets
    // it, only ever runs on the template. Retypes the "self" symbol
    // basically.
    if (const auto self_param = new_fn_proto->FnParamGroup->GetSelfParam(); self_param != nullptr) {
      auto substituted_self = self_param->Type->SubstituteGenerics(combined_generics.GetAllArgs());
      if (not substituted_self->IsSelfType()) {
        substituted_self->Stage7_AnalyseSemantics(&tm, meta);
        self_param->Type = substituted_self;
        const auto self_name = self_param->ExtractName();
        if (const auto self_sym = new_fn_scope->Children[0]->GetVarSymbol(self_name.get(), true);
          self_sym != nullptr) {
          self_sym->Type = substituted_self->WithConvention(asts::AstClone(self_param->Conv));
        }
      }
    }
    new_fn_proto->VariadicPackType = asts::AstClone(variadic_pack_type);

    // A variadic parameter declares one element ("..b: T") but
    // binds the whole tuple the call collapsed its trailing
    // arguments into. Retyped here for the same reason "self"
    // is above: this clone inherited the template's symbol, and
    // stage 6, which types it, only runs on the template.
    if (variadic_pack_type != nullptr) {
      auto pack_type = asts::AstClone(variadic_pack_type);
      pack_type->Stage7_AnalyseSemantics(&tm, meta);
      const auto pack_name = new_fn_proto->FnParamGroup->GetVariadicParams()->ExtractName();
      if (const auto pack_sym = new_fn_scope->Children[0]->GetVarSymbol(pack_name.get(), true);
        pack_sym != nullptr) {
        pack_sym->Type = std::move(pack_type);
      }
    }

    new_fn_proto->ReturnType = new_fn_proto->ReturnType->SubstituteGenerics(combined_generics.GetAllArgs());
    new_fn_proto->ReturnType->Stage7_AnalyseSemantics(&tm, meta);

    // Check the new return type isn't a borrow type.
    RaiseIf<SppSecondClassBorrowViolationError>(
      IsTypeBorrowed(*new_fn_proto->ReturnType, tm),
      {sm->CurrentScope},
      ERR_ARGS(*new_fn_proto->ReturnType, *new_fn_proto->ReturnType, "substituted function return type"));

    //
    const auto type_is_concrete = [&](asts::TypeAst const &type) {
      const auto resolved = type_utils::ResolveAndSubstituteSelfType(type, *new_fn_scope, tm, *meta);
      return type_utils::IsTypeFullyConcrete(*resolved, *new_fn_scope);
    };

    generic_sub_slot.IsConcrete =
      genex::all_of(combined_generics.Args | genex::views::ptr, [&](auto const *arg) {
        if (const auto type_arg = arg->template To<asts::GenericArgumentTypeAst>(); type_arg != nullptr) {
          return type_utils::IsTypeFullyConcrete(*type_arg->Val, *sm->CurrentScope);
        }
        if (const auto comp_arg = arg->template To<asts::GenericArgumentCompAst>(); comp_arg != nullptr) {
          return comp_arg->Val->template To<asts::IdentifierAst>() == nullptr;
        }
        return true;
      })
      and type_is_concrete(*new_fn_proto->ReturnType)
      and genex::all_of(new_fn_proto->FnParamGroup->GetAllParams(), [&](auto const *p) {
        return type_is_concrete(*p->Type);
      });

    // Save the generic implementation against the base function,
    // and update the active scope and prototype.
    const auto new_fn_proto_ptr = new_fn_proto.get();
    generic_sub_slot.Proto = std::move(new_fn_proto);
    return {new_fn_proto_ptr, new_fn_scope};
  }

  return {fn_proto, fn_scope};
}

auto spp::analyse::utils::overload_utils::ManageMatchedOverloads(
  asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
  Vec<PassedOverload> const &pass_overloads,
  Vec<FailedOverload> const &fail_overloads,
  asts::FunctionCallArgumentGroupAst const &arg_group,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> void {
  // If there are no pass overloads, raise an error.
  using namespace std::string_literals;
  if (pass_overloads.IsEmpty()) {
    auto failed_signatures_and_errors = "\n" + (fail_overloads
      | genex::views::transform([](auto const &f) {
        return "    - "s + f.Proto->PrintSignature("") + ": "s + f.Reason;
      })
      | genex::views::intersperse("\n"_str)
      | genex::views::join
      | genex::to<Str>());

    auto arg_usage_signature = arg_group.Args
      | genex::views::transform([sm, meta](auto const &x) {
        return x->GetSelfType() == nullptr ? x->InferType(sm, meta)->ToString() : "Self";
      })
      | genex::views::intersperse(", "_str)
      | genex::views::join
      | genex::to<Str>();

    auto sub_errors = fail_overloads
      | genex::views::transform([](auto const &f) { return f.Error; })
      | genex::to<Vec>();

    Raise<errors::SppFunctionCallNoValidSignaturesError>(
      {sm->CurrentScope}, ERR_ARGS(fn_call, failed_signatures_and_errors, arg_usage_signature),
      std::move(sub_errors));
  }

  // If there are multiple pass overloads, raise an error.
  if (pass_overloads.Len() > 1) {
    auto signatures = "\n" + (pass_overloads
      | genex::views::transform([](auto const &x) { return "    - "s + x.Proto->PrintSignature(""); })
      | genex::views::intersperse("\n"_str)
      | genex::views::join
      | genex::to<Str>());

    auto arg_usage_signature = arg_group.Args
      | genex::views::transform([sm, meta](auto const &x) {
        return x->GetSelfType() == nullptr ? x->InferType(sm, meta)->ToString() : "Self";
      })
      | genex::views::intersperse(", "_str)
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

  // Check any params are "Void", pop them (indexes because
  // of unique pointers).
  for (auto &&i : genex::views::iota(0uz, fn_proto.FnParamGroup->Params.Len())) {
    if (type_utils::IsTypeVoid(*fn_proto.FnParamGroup->Params[i]->Type, *fn_scope)) {
      genex::actions::erase(
        fn_proto.FnParamGroup->Params, fn_proto.FnParamGroup->Params.begin() + static_cast<sys::ssize_t>(i));
    }
  }

  // Recreate the lists of function parameters, and their
  // names ("Void" removed, generics etc).
  const auto func_params = fn_proto.FnParamGroup.get();
  const auto func_param_names = fn_proto.FnParamGroup->Params
    | genex::views::transform([](auto &&x) { return x->ExtractName(); })
    | genex::to<Vec>();
  const auto func_param_names_req = fn_proto.FnParamGroup->GetRequiredParams()
    | genex::views::transform([](auto &&x) { return x->ExtractName(); })
    | genex::to<Vec>();
  const auto func_arg_names = func_args.GetKeywordArgs()
    | genex::views::transform([](auto const &x) { return x->Name.get(); })
    | genex::to<Vec>();

  // Check for any keyword arguments that don't have a
  // corresponding parameter.
  // Todo: Can we use: "analyse::utils::func_utils::enforce_no_invalid_fn_args()"?
  const auto invalid_args = func_arg_names
    | genex::views::not_in(func_param_names, genex::meta::deref, genex::meta::deref)
    | genex::to<Vec>();

  const auto param_ctx = func_params->Params.IsEmpty()
    ? static_cast<asts::Ast const*>(func_params)
    : static_cast<asts::Ast const*>(func_params->Params[0].get());
  RaiseIf<SppArgumentNameInvalidError>(
    not invalid_args.IsEmpty(), {sm->CurrentScope},
    ERR_ARGS(*param_ctx, "parameter", *invalid_args[0], "argument"));

  // Check for missing parameters that don't have a
  // corresponding argument.
  const auto missing_params = func_param_names_req
    | genex::views::not_in(func_arg_names, genex::meta::deref, genex::meta::deref)
    | genex::to<Vec>();
  RaiseIf<SppArgumentMissingError>(
    not missing_params.IsEmpty(), {sm->CurrentScope},
    ERR_ARGS(*missing_params[0], "parameter", fn_call, "argument"));

  // Type check the arguments against the parameters. Sort
  // the arguments into parameter order first.
  auto sorted_func_arguments = func_args.GetKeywordArgs();
  genex::actions::sort(
    sorted_func_arguments,
    {}, [&](asts::FunctionCallArgumentKeywordAst *arg) {
      return genex::position(func_param_names, [&arg](auto const &param) { return *arg->Name == *param; });
    });

  for (auto [arg, param] : genex::views::zip(sorted_func_arguments, func_params->GetAllParams())) {
    auto p_type = fn_scope->GetTypeSymbol(param->Type.get())->FqName()->WithConvention(
      asts::AstClone(param->Type->GetConvention()));
    if (p_type->IsSelfType()) {
      p_type = asts::AstClone(meta->PostfixExpressionLhs->To<asts::PostfixExpressionAst>()->Lhs->To<asts::TypeAst>())->
        WithConvention(asts::AstClone(p_type->GetConvention()));
    }

    auto a_type = arg->InferType(sm, meta);
    auto temp = type_utils::GenericInferenceMap();

    if (const auto variadic_param = param->To<asts::FunctionParameterVariadicAst>(); variadic_param != nullptr) {
      const auto variadic_gn_param = fn_proto.GetNonGenericImpl()->GnParamGroup->GetVariadicParams();
      const auto orig_name = dynamic_shared_cast<asts::TypeIdentifierAst>(variadic_param->Source.OriginalType);
      const auto is_variadic_generic_type = variadic_gn_param != nullptr
        and orig_name != nullptr
        and *orig_name == *dynamic_shared_cast<asts::TypeIdentifierAst>(variadic_gn_param->Name);

      if (not is_variadic_generic_type) {
        auto ts = Vec(a_type->LastTypePart()->GnArgGroup->Args.Len(), p_type);
        p_type = asts::generate::common_types::TupleType(param->PosStart(), std::move(ts));
        p_type->Stage7_AnalyseSemantics(sm, meta);
      }
    }

    // Special case for "self" parameters.
    if (const auto self_param = param->To<asts::FunctionParameterSelfAst>(); self_param != nullptr) {
      arg->Conv = asts::AstClone(self_param->Conv);
    }

    // Regular parameter without arg folding. The double check is
    // required for generics applied to the superclass in sup-ext
    // that cannot be substituted because they can be anything,
    // so reverse type check them with the "relaxed" variation.
    // This is the only place this is required.
    else if (not type_utils::ConventionEq(*p_type, *a_type)
      or not TypeEq(*p_type, *a_type, *fn_scope, *sm->CurrentScope)) {
      // If the parameter's type is a generic that is rigid at
      // the call site (defined in a scope enclosing the caller,
      // so already fixed), the argument must match it exactly.
      const auto param_is_rigid_generic = IsRigidGenericAtCaller(
        *p_type, *sm->CurrentScope);
      const auto relaxed_matched = not param_is_rigid_generic
        and RelaxedTypeEq(*a_type, *p_type, *sm->CurrentScope, *fn_scope, temp);

      // A relaxed match that only held because it bound a
      // generic the caller cannot choose is not a match.
      const auto relaxed_bound_rigid = relaxed_matched and genex::any_of(temp, [&](auto const &binding) {
        return IsRigidBindingAtCaller(*binding.first, *sm->CurrentScope);
      });

      RaiseIf<SppTypeMismatchError>(
        not relaxed_matched or relaxed_bound_rigid,
        {fn_scope, sm->CurrentScope}, ERR_ARGS(*param, *p_type, *arg, *a_type));
    }

    // The argument may have matched its parameter by forwarding
    // ("&Vec[T]" satisfying a "&View[T]" parameter), in which
    // case the value the callee is handed is the forwarded-to
    // one, so the argument becomes that call. This is the
    // argument-position counterpart of a method being called on
    // the value its receiver forwards to.
    //
    // Todo: an argument accepted by the relaxed match above never reaches here, because that branch and this one are
    //  alternatives. A parameter written as "&Self" is relaxed-matched against anything, so "eq(&self, that: &Self)"
    //  on a "StrView" takes a "&Str" unforwarded and reads it through "StrView"'s "{ptr, length}" shape - which is
    //  why "Str == Str" is false for equal strings. Moving the check out of the "else" is not enough on its own;
    //  "TypeFwdEq" also returns false for this pair and it is not yet clear why.
    else if (type_utils::TypeFwdEq(*a_type, *p_type, *sm->CurrentScope, *fn_scope)) {
      if (auto fwd_call = type_utils::BuildFwdCall(*arg->Val, *a_type, sm, meta); fwd_call != nullptr) {
        arg->Val = std::move(fwd_call);
      }
    }
  }
}
