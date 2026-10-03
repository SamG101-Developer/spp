module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.fn_values;
import spp.analyse.scopes.instance_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.overload_resolution;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.utils.algorithms;
import spp.utils.interner;
import spp.utils.ptr;
import spp.utils.types;
import genex;

namespace spp::analyse::utils::fn_values {
  namespace {
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
  }
}

auto spp::analyse::utils::fn_values::FnBlockOf(
  Scope const &scope) -> Pair<SupPrototypeExtensionAst*, FunctionPrototypeAst*> {
  const auto ext = AstAs<SupPrototypeExtensionAst>(scope.AstNode);
  const auto body = ext != nullptr ? AstBody(ext) : Vec<Ast*>();
  const auto proto = not body.IsEmpty() ? body[0]->To<FunctionPrototypeAst>() : nullptr;
  return {proto != nullptr ? ext : nullptr, proto};
}

auto spp::analyse::utils::fn_values::GetFnValueName(
  TypeRef const &type) -> Pair<Shared<IdentifierAst>, Scope const*> {
  // Only a function's own "$" mock names a function, and only held by value: a borrow of one is not that function.
  // A closure's mock is unnamed by definition (its function type is attached directly), and has no block to walk.
  const auto mock_sym = type.KindSymbol();
  if (mock_sym == nullptr or mock_sym->Kind != TypeKind::FnMock) { return {nullptr, nullptr}; }
  if (mock_sym->LinkedScope == nullptr) { return {nullptr, nullptr}; }

  // Iterate the scopes on the function type ie $Type,
  // extracting the function prototype out of it. Return
  // a match. Always only 1 sup-ext for the $Types, so
  // the name and scope are always correct here.
  for (auto const *ext_scope : mock_sym->LinkedScope->DirectSupScopes) {
    const auto proto = FnBlockOf(*ext_scope).second;
    if (proto == nullptr) { continue; }

    // A method's overloads can be spread over several "sup" blocks
    // of its owner, so a non-generic owner's class scope is named,
    // which gathers the overloads from all of them.
    const auto block = ext_scope->Parent;
    const auto block_node = block->AstNode;
    if (AstAs<SupPrototypeFunctionsAst>(block_node) != nullptr
      or AstAs<SupPrototypeExtensionAst>(block_node) != nullptr) {
      const auto owner_sym = block->FindHeadSymbol(*AstName(block_node));
      if (owner_sym != nullptr and not owner_sym->IsGn() and owner_sym->LinkedScope != nullptr
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

auto spp::analyse::utils::fn_values::MatchFnValue(
  TypeRef const &mock, TypeRef const &func,
  Scope const &func_scope) -> std::optional<FnValueMatch> {
  //
  using type_compare::RelaxedTypeEq;
  using type_compare::TypeEq;

  // The target has to be a function type held by value, and the
  // value a named function's mock.
  if (func.Symbol == nullptr or func.IsBorrowed() or func.Symbol->IsMock()
    or not type_predicates::IsTypeFunction(func, func_scope)
    or GetFnValueName(mock).first == nullptr) {
    return std::nullopt;
  }

  // The mock carries the superimposed function type of each overload.
  const auto mock_sym = mock.Symbol;
  const auto func_type_ast = func.Symbol->FqName();
  auto const &func_type = *func_type_ast;

  // Each overload attaches its own "sup $F ext FunXxx { fun ... }"
  // block to the mock. Its function type is compared along with the
  // ones above it, as a "FunRef" is also a "FunMut" and a "FunMov".
  auto match = std::optional<FnValueMatch>();
  for (auto const *ext_scope : mock_sym->LinkedScope->DirectSupScopes) {
    const auto [ext, proto] = FnBlockOf(*ext_scope);
    const auto kind_sym = proto != nullptr
      ? ext_scope->FindHeadSymbol(*ext->SuperCls)
      : nullptr;
    if (kind_sym == nullptr or kind_sym->LinkedScope == nullptr) { continue; }

    // The kind is compared bare, so it resolves anywhere; the
    // signature where the overload's own generics do.
    auto const *target_kind = func_scope.FindHeadSymbol(func_type);
    if (target_kind == nullptr) { continue; }
    auto const *const target_tmpl = TypeRef::OfKind(*target_kind, func_scope).Template();
    auto kinds = Vec<Scope const*>{kind_sym->LinkedScope};
    kinds.AppendRange(kind_sym->LinkedScope->GetSupScopes());
    if (not genex::any_of(kinds, [&](auto const *kind) {
      return kind->LinkedTypeSymbol != nullptr
        and TypeRef::OfKind(*kind).Template() == target_tmpl;
    })) { continue; }

    const auto own_generics = ext->SuperCls->LastTypePart()->GnArgGroup.get();
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

    // Its own signature with what it inferred bound, read where the target is.
    const auto bindings = type_resolution::BindInferred(own_inferred, *proto->GnParamGroup, func_scope);
    const auto read = [&](TypeAst const &own) { return type_resolution::ReadType(own, ExprSubst::Across(*ext_scope, bindings, func_scope)); };
    if (not type_compare::Assignable(*read(*own_args->TypeVal), *target_args->TypeVal, func_scope, func_scope)
      or not type_compare::Assignable(*read(*own_out->TypeVal), *target_out->TypeVal, func_scope, func_scope)) {
      continue;
    }

    // A non-generic overload wins outright; a generic one only if
    // none does.
    auto found = FnValueMatch{
      .Proto = proto, .FnScope = ext_scope, .GnArgs = GenericArgumentGroupAst::FromMap(own_inferred)};
    if (own_params.IsEmpty()) { return found; }
    if (not match.has_value()) { match = std::move(found); }
  }
  return match;
}

auto spp::analyse::utils::fn_values::InstantiateFnValue(
  TypeRef const &value, TypeRef const &target,
  ScopeManager *sm, meta::CompilerMetaData *meta) -> void {
  const auto match = MatchFnValue(value, target, *sm->CurrentScope);
  if (not match.has_value() or match->GnArgs->Args.IsEmpty()) { return; }
  auto tm = ScopeManager(sm->GlobalScope, const_cast<Scope*>(match->FnScope));
  monomorphization::InstantiateOverload(match->Proto, match->FnScope, *match->GnArgs, &tm, meta);
}

auto spp::analyse::utils::fn_values::FindFnValue(
  TypeRef const &value, TypeRef const &target,
  ScopeManager const &sm) -> FunctionPrototypeAst* {
  const auto match = MatchFnValue(value, target, *sm.CurrentScope);
  if (not match.has_value()) { return nullptr; }
  if (match->GnArgs->Args.IsEmpty()) { return match->Proto; }
  return monomorphization::FindInstantiatedOverload(match->Proto, *match->GnArgs, &sm);
}

namespace spp::analyse::utils::fn_values {
  namespace {
    /**
     * How to read two functions' types in "fn_a"'s terms, as substitutions over "TypeId"s - the first for "fn_a"'s own
     * types, the second for "fn_b"'s. "Self" is the type implementing them both (the class "fn_a" is written for).
     * "fn_b"'s own generic parameters are "fn_a"'s, by position: a signature is the same up to renaming them. "fn_b"'s
     * block's parameters are what the class "fn_a" is written for binds them to, where "fn_b" leaves them unbound: its
     * block is matched against the super class the implementer extends, as a receiver binds a block's generics
     * ("sup [Rhs] Eq[Rhs]" against "ext Eq", which is "Eq[Rhs=Self]" with its default filled); and a parameter "fn_b"'s
     * scope binds to "Self" ("Ret" of "BitAnd[Rhs=Self, Ret=Self]") is the implementer too. None when their own
     * generics differ in kind, or a part does not key.
     */
    auto IdTermsOf(
      FunctionPrototypeAst const &fn_a, Scope const &scope_a,
      FunctionPrototypeAst const &fn_b, Scope const &scope_b)
      -> std::optional<Pair<GenericSubst, GenericSubst>> {
      auto subst_a = GenericSubst();
      auto subst_b = GenericSubst();

      auto const *const a_self = scope_a.FindSelfSymbol();
      auto const *const a_cls = a_self != nullptr ? a_self->AsBound() : nullptr;
      const auto self_id = a_cls != nullptr and a_cls->Type != nullptr
        ? BareTypeId(scope_a.TypeIdOfSymbol(*a_cls, 0))
        : nullptr;
      if (self_id != nullptr) {
        subst_a.TypeParams.emplace_back(0, self_id);
        subst_b.TypeParams.emplace_back(0, self_id);
      }

      // "fn_b"'s parameter named "name": a parameter, or a binding of one to nothing yet; one bound to a type is keyed
      // as that type already, so has nothing to rename.
      const auto b_param = [&](TypeIdentifierAst const &name) -> std::uint64_t {
        auto const *const param = scope_b.FindTypeSymbol(&name);
        if (param == nullptr or param->ParamId() == 0) { return 0; }
        if (param->Kind == TypeKind::GnTypeArg and param->AsBound() != param) { return 0; }
        return param->ParamId();
      };
      const auto bind_b = [&](const std::uint64_t pid, const TypeId value) {
        if (pid == 0 or value == nullptr) { return; }
        for (auto const &[p, _] : subst_b.TypeParams) { if (p == pid) { return; } }
        subst_b.TypeParams.emplace_back(pid, value);
      };

      const auto own_params = [](FunctionPrototypeAst const &f) {
        return f.GnParamGroup->GetAllParams()
          | genex::views::filter([](auto const *p) { return not p->IsInherited; })
          | genex::to<Vec>();
      };
      const auto own_a = own_params(fn_a);
      const auto own_b = own_params(fn_b);
      for (auto i = 0uz; i < own_a.Len() and i < own_b.Len(); ++i) {
        if (own_a[i]->IsTypeParam() != own_b[i]->IsTypeParam()) { return std::nullopt; }
        if (own_b[i]->IsTypeParam()) {
          auto const *const a_param = scope_a.FindTypeSymbol(own_a[i]->Name.get());
          if (a_param == nullptr) { continue; }
          bind_b(b_param(*own_b[i]->Name->ToUnchecked<TypeIdentifierAst>()), scope_a.TypeIdOfSymbol(*a_param, 0));
          continue;
        }
        auto const *const b_var = scope_b.FindVarSymbol(IdentifierAst::FromType(*own_b[i]->Name).get());
        auto const *const a_var = scope_a.FindVarSymbol(IdentifierAst::FromType(*own_a[i]->Name).get());
        if (b_var == nullptr or b_var->ParamId() == 0 or a_var == nullptr) { continue; }
        subst_b.CompParams.emplace_back(b_var->ParamId(), scope_a.CompIdOfSymbol(*a_var));
      }

      const auto enclosing = [](Scope const &scope, auto const pred) -> Ast* {
        for (auto const *s = &scope; s != nullptr; s = s->Parent) {
          if (s->AstNode != nullptr and pred(*s->AstNode)) { return s->AstNode; }
        }
        return nullptr;
      };
      // A method's own "sup $F ext FunXxx" block is not the one it was written in.
      const auto is_sup = [](Ast const &n) {
        auto *const node = const_cast<Ast*>(&n);
        return (n.To<SupPrototypeFunctionsAst>() != nullptr or n.To<SupPrototypeExtensionAst>() != nullptr)
          and not AstName(node)->IsCompilerGeneratedType();
      };
      auto *const b_block = enclosing(scope_b, is_sup);
      auto *const a_ext_node = enclosing(scope_a, [&is_sup](Ast const &n) {
        return is_sup(n) and n.To<SupPrototypeExtensionAst>() != nullptr;
      });
      auto const *const a_ext = a_ext_node != nullptr ? a_ext_node->To<SupPrototypeExtensionAst>() : nullptr;
      if (self_id != nullptr and b_block != nullptr and a_ext != nullptr) {
        auto const *const super_sym = TypeRef::Of(*a_ext->SuperCls, scope_a).Symbol;
        auto inferred = type_compare::GenericInferenceMap();
        if (super_sym != nullptr and type_compare::RelaxedTypeEq(
          *super_sym->FqName(), *AstName(b_block), scope_a, scope_b, inferred, false, false)) {
          for (auto const &[name, val] : inferred) {
            auto const *const typed = val->To<TypeAst>();
            if (typed == nullptr) { continue; }
            bind_b(b_param(*name), typed->IsSelfType() ? self_id : scope_a.TypeIdOf(*typed));
          }
        }
      }
      if (self_id != nullptr) {
        for (auto &generic : scope_b.GetGns()) {
          if (generic->TypeName() == nullptr or generic->IsCompArg() or not generic->TypeVal->IsSelfType()) {
            continue;
          }
          bind_b(b_param(*generic->TypeName()->LastTypePart()), self_id);
        }
      }
      return Pair<GenericSubst, GenericSubst>{std::move(subst_a), std::move(subst_b)};
    }
  }
}

auto spp::analyse::utils::fn_values::CheckForConflictingOverload(
  Scope const &this_scope, Scope const *target_scope,
  FunctionPrototypeAst const &new_fn, ScopeManager &sm,
  meta::CompilerMetaData *meta) -> FunctionPrototypeAst* {
  //

  // Get the methods that belong to this type, or any
  // of its supertypes.
  const auto existing = overload_resolution::GetAllFnScopes(*new_fn.Name, target_scope, sm, meta);
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

    // Ignore if there are a different number of required
    // generic parameters.
    if (new_fn.GnParamGroup->GetTypeParams().Len() != old_fn->GnParamGroup->GetTypeParams().Len()) { continue; }
    if (new_fn.GnParamGroup->GetCompParams().Len() != old_fn->GnParamGroup->GetCompParams().Len()) { continue; }

    // The two are compared with the old one's generics renamed to the new one's ("f[T](a: T)" and "f[U](a: U)"
    // conflict), and parameters with either borrow as the same: a call cannot choose between "&Bool" and "&mut Bool",
    // but can between "T" and "&T". A return type keeps its convention, as return types overload.
    const auto terms = IdTermsOf(new_fn, this_scope, *old_fn, *old_scope);
    if (not terms.has_value()) { continue; }
    auto const &subst_new = terms->first;
    auto const &subst_old = terms->second;
    const auto id_of = [&](TypeAst const &new_type, TypeAst const &old_type) {
      return Pair<TypeId, TypeId>{
        SubstituteTypeId(this_scope.TypeIdOf(new_type), subst_new),
        SubstituteTypeId(old_scope->TypeIdOf(old_type), subst_old)};
    };
    const auto same = [&](TypeAst const &new_type, TypeAst const &old_type) {
      if ((new_type.GetConvention() == nullptr) != (old_type.GetConvention() == nullptr)) { return false; }
      const auto [n, o] = id_of(*new_type.WithoutConvention(), *old_type.WithoutConvention());
      return n != nullptr and n == o;
    };

    // Ignore if the return types are different.
    if (const auto [n, o] = id_of(*new_fn.ReturnType, *old_fn->ReturnType); n == nullptr or n != o) { continue; }

    // Get the two parameter lists and create copies to
    // remove duplicate parameters from.
    auto params_new = AstCloneVec(new_fn.FnParamGroup->Params);
    auto params_old = AstCloneVec(old_fn->FnParamGroup->Params);

    // Remove all the required parameters on the first
    // parameter list off of the other parameter list.
    for (auto [p, q] : genex::views::zip(new_fn.FnParamGroup->Params | genex::views::ptr,
                                         old_fn->FnParamGroup->Params | genex::views::ptr)) {
      if (same(*p->Type, *q->Type)) {
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

auto spp::analyse::utils::fn_values::SameSignature(
  FunctionPrototypeAst const &fn_a, Scope const &scope_a,
  FunctionPrototypeAst const &fn_b, Scope const &scope_b) -> bool {
  //

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

  const auto terms = IdTermsOf(fn_a, scope_a, fn_b, scope_b);
  if (not terms.has_value()) { return false; }
  auto const &[subst_a, subst_b] = *terms;

  // All parameters must have the same types: each keyed where it is written, then read in "fn_a"'s terms.
  for (auto const &[p, q] : genex::views::zip(params_a, params_b)) {
    const auto id_a = SubstituteTypeId(scope_a.TypeIdOf(*p->Type), subst_a);
    if (id_a == nullptr or id_a != SubstituteTypeId(scope_b.TypeIdOf(*q->Type), subst_b)) { return false; }
  }

  // The function type (subroutine vs coroutine) must match.
  if (fn_a.TokFun->TokenType != fn_b.TokFun->TokenType) {
    return false;
  }

  // The return type may narrow what the other promises ("fn_a" is the override or implementation): whatever it
  // returns must be taken where the other's return type is wanted. Compared as identities in "fn_a"'s terms; two that
  // differ are checked by assignment, each made where it is not yet ("TypeRef::Substitute").
  const auto ret_of = [&scope_a](FunctionPrototypeAst const &fn, Scope const &scope, GenericSubst const &subst) {
    auto const &ret = *fn.ReturnType;
    const auto id = scope.TypeIdOf(ret);
    const auto ref = TypeRef::Of(id, scope, asts::ConventionTag::MOV, ret.IsNeverType());

    // An identity naming no one symbol until the substitution replaces it ("Self", keyed by its spelling, or a binding
    // keyed whole, "Ret" bound to "Self") is read after it.
    // Read as a type, so an instantiation not made yet is made ("TypeRef::Of").
    if (ref.Id == nullptr and id != nullptr) {
      const auto substituted = scope_a.TypeAstOf(SubstituteTypeId(id, subst));
      return substituted != nullptr ? TypeRef::Of(*substituted, scope_a) : TypeRef();
    }
    return ref.Substitute(subst, scope_a, true);
  };
  // The same identity in "fn_a"'s terms is the same type, made or not (as the parameters are compared).
  const auto ret_id_a = SubstituteTypeId(scope_a.TypeIdOf(*fn_a.ReturnType), subst_a);
  if (ret_id_a != nullptr and ret_id_a == SubstituteTypeId(scope_b.TypeIdOf(*fn_b.ReturnType), subst_b)) {
    return not(hs(&fn_a) != hs(&fn_b) or (sc(&fn_a) and *sc(&fn_a) != sc(&fn_b)) or (not sc(&fn_a) and sc(&fn_b)));
  }
  const auto ret_a = ret_of(fn_a, scope_a, subst_a);
  const auto ret_b = ret_of(fn_b, scope_b, subst_b);


  if (ret_a.Id == nullptr or ret_b.Id == nullptr) { return false; }
  if (not ret_a.SameAs(ret_b) and not type_compare::Assignable(ret_b, ret_a, scope_a, scope_a)) { return false; }

  // Check the self parameters' conventions.
  return not(hs(&fn_a) != hs(&fn_b) or (sc(&fn_a) and *sc(&fn_a) != sc(&fn_b)) or (not sc(&fn_a) and sc(&fn_b)));
}

auto spp::analyse::utils::fn_values::CheckForConflictingOverride(
  Scope const &this_scope, Scope const *target_scope,
  FunctionPrototypeAst const &new_fn, ScopeManager &sm,
  meta::CompilerMetaData *meta, Scope const *exclude_scope)
  -> FunctionPrototypeAst* {
  // Get the existing functions that belong to this
  // type, or any of its supertypes.
  const auto existing = overload_resolution::GetAllFnScopes(*new_fn.Name, target_scope, sm, meta);

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

auto spp::analyse::utils::fn_values::IsTargetCallable(
  ExpressionAst &expr, ScopeManager &sm,
  meta::CompilerMetaData *meta) -> Shared<const TypeAst> {
  // Get the type of the expression, then find its functional
  // type. The functional type is the "FunRef|FunMut|FunMov" the
  // expression is or superimposes - a generic gets its one from
  // the constraints - and is null for anything not callable,
  // which the caller reports as "no valid signatures".

  // A parameter declared against a generic is called through
  // the interface its constraint promised, recorded on the
  // symbol when the instantiation was made. Read before the
  // type, which by then is whatever the generic was substituted
  // with.
  if (const auto sym = sm.CurrentScope->FindVarSymbolOutermost(expr).first;
    sym != nullptr and sym->CallableAsType != nullptr) {
    return sym->CallableAsType;
  }

  const auto expr_type = expr.InferType(&sm, meta);
  const auto callable = marker_sups::FindFnSup(*expr_type, *sm.CurrentScope);
  return callable.Symbol != nullptr ? callable.AstIn(*sm.CurrentScope) : nullptr;
}
