module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.func_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.generic_bindings;
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
import genex;

auto spp::analyse::utils::func_utils::GetFuncOwnerTypeAndFuncName(
  asts::ExpressionAst const &lhs,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> Tup<Shared<asts::TypeAst>, scopes::Scope const*, Shared<asts::IdentifierAst>> {
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

  // If the lhs is an identifier, it must be a variable
  // symbol, not a namespace symbol.
  if (lhs_as_ident and sm.CurrentScope->GetVarSymbol(lhs_as_ident) == nullptr) {
    RaiseMissingIdentifierAndClosestOptions(*lhs_as_ident, sm.CurrentScope->AllVarSymbols(), {}, sm);
  }

  // Variables that will be set in each branch, and
  // returned. These are used to determine what variation
  // of function call is being performed.
  auto fn_owner_type = Shared<asts::TypeAst>(nullptr);
  auto fn_owner_scope = static_cast<scopes::Scope const*>(nullptr);
  auto fn_name = Shared<asts::IdentifierAst>(nullptr);

  // Runtime access into an object: "object.method()".
  // No namespacing involved.
  if (postfix_lhs != nullptr and runtime_field != nullptr) {
    fn_owner_type = postfix_lhs->Lhs->InferType(&sm, meta);
    fn_name = runtime_field->Name;
    fn_owner_scope = sm.CurrentScope->GetTypeSymbol(fn_owner_type.get())->LinkedScope;
  }

  // Static access into a type: "Type::method()" or
  // "ns::Type::method()".
  else if (static_field != nullptr and postfix_lhs_as_type != nullptr) {
    fn_owner_type = asts::AstCloneShared(postfix_lhs_as_type);
    fn_name = static_field->Name;
    fn_owner_scope = sm.CurrentScope->GetTypeSymbol(fn_owner_type.get())->LinkedScope;
  }

  // Direct access into a namespaced free function:
  // "std::io::print(variable)".
  else if (postfix_lhs != nullptr and static_field != nullptr) {
    fn_owner_scope = sm.CurrentScope->ConvertPostfixToNestedScope(postfix_lhs->Lhs.get());
    fn_name = static_field->Name;
    fn_owner_type = fn_owner_scope->GetVarSymbol(fn_name.get())->Type;
  }

  // Direct access into a non-namespaced function:
  // "function()":
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

  return {fn_owner_type, fn_owner_scope, fn_name};
}

auto spp::analyse::utils::func_utils::ConvertMethodToFuncForm(
  asts::TypeAst const &function_owner_type,
  asts::IdentifierAst const &function_name,
  asts::PostfixExpressionAst const &lhs,
  asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> Pair<Unique<asts::PostfixExpressionAst>, Unique<asts::PostfixExpressionOperatorFunctionCallAst>> {
  // A method reached through a forwarding type is invoked
  // on the forwarded-to value, not on the object that forwards
  // to it: "w.greet()" calls "greet" on "w.fwd_ref()". The
  // member access has already built that call, so use it as
  // the receiver; a method found on the object's own type uses
  // the object itself.
  // Todo: Check this for when we use a method on a type who has a forwarding type, but the forward isn't used.
  const auto member_access = lhs.Op->To<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>();
  const auto fwd_receiver = member_access != nullptr ? member_access->GetFwdReceiver() : nullptr;
  const auto self_expr = fwd_receiver != nullptr ? fwd_receiver : lhs.Lhs.get();
  auto self_arg_val = asts::AstClone(self_expr);

  // Create the static method access (without the function
  // call and args).
  auto field = MakeUnique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(
    nullptr, AstClone(&function_name));
  auto field_access = MakeUnique<asts::PostfixExpressionAst>(
    AstClone(&function_owner_type), std::move(field));

  // Create an argument for "self" and inject it into the
  // current arguments.
  auto self_arg = MakeUnique<asts::FunctionCallArgumentPositionalAst>(
    nullptr, nullptr, std::move(self_arg_val));
  auto fn_args = std::move(fn_call.FnArgGroup->Args);
  fn_args.Insert(fn_args.begin(), std::move(self_arg));

  // Create the function call with the new arguments.
  auto new_fn_call = MakeUnique<asts::PostfixExpressionOperatorFunctionCallAst>(
    AstClone(fn_call.GnArgGroup), AstClone(fn_call.FnArgGroup), nullptr);
  new_fn_call->FnArgGroup->Args = std::move(fn_args);

  // The forwarding receiver is a "GenOnce" call that resumes
  // itself, so its type is the borrow it yields. Infer it
  // with resumption allowed, whatever the surrounding
  // expression asked for (an "async" call suppresses it).
  meta->Save();
  meta->PreventAutoGeneratorResume = false;
  new_fn_call->FnArgGroup->Args[0]->SetSelfType(self_expr->InferType(&sm, meta));
  meta->Restore();
  new_fn_call->Source.OriginalExpr = fn_call.Source.OriginalExpr;

  // Return the new ASTs.
  return {std::move(field_access), std::move(new_fn_call)};
}

auto spp::analyse::utils::func_utils::GetAllFunctionScopes(
  asts::IdentifierAst const &target_fn_name,
  scopes::Scope const *target_scope,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> Vec<FunctionOverload> {
  // If the name is empty (non-symbolic call) then return
  // "no scopes". If the target scope is nullptr, then
  // the functions are being superimposed over a generic type.
  if (target_fn_name.Val.empty() or target_scope == nullptr) { return {}; }

  // Get the function-type name from the function: "func()"
  // => "$Func".
  const auto mapped_name = target_fn_name.ToFuncIdentifier();
  auto overload_scopes = Vec<FunctionOverload>();

  auto is_valid_ext_scope = [mapped_name=mapped_name.get()](auto const *scope) {
    const auto ext = scope->AstNode->template To<asts::SupPrototypeExtensionAst>();
    if (ext == nullptr) { return false; }
    const auto ext_name = dynamic_shared_cast<asts::TypeIdentifierAst>(ext->Name);
    return ext_name != nullptr and ext_name->Name == mapped_name->Val;
  };

  // Check for namespaced (module-level) functions (they
  // will have no inheritable generics as they are free
  // functions, not inside a "sup" block).
  if (target_scope->NsSym != nullptr) {
    for (auto *ancestor_scope : target_scope->Ancestors()) {
      for (auto const *sup_scope : ancestor_scope->Children
           | genex::views::ptr
           | genex::views::filter(is_valid_ext_scope)) {
        overload_scopes.EmplaceBack(FunctionOverload{
          .FnScope = sup_scope,
          .Proto = asts::AstBody(sup_scope->AstNode)[0]->To<asts::FunctionPrototypeAst>(),
          .SupGenerics = asts::GenericArgumentGroupAst::NewEmpty(),
          .FwdType = nullptr
        });
      }
    }
  }

  // Functions belonging to a type will have inheritance
  // generics from "sup [...] Type { ... }"
  else {
    // If a class scope was provided, get all the sup scopes
    // attached to it, otherwise use the specific sup scope
    // exclusively.
    const auto sup_scopes = target_scope->AstNode->To<asts::ClassPrototypeAst>() != nullptr
      ? target_scope->SupScopesConst()
      : Vec{target_scope};

    // From the super scopes, check each one for the structure
    // "sup $Func ext FunXXX { ... }" super-imposition.
    // Todo: use the "is_valid_ext_scope"?
    for (auto *sup_scope : sup_scopes) {
      for (auto *sup_ast : asts::AstBody(sup_scope->AstNode)
           | genex::views::cast_dynamic<asts::SupPrototypeExtensionAst*>()) {
        if (sup_ast->Name->ToUnchecked<asts::TypeIdentifierAst>()->Name == mapped_name->Val) {
          overload_scopes.EmplaceBack(FunctionOverload{
            .FnScope = sup_scope,
            .Proto = asts::AstBody(sup_ast)[0]->To<asts::FunctionPrototypeAst>(),
            .SupGenerics = MakeUnique<asts::GenericArgumentGroupAst>(nullptr, sup_scope->GetGenerics(), nullptr),
            .FwdType = nullptr
          });
        }
      }
    }

    // When a derived class has overridden a method, the base
    // method must be removed.
    for (auto const &o1 : overload_scopes) {
      for (auto const &o2 : overload_scopes) {
        // This depth difference checker ensures the derived
        // version is kept.
        if (o1.Proto != o2.Proto
          and target_scope->DepthDiff(o1.FnScope) < target_scope->DepthDiff(o2.FnScope)) {
          // Todo: the override check reads the prototype's grandparent scope, so the candidate's own scope is spliced
          //  in and put back around it. Give "CheckForConflictingOverride" the scope instead of borrowing the tree.
          const auto temp = o1.Proto->GetAstScope()->Parent->Parent;
          o1.Proto->GetAstScope()->Parent->Parent = const_cast<scopes::Scope*>(o1.FnScope);

          auto conflict =
            CheckForConflictingOverride(*o1.Proto->GetAstScope()->Parent, o2.FnScope, *o1.Proto, sm, meta);
          if (conflict != nullptr) {
            overload_scopes |= genex::actions::remove_if([conflict](auto const &info) {
              return info.Proto == conflict;
            });
          }
          o1.Proto->GetAstScope()->Parent->Parent = temp;
        }
      }
    }

    // Adjust the scope to the inner function scope.
    for (auto &info : overload_scopes) {
      info.FnScope = (info.FnScope->Children
        | genex::views::ptr
        | genex::views::filter(is_valid_ext_scope)
        | genex::to<Vec>())[0];
    }
  }

  // Next, get scopes from "forwarding types" (ie FwdRef
  // and FwdMut return types).
  if (target_scope->TySym != nullptr and meta->CurrentStage >= 9.0) {
    auto [fwd_ref_type, fwd_mut_type] = type_utils::GetFwdTypes(*target_scope->TySym->FqName(), sm);
    if (fwd_ref_type != nullptr) {
      const auto inner_type = fwd_ref_type->LastTypePart()->GnArgGroup->TypeAt("T")->Val;
      auto inner_scopes = GetAllFunctionScopes(
        target_fn_name, sm.CurrentScope->GetTypeSymbol(inner_type.get())->LinkedScope, sm, meta);
      for (auto &i : inner_scopes) {
        i.FwdType = asts::AstCloneShared(inner_type);
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

  // Get the methods that belong to this type, or any
  // of its supertypes.
  const auto existing = GetAllFunctionScopes(*new_fn.Name, target_scope, sm, meta);

  // Check for an overload conflict with all functions
  // of the same name.
  for (auto const &[old_scope, old_fn, _, _] : existing) {
    // Ignore if the method is an identical match on a
    // base class (override) or is the same object.
    if (old_fn == &new_fn) { continue; }
    if (old_fn == CheckForConflictingOverride(this_scope, old_scope, new_fn, sm, meta, old_scope)) { continue; }

    // Ignore if the return types are different.
    if (not TypeEq(*new_fn.ReturnType, *old_fn->ReturnType, this_scope, *old_scope)) { continue; }

    // Ignore if there are a different number of required
    // generic parameters.
    if (new_fn.GnParamGroup->GetTypeParams().Len() != old_fn->GnParamGroup->GetTypeParams().Len()) { continue; }
    if (new_fn.GnParamGroup->GetCompParams().Len() != old_fn->GnParamGroup->GetCompParams().Len()) { continue; }

    // Get the two parameter lists and create copies to
    // remove duplicate parameters from.
    auto params_new = asts::AstCloneVec(new_fn.FnParamGroup->Params);
    auto params_old = asts::AstCloneVec(old_fn->FnParamGroup->Params);

    // Remove all the required parameters on the first
    // parameter list off of the other parameter list.
    for (auto [p, q] : genex::views::zip(new_fn.FnParamGroup->Params | genex::views::ptr,
                                         old_fn->FnParamGroup->Params | genex::views::ptr)) {
      if (TypeEq(*p->Type, *q->Type, this_scope, *old_scope)) {
        params_new |= genex::actions::remove_if([pe=p->ExtractNames()](auto &&x) {
          return genex::equals(x->ExtractNames(), std::move(pe), {}, genex::meta::deref, genex::meta::deref);
        });
        params_old |= genex::actions::remove_if([qe=q->ExtractNames()](auto &&x) {
          return genex::equals(x->ExtractNames(), std::move(qe), {}, genex::meta::deref, genex::meta::deref);
        });
      }
    }

    // If neither parameter list contains a required
    // parameter, throw an error.
    const auto tmp = genex::views::concat(
      params_new | genex::views::ptr,
      params_old | genex::views::ptr) | genex::to<Vec>();
    if (genex::operations::empty(tmp
      | genex::views::cast_dynamic<asts::FunctionParameterRequiredAst*>()
      | genex::to<Vec>())) {
      return old_fn;
    }
  }
  return nullptr;
}

auto spp::analyse::utils::func_utils::SameSignature(
  asts::FunctionPrototypeAst const &fn_a,
  scopes::Scope const &scope_a,
  asts::FunctionPrototypeAst const &fn_b,
  scopes::Scope const &scope_b)
  -> bool {
  //
  using type_utils::TypeEq;

  // Helper function to check whether a "self" parameter
  // is present.
  auto hs = [](asts::FunctionPrototypeAst const *f) {
    return f->FnParamGroup->GetSelfParam() != nullptr;
  };

  // Helper function to get the type of the convention AST
  // applied to the "self" parameter.
  auto sc = [&hs](asts::FunctionPrototypeAst const *f) {
    return hs(f) ? f->FnParamGroup->GetSelfParam()->Conv.get() : nullptr;
  };

  auto param_names_eq = [](auto const &a, auto const &b) {
    if (a.Len() != b.Len()) { return false; }
    for (auto const &[x, y] : genex::views::zip(a, b)) {
      if (*x != *y) { return false; }
    }
    return true;
  };

  // The names must match. Note that the "cmp" state does
  // NOT have to match.
  if (*fn_a.Name != *fn_b.Name) { return false; }

  // Get the two parameter lists and create copies.
  auto params_a = fn_a.FnParamGroup->GetNonSelfParams();
  auto params_b = fn_b.FnParamGroup->GetNonSelfParams();

  // Get a list of conditions to check for conflicting
  // functions.
  if (params_a.Len() != params_b.Len()) { return false; }

  // All parameters must have the same names.
  if (genex::any_of(
    genex::views::zip(params_a, params_b) | genex::to<Vec>(),
    [&](auto pq) { return not param_names_eq(spp::get<0>(pq)->ExtractNames(), spp::get<1>(pq)->ExtractNames()); })) {
    return false;
  }

  // All parameters must have the same types.
  if (genex::any_of(
    genex::views::zip(params_a, params_b) | genex::to<Vec>(),
    [&](auto pq) { return not TypeEq(*spp::get<0>(pq)->Type, *spp::get<1>(pq)->Type, scope_a, scope_b, false); })) {
    return false;
  }

  // The function type (subroutine vs coroutine) must match.
  if (fn_a.TokFun->TokenType != fn_b.TokFun->TokenType) {
    return false;
  }

  // The return types must be symbolically equal.
  if (not TypeEq(*fn_a.ReturnType, *fn_b.ReturnType, scope_a, scope_b, false)) {
    return false;
  }

  // Check the self parameters' conventions.
  return not(hs(&fn_a) != hs(&fn_b) or (sc(&fn_a) and *sc(&fn_a) != sc(&fn_b)) or (not sc(&fn_a) and sc(&fn_b)));
}

auto spp::analyse::utils::func_utils::CheckForConflictingOverride(
  scopes::Scope const &this_scope,
  scopes::Scope const *target_scope,
  asts::FunctionPrototypeAst const &new_fn,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta,
  scopes::Scope const *exclude_scope)
  -> asts::FunctionPrototypeAst* {
  // Get the existing functions that belong to this
  // type, or any of its supertypes.
  const auto existing = GetAllFunctionScopes(*new_fn.Name, target_scope, sm, meta);

  // Check for an overload conflict with all functions
  // of the same name.
  for (auto const &[old_scope, old_fn, _, _] : existing) {
    // Ignore if the method is the same object.
    if (old_fn == &new_fn) { continue; }
    if (old_scope == exclude_scope) { continue; }

    // The functions must have identical signatures to
    // conflict, so return the old function.
    if (SameSignature(new_fn, this_scope, *old_fn, *old_scope)) { return old_fn; }
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

  // Check that each argument satisfies its constraints.
  for (auto [i, p_name] : p_names | genex::views::enumerate) {
    auto matching = type_args
      | genex::views::filter([&](auto const *a) { return a->ViewName() == p_name->Name; })
      | genex::to<Vec>();
    if (matching.IsEmpty()) { continue; }

    const auto arg_sym = sm.CurrentScope->GetTypeSymbol(matching[0]->Val.get());
    auto *const con_scope = arg_sym != nullptr and arg_sym->LinkedScope != nullptr
      ? arg_sym->LinkedScope
      : sm.CurrentScope;
    auto con_sm = scopes::ScopeManager(sm.GlobalScope, con_scope);

    // Cross apply the inferred arguments into this
    // parameter's constraints.
    auto p_cons = Vec<Shared<asts::TypeAst>>();
    for (auto p_con : p_con_groups[i]) {
      auto def_type_raw = p_con->WithoutGenerics();
      if (auto def_val_type_sym = owner_scope.GetTypeSymbol(def_type_raw.get()); def_val_type_sym != nullptr and meta.
        CurrentStage > 4) {
        auto temp = def_val_type_sym->FqName();
        temp = temp->WithGenerics(asts::AstClone(p_con->LastTypePart()->GnArgGroup));
        p_con = std::move(temp);
      }

      auto sub = p_con->SubstituteGenerics(all_args);
      meta.Save();
      meta.AllowAbstractType = true;
      sub->Stage7_AnalyseSemantics(&con_sm, &meta);
      meta.Restore();
      p_cons.push_back(std::move(sub));
    }

    // Raise an error if any constraint of this argument is not satisfied.
    const auto unsatisfied = type_utils::EnforceGenericConstraintsOneArg(
      p_cons, *matching[0]->Val, owner_scope, *sm.CurrentScope);
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

static auto CollectDirectInferences(
  spp::Shared<spp::asts::TypeAst> const &source_type,
  spp::Shared<spp::asts::TypeAst> const &target_type,
  spp::Shared<spp::asts::IdentifierAst> const &target_name,
  spp::Vec<spp::Shared<spp::asts::TypeIdentifierAst>> const &type_p_names,
  spp::Vec<spp::Shared<spp::asts::TypeIdentifierAst>> const &variadic_type_p_names,
  spp::Vec<spp::Shared<spp::asts::TypeIdentifierAst>> const &comp_p_names,
  spp::Shared<spp::asts::IdentifierAst> const &variadic_fn_param_name,
  spp::analyse::scopes::Scope const &owner_scope,
  spp::analyse::scopes::ScopeManager &sm,
  spp::analyse::utils::generic_bindings::GenericBindingSet &bindings)
  -> void {
  //
  auto temp_gs = spp::analyse::utils::type_utils::GenericInferenceMap();
  spp::analyse::utils::type_utils::RelaxedTypeEq(
    *source_type->WithoutConvention(),
    *target_type->WithoutConvention(),
    *sm.CurrentScope, owner_scope, temp_gs, true);

  const auto is_variadic_param_slot =
    variadic_fn_param_name != nullptr
    and target_name != nullptr
    and *target_name == *variadic_fn_param_name;

  for (auto const &[inferred_name, inferred_val] : temp_gs) {
    auto *typed = inferred_val->To<spp::asts::TypeAst>();
    const auto declared_type = genex::contains(type_p_names, *inferred_name, genex::meta::deref);
    const auto declared_comp = genex::contains(comp_p_names, *inferred_name, genex::meta::deref);

    if (declared_type) {
      if (typed == nullptr) { continue; }
      auto shared = typed->shared_from_this();
      if (is_variadic_param_slot and not genex::contains(variadic_type_p_names, *inferred_name, genex::meta::deref)) {
        auto const &inner = shared->LastTypePart()->GnArgGroup->Args[0];
        shared = inner->ToUnchecked<spp::asts::GenericArgumentTypeAst>()->Val;
      }
      bindings.Add(inferred_name, std::move(shared));
    }
    else if (declared_comp) {
      bindings.Add(inferred_name, inferred_val);
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

  // Separate param lists and extract names and constraint
  // groups.
  const auto type_params = p_group.GetTypeParams();
  const auto comp_params = p_group.GetCompParams();

  auto type_p_names = type_params
    | genex::views::transform([](auto *x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();
  auto variadic_type_p_names = type_params
    | genex::views::filter([](auto *x) { return x->template To<asts::GenericParameterTypeVariadicAst>() != nullptr; })
    | genex::views::transform([](auto *x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();
  auto comp_p_names = comp_params
    | genex::views::transform([](auto *x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();

  // Every candidate a parameter is offered goes into one
  // binding set. The written arguments go in first, so
  // they are the ones a later candidate has to agree with.
  auto bindings = generic_bindings::GenericBindingSet::FromNamedArgs(a_group, sm);
  auto type_a_names = bindings.Args()
    | genex::views::cast_dynamic<asts::GenericArgumentTypeKeywordAst*>()
    | genex::views::transform([](auto const *x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();

  // First inference comes from the infer source and target
  // maps.
  for (auto const &[target_name, target_type] : infer_target) {
    if (not infer_source.contains(target_name)) { continue; }
    CollectDirectInferences(
      infer_source.at(target_name), target_type, target_name, type_p_names, variadic_type_p_names, comp_p_names,
      variadic_fn_param_name, owner_scope, sm, bindings);
  }

  // Next is constraint based inference, where for example
  // "[U, F: FunRef[(), U]]" can infer "U" from the return
  // type of whatever subtype of "F" is "FunRef", and "U"
  // is extractable as a generic argument.
  {
    for (auto *param : type_params) {
      if (param->Constraints->Constraints.IsEmpty()) { continue; }
      const auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(param->Name);

      // Find the inferred concrete type for this param.
      const auto inferred_type = bindings.Resolved(cast_name.get()).Type;
      if (inferred_type == nullptr) { continue; }

      // Build the candidate list from the type and all
      // subtypes.
      const auto concrete_sym = sm.CurrentScope->GetTypeSymbol(inferred_type.get());
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
        // Try each candidate in order and stop at the first
        // match.
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
        // false-negative rejecting a valid call here. TODO
        if (not candidates.IsEmpty() and not matched) {
          const auto constraint_drives_inference = genex::any_of(
            type_params, [&](auto const *other) { return constraint->ContainsGenerics(*other); });
          RaiseIf<SppGenericConstraintError>(
            constraint_drives_inference,
            {sm.CurrentScope, &owner_scope}, ERR_ARGS(*constraint, *inferred_type));
        }

        for (auto const &[inferred_name, inferred_val] : temp_gs) {
          // Skip names already bound to a type, to avoid
          // duplicate entries.
          if (bindings.ContainsType(inferred_name.get())) { continue; }

          if (genex::contains(type_p_names, *inferred_name, genex::meta::deref)) {
            auto *typed = inferred_val->To<asts::TypeAst>();
            if (typed == nullptr) { continue; }
            bindings.Add(inferred_name, typed->shared_from_this());
          }
          else if (genex::contains(comp_p_names, *inferred_name, genex::meta::deref)) {
            bindings.Add(inferred_name, inferred_val);
          }
        }
      }
    }
  }

  // Apply optional defaults for still unknown type params.
  // Type params may need qualification.
  for (auto *opt_param : type_params | genex::views::cast_dynamic<asts::GenericParameterTypeOptionalAst*>()) {
    const auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(opt_param->Name);
    if (bindings.ContainsType(cast_name.get())) { continue; }
    auto def_type = opt_param->DefaultVal;
    auto def_type_raw = def_type->WithoutGenerics();
    if (auto def_sym = owner_scope.GetTypeSymbol(def_type_raw.get()); def_sym != nullptr and meta.CurrentStage > 4) {
      auto temp = def_sym->FqName()->WithConvention(asts::AstClone(def_type->GetConvention()));
      if (not type_utils::IsTypeSelf(*def_type)) {
        temp = temp->WithGenerics(asts::AstClone(def_type->LastTypePart()->GnArgGroup));
      }
      def_type = std::move(temp);
    }
    bindings.Add(cast_name, std::move(def_type));
  }

  // Apply optional defaults for still unknown comp params.
  for (auto *opt_param : comp_params | genex::views::cast_dynamic<asts::GenericParameterCompOptionalAst*>()) {
    const auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(opt_param->Name);
    if (bindings.ContainsComp(cast_name.get())) { continue; }
    bindings.Add(cast_name, opt_param->DefaultVal.get());
  }

  // Validate there are no conflicting candidates or
  // uninferred required params.
  bindings.EnforceNoConflicts(sm);
  EnforceNoUninferredGnArgs(type_p_names, bindings.TypeNames(), owner_scope, owner, sm);
  EnforceNoUninferredGnArgs(comp_p_names, bindings.CompNames(), owner_scope, owner, sm);

  // Cross-apply: substitute all known values (type +
  // comp together) into each type param's resolved
  // type. Handles "Vec[T, A=Alloc[T]]" style defaults
  // and type<->comp cross-substitution.
  for (auto const &type_name : bindings.TypeNames()) {
    if (genex::contains(type_a_names, *type_name, genex::meta::deref)) { continue; }

    // Substitute through everything else that is known,
    // skipping this name to avoid cycles.
    auto other_unified = bindings.ToInferenceMap();
    other_unified.erase(type_name);
    const auto other_group = asts::GenericArgumentGroupAst::FromMap(other_unified);

    auto t = bindings.Resolved(type_name.get()).Type->SubstituteGenerics(other_group->GetAllArgs());
    t->Stage7_AnalyseSemantics(&sm, &meta);
    bindings.Replace(type_name.get(), std::move(t));
  }

  // Emit the final arg list, in parameter declaration
  // order.
  a_group.Args = bindings.ToArgs(p_group);

  // Cmp argument type-checking (semantic stage only).
  // Done after args are restored onto a_group so that
  // the following issue is solved: when analysing
  // "SizedIntegerSigned[32_u32]", "32_u32" is inferred
  // and checked. But as it is inferred, the generics
  // were missing, because this function temporarily
  // removes them. So we only analyse AFTER they are
  // re-added having been checked.
  if (meta.CurrentStage > 7) {
    const auto all_final_group = asts::GenericArgumentGroupAst::FromMap(bindings.ToInferenceMap());
    const auto all_final_args = all_final_group->GetAllArgs();

    // Walk the parameters, not the bindings: the two
    // used to be sorted into the same order and zipped,
    // which only held while every comp parameter had a binding.
    for (auto *param : comp_params) {
      auto *inferred_val = bindings.Resolved(
        dynamic_shared_cast<asts::TypeIdentifierAst>(param->Name).get()).Comp;
      if (inferred_val == nullptr) { continue; }
      auto a_type = owner_scope.GetTypeSymbol(inferred_val->InferType(&sm, &meta).get())->FqName();
      auto p_type = param->Type->SubstituteGenerics(all_final_args);

      if (param->To<asts::GenericParameterCompVariadicAst>()) {
        for (auto const &inner : a_type->LastTypePart()->GnArgGroup->Args
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
  const auto is_generic = sm.CurrentScope->GetTypeSymbol(expr_type.get())->IsGeneric;
  return is_generic ? func_type : expr_type;
}

auto spp::analyse::utils::func_utils::CreateCallablePrototype(
  asts::TypeAst const &expr_type)
  -> Unique<asts::FunctionPrototypeAst> {
  // Extract the parameter and return types from the
  // expression type.
  auto ret_ty = expr_type.LastTypePart()->GnArgGroup->TypeAt("Out")->Val;
  auto param_tys = expr_type.LastTypePart()->GnArgGroup->TypeAt("Args")->Val->LastTypePart()->GnArgGroup->GetTypeArgs()
    | genex::views::transform([](auto *g) {
      return MakeUnique<asts::FunctionParameterRequiredAst>(nullptr, nullptr, g->Val);
    })
    | spp::views::cast_unique<asts::FunctionParameterAst>();

  // Create a function prototype based off of the parameter
  // and return type.
  // Todo: When might it be a coroutine, not a subroutine?
  // Todo: Do we set "cmp" here for the subroutine ever?
  auto dummy_param_group = MakeUnique<asts::FunctionParameterGroupAst>(
    nullptr, std::move(param_tys), nullptr);
  auto dummy_name = MakeUnique<asts::IdentifierAst>(
    0uz, "<anonymous>");
  auto dummy_overload = MakeUnique<asts::SubroutinePrototypeAst>(
    SPP_NO_ANNOTATIONS, nullptr, nullptr, std::move(dummy_name),
    nullptr, std::move(dummy_param_group),
    nullptr, std::move(ret_ty), nullptr);

  // Return the function prototype.
  return dummy_overload;
}
