module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.expr_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.strings;
import genex;
import std;

namespace spp::analyse::utils::expr_utils {
  namespace {
    /// The body that both the wrapper function "ScopesDeclaringVar"
    /// and "ScopesDeclaringType" use; walk the type's own scope and
    /// its superimpositions, asking each one for the name and
    /// record how far it sits from where the lookup began.
    template <typename Entry, typename Name, typename Lookup>
    auto ScopesDeclaring(
      Scope &type_scope, Name const &name, Lookup &&lookup) -> Vec<Entry> {
      // Build a vector of the type scope and all of its super scopes
      // (of any level).
      auto scopes = Vec{&type_scope};
      scopes.AppendRange(type_scope.SupScopes());

      // If we discover the symbol within the scope, keep it, otherwise
      // discard. We are left only with the scopes that found contain
      // the symbol directly.
      auto out = Vec<Entry>();
      for (auto *const scope : scopes) {
        if (auto *const sym = lookup(*scope, name); sym != nullptr) {
          out.EmplaceBack(Entry{.Depth = type_scope.DepthDiff(scope), .Where = scope, .Symbol = sym});
        }
      }
      return out;
    }

    /// The body both the "ClosestScopes" wrap.
    template <typename Entry>
    auto ClosestScopesImpl(
      Vec<Entry> const &candidates) -> Vec<Entry> {
      // Find the minimum depth from all the provided info
      // blocks. This is the depth that must be unique for
      // a non-ambiguous lookup.
      if (candidates.IsEmpty()) { return {}; }
      auto min_depth = candidates[0].Depth;
      for (auto const &c : candidates) { min_depth = std::min(min_depth, c.Depth); }

      // Only keep the info blocks whose depth matches the
      // minimum depth. This could be any number of the
      // scopes, and we check the count for an error later.
      auto out = Vec<Entry>();
      for (auto const &c : candidates) {
        if (c.Depth == min_depth) { out.EmplaceBack(c); }
      }
      return out;
    }

    /// The body both the "RaiseIfAmbiguous" wrap.
    template <typename Entry>
    auto RaiseIfAmbiguousImpl(
      Vec<Entry> const &closest, Ast const &access,
      ScopeManager const &sm) -> void {
      // If there is a maximum of 1 scope containing the
      // target identifier/type-identifier, then return.
      // Otherwise, there is an ambiguity, so raise an
      // error.
      using errors::SppAmbiguousMemberAccessError;
      if (closest.Len() <= 1) { return; }
      Raise<SppAmbiguousMemberAccessError>(
        {closest[0].Where, closest[1].Where, sm.CurrentScope},
        ERR_ARGS(*closest[0].Symbol->Name, *closest[1].Symbol->Name, access));
    }
  }
}

auto spp::analyse::utils::expr_utils::IsPrimaryExprTypeValid(
  ExpressionAst const &expr, ScopeManager const &sm,
  PrimaryExpressionOptions &&options) -> bool {
  // Only allow types if types are explicitly allowed, or
  // are zero type.
  if (not options.AllowTypeAst and expr.To<TypeAst>() != nullptr) {
    const auto type_sym = sm.CurrentScope->GetTypeSymbol(expr.To<TypeAst>());
    return type_sym->IsZeroType();
  }

  // Only allow tokens when they're explicit allowed,
  // like "5 + .."
  if (not options.AllowTokenAst and expr.To<TokenAst>() != nullptr) { return false; }
  return true;
}

auto spp::analyse::utils::expr_utils::ValidateNoUnreachableCode(
  Vec<StatementAst*> const &members, ScopeManager const &sm) -> void {
  using errors::SppUnreachableCodeError;

  // Check for statements after a terminating statement has
  // been reached. Asked of the statement rather than matched
  // against "ret" and "exit"/"skip" by hand: a block whose
  // last statement returns, or a "case" whose every branch
  // does, ends the scope just as surely, and only the two
  // written forms were being caught.
  for (auto const &[i, member] : members | genex::views::enumerate) {
    RaiseIf<SppUnreachableCodeError>(
      member->Terminates() and member != members.Back(),
      {sm.CurrentScope}, ERR_ARGS(*member, *members[i + 1]));
  }
}

auto spp::analyse::utils::expr_utils::ValidateDiscardedValue(
  Ast &member, Scope *scope, ScopeManager const &sm,
  CompilerMetaData *const meta) -> void {
  using errors::SppDiscardedValueError;
  using type_predicates::IsTypeVoid;

  if (scope == nullptr) { return; }

  // A "case" written as a statement discards whatever its branches
  // end on, so that is where the discard is: reporting the "case"
  // itself would point at the wrong thing and say nothing about
  // which branch is at fault.
  if (auto const *case_expr = member.To<CaseExpressionAst>()) {
    for (auto const &branch : case_expr->Branches) {
      if (branch->Body == nullptr or branch->Body->Members.IsEmpty()) { continue; }
      ValidateDiscardedValue(*branch->Body->FinalMember(), branch->Body->GetAstScope(), sm, meta);
    }
    return;
  }

  // Same for a bare block, whose value is its final statement.
  if (auto const *block = member.To<InnerScopeExpressionAst>()) {
    if (block->Members.IsEmpty()) { return; }
    ValidateDiscardedValue(*block->FinalMember(), block->GetAstScope(), sm, meta);
    return;
  }

  // Only an expression produces a value at all; a "let" or an
  // assignment is a statement and has nothing to discard.
  const auto expr = member.To<StatementAst>();
  if (expr == nullptr or expr->To<ExpressionAst>() == nullptr) { return; }

  // Inferred against the scope the statement was written in, which
  // is not necessarily the one the walk is currently sitting in.
  auto tm = ScopeManager(sm.GlobalScope, scope);
  const auto type = [&] {
    const auto _meta_guard = MetaGuard(meta);
    meta->IgnoreMissingElseBranchForInference = true;
    return expr->InferType(&tm, meta);
  }();
  if (type == nullptr) { return; }

  const auto type_name = type->ToString();
  if (IsTypeVoid(TypeRef::OfHead(*type, *scope), *scope) or type->IsNeverType()) { return; }
  Raise<SppDiscardedValueError>({scope}, ERR_ARGS(member, StrView(type_name)));
}

auto spp::analyse::utils::expr_utils::MemberReachableBy(
  VariableSymbol const &sym, const MemberAccessForm form) -> bool {
  // A method is reached either way: how it is called (static
  // or runtime) is what decides whether it errors. Anything
  // with no runtime storage of its own is reached statically,
  // and the rest - attributes - at runtime.
  if (sym.Kind == VariableKind::Function) { return true; }
  return sym.IsCompTime() == (form == MemberAccessForm::Static);
}

auto spp::analyse::utils::expr_utils::LookupMemberForAccess(
  Scope &type_scope, IdentifierAst const &name,
  const MemberAccessForm form) -> VariableSymbol* {
  // Get all the scopes that declare this variable (as a field),
  // keep the ones this form can reach, and take the nearest.
  const auto closest = ClosestScopes(
    MembersReachableBy(ScopesDeclaringVar(type_scope, name, false), form));
  return closest.IsEmpty() ? nullptr : closest[0].Symbol;
}

auto spp::analyse::utils::expr_utils::MembersReachableBy(
  Vec<DeclaringVarScope> const &candidates,
  const MemberAccessForm form) -> Vec<DeclaringVarScope> {
  auto out = Vec<DeclaringVarScope>();
  for (auto const &candidate : candidates) {
    if (MemberReachableBy(*candidate.Symbol, form)) { out.EmplaceBack(candidate); }
  }
  return out;
}

auto spp::analyse::utils::expr_utils::ScopesDeclaringVar(
  Scope &type_scope, IdentifierAst const &name,
  const bool sup_scope_search) -> Vec<DeclaringVarScope> {
  // So a super scope search filtered to the scopes
  // that contain the variable symbol specified by
  // the identifier.
  return ScopesDeclaring<DeclaringVarScope>(
    type_scope, name, [sup_scope_search](Scope const &scope, IdentifierAst const &n) {
      return scope.GetVarSymbol(&n, true, sup_scope_search);
    });
}

auto spp::analyse::utils::expr_utils::ScopesDeclaringType(
  Scope &type_scope, TypeIdentifierAst const &name,
  const bool sup_scope_search) -> Vec<DeclaringTypeScope> {
  // So a super scope search filtered to the scopes
  // that contain the type symbol specified by the
  // type identifier.
  return ScopesDeclaring<DeclaringTypeScope>(
    type_scope, name, [sup_scope_search](Scope const &scope, TypeIdentifierAst const &n) {
      return scope.GetTypeSymbol(&n, true, sup_scope_search);
    });
}

auto spp::analyse::utils::expr_utils::ClosestScopes(
  Vec<DeclaringVarScope> const &candidates)
  -> Vec<DeclaringVarScope> {
  // Wrap the implementation function to filter to
  // the info blocks containing minimum depth scopes.
  return ClosestScopesImpl(candidates);
}

auto spp::analyse::utils::expr_utils::ClosestScopes(
  Vec<DeclaringTypeScope> const &candidates)
  -> Vec<DeclaringTypeScope> {
  // Wrap the implementation function to filter to
  // the info blocks containing minimum depth scopes.
  return ClosestScopesImpl(candidates);
}

auto spp::analyse::utils::expr_utils::RaiseIfAmbiguous(
  Vec<DeclaringVarScope> const &closest,
  Ast const &access, ScopeManager const &sm) -> void {
  // Wrap the implementation function to raise an error
  // if there are more than 1 scopes at the equal minimum
  // level => ambiguous lookup.
  RaiseIfAmbiguousImpl(closest, access, sm);
}

auto spp::analyse::utils::expr_utils::RaiseIfAmbiguous(
  Vec<DeclaringTypeScope> const &closest,
  Ast const &access, ScopeManager const &sm) -> void {
  // Wrap the implementation function to raise an error
  // if there are more than 1 scopes at the equal minimum
  // level => ambiguous lookup.
  RaiseIfAmbiguousImpl(closest, access, sm);
}

auto spp::analyse::utils::expr_utils::RaiseMissingIdentifierAndClosestOptions(
  IdentifierAst const &identifier, Vec<VariableSymbol*> const &var_symbols,
  Vec<NamespaceSymbol*> const &ns_symbols, ScopeManager const &sm) -> void {
  using errors::SppIdentifierUnknownError;
  using spp::utils::strings::ClosestMatch;

  //
  const auto v_alternatives = var_symbols
    | genex::views::transform([](auto const &x) { return x->Name->Val; })
    | genex::to<Vec>();
  const auto ns_alternatives = ns_symbols
    | genex::views::transform([](auto const &x) { return x->Name->Val; })
    | genex::to<Vec>();
  const auto alternatives = genex::views::concat(v_alternatives, ns_alternatives) | genex::to<Vec>();

  //
  const auto closest_match = ClosestMatch(identifier.Val, alternatives);
  Raise<SppIdentifierUnknownError>(
    {sm.CurrentScope}, ERR_ARGS(identifier, "identifier", closest_match));
}

auto spp::analyse::utils::expr_utils::RaiseMissingTypeIdentifierAndClosestOptions(
  TypeIdentifierAst const &identifier, Vec<TypeSymbol*> const &symbols,
  ScopeManager const &sm) -> void {
  using spp::utils::strings::ClosestMatch;

  //
  const auto alternatives = symbols
    | genex::views::filter([](auto const &x) { return not x->IsMock(); })
    | genex::views::transform([](auto const &x) { return x->Name->Name; })
    | genex::to<Vec>();

  //
  const auto closest_match = ClosestMatch(identifier.Name, alternatives);
  Raise<errors::SppIdentifierUnknownError>(
    {sm.CurrentScope}, ERR_ARGS(identifier, "type identifier", closest_match));
}
