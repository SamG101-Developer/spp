module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.marker_sups;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.aliases;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import genex;
import std;

namespace spp::analyse::utils::marker_sups {
  namespace {
    /**
     * The type itself, then each class it is superimposed as, that is a marker: "Gen", "Try" and the like are
     * superimposed as often as they are the type written. The type itself is judged as it is held (a borrow is no
     * marker), its super classes as the classes they are.
     * @param ref The type, as it is held.
     * @param scope The scope the type and its super classes are read in.
     * @param is_marker Whether a type is the marker being searched for.
     * @return Every match, the type itself first.
     */
    auto FindMarkerSups(
      TypeRef const &ref,
      Scope const &scope,
      std::function<bool(TypeRef const&)> const &is_marker)
      -> Vec<TypeRef> {
      auto found = Vec<TypeRef>();
      if (is_marker(ref)) { found.EmplaceBack(ref); }
      for (auto const &sup : type_members::SuperClsRefs(ref, scope)) {
        if (is_marker(sup)) { found.EmplaceBack(sup); }
      }
      return found;
    }
  }
}

auto spp::analyse::utils::marker_sups::FindFnSup(
  TypeAst const &type,
  Scope const &scope)
  -> TypeRef {
  //
  const auto type_sym = scope.FindTypeSymbol(&type);

  // Callable quick-fix to use the constrained callable type
  // rather than the genuine one for memory-analysis reasons;
  // constraint of FunMov but passed as FunMut needs to still
  // use the FunMov overload.
  for (auto const &constraint : type_sym->TypeConstraints) {
    if (type_predicates::IsTypeFunction(TypeRef::ForKindCheck(*constraint, scope), scope)) {
      return TypeRef::Of(*constraint, scope);
    }
  }

  // Check the type itself and all its supertypes (a type
  // superimposing a function type is also callable). Either is
  // looked at through an alias ("ForKindCheck" and "Of" both
  // follow one): an alias of a function type is callable as its
  // target.
  if (not type.IsCompilerGeneratedType()
    and type_predicates::IsTypeFunction(TypeRef::ForKindCheck(type, scope), scope)) {
    return TypeRef::Of(type, scope);
  }
  for (auto const &sup : type_members::SuperClsRefs(TypeRef::ForKindCheck(*type_sym, scope), scope)) {
    if (type_predicates::IsTypeFunction(sup, scope)) { return sup; }
  }

  return TypeRef();
}

auto spp::analyse::utils::marker_sups::FindGenSup(
  TypeRef const &ref,
  Scope const &scope,
  ExpressionAst const &expr,
  std::function<Shared<TypeAst>()> const &spell,
  StrView what,
  const bool raise)
  -> TypeRef {
  //
  using errors::SppExpressionNotGeneratorError;
  using errors::SppExpressionAmbiguousGeneratorError;

  // A generic type is deliberately *not* rejected here: a parameter constrained to "Gen[T]" is a generator, its
  // "LinkedScope" carries the constraint's super types, and "Iterator::concat" relies on that. The sibling guards in
  // "FindTrySup" and "FindFwdSups" bail out on generics instead, which is the inconsistency their shared Todo is
  // about - so only the lookup itself is guarded.
  // Todo: Like Copy, can we rely on constraints here? Add
  //  unit tests. Reconcile with the "IsGn()" early-outs in "FindTrySup"/"FindFwdSups" at the same time.
  const auto type_sym = ref.Symbol;
  if (type_sym == nullptr) {
    if (raise) {
      const auto type = spell();
      Raise<SppExpressionNotGeneratorError>({&scope}, ERR_ARGS(expr, *type, what));
    }
    return TypeRef();
  }

  // Search the type itself, then its super classes by symbol, for a direct generator type.
  const auto generator_candidates = FindMarkerSups(
    ref, scope, [&](TypeRef const &t) { return type_predicates::IsTypeGenerator(t, scope); });

  // If there are no Gen or GenOnce super types, then the
  // generator and yield type cannot be obtained, so either
  // throw an error or return nullptr.
  if (generator_candidates.IsEmpty()) {
    if (raise) {
      const auto type = spell();
      Raise<SppExpressionNotGeneratorError>({&scope}, ERR_ARGS(expr, *type, what));
    }
    return TypeRef();
  }

  // If there are more than 1 Gen or GenOnce super types, then
  // the generator and yield types would be ambiguous, so either
  // throw an error or return nullptr.
  if (generator_candidates.Len() > 1) {
    if (raise) {
      const auto type = spell();
      Raise<SppExpressionAmbiguousGeneratorError>({&scope}, ERR_ARGS(expr, *type, what));
    }
    return TypeRef();
  }

  return generator_candidates[0];
}

auto spp::analyse::utils::marker_sups::GenYieldOf(
  TypeRef const &gen)
  -> TypeRef {
  return gen.Symbol != nullptr ? gen.Symbol->TypeArgRef("Yield") : TypeRef();
}

auto spp::analyse::utils::marker_sups::IsGenOnce(
  TypeRef const &gen,
  Scope const &scope)
  -> bool {
  return gen.Symbol != nullptr and gen.IsA(*generate::common_types_precompiled::GEN_ONCE, scope);
}

auto spp::analyse::utils::marker_sups::EnforceYieldTypeWithoutGenDone(
  TypeRef const &gen,
  Scope const &scope,
  Ast const &expr,
  const StrView what)
  -> void {
  //
  using type_compare::TypeEq;
  using type_compare::VariantMemberRefs;
  const auto yield_ref = GenYieldOf(gen);
  if (yield_ref.Symbol == nullptr or IsGenOnce(gen, scope)) { return; }

  // A variant yield is checked member by member, as that is how
  // it is flattened into the result.
  const auto done_ref = TypeRef::Of(*generate::common_types::GenDone(expr.PosStart()), scope);
  if (done_ref.Symbol == nullptr) { return; }
  auto members = VariantMemberRefs(yield_ref, scope);
  if (members.IsEmpty()) { members.EmplaceBack(yield_ref); }

  const auto holds_done = genex::any_of(
    members, [&](auto const &m) { return m.Symbol != nullptr and TypeEq(m, done_ref, scope, scope); });
  if (holds_done) {
    Raise<errors::SppYieldTypeContainsGenDoneError>({&scope}, ERR_ARGS(expr, ErrTypeAt(yield_ref, expr), what));
  }
}

auto spp::analyse::utils::marker_sups::FindTrySup(
  TypeRef const &ref,
  ExpressionAst const &expr,
  std::function<Shared<TypeAst>()> const &spell,
  ScopeManager const &sm,
  StrView what,
  const bool raise)
  -> TypeRef {
  // Generic types are not Try types, so return nullptr.
  // Todo: Like Copy, can we rely on constraints here? Add
  //  unit tests.
  const auto type_sym = ref.Symbol;
  if (type_sym == nullptr or type_sym->IsGn()) { return TypeRef(); }

  // Search the type itself, then its super classes by symbol, for a direct try type.
  const auto try_type_candidates = FindMarkerSups(
    ref, *sm.CurrentScope, [&](TypeRef const &t) { return type_predicates::IsTypeTry(t, *sm.CurrentScope); });

  // If there are no Try super types, then the try type cannot
  // be obtained, so either throw an error or return nullptr.
  if (try_type_candidates.IsEmpty()) {
    if (raise) {
      const auto type = spell();
      Raise<errors::SppExpressionNotTryError>({sm.CurrentScope}, ERR_ARGS(expr, *type));
    }
    return TypeRef();
  }

  // If there are more than 1 Try super types, then the Try
  // type would be ambiguous, so either throw an error or
  // return nullptr.
  if (try_type_candidates.Len() > 1) {
    if (raise) {
      const auto type = spell();
      Raise<errors::SppExpressionAmbiguousTryError>({sm.CurrentScope}, ERR_ARGS(expr, *type, what));
    }
    return TypeRef();
  }

  return try_type_candidates[0];
}

auto spp::analyse::utils::marker_sups::FindFwdSups(
  TypeRef const &ref,
  Scope const &scope)
  -> Pair<TypeRef, TypeRef> {
  //
  using generate::common_types_precompiled::FWD_MUT;
  using generate::common_types_precompiled::FWD_REF;

  // Generic types do not have forward types, so return nullptr.
  if (ref.Symbol == nullptr or ref.Symbol->IsGn() or ref.Symbol->LinkedScope == nullptr) { return {}; }

  // The first FwdRef and the first FwdMut, searching the type (by value) and then its super classes.
  const auto self = ref.WithoutConvention();
  const auto first = [&](TypeAst const &marker) -> TypeRef {
    const auto found = FindMarkerSups(self, scope, [&](TypeRef const &t) { return t.IsA(marker, scope); });
    return found.IsEmpty() ? TypeRef() : found[0];
  };
  return {first(*FWD_REF), first(*FWD_MUT)};
}

auto spp::analyse::utils::marker_sups::FwdTargetOf(
  TypeRef const &fwd)
  -> TypeRef {
  return fwd.Symbol != nullptr ? fwd.Symbol->TypeArgRef("T") : TypeRef();
}

auto spp::analyse::utils::marker_sups::BuildFwdCall(
  ExpressionAst const &receiver,
  TypeRef const &receiver_ref,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Unique<PostfixExpressionAst> {
  // The receiver is analysed a second time in the built call (it is a clone of an already analysed expression), which
  // is what the other operators that map themselves onto a method call do too.
  if (not CanForward(receiver_ref, *sm->CurrentScope)) { return nullptr; }
  return BuildFwdCall(AstClone(&receiver), receiver_ref, sm, meta);
}

auto spp::analyse::utils::marker_sups::BuildFwdCall(
  Unique<ExpressionAst> &&receiver,
  TypeRef const &receiver_ref,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Unique<PostfixExpressionAst> {
  // A type forwards by superimposing "FwdRef" or "FwdMut", whose coroutines are "fwd_ref" and "fwd_mut".
  if (not CanForward(receiver_ref, *sm->CurrentScope)) { return nullptr; }
  const auto [fwd_ref, fwd_mut] = FindFwdSups(receiver_ref, *sm->CurrentScope);
  const auto fwd_ref_target = FwdTargetOf(fwd_ref);
  const auto fwd_mut_target = FwdTargetOf(fwd_mut);

  // Which of the two is taken follows the receiver's own convention: a value borrowed mutably forwards to a mutable
  // borrow of what it points at. Preferring the immutable one unconditionally turned a "&mut" receiver into a "&"
  // yield, which is what a mutable forward was for in the first place. Fall back to whichever exists when the
  // preferred one does not.
  const auto wants_mut = receiver_ref.Conv == ConventionTag::MUT and fwd_mut_target.Symbol != nullptr;

  // Build "<receiver>.fwd_ref()". The forwarding coroutines return a "GenOnce", so the call resumes itself and the
  // expression evaluates to the borrow of the forwarded-to value.
  auto field_name = MakeUnique<IdentifierAst>(
    receiver->PosStart(), wants_mut or fwd_ref_target.Symbol == nullptr ? "fwd_mut" : "fwd_ref");
  auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
  auto member_access = MakeUnique<PostfixExpressionAst>(std::move(receiver), std::move(field));
  auto func_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
  auto fwd_call = MakeUnique<PostfixExpressionAst>(std::move(member_access), std::move(func_call));

  // Analyse the built call, so that it can be inferred from and generated like any other analysed expression.
  fwd_call->Stage7_AnalyseSemantics(sm, meta);
  return fwd_call;
}

auto spp::analyse::utils::marker_sups::CanForward(
  TypeRef const &receiver_ref,
  Scope const &scope)
  -> bool {
  const auto [fwd_ref, fwd_mut] = FindFwdSups(receiver_ref, scope);
  return FwdTargetOf(fwd_ref).Symbol != nullptr or FwdTargetOf(fwd_mut).Symbol != nullptr;
}
