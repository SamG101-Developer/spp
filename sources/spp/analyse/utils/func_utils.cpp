module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.func_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.generic_bindings;
import spp.analyse.utils.overload_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_optional_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.subroutine_prototype_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
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

namespace spp::analyse::utils::func_utils {
  namespace {
    /// The owning scope of a function, where the scope's ast
    /// is the function, and the parent is the "sup $FuncName
    /// ext ..."
    auto OwningBlockOf(
      Scope const &found_in, FunctionPrototypeAst const *fn) -> Scope const* {
      for (const auto block : found_in.Children | genex::views::ptr) {
        if (block->AstNode == nullptr or block->AstNode->To<SupPrototypeExtensionAst>() == nullptr) { continue; }
        const auto body = AstBody(block->AstNode);
        if (not body.IsEmpty() and body[0]->template To<FunctionPrototypeAst>() == fn) { return block; }
      }
      return nullptr;
    }

    /// Get the "sup" block a function prototype was declared
    /// in (or nullptr for a free function, whose context is the
    /// module prototype => no superimposition).
    auto _SupBlockOf(
      FunctionPrototypeAst const &fn) -> Ast* {
      const auto ctx = fn.GetAstCtx();
      if (ctx == nullptr) { return nullptr; }
      const auto is_sup = ctx->To<SupPrototypeFunctionsAst>() != nullptr
        or ctx->To<SupPrototypeExtensionAst>() != nullptr;
      return is_sup ? ctx : nullptr;
    }

    /// Determine if two "sup" blocks can ever apply to the same
    /// instantiation. This is because blocks over the same generic
    /// type, can carry disjoint constraints. Atom[T: Integer] and
    /// Atom[T: FloatingPoint] are two different instantiations of
    /// the same generic type, but they are not the same block,
    /// so they never see each other. But, they can have
    /// "conflicting" functions.
    auto _SupBlocksOverlap(
      Ast *const sup_a, Ast *const sup_b) -> bool {
      // Free functions, and members of one block, always share
      // a context.
      if (sup_a == nullptr or sup_b == nullptr or sup_a == sup_b) { return true; }

      // Either block may hold the generic, so both orders are
      // tried.
      auto generics = type_compare::GenericInferenceMap();
      auto const &a = *AstName(sup_a);
      auto const &b = *AstName(sup_b);
      return
        type_compare::RelaxedTypeEq(a, b, *sup_a->GetAstScope(), *sup_b->GetAstScope(), generics) or
        type_compare::RelaxedTypeEq(b, a, *sup_b->GetAstScope(), *sup_a->GetAstScope(), generics);
    }

    /// Check for invalid arguments being present in a function
    /// call.
    auto EnforceNoInvalidFnArgs(
      Vec<FunctionParameterAst*> const &params,
      Vec<FunctionCallArgumentKeywordAst*> const &named_args,
      ScopeManager &sm) -> void {
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
      // The context is the first parameter, but there may not
      // be one - "fun f()" called as "f(x=1)" has an invalid
      // name and nothing to point at - so fall back to the
      // offending argument rather than indexing an empty list.
      if (not invalid_arg_names.IsEmpty()) {
        auto const *const param_ctx = params.IsEmpty()
          ? static_cast<Ast const*>(invalid_arg_names[0].get())
          : static_cast<Ast const*>(params[0]);

        Raise<SppArgumentNameInvalidError>(
          {sm.CurrentScope},
          ERR_ARGS(*param_ctx, "fn param", *invalid_arg_names[0], "fn arg"));
      }
    }

    /// When analysing lists of function overloads that can be
    /// called, we need to prune off base class functions that
    /// have overrides present, otherwise we get false
    /// ambiguities being reported. The depth difference is what
    /// is used to determine the override.
    auto PruneOverriddenOverloads(
      Vec<FunctionOverload> &overload_scopes, Scope const *target_scope,
      ScopeManager &sm, meta::CompilerMetaData *meta) -> void {
      for (auto const &o1 : overload_scopes) {
        for (auto const &o2 : overload_scopes) {
          // This depth difference checker ensures the derived version is kept.
          if (o1.Proto != o2.Proto
            and target_scope->DepthDiff(o1.FnScope) < target_scope->DepthDiff(o2.FnScope)) {
            // The prototype comes off the ast, which a generic "sup"
            // block's instantiations share with its template, so its
            // own scope is the template's, under the template's
            // unbound parameters. The instantiation it was found
            // through clones the block's member scopes along with it,
            // and its copy of the function's block resolves the
            // signature against the instantiation's bindings instead.
            // Without one - the template itself, or a blanket block
            // attached unsubstituted - the template's block is the right
            // one.
            const auto own_block = OwningBlockOf(*o1.FnScope, o1.Proto);
            auto const &sig_scope = own_block != nullptr ? *own_block : *o1.Proto->GetAstScope()->Parent;
            auto conflict = CheckForConflictingOverride(sig_scope, o2.FnScope, *o1.Proto, sm, meta);
            if (conflict != nullptr) {
              overload_scopes |= genex::actions::remove_if([conflict](auto const &info) {
                return info.Proto == conflict;
              });
            }
          }
        }
      }
    }

    /// Point each overload at the "sup" block that actually
    /// declares it, rather than the scope it was found through.
    /// Todo: This is a bit of a hack, but it works. Tidy module
    ///  and eliminate this.
    auto NarrowToOwningBlock(
      Vec<FunctionOverload> &overload_scopes,
      auto const &is_valid_ext_scope)
      -> void {
      for (auto &info : overload_scopes) {
        // "Any block will do" as a fallback, but there may be
        // none at all, in which case the scope stays as it was.
        auto owning_block = OwningBlockOf(*info.FnScope, info.Proto);
        if (owning_block == nullptr) {
          const auto blocks = info.FnScope->Children
            | genex::views::ptr
            | genex::views::filter(is_valid_ext_scope)
            | genex::to<Vec>();
          if (not blocks.IsEmpty()) { owning_block = blocks[0]; }
        }
        if (owning_block != nullptr) { info.FnScope = owning_block; }
      }
    }

    /// The "sup $F ext FunXXX { ... }" block an overload was
    /// lowered to, and the overload; or nullptrs.
    auto FunctionBlockOf(
      Scope const &scope) -> Pair<SupPrototypeExtensionAst*, FunctionPrototypeAst*> {
      const auto ext = AstAs<SupPrototypeExtensionAst>(scope.AstNode);
      const auto body = ext != nullptr ? AstBody(ext) : Vec<Ast*>();
      const auto proto = not body.IsEmpty() ? body[0]->To<FunctionPrototypeAst>() : nullptr;
      return {proto != nullptr ? ext : nullptr, proto};
    }
  }
}

auto spp::analyse::utils::func_utils::GetAllFunctionScopes(
  IdentifierAst const &target_fn_name, Scope const *target_scope,
  ScopeManager &sm, meta::CompilerMetaData *meta) -> Vec<FunctionOverload> {
  // If the name is empty (non-symbolic call) then return
  // "no scopes". If the target scope is nullptr, then the
  // functions are being superimposed over a generic type.
  if (target_fn_name.Val.empty() or target_scope == nullptr) { return {}; }

  // Get the function-type name from the function: "func()"
  // => "$Func".
  const auto mapped_name = target_fn_name.ToFuncIdentifier();
  auto overload_scopes = Vec<FunctionOverload>();

  auto is_valid_ext_scope = [mapped_name=mapped_name.get()](auto const *scope) {
    const auto ext = AstAs<SupPrototypeExtensionAst>(scope->AstNode);
    if (ext == nullptr) { return false; }
    const auto ext_name = dynamic_shared_cast<TypeIdentifierAst>(ext->Name);
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
          .Proto = AstBody(sup_scope->AstNode)[0]->To<FunctionPrototypeAst>(),
          .SupGenerics = GenericArgumentGroupAst::NewEmpty(),
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
    // From the super scopes, check each one for the structure
    // "sup $Func ext FunXXX { ... }" super-imposition.
    // Todo: use the "is_valid_ext_scope"?
    const auto collect_from = [&](Scope const *const sup_scope) -> void {
      for (auto *sup_ast : AstBody(sup_scope->AstNode)
           | genex::views::cast_dynamic<SupPrototypeExtensionAst*>()) {
        if (sup_ast->Name->ToUnchecked<TypeIdentifierAst>()->Name == mapped_name->Val) {
          overload_scopes.EmplaceBack(FunctionOverload{
            .FnScope = sup_scope,
            .Proto = AstBody(sup_ast)[0]->To<FunctionPrototypeAst>(),
            .SupGenerics = MakeUnique<GenericArgumentGroupAst>(nullptr, sup_scope->GetGenerics(), nullptr),
            .FwdType = nullptr
          });
        }
      }
    };

    // A class scope contributes every super scope attached to it, read from its own list rather than copied out of it;
    // anything else contributes itself, exclusively. Each "Scope*" converts to "Scope const*" as it is read, which a
    // vector of one cannot do for a vector of the other.
    if (AstAs<ClassPrototypeAst>(target_scope->AstNode) != nullptr) {
      for (auto const *sup_scope : target_scope->SupScopes()) { collect_from(sup_scope); }
    }
    else {
      collect_from(target_scope);
    }

    PruneOverriddenOverloads(overload_scopes, target_scope, sm, meta);
    NarrowToOwningBlock(overload_scopes, is_valid_ext_scope);
  }

  // Next, get scopes from "forwarding types" (ie FwdRef
  // and FwdMut return types). Forwarding is a fallback, so
  // the receiver's own methods shadow the forwarded-to ones;
  // without this, a type whose forwarded-to type also forwards
  // (eg "NonNull[Str]" -> "&Str" -> "&StrView") sees both
  // "fwd_ref" overloads and the forwarding call is ambiguous.
  if (target_scope->TySym != nullptr and meta->CurrentStage >= meta::CompilerStage::kAnalyseSemantics and
    overload_scopes.IsEmpty()) {
    // Either forwarding type carries the methods. "FwdMut" was bound and then never read, so a type superimposing only
    // "FwdMut" got no forwarded methods here, while "BuildFwdCall" would happily build a "fwd_mut()" call for it in
    // argument position - the two forwarding paths disagreed.
    const auto [fwd_ref_sym, fwd_mut_sym] = type_utils::GetFwdTypes(*target_scope->TySym, *sm.CurrentScope);
    const auto fwd_sym = fwd_ref_sym != nullptr ? fwd_ref_sym : fwd_mut_sym;
    if (fwd_sym != nullptr) {
      const auto inner_sym = fwd_sym->BoundTypeArg("T");
      auto inner_scopes = inner_sym != nullptr
        ? GetAllFunctionScopes(target_fn_name, inner_sym->LinkedScope, sm, meta)
        : Vec<FunctionOverload>{};
      for (auto &i : inner_scopes) {
        i.FwdType = AstCloneShared(inner_sym->FqName());
      }
      overload_scopes.AppendRange(std::move(inner_scopes));
    }
  }

  // Remove duplicate overloads that are the same pointer (ie
  // exact protos). Todo: Likely a bandaid over an issue.
  auto unique_overloads = Vec<FunctionOverload>();
  for (auto &&info : overload_scopes) {
    const auto already_seen = genex::any_of(unique_overloads, [&info](auto const &seen) {
      return seen.Proto == info.Proto and seen.FnScope == info.FnScope;
    });
    if (not already_seen) { unique_overloads.EmplaceBack(std::move(info)); }
  }

  // Return all the found function scopes.
  return unique_overloads;
}

auto spp::analyse::utils::func_utils::GetFunctionValueName(
  TypeRef const &type) -> Pair<Shared<IdentifierAst>, Scope const*> {
  // Only a function's own "$" mock names a function, and only held by value: a borrow of one is not that function.
  // A closure's mock is unnamed by definition (its function type is attached directly), and has no block to walk.
  const auto mock_sym = type.KindSym();
  if (mock_sym == nullptr or mock_sym->Kind != TypeKind::FunctionMock) { return {nullptr, nullptr}; }
  if (mock_sym->LinkedScope == nullptr) { return {nullptr, nullptr}; }

  // Iterate the scopes on the function type ie $Type,
  // extracting the function prototype out of it. Return
  // a match. Always only 1 sup-ext for the $Types, so
  // the name and scope are always correct here.
  for (auto const *ext_scope : mock_sym->LinkedScope->DirectSupScopes) {
    const auto proto = FunctionBlockOf(*ext_scope).second;
    if (proto == nullptr) { continue; }

    // A method's overloads can be spread over several "sup" blocks
    // of its owner, so a non-generic owner's class scope is named,
    // which gathers the overloads from all of them.
    const auto block = ext_scope->Parent;
    const auto block_node = block->AstNode;
    if (AstAs<SupPrototypeFunctionsAst>(block_node) != nullptr
      or AstAs<SupPrototypeExtensionAst>(block_node) != nullptr) {
      const auto owner_sym = block->GetTypeSymbol(AstName(block_node)->WithoutGenerics().get());
      if (owner_sym != nullptr and not owner_sym->IsTypeGeneric() and owner_sym->LinkedScope != nullptr
        and owner_sym->Type != nullptr and owner_sym->Type->GnParamGroup->Params.IsEmpty()) {
        return {proto->Name, owner_sym->LinkedScope};
      }
    }
    return {proto->Name, block};
  }

  // Failsafe against no matches (doubt this will ever
  // be reached even, but c++ type safety).
  return {nullptr, nullptr};
}

auto spp::analyse::utils::func_utils::MatchFunctionValue(
  TypeRef const &mock, TypeRef const &func,
  Scope const &func_scope) -> std::optional<FunctionValueMatch> {
  //
  using type_compare::RelaxedTypeEq;
  using type_compare::TypeEq;

  // The target has to be a function type held by value, and the
  // value a named function's mock.
  if (func.Sym == nullptr or func.IsBorrowed() or func.Sym->IsMock()
    or not type_predicates::IsTypeFunc(func, func_scope)
    or GetFunctionValueName(mock).first == nullptr) {
    return std::nullopt;
  }

  // The mock carries the superimposed function type of each overload.
  const auto mock_sym = mock.Sym;
  const auto func_type_ast = func.Sym->FqName();
  auto const &func_type = *func_type_ast;

  // Each overload attaches its own "sup $F ext FunXxx { fun ... }"
  // block to the mock. Its function type is compared along with the
  // ones above it, as a "FunRef" is also a "FunMut" and a "FunMov".
  auto match = std::optional<FunctionValueMatch>();
  for (auto const *ext_scope : mock_sym->LinkedScope->DirectSupScopes) {
    const auto [ext, proto] = FunctionBlockOf(*ext_scope);
    const auto kind_sym = proto != nullptr
      ? ext_scope->GetTypeSymbol(ext->SuperClass->WithoutGenerics().get())
      : nullptr;
    if (kind_sym == nullptr or kind_sym->LinkedScope == nullptr) { continue; }

    // The kind is compared bare, so it resolves anywhere; the
    // signature where the overload's own generics do.
    auto const *target_kind = func_scope.GetTypeSymbol(func_type.WithoutConvention()->WithoutGenerics().get());
    if (target_kind == nullptr) { continue; }
    auto kinds = Vec<Scope const*>{kind_sym->LinkedScope};
    kinds.AppendRange(kind_sym->LinkedScope->SupScopes());
    if (not genex::any_of(kinds, [&](auto const *kind) {
      return kind->TySym != nullptr
        and type_predicates::TemplateOf(*kind->TySym, *kind) == type_predicates::TemplateOf(*target_kind, func_scope);
    })) { continue; }

    const auto own_generics = ext->SuperClass->LastTypePart()->GnArgGroup.get();
    const auto target_generics = func_type.WithoutConvention()->LastTypePart()->GnArgGroup.get();
    const auto own_args = own_generics->At("Args");
    const auto target_args = target_generics->At("Args");
    const auto own_out = own_generics->At("Out");
    const auto target_out = target_generics->At("Out");
    if (own_args == nullptr or own_out == nullptr or target_args == nullptr or target_out == nullptr) { continue; }

    // A generic overload binds its own generics off the target,
    // which is why its own signature is on the right. Either way
    // the signature is then checked exactly, as the inference
    // accepts a generic bound twice.
    auto const &own_params = proto->GnParamGroup->Params;
    auto inferred = type_compare::GenericInferenceMap();
    if (not own_params.IsEmpty() and (
      not RelaxedTypeEq(*target_args->TypeVal, *own_args->TypeVal, func_scope, *ext_scope, inferred)
      or not RelaxedTypeEq(*target_out->TypeVal, *own_out->TypeVal, func_scope, *ext_scope, inferred))) { continue; }

    auto own_inferred = type_compare::GenericInferenceMap();
    for (auto const &[name, val] : inferred) {
      if (genex::any_of(own_params, [&](auto const &p) { return *p->Name == *name; })) {
        own_inferred.insert({name, val});
      }
    }
    if (own_inferred.size() != own_params.Len()) { continue; }

    auto generic_args = GenericArgumentGroupAst::FromMap(own_inferred);
    if (not TypeEq(
        *own_args->TypeVal->SubstituteGenerics(generic_args->GetAllArgs()),
        *target_args->TypeVal, *ext_scope, func_scope)
      or not TypeEq(
        *own_out->TypeVal->SubstituteGenerics(generic_args->GetAllArgs()),
        *target_out->TypeVal, *ext_scope, func_scope)) {
      continue;
    }

    // A non-generic overload wins outright; a generic one only if
    // none does.
    if (own_params.IsEmpty()) {
      return FunctionValueMatch{.Proto = proto, .FnScope = ext_scope, .GenericArgs = std::move(generic_args)};
    }
    if (not match.has_value()) {
      match = FunctionValueMatch{.Proto = proto, .FnScope = ext_scope, .GenericArgs = std::move(generic_args)};
    }
  }
  return match;
}

auto spp::analyse::utils::func_utils::InstantiateFunctionValue(
  TypeRef const &value, TypeRef const &target,
  ScopeManager *sm, meta::CompilerMetaData *meta) -> void {
  const auto match = MatchFunctionValue(value, target, *sm->CurrentScope);
  if (not match.has_value() or match->GenericArgs->Args.IsEmpty()) { return; }
  auto tm = ScopeManager(sm->GlobalScope, const_cast<Scope*>(match->FnScope));
  overload_utils::InstantiateOverload(match->Proto, match->FnScope, *match->GenericArgs, &tm, meta);
}

auto spp::analyse::utils::func_utils::FindFunctionValue(
  TypeRef const &value, TypeRef const &target,
  ScopeManager const &sm) -> FunctionPrototypeAst* {
  const auto match = MatchFunctionValue(value, target, *sm.CurrentScope);
  if (not match.has_value()) { return nullptr; }
  if (match->GenericArgs->Args.IsEmpty()) { return match->Proto; }
  return overload_utils::FindInstantiatedOverload(match->Proto, *match->GenericArgs, &sm);
}

auto spp::analyse::utils::func_utils::CheckForConflictingOverload(
  Scope const &this_scope, Scope const *target_scope,
  FunctionPrototypeAst const &new_fn, ScopeManager &sm,
  meta::CompilerMetaData *meta) -> FunctionPrototypeAst* {
  //
  using type_compare::TypeEq;

  // Get the methods that belong to this type, or any
  // of its supertypes.
  const auto existing = GetAllFunctionScopes(*new_fn.Name, target_scope, sm, meta);
  const auto new_sup = _SupBlockOf(new_fn);

  // Check for an overload conflict with all functions
  // of the same name.
  for (auto const &[old_scope, old_fn, _, _] : existing) {
    // Ignore if the method is an identical match on a
    // base class (override) or is the same object.
    if (old_fn == &new_fn) { continue; }
    if (old_fn == CheckForConflictingOverride(this_scope, old_scope, new_fn, sm, meta, old_scope)) { continue; }

    // Ignore if the two methods come from "sup" blocks that
    // are disjoint specializations of the same type, such as
    // "sup [T: Integer] Atom[T]" against "sup [T: FloatingPoint]
    // Atom[T]". These never apply to the same instantiation,
    // so they aren't overloads of each other.
    if (not _SupBlocksOverlap(new_sup, _SupBlockOf(*old_fn))) { continue; }

    // Ignore if the return types are different.
    if (not TypeEq(*new_fn.ReturnType, *old_fn->ReturnType, this_scope, *old_scope)) { continue; }

    // Ignore if there are a different number of required
    // generic parameters.
    if (new_fn.GnParamGroup->GetTypeParams().Len() != old_fn->GnParamGroup->GetTypeParams().Len()) { continue; }
    if (new_fn.GnParamGroup->GetCompParams().Len() != old_fn->GnParamGroup->GetCompParams().Len()) { continue; }

    // Get the two parameter lists and create copies to
    // remove duplicate parameters from.
    auto params_new = AstCloneVec(new_fn.FnParamGroup->Params);
    auto params_old = AstCloneVec(old_fn->FnParamGroup->Params);

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
      | genex::views::cast_dynamic<FunctionParameterRequiredAst*>()
      | genex::to<Vec>())) {
      return old_fn;
    }
  }
  return nullptr;
}

auto spp::analyse::utils::func_utils::SameSignature(
  FunctionPrototypeAst const &fn_a, Scope const &scope_a,
  FunctionPrototypeAst const &fn_b, Scope const &scope_b) -> bool {
  //
  using type_compare::TypeEq;

  // Helper function to check whether a "self" parameter
  // is present.
  auto hs = [](FunctionPrototypeAst const *f) {
    return f->FnParamGroup->GetSelfParam() != nullptr;
  };

  // Helper function to get the type of the convention AST
  // applied to the "self" parameter.
  auto sc = [&hs](FunctionPrototypeAst const *f) {
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
  Scope const &this_scope, Scope const *target_scope,
  FunctionPrototypeAst const &new_fn, ScopeManager &sm,
  meta::CompilerMetaData *meta, Scope const *exclude_scope)
  -> FunctionPrototypeAst* {
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

auto spp::analyse::utils::func_utils::NameFnArgs(
  FunctionCallArgumentGroupAst &a_group, FunctionParameterGroupAst const &p_group,
  ScopeManager &sm, meta::CompilerMetaData *const meta,
  Vec<GenericArgumentAst*> const &generic_args, Scope *const callee_scope)
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
    // Create the keyword argument from the positional argument. It
    // is named after the parameter, but placed where the argument
    // was written, as the parameter's own name is in the callee.
    auto kw_arg = MakeUnique<FunctionCallArgumentKeywordAst>(
      MakeShared<IdentifierAst>(positional_arg->PosStart(), Str(p_names.Front()->Val)), nullptr, nullptr, nullptr);
    p_names |= genex::actions::pop_front();

    // The variadic parameter requires a tuple of the remaining arguments.
    // Todo: The pack drops each argument's convention, because a tuple literal has no way to hold a borrow (borrows
    //  are second-class, so "(&x, 1)" is a syntax error). That makes "Ts" infer as "Tup[S32]" where the callee sees
    //  "&S32", which is what stops "async a(&x)" resolving against "F: FunMov[(Ts), T]" - the mock's own function
    //  type keeps the convention. It also means the pack is never memory-checked, so a moved-from argument passed
    //  variadically twice goes unreported.
    if (p_names.IsEmpty() and is_variadic) {
      auto elems = a_group.Args
        | genex::views::move
        | genex::views::drop(i)
        | genex::views::transform([](auto &&x) { return asts::AstClone(x->Val); })
        | genex::to<Vec>();
      kw_arg->Val = MakeUnique<TupleLiteralAst>(nullptr, std::move(elems), nullptr);
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
  auto ordered_args = Vec<Unique<FunctionCallArgumentAst>>();
  for (auto const *param : p_group.GetAllParams()) {
    const auto param_name = param->ExtractName();

    auto matched = false;
    for (auto &&arg : a_group.Args) {
      const auto kw_arg = arg != nullptr
        ? arg->To<FunctionCallArgumentKeywordAst>()
        : nullptr;

      if (kw_arg == nullptr or kw_arg->Name->Val != param_name->Val) { continue; }
      ordered_args.EmplaceBack(std::move(arg));
      matched = true;
      break;
    }
    if (matched) { continue; }

    // A variadic parameter the call gave nothing to must receive
    // an empty pack. This is because if we have a variadic generic
    // for a variadic function parameter: `fun f[..Ts](..a: Ts)` -
    // then no argument leaves `Ts` as "uninferred". Instead, force
    // `Tup[]`.
    if (param->To<FunctionParameterVariadicAst>() != nullptr) {
      auto empty_pack = MakeUnique<TupleLiteralAst>(
        nullptr, Vec<Unique<ExpressionAst>>(), nullptr);
      ordered_args.EmplaceBack(MakeUnique<FunctionCallArgumentKeywordAst>(
        param_name, nullptr, nullptr, std::move(empty_pack)));
      continue;
    }

    // Leftover optional parameters inject their argument into the
    // call site (unlike Python, which executes once for all func
    // calls).
    const auto optional_param = param->To<FunctionParameterOptionalAst>();
    if (optional_param == nullptr or optional_param->DefaultVal == nullptr) { continue; }

    // Translate the default out of the callee's terms as it is
    // materialised. The parameter's own type is substituted when
    // the instantiation's prototype is built, and its default is
    // an expression, so it needs the expression-level walk for
    // the same reason - otherwise "alloc: A = A()" arrives here
    // as an "A()" the caller has no "A" for.
    auto const &written = optional_param->Source.OriginalDefaultVal != nullptr
      ? optional_param->Source.OriginalDefaultVal
      : optional_param->DefaultVal;
    auto default_val = generic_args.IsEmpty()
      ? AstClone(optional_param->DefaultVal)
      : AstClone(written->SubstituteGenericsExpr(generic_args));

    // Analyse the substitution where the default was written.
    if (not generic_args.IsEmpty() and meta != nullptr) {
      const auto outer_scope = sm.CurrentScope;
      if (callee_scope != nullptr) { sm.CurrentScope = callee_scope; }
      default_val->Stage7_AnalyseSemantics(&sm, meta);
      sm.CurrentScope = outer_scope;
    }

    ordered_args.EmplaceBack(MakeUnique<FunctionCallArgumentKeywordAst>(
      param_name, nullptr, nullptr, std::move(default_val)));
  }
  a_group.Args = std::move(ordered_args);
}

auto spp::analyse::utils::func_utils::IsTargetCallable(
  ExpressionAst &expr, ScopeManager &sm,
  meta::CompilerMetaData *meta) -> Shared<const TypeAst> {
  // Get the type of the expression, then find its functional
  // type. The functional type is the "FunRef|FunMut|FunMov" the
  // expression is or superimposes - a generic gets its one from
  // the constraints - and is null for anything not callable,
  // which the caller reports as "no valid signatures".
  using type_utils::GetFunctionalType;

  // A parameter declared against a generic is called through
  // the interface its constraint promised, recorded on the
  // symbol when the instantiation was made. Read before the
  // type, which by then is whatever the generic was substituted
  // with.
  if (const auto sym = sm.CurrentScope->GetVarSymbolOutermost(expr).first;
    sym != nullptr and sym->CallableAsType != nullptr) {
    return sym->CallableAsType;
  }

  const auto expr_type = expr.InferType(&sm, meta);
  return GetFunctionalType(*expr_type, *sm.CurrentScope);
}
