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
import spp.asts.function_parameter_optional_ast;
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
import spp.asts.object_initializer_argument_ast;
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
import spp.asts.utils.generic_substitution;
import spp.utils.algorithms;
import spp.utils.ptr;
import spp.utils.types;
import spp.utils.uid;
import genex;

namespace {
  /**
   * Temporarily re-parent a scope, putting the original parent back however the enclosing block is left - including by
   * a thrown semantic error.
   *
   * Overload resolution catches per-candidate exceptions, so a bare restore after a call that raises leaves the scope
   * tree permanently wrongly-parented: every later lookup through that scope silently resolves against the wrong
   * ancestors, with no failure at the point the damage is done.
   */
  struct ScopeParentSwap {
    spp::analyse::scopes::Scope *Scope;
    spp::analyse::scopes::Scope *Original;

    ScopeParentSwap(spp::analyse::scopes::Scope *const scope, spp::analyse::scopes::Scope *const replacement) :
      Scope(scope), Original(scope->Parent) { scope->Parent = replacement; }

    ~ScopeParentSwap() { Scope->Parent = Original; }

    ScopeParentSwap(ScopeParentSwap const&) = delete;
    ScopeParentSwap(ScopeParentSwap&&) = delete;
    auto operator=(ScopeParentSwap const&) -> ScopeParentSwap& = delete;
    auto operator=(ScopeParentSwap&&) -> ScopeParentSwap& = delete;
  };
}

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
          // The prototype reached here belongs to the template's subtree, not to the instantiation the overload was
          // found through: a substituted "sup" scope shares its ast node with the template it was cloned from (see
          // "Scope"'s copy constructor), so reading the block's members off that ast yields the template's, whose
          // scopes sit under the template's own generic parameters - unbound. Splicing the found scope in is what
          // makes the type comparison below resolve against this instantiation's bindings instead.
          //
          // Todo: this is a band-aid over one ast node being aliased by a template scope and its instantiations, which
          //  is what makes "GetAstScope" ambiguous in the first place. The instantiation owning its own subtree would
          //  remove the need for it entirely; passing the scope to "CheckForConflictingOverride" would not, because
          //  the comparison inside resolves through that scope's *ancestors*, which is what is really being supplied.
          //  There is no substituted block scope to use instead - "CreateGenericSupScope" clones a block's own symbols
          //  but not its subtree, so the instantiation has no member scopes of its own.
          const auto swap = ScopeParentSwap(
            o1.Proto->GetAstScope()->Parent, const_cast<scopes::Scope*>(o1.FnScope));

          auto conflict =
            CheckForConflictingOverride(*o1.Proto->GetAstScope()->Parent, o2.FnScope, *o1.Proto, sm, meta);
          if (conflict != nullptr) {
            overload_scopes |= genex::actions::remove_if([conflict](auto const &info) {
              return info.Proto == conflict;
            });
          }
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



auto spp::analyse::utils::func_utils::NameFnArgs(
  asts::FunctionCallArgumentGroupAst &a_group,
  asts::FunctionParameterGroupAst const &p_group,
  scopes::ScopeManager &sm,
  Vec<asts::GenericArgumentAst*> const &generic_args)
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

  // Put the arguments into the parameters' own order, materialising
  // a default value for every optional parameter the call left out.
  // Ordering by parameter is needed for LLVM to do an ordinal match
  // despite S++ operating with keyword-matching.
  auto ordered_args = UniqueVec<asts::FunctionCallArgumentAst>();
  for (auto const *param : p_group.GetAllParams()) {
    const auto param_name = param->ExtractName();

    auto matched = false;
    for (auto &&arg : a_group.Args) {
      const auto kw_arg = arg != nullptr
        ? arg->To<asts::FunctionCallArgumentKeywordAst>()
        : nullptr;

      if (kw_arg == nullptr or kw_arg->Name->Val != param_name->Val) { continue; }
      ordered_args.EmplaceBack(std::move(arg));
      matched = true;
      break;
    }
    if (matched) { continue; }

    // Leftover optional parameters inject their argument into the
    // callsite (unlike Python, which executes once for all func
    // calls).
    const auto optional_param = param->To<asts::FunctionParameterOptionalAst>();
    if (optional_param == nullptr or optional_param->DefaultVal == nullptr) { continue; }
    ordered_args.EmplaceBack(MakeUnique<asts::FunctionCallArgumentKeywordAst>(
      param_name, nullptr, nullptr, asts::AstClone(optional_param->DefaultVal)));
  }
  a_group.Args = std::move(ordered_args);
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
