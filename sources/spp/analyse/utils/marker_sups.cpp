module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.marker_sups;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
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
      -> Vec<TypeSymbol*> {
      auto found = Vec<TypeSymbol*>();
      if (is_marker(ref)) { found.EmplaceBack(ref.Sym); }
      for (auto *sup : type_members::SuperClassTypes(*ref.Sym)) {
        if (is_marker(TypeRef::OfResolved(*sup, scope))) { found.EmplaceBack(sup); }
      }
      return found;
    }
  }
}

auto spp::analyse::utils::marker_sups::GetFunctionalType(
  TypeAst const &type,
  Scope const &scope)
  -> Shared<const TypeAst> {
  //
  const auto type_sym = scope.GetTypeSymbol(&type);

  // Callable quick-fix to use the constrained callable type
  // rather than the genuine one for memory-analysis reasons;
  // constraint of FunMov but passed as FunMut needs to still
  // use the FunMov overload.
  for (auto const &constraint : type_sym->GenericConstraints) {
    const auto target = type_resolution::ThroughAlias(*constraint, scope);
    if (type_predicates::IsTypeFunc(TypeRef::OfHead(*target, scope), scope)) { return target; }
  }

  // Check the type itself and all its supertypes (a type
  // superimposing a function type is also callable). Either is
  // looked at through an alias: an alias of a function type is
  // callable as its target.
  const auto target = type_resolution::ThroughAlias(type, scope);
  if (not type.IsCompilerGeneratedType() and type_predicates::IsTypeFunc(TypeRef::OfHead(*target, scope), scope)) {
    return target;
  }
  for (auto *sup : type_members::SuperClassTypes(*type_sym)) {
    if (type_predicates::IsTypeFunc(*sup, scope)) { return sup->FqName(); }
  }

  return nullptr;
}

auto spp::analyse::utils::marker_sups::GetGenAndYieldTypes(
  TypeRef const &ref,
  Scope const &scope,
  ExpressionAst const &expr,
  std::function<Shared<TypeAst>()> const &spell,
  StrView what,
  const bool raise)
  -> Tup<TypeSymbol*, Shared<TypeAst>, bool> {
  //
  using generate::common_types_precompiled::GEN_ONCE;
  using errors::SppExpressionNotGeneratorError;
  using errors::SppExpressionAmbiguousGeneratorError;

  // A generic type is deliberately *not* rejected here: a parameter constrained to "Gen[T]" is a generator, its
  // "LinkedScope" carries the constraint's super types, and "Iterator::concat" relies on that. The sibling guards in
  // "GetTryType" and "GetFwdTypes" bail out on generics instead, which is the inconsistency their shared Todo is
  // about - so only the lookup itself is guarded.
  // Todo: Like Copy, can we rely on constraints here? Add
  //  unit tests. Reconcile with the "IsTypeGeneric()" early-outs in "GetTryType"/"GetFwdTypes" at the same time.
  const auto type_sym = ref.Sym;
  if (type_sym == nullptr) {
    if (raise) {
      const auto type = spell();
      Raise<SppExpressionNotGeneratorError>({&scope}, ERR_ARGS(expr, *type, what));
    }
    return {nullptr, nullptr, false};
  }

  // Search the type itself, then its super classes by symbol, for a direct generator type.
  const auto generator_candidates = FindMarkerSups(
    ref, scope, [&](TypeRef const &t) { return type_predicates::IsTypeGen(t, scope); });

  // If there are no Gen or GenOnce super types, then the
  // generator and yield type cannot be obtained, so either
  // throw an error or return nullptr.
  if (generator_candidates.IsEmpty()) {
    if (raise) {
      const auto type = spell();
      Raise<SppExpressionNotGeneratorError>({&scope}, ERR_ARGS(expr, *type, what));
    }
    return {nullptr, nullptr, false};
  }

  // If there are more than 1 Gen or GenOnce super types, then
  // the generator and yield types would be ambiguous, so either
  // throw an error or return nullptr.
  if (generator_candidates.Len() > 1) {
    if (raise) {
      const auto type = spell();
      Raise<SppExpressionAmbiguousGeneratorError>({&scope}, ERR_ARGS(expr, *type, what));
    }
    return {nullptr, nullptr, false};
  }

  // Extract the generator and yield type from the candidates.
  // Accessing [0] is safe as we have already done the validation
  // beforehand.
  auto *const generator_sym = generator_candidates[0];
  auto yield_type = generator_sym->TypeArgType("Yield");
  auto is_once = type_compare::IsTemplate(*generator_sym, *GEN_ONCE, scope);

  // Return all the information about the generator type.
  return {generator_sym, yield_type, is_once};
}

auto spp::analyse::utils::marker_sups::EnforceYieldTypeWithoutGenDone(
  TypeAst const *yield_type,
  const bool is_once,
  Scope const &scope,
  Ast const &expr,
  const StrView what)
  -> void {
  //
  using type_compare::TypeEq;
  using type_compare::VariantMembers;
  if (is_once or yield_type == nullptr) { return; }

  // A variant yield is checked member by member, as that is how
  // it is flattened into the result.
  const auto done_ref = TypeRef::Of(*generate::common_types::GenDone(expr.PosStart()), scope);
  if (done_ref.Sym == nullptr) { return; }
  const auto yield_ref = TypeRef::Of(*yield_type, scope);
  auto members = VariantMembers(yield_ref, scope);
  if (members.IsEmpty()) { members.EmplaceBack(yield_ref); }

  const auto holds_done = genex::any_of(
    members, [&](auto const &m) { return m.Sym != nullptr and TypeEq(m, done_ref, scope, scope); });
  if (holds_done) {
    Raise<errors::SppYieldTypeContainsGenDoneError>({&scope}, ERR_ARGS(expr, *yield_type, what));
  }
}

auto spp::analyse::utils::marker_sups::GetTryType(
  TypeRef const &ref,
  ExpressionAst const &expr,
  std::function<Shared<TypeAst>()> const &spell,
  ScopeManager const &sm,
  StrView what,
  const bool raise)
  -> TypeSymbol* {
  // Generic types are not Try types, so return nullptr.
  // Todo: Like Copy, can we rely on constraints here? Add
  //  unit tests.
  const auto type_sym = ref.Sym;
  if (type_sym == nullptr or type_sym->IsTypeGeneric()) { return nullptr; }

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
    return nullptr;
  }

  // If there are more than 1 Try super types, then the Try
  // type would be ambiguous, so either throw an error or
  // return nullptr.
  if (try_type_candidates.Len() > 1) {
    if (raise) {
      const auto type = spell();
      Raise<errors::SppExpressionAmbiguousTryError>({sm.CurrentScope}, ERR_ARGS(expr, *type, what));
    }
    return nullptr;
  }

  // Extract the Try type and return it.
  return try_type_candidates[0];
}

auto spp::analyse::utils::marker_sups::GetFwdTypes(
  TypeSymbol const &sym,
  Scope const &scope)
  -> Pair<TypeSymbol*, TypeSymbol*> {
  //
  using generate::common_types_precompiled::FWD_MUT;
  using generate::common_types_precompiled::FWD_REF;

  // Generic types do not have forward types, so return nullptr.
  if (sym.IsTypeGeneric() or sym.LinkedScope == nullptr) { return {nullptr, nullptr}; }

  // The first FwdRef and the first FwdMut, searching the type and then its super classes.
  const auto self = TypeRef::OfResolved(const_cast<TypeSymbol&>(sym), scope);
  const auto first = [&](TypeAst const &marker) -> TypeSymbol* {
    const auto found = FindMarkerSups(self, scope, [&](TypeRef const &t) {
      return type_compare::IsTemplate(*t.Sym, marker, scope);
    });
    return found.IsEmpty() ? nullptr : found[0];
  };
  return {first(*FWD_REF), first(*FWD_MUT)};
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
  const auto [fwd_ref_type, fwd_mut_type] = GetFwdTypes(*receiver_ref.Sym, *sm->CurrentScope);

  // Which of the two is taken follows the receiver's own convention: a value borrowed mutably forwards to a mutable
  // borrow of what it points at. Preferring the immutable one unconditionally turned a "&mut" receiver into a "&"
  // yield, which is what a mutable forward was for in the first place. Fall back to whichever exists when the
  // preferred one does not.
  const auto wants_mut = receiver_ref.Conv == ConventionTag::MUT and fwd_mut_type != nullptr;

  // Build "<receiver>.fwd_ref()". The forwarding coroutines return a "GenOnce", so the call resumes itself and the
  // expression evaluates to the borrow of the forwarded-to value.
  auto field_name = MakeUnique<IdentifierAst>(
    receiver->PosStart(), wants_mut or fwd_ref_type == nullptr ? "fwd_mut" : "fwd_ref");
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
  if (receiver_ref.Sym == nullptr) { return false; }
  const auto [fwd_ref_type, fwd_mut_type] = GetFwdTypes(*receiver_ref.Sym, scope);
  return fwd_ref_type != nullptr or fwd_mut_type != nullptr;
}
