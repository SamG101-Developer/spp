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
import spp.analyse.utils.type_compare;
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
    /**
     * Temporarily re-parent a scope, putting the original parent back however the enclosing block is left - including by
     * a thrown semantic error.
     *
     * Overload resolution catches per-candidate exceptions, so a bare restore after a call that raises leaves the scope
     * tree permanently wrongly-parented: every later lookup through that scope silently resolves against the wrong
     * ancestors, with no failure at the point the damage is done.
     */
    struct ScopeParentSwap {
      scopes::Scope *Scope;
      scopes::Scope *Original;

      ScopeParentSwap(
        scopes::Scope *const scope,
        scopes::Scope *const replacement) :
        Scope(scope), Original(scope->Parent) {
        scope->Parent = replacement;
        scopes::BumpScopeLinkageGeneration();
      }

      ~ScopeParentSwap() {
        Scope->Parent = Original;
        scopes::BumpScopeLinkageGeneration();
      }

      ScopeParentSwap(ScopeParentSwap const&) = delete;
      ScopeParentSwap(ScopeParentSwap&&) = delete;
      auto operator=(ScopeParentSwap const&) -> ScopeParentSwap& = delete;
      auto operator=(ScopeParentSwap&&) -> ScopeParentSwap& = delete;
    };

    /**
     * Get the "sup" block a function prototype was declared in, or @c nullptr for a free function (whose context is the
     * module prototype, which superimposes nothing).
     */
    auto _SupBlockOf(
      asts::FunctionPrototypeAst const &fn)
      -> asts::Ast* {
      auto *ctx = fn.GetAstCtx();
      if (ctx == nullptr) { return nullptr; }
      const auto is_sup = ctx->To<asts::SupPrototypeFunctionsAst>() != nullptr
        or ctx->To<asts::SupPrototypeExtensionAst>() != nullptr;
      return is_sup ? ctx : nullptr;
    }

    /**
     * Determine whether two "sup" blocks can ever apply to the same instantiation. Blocks over the same generic type can
     * carry disjoint constraints ("sup [T: Integer] Atom[T]" against "sup [T: FloatingPoint] Atom[T]"), which makes them
     * specializations that never both attach to one type, so their members never see each other.
     */
    auto _SupBlocksOverlap(
      asts::Ast *const sup_a,
      asts::Ast *const sup_b)
      -> bool {
      // Free functions, and members of one block, always share a context.
      if (sup_a == nullptr or sup_b == nullptr or sup_a == sup_b) { return true; }

      auto generics = type_compare::GenericInferenceMap();
      return type_compare::RelaxedTypeEq(
        *asts::AstName(sup_a), *asts::AstName(sup_b),
        *sup_a->GetAstScope(), *sup_b->GetAstScope(), generics);
    }

    auto EnforceNoInvalidFnArgs(
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

      // Raise an error if any invalid argument names were found. The context is the first parameter, but there may not
      // be one - "fun f()" called as "f(x=1)" has an invalid name and nothing to point at - so fall back to the
      // offending argument rather than indexing an empty list. Written as a guarded raise because the fallback itself
      // reads "invalid_arg_names[0]", which only "RaiseIf"'s lazy error arguments would have protected.
      if (not invalid_arg_names.IsEmpty()) {
        auto const *const param_ctx = params.IsEmpty()
          ? static_cast<asts::Ast const*>(invalid_arg_names[0].get())
          : static_cast<asts::Ast const*>(params[0]);
        Raise<SppArgumentNameInvalidError>(
          {sm.CurrentScope}, ERR_ARGS(*param_ctx, "fn param", *invalid_arg_names[0], "fn arg"));
      }
    }

    /**
     * Drop base-class overloads that a derived class has overridden, so that a call reaches the derived version. The
     * depth difference from the scope the lookup started at is what identifies which of a pair is the derived one.
     * @param overload_scopes The overloads found so far, pruned in place.
     * @param target_scope The scope the lookup started from, which depths are measured against.
     * @param sm The scope manager.
     * @param meta Associated metadata.
     */
    auto PruneOverriddenOverloads(
      Vec<FunctionOverload> &overload_scopes,
      scopes::Scope const *target_scope,
      scopes::ScopeManager &sm,
      asts::meta::CompilerMetaData *meta)
      -> void {
      for (auto const &o1 : overload_scopes) {
        for (auto const &o2 : overload_scopes) {
          // This depth difference checker ensures the derived version is kept.
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
    }

    /**
     * Point each overload at the "sup" block that actually declares it, rather than the scope it was found through.
     * @param overload_scopes The overloads to adjust in place.
     * @param is_valid_ext_scope Predicate picking the candidate "sup" blocks out of a scope's children.
     */
    auto NarrowToOwningBlock(
      Vec<FunctionOverload> &overload_scopes,
      auto const &is_valid_ext_scope)
      -> void {
      for (auto &info : overload_scopes) {
        const auto blocks = info.FnScope->Children
          | genex::views::ptr
          | genex::views::filter(is_valid_ext_scope)
          | genex::to<Vec>();

        auto owning_block = static_cast<scopes::Scope const*>(nullptr);
        for (auto const *block : blocks) {
          auto const body = asts::AstBody(block->AstNode);
          if (not body.IsEmpty() and body[0]->template To<asts::FunctionPrototypeAst>() == info.Proto) {
            owning_block = block;
            break;
          }
        }

        // "Any block will do" as a fallback, but there may be none at all, in which case the scope stays as it was.
        if (owning_block == nullptr and not blocks.IsEmpty()) { owning_block = blocks[0]; }
        if (owning_block != nullptr) { info.FnScope = owning_block; }
      }
    }
  }
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
    const auto ext = AstAs<asts::SupPrototypeExtensionAst>(scope->AstNode);
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
    const auto sup_scopes = AstAs<asts::ClassPrototypeAst>(target_scope->AstNode) != nullptr
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

    PruneOverriddenOverloads(overload_scopes, target_scope, sm, meta);
    NarrowToOwningBlock(overload_scopes, is_valid_ext_scope);
  }

  // Next, get scopes from "forwarding types" (ie FwdRef
  // and FwdMut return types). Forwarding is a fallback, so
  // the receiver's own methods shadow the forwarded-to ones;
  // without this, a type whose forwarded-to type also forwards
  // (eg "NonNull[Str]" -> "&Str" -> "&StrView") sees both
  // "fwd_ref" overloads and the forwarding call is ambiguous.
  if (target_scope->TySym != nullptr and meta->CurrentStage >= asts::meta::CompilerStage::kAnalyseSemantics and overload_scopes.IsEmpty()) {
    // Either forwarding type carries the methods. "FwdMut" was bound and then never read, so a type superimposing only
    // "FwdMut" got no forwarded methods here, while "BuildFwdCall" would happily build a "fwd_mut()" call for it in
    // argument position - the two forwarding paths disagreed.
    auto [fwd_ref_type, fwd_mut_type] = type_utils::GetFwdTypes(*target_scope->TySym->FqName(), sm);
    const auto fwd_type = fwd_ref_type != nullptr ? fwd_ref_type : fwd_mut_type;
    if (fwd_type != nullptr) {
      const auto inner_type = fwd_type->LastTypePart()->GnArgGroup->TypeAt("T")->Val;
      const auto inner_sym = sm.CurrentScope->GetTypeSymbol(inner_type.get());
      auto inner_scopes = inner_sym != nullptr
        ? GetAllFunctionScopes(target_fn_name, inner_sym->LinkedScope, sm, meta)
        : Vec<FunctionOverload>{};
      for (auto &i : inner_scopes) {
        i.FwdType = asts::AstCloneShared(inner_type);
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

auto spp::analyse::utils::func_utils::CheckForConflictingOverload(
  scopes::Scope const &this_scope,
  scopes::Scope const *target_scope,
  asts::FunctionPrototypeAst const &new_fn,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> asts::FunctionPrototypeAst* {
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
  using type_compare::TypeEq;

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

auto spp::analyse::utils::func_utils::NameFnArgs(
  asts::FunctionCallArgumentGroupAst &a_group,
  asts::FunctionParameterGroupAst const &p_group,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *const meta,
  Vec<asts::GenericArgumentAst*> const &generic_args,
  scopes::Scope *const callee_scope)
  -> void {
  //
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
  auto ordered_args = Vec<Unique<asts::FunctionCallArgumentAst>>();
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

    // A variadic parameter the call gave nothing to must receive
    // an empty pack. This is because if we have a variadic generic
    // for a variadic function parameter: `fun f[..Ts](..a: Ts)` -
    // then no argument leaves `Ts` as "uninferred". Instead, force
    // `Tup[]`.
    if (param->To<asts::FunctionParameterVariadicAst>() != nullptr) {
      auto empty_pack = MakeUnique<asts::TupleLiteralAst>(
        nullptr, Vec<Unique<asts::ExpressionAst>>(), nullptr);
      ordered_args.EmplaceBack(MakeUnique<asts::FunctionCallArgumentKeywordAst>(
        param_name, nullptr, nullptr, std::move(empty_pack)));
      continue;
    }

    // Leftover optional parameters inject their argument into the
    // callsite (unlike Python, which executes once for all func
    // calls).
    const auto optional_param = param->To<asts::FunctionParameterOptionalAst>();
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
      ? asts::AstClone(optional_param->DefaultVal)
      : asts::AstClone(written->SubstituteGenericsExpr(generic_args));

    // Analyse the substitution where the default was written.
    if (not generic_args.IsEmpty() and meta != nullptr) {
      const auto outer_scope = sm.CurrentScope;
      if (callee_scope != nullptr) { sm.CurrentScope = callee_scope; }
      default_val->Stage7_AnalyseSemantics(&sm, meta);
      sm.CurrentScope = outer_scope;
    }

    ordered_args.EmplaceBack(MakeUnique<asts::FunctionCallArgumentKeywordAst>(
      param_name, nullptr, nullptr, std::move(default_val)));
  }
  a_group.Args = std::move(ordered_args);
}

auto spp::analyse::utils::func_utils::IsTargetCallable(
  asts::ExpressionAst &expr,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *meta)
  -> Shared<const asts::TypeAst> {
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
