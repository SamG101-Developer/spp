module spp.analyse.utils.fn_signatures;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.overload_resolution;
import spp.analyse.utils.sup_blocks;
import spp.analyse.utils.type_compare;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import spp.utils.types;
import genex;

namespace spp::analyse::utils::fn_signatures {
  namespace {
    /// Get the "sup" block a function prototype was declared
    /// in (or nullptr for a free function, whose context is the
    /// module prototype => no superimposition).
    auto _SupBlockOf(
      FunctionPrototypeAst const &fn) -> Ast* {
      const auto ctx = fn.GetAstCtx();
      return sup_blocks::IsSupBlock(ctx) ? ctx : nullptr;
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
      // tried, by identity, each read where it is written.
      auto const &scope_a = *sup_a->GetAstScope();
      auto const &scope_b = *sup_b->GetAstScope();
      const auto a = scope_a.TypeIdOf(*AstName(sup_a));
      const auto b = scope_b.TypeIdOf(*AstName(sup_b));
      if (a == nullptr or b == nullptr) { return false; }
      auto fwd = scopes::GenericSubst();
      auto rev = scopes::GenericSubst();
      return scopes::UnifyTypeIds(a, b, scope_a, scope_b, fwd) or scopes::UnifyTypeIds(b, a, scope_b, scope_a, rev);
    }
  }
}

namespace spp::analyse::utils::fn_signatures {
  namespace {
    /**
     * How to read two functions' types in "fn_a"'s terms, as substitutions over "TypeId"s - the first for "fn_a"'s own
     * types, the second for "fn_b"'s. "Self" is the type implementing them both (the class "fn_a" is written for).
     * "fn_b"'s own generic parameters are "fn_a"'s, by position: a signature is the same up to renaming them. "fn_b"'s
     * block's parameters are what the class "fn_a" is written for binds them to, where "fn_b" leaves them unbound: its
     * block is matched against the super class the implementer extends, as a receiver binds a block's generics
     * ("sup [Rhs] Eq[Rhs]" against "ext Eq", which is "Eq[Rhs=Self]" with its default filled); and a parameter "fn_b"'s
     * scope binds to "Self" ("Ret" of "BitAnd[Rhs=Self, Ret=Self]") is the implementer too. None when their own
     * generics differ in kind (type or comp, a pack or one: "..xs: Ts" takes a mix of types, "..xs: T" one), or a part
     * does not key.
     */
    auto IdTermsOf(
      FunctionPrototypeAst const &fn_a, Scope const &scope_a,
      FunctionPrototypeAst const &fn_b, Scope const &scope_b)
      -> Opt<Pair<GenericSubst, GenericSubst>> {
      auto subst_a = GenericSubst();
      auto subst_b = GenericSubst();

      auto const *const a_self = scope_a.FindSelfSymbol();
      auto const *const a_cls = a_self != nullptr ? a_self->AsBound() : nullptr;
      const auto self_id = a_cls != nullptr and a_cls->Type != nullptr
        ? BareOf(scope_a.TypeIdOfSymbol(*a_cls))
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
      for (auto i = 0uz; i < own_a.size() and i < own_b.size(); ++i) {
        if (own_a[i]->IsTypeParam() != own_b[i]->IsTypeParam()) { return Nullopt; }
        if (own_a[i]->IsVariadic() != own_b[i]->IsVariadic()) { return Nullopt; }
        if (own_b[i]->IsTypeParam()) {
          auto const *const a_param = scope_a.FindTypeSymbol(own_a[i]->Name.get());
          if (a_param == nullptr) { continue; }
          bind_b(b_param(*own_b[i]->Name->ToUnchecked<TypeIdentifierAst>()), scope_a.TypeIdOfSymbol(*a_param));
          continue;
        }
        auto const *const b_var = scope_b.FindVarSymbol(IdentifierAst::FromType(*own_b[i]->Name).get());
        auto const *const a_var = scope_a.FindVarSymbol(IdentifierAst::FromType(*own_a[i]->Name).get());
        if (b_var == nullptr or b_var->ParamId() == 0 or a_var == nullptr) { continue; }
        subst_b.CompParams.emplace_back(b_var->ParamId(), scope_a.CompIdOfSymbol(*a_var));
      }

      // The blocks each method was written in (a method's own "sup $F ext FunXxx" block is not one).
      auto const *const b_block_scope = sup_blocks::EnclosingSupBlock(scope_b);
      auto *const b_block = b_block_scope != nullptr ? b_block_scope->AstNode : nullptr;
      auto const *const a_block_scope = sup_blocks::EnclosingSupBlock(scope_a);
      auto const *const a_ext = a_block_scope != nullptr
        ? AstAs<SupPrototypeExtensionAst>(a_block_scope->AstNode)
        : nullptr;
      if (self_id != nullptr and b_block != nullptr and a_ext != nullptr) {
        // "fn_b"'s block's generics bound off "fn_a"'s super class, "Self" in what they bind read as "fn_a"'s.
        auto const *const super_sym = TypeRef::Of(*a_ext->SuperCls, scope_a).Symbol;
        const auto super_id = super_sym != nullptr ? scope_a.TypeIdOfSymbol(*super_sym) : nullptr;
        const auto b_pattern = scope_b.TypeIdOf(*AstName(b_block));
        auto inferred = GenericSubst();
        if (super_id != nullptr and b_pattern != nullptr
          and scopes::UnifyTypeIds(BareOf(super_id), b_pattern, scope_a, scope_b, inferred, false, false)) {
          auto self_subst = GenericSubst();
          self_subst.TypeParams.emplace_back(0, self_id);
          for (auto const &[pid, val] : inferred.TypeParams) { bind_b(pid, SubstituteTypeId(val, self_subst)); }
        }
      }
      if (self_id != nullptr) {
        for (auto &generic : scope_b.GetGns()) {
          if (generic->KeywordName() == nullptr or generic->IsCompArg() or not generic->TypeVal->IsSelfType()) {
            continue;
          }
          bind_b(b_param(*generic->KeywordName()->LastTypePart()), self_id);
        }
      }
      return Pair<GenericSubst, GenericSubst>{std::move(subst_a), std::move(subst_b)};
    }

    /**
     * Two prototypes read in one set of terms ("IdTermsOf": the second's generics renamed to the first's), so a type
     * of either is compared with the other's by identity: each keyed where it is written, then substituted.
     */
    struct SigTerms {
      Scope const &ScopeA;
      Scope const &ScopeB;
      GenericSubst SubstA;
      GenericSubst SubstB;

      auto IdA(TypeAst const &type) const -> TypeId {
        return SubstituteTypeId(ScopeA.TypeIdOf(type), SubstA);
      }

      auto IdB(TypeAst const &type) const -> TypeId {
        return SubstituteTypeId(ScopeB.TypeIdOf(type), SubstB);
      }

      /// Whether "a" (of the first) and "b" (of the second) are one type. With "any_borrow", a borrow is the same
      /// as a borrow of either kind ("&Bool" and "&mut Bool", which a call cannot choose between), and a value only a
      /// value.
      auto Same(TypeAst const &a, TypeAst const &b, const bool any_borrow = false) const -> bool {
        if (any_borrow and (a.GetConvention() == nullptr) != (b.GetConvention() == nullptr)) { return false; }
        const auto id_a = any_borrow ? IdA(*a.WithoutConvention()) : IdA(a);
        const auto id_b = any_borrow ? IdB(*b.WithoutConvention()) : IdB(b);
        return id_a != nullptr and id_a == id_b;
      }
    };

    /**
     * "SigTerms" for two prototypes; none where "IdTermsOf" has none.
     */
    auto SigTermsOf(
      FunctionPrototypeAst const &fn_a, Scope const &scope_a,
      FunctionPrototypeAst const &fn_b, Scope const &scope_b)
      -> Opt<SigTerms> {
      auto terms = IdTermsOf(fn_a, scope_a, fn_b, scope_b);
      if (not terms.has_value()) { return Nullopt; }
      return SigTerms{scope_a, scope_b, std::move(terms->first), std::move(terms->second)};
    }
  }
}

auto spp::analyse::utils::fn_signatures::CheckForConflictingOverload(
  Scope const &this_scope, Scope const *target_scope,
  FunctionPrototypeAst const &new_fn, ScopeManager &sm,
  meta::CompilerMetaData *meta) -> Pair<FunctionPrototypeAst*, Scope const*> {
  //

  // Get the methods that belong to this type, or any
  // of its supertypes.
  const auto existing = overload_resolution::GetAllFnScopes(*new_fn.Name, target_scope, sm, meta);
  const auto new_sup = _SupBlockOf(new_fn);

  // Check for an overload conflict with all functions
  // of the same name.
  for (auto const &[old_scope, old_fn, _1, _2] : existing) {
    // Ignore the same object. A method it overrides on a base
    // class is not among them: "GetAllFnScopes" applies overrides.
    if (old_fn == &new_fn) { continue; }

    // Ignore if the two methods come from "sup" blocks that
    // are disjoint specializations of the same type, such as
    // "sup [T: Integer] Atom[T]" against "sup [T: FloatingPoint]
    // Atom[T]". These never apply to the same instantiation,
    // so they aren't overloads of each other.
    if (not _SupBlocksOverlap(new_sup, _SupBlockOf(*old_fn))) { continue; }

    // Ignore if there are a different number of required
    // generic parameters.
    if (new_fn.GnParamGroup->GetTypeParams().size() != old_fn->GnParamGroup->GetTypeParams().size()) { continue; }
    if (new_fn.GnParamGroup->GetCompParams().size() != old_fn->GnParamGroup->GetCompParams().size()) { continue; }

    // The two are compared with the old one's generics renamed to the new one's ("f[T](a: T)" and "f[U](a: U)"
    // conflict), and parameters with either borrow as the same: a call cannot choose between "&Bool" and "&mut Bool",
    // but can between "T" and "&T". A return type keeps its convention, as return types overload.
    const auto terms = SigTermsOf(new_fn, this_scope, *old_fn, *old_scope);
    if (not terms.has_value()) { continue; }

    // Ignore if the return types are different.
    if (not terms->Same(*new_fn.ReturnType, *old_fn->ReturnType)) { continue; }

    // Get the two parameter lists and create copies to
    // remove duplicate parameters from.
    auto params_new = AstCloneVec(new_fn.FnParamGroup->Params);
    auto params_old = AstCloneVec(old_fn->FnParamGroup->Params);

    // Remove all the required parameters on the first
    // parameter list off of the other parameter list.
    for (auto [p, q] : genex::views::zip(new_fn.FnParamGroup->Params | genex::views::ptr,
                                         old_fn->FnParamGroup->Params | genex::views::ptr)) {
      if (terms->Same(*p->Type, *q->Type, true)) {
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
      return {old_fn, old_scope};
    }
  }
  return {nullptr, nullptr};
}

auto spp::analyse::utils::fn_signatures::SameSignature(
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
    if (a.size() != b.size()) { return false; }
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
  if (params_a.size() != params_b.size()) { return false; }

  // All parameters must have the same names.
  if (genex::any_of(
    genex::views::zip(params_a, params_b) | genex::to<Vec>(),
    [&](auto pq) { return not param_names_eq(spp::get<0>(pq)->ExtractNames(), spp::get<1>(pq)->ExtractNames()); })) {
    return false;
  }

  const auto terms = SigTermsOf(fn_a, scope_a, fn_b, scope_b);
  if (not terms.has_value()) { return false; }
  auto const &subst_a = terms->SubstA;
  auto const &subst_b = terms->SubstB;

  // All parameters must have the same types, read in "fn_a"'s terms.
  for (auto const &[p, q] : genex::views::zip(params_a, params_b)) {
    if (not terms->Same(*p->Type, *q->Type)) { return false; }
  }

  // The function type (subroutine vs coroutine) must match, as must the "self" parameters' conventions.
  if (fn_a.TokFun->TokenType != fn_b.TokFun->TokenType) {
    return false;
  }
  if (hs(&fn_a) != hs(&fn_b) or (sc(&fn_a) and *sc(&fn_a) != sc(&fn_b)) or (not sc(&fn_a) and sc(&fn_b))) {
    return false;
  }

  // The return type may narrow what the other promises ("fn_a" is the override or implementation): whatever it
  // returns must be taken where the other's return type is wanted. Compared as identities in "fn_a"'s terms; two that
  // differ are checked by assignment, each made where it is not yet ("TypeRef::Substitute").
  const auto ret_of = [&scope_a](FunctionPrototypeAst const &fn, Scope const &scope, GenericSubst const &subst) {
    auto const &ret = *fn.ReturnType;
    const auto id = scope.TypeIdOf(ret);
    const auto ref = TypeRef::Of(id, scope, ConventionTag::MOV, ret.IsNeverType());

    // An identity naming no one symbol until the substitution replaces it ("Self", parameter 0 read through the scope,
    // or a binding keyed whole, "Ret" bound to "Self") is read after it. Read as a type, so an instantiation not made
    // yet is made ("TypeRef::Of").
    if (ref.Id == nullptr and id != nullptr) {
      const auto substituted = scope_a.TypeAstOf(SubstituteTypeId(id, subst));
      return substituted != nullptr ? TypeRef::Of(*substituted, scope_a) : TypeRef();
    }
    return ref.Substitute(subst, scope_a, TypeRef::OnMissing::Make);
  };
  // The same identity in "fn_a"'s terms is the same type, made or not (as the parameters are compared).
  if (terms->Same(*fn_a.ReturnType, *fn_b.ReturnType)) { return true; }
  const auto ret_a = ret_of(fn_a, scope_a, subst_a);
  const auto ret_b = ret_of(fn_b, scope_b, subst_b);
  if (ret_a.Id == nullptr or ret_b.Id == nullptr) { return false; }
  return ret_a.SameAs(ret_b) or type_compare::Assignable(ret_b, ret_a, scope_a, scope_a);
}

auto spp::analyse::utils::fn_signatures::CheckForConflictingOverride(
  Scope const &this_scope, Scope const *target_scope,
  FunctionPrototypeAst const &new_fn, ScopeManager &sm,
  meta::CompilerMetaData *meta)
  -> FunctionPrototypeAst* {
  // Get the existing functions that belong to this
  // type, or any of its supertypes.
  const auto existing = overload_resolution::GetAllFnScopes(*new_fn.Name, target_scope, sm, meta);

  // Check for an overload conflict with all functions
  // of the same name.
  for (auto const &[old_scope, old_fn, _1, _2] : existing) {
    // Ignore if the method is the same object.
    if (old_fn == &new_fn) { continue; }

    // The functions must have identical signatures to
    // conflict, so return the old function.
    if (SameSignature(new_fn, this_scope, *old_fn, *old_scope)) { return old_fn; }
  }

  return nullptr;
}
