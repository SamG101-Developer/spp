module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.regions;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_members;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_early_return_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.utils.ast_utils;
import spp.utils.ice;
import genex;
import std;

SPP_MOD_BEGIN
spp::analyse::utils::regions::PlaceAt::PlaceAt() = default;

spp::analyse::utils::regions::PlaceAt::PlaceAt(
  Ast const *site, Opt<Place> of) :
  Site(site), Of(std::move(of)) {
}

spp::analyse::utils::regions::PlaceAt::PlaceAt(PlaceAt const &) = default;

spp::analyse::utils::regions::PlaceAt::PlaceAt(PlaceAt &&) noexcept = default;

auto spp::analyse::utils::regions::PlaceAt::operator=(PlaceAt const &) -> PlaceAt& = default;

auto spp::analyse::utils::regions::PlaceAt::operator=(PlaceAt &&) noexcept -> PlaceAt& = default;

spp::analyse::utils::regions::PlaceAt::~PlaceAt() = default;
SPP_MOD_END

namespace spp::analyse::utils::regions {
  namespace {
    /// A place written out as its root's name and its steps
    /// ("a.b.c").
    auto PlaceText(
      Place const &place) -> Str {
      // The output string starts as the name on the root variable
      // symbol. For each part, un-intern the step part and append
      // to the output string.
      auto out = place.Root != nullptr ? place.Root->Name->Val : Str();
      for (auto const &step : place.Steps) {
        out += "." + Str(spp::utils::InternedText(step.Name)) + (step.Opaque ? "()" : "");
      }
      return out;
    }
  }
}

auto spp::analyse::utils::regions::PlaceOf(
  Ast const &ast, Scope const &scope) -> Opt<Place> {
  // Walk in from the outermost step: an attribute, or a
  // method called on a place, is a step; a dereference is
  // none. A call of anything else (a function, a value)
  // hands back a temporary, which is no place.
  auto steps = Vec<PlaceStep>();
  auto cur = &ast;
  while (const auto postfix = cur->To<PostfixExpressionAst>()) {
    // For a runtime member access, get the interned if of
    // the member being accessed, and add it to the steps.
    const auto member = postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>();
    if (member != nullptr) {
      steps.push_back({.Name = member->Name->NameId()});
      cur = postfix->Lhs.get();
    }

    // Break at a static member access, because we are no
    // longer looking at a place. Move into the left part
    // of a deref operation.
    else if (IsStaticMemberAccess(cur)) { break; }
    else if (IsDeref(cur)) { cur = postfix->Lhs.get(); }

    // A method call, in whatever form it was written ("v.method(..)", "Type::method(self=v, ..)", or "v[mut i]",
    // which is "index_mut"), is derived from its receiver: the call's "self" argument, the method a step taken into
    // it. A call of anything else (a function, a value) hands back a temporary.
    else if (const auto call = postfix->Op->To<PostfixExpressionOperatorFunctionCallAst>(); call != nullptr) {
      const auto callee = postfix->Lhs.get();

      // If this is not a static or runtime member being
      // called as a function, then it is a free function.
      if (not IsStaticMemberAccess(callee) and not IsRuntimeMemberAccess(callee)) { return Nullopt; }

      // Get the "receiver" of the call, which is either
      // the call's "self" argument, or the variable. No
      // "self" (namespaced free function or static method)
      // is also a free function.
      auto receiver = static_cast<Ast const*>(nullptr);
      for (const auto arg : call->FnArgGroup->GetKeywordArgs()) {
        if (arg->Name->Val == "self") { receiver = arg->Val.get(); }
      }
      if (receiver == nullptr and IsRuntimeMemberAccess(callee)) {
        receiver = callee->ToUnchecked<PostfixExpressionAst>()->Lhs.get();
      }
      if (receiver == nullptr) { return Nullopt; }

      // Get the operator from the postfix left-hand-side
      // of the function call operation. Add the part into
      // the steps for the memory location.
      const auto form_op = callee->ToUnchecked<PostfixExpressionAst>()->Op.get();
      if (const auto static_ = form_op->To<PostfixExpressionOperatorStaticMemberAccessAst>(); static_ != nullptr) {
        steps.push_back({.Name = static_->Name->NameId(), .Opaque = true});
      }
      else {
        const auto runtime = form_op->ToUnchecked<PostfixExpressionOperatorRuntimeMemberAccessAst>();
        steps.push_back({.Name = runtime->Name->NameId(), .Opaque = true});
      }
      cur = receiver;
    }

    // A "?" moves the value out of its operand, into a temporary.
    else if (postfix->Op->To<PostfixExpressionOperatorEarlyReturnAst>() != nullptr) { return Nullopt; }
    else { cur = postfix->Lhs.get(); }
  }

  // The root: a variable named outright, or a static member
  // reached through its type or namespace.
  const auto root = cur->To<IdentifierAst>() != nullptr or IsStaticMemberAccess(cur)
    ? scope.FindVarSymbolOutermost(*cur).first
    : nullptr;
  if (root == nullptr) { return Nullopt; }

  // Reverse the steps (they were built in reverse order).
  // and bind the root into the "Place" instance.
  steps | genex::actions::reverse;
  return Place{.Root = root, .Steps = std::move(steps)};
}

auto spp::analyse::utils::regions::Relate(
  Place const &p1, Place const &p2) -> MemRegionRelation {
  // Disjoint unless they hang off the same variable and agree
  // on every step the shorter one takes. Where they first
  // part ways, an opaque step on either side may still reach
  // the other.

  // If the two roots are not the same, then they must be
  // disjoint. Return as early guard.
  if (p1.Root != p2.Root) { return MemRegionRelation::Disjoint; }

  // Determine the lowest possible common set of steps by
  // the length of the steps vector.
  const auto common = std::min(p1.Steps.size(), p2.Steps.size());

  // If the interned identifier on the part lists are the same
  // then move into the next part. Otherwise, an opaque part
  // requires the overlapping tag (an index could cover any
  // part on the root), otherwise disjoint, like "a.b"/"a.c".
  for (auto i = 0uz; i < common; ++i) {
    if (p1.Steps[i] == p2.Steps[i]) { continue; }
    return p1.Steps[i].Opaque or p2.Steps[i].Opaque
      ? MemRegionRelation::Overlapping
      : MemRegionRelation::Disjoint;
  }

  // If "p1" is smaller, then it is containing the "p2". Other
  // way around means it is being contained by the broader "p2".
  return p1.Steps.size() <= p2.Steps.size()
    ? MemRegionRelation::Contains
    : MemRegionRelation::ContainedBy;
}

auto spp::analyse::utils::regions::PlaceAtOf(
  Ast const &ast, Scope const &scope) -> PlaceAt {
  // Bind the ast into the place struct, and forward the ast
  // into the general place analysis function.
  return PlaceAt(&ast, PlaceOf(ast, scope));
}

auto spp::analyse::utils::regions::Overlap(
  PlaceAt const &p1, PlaceAt const &p2) -> bool {
  // Either holding the other is an overlap; a value naming
  // no place shares memory with nothing.
  return p1.Of.has_value() and p2.Of.has_value()
    and Relate(*p1.Of, *p2.Of) != MemRegionRelation::Disjoint;
}

auto spp::analyse::utils::regions::FirstUnaccountedPart(
  VariableSymbol const &sym, Place const &region,
  ScopeManager const &sm) -> Str {
  // Nothing to walk without a type.
  if (sym.Type == nullptr) { Ice(*sym.Name, *sm.CurrentScope, "destructured variable has no type"); }

  // Walk the whole region, so that "a.b.c" lands on the type
  // of "c" and the scope that type resolves in.
  const auto [region_ref, region_scope] = DescendToPart(
    TypeRef::Of(*sym.Type, *sm.CurrentScope), *sm.CurrentScope,
    region.Steps, region.Steps.size());
  if (region_ref.Symbol == nullptr or region_scope == nullptr) { return Str(); }

  // The caller has established that the pattern took this place
  // apart, so its parts are walked whether or not any of them
  // recorded a move.
  auto unaccounted = Str();
  ignore = RegionConsumed(
    region, region_ref, *region_scope, memory_state::StateOf(*sm.CurrentScope, sym).PartialMoves,
    &unaccounted, true);
  return unaccounted;
}

auto spp::analyse::utils::regions::IsAttr(
  Ast const *expr, ScopeManager const *sm) -> bool {
  // Determine if the AST node is an attribute access.
  if (not IsRuntimeMemberAccess(expr)) { return false; }

  // Perform validation on the actual attribute too.
  auto const var_symbol_outermost = sm->CurrentScope->FindVarSymbolOutermost(*expr);
  return var_symbol_outermost.first != nullptr;
}

auto spp::analyse::utils::regions::DescendToPart(
  TypeRef const &root, Scope const &root_scope, Vec<PlaceStep> const &steps,
  const std::size_t count) -> Pair<TypeRef, Scope const*> {
  // Start at the root type-ref and scope, and loop from the
  // 0th element upto the provided count or the number of steps.
  auto part = Pair{root, &root_scope};
  for (auto i = 0uz; i < count and i < steps.size(); ++i) {
    if (part.first.Symbol == nullptr or part.second == nullptr) { break; }

    // For all the parts (field / tuple indexes) on the type,
    // if we find a match by interned id, save the type of it.
    auto next = Pair<TypeRef, Scope const*>{};
    for (auto const &[step, _, type, ref, where] : type_members::GetAllParts(part.first, *part.second)) {
      if (not steps[i].Opaque and step->NameId() == steps[i].Name) {
        next = {ref, where};
        break;
      }
    }

    // Move into the next part by setting the discovered next
    // part as the current part.
    part = next;
  }

  // The innermost processed part.
  return part;
}

auto spp::analyse::utils::regions::RegionConsumed(
  Place const &region, TypeRef const &type, Scope const &scope, Vec<PlaceAt> const &moves,
  Str *const unaccounted, const bool descend_regardless) -> bool {
  // If there is a registered move of the entire region,
  // then a full consume has been done, so return true.
  // Otherwise, detect if a different move contains the
  // region.
  auto touched = false;
  for (auto const &move : moves) {
    const auto relation = Relate(*move.Of, region);
    if (relation == MemRegionRelation::Contains) { return true; }
    if (relation == MemRegionRelation::ContainedBy) { touched = true; }
  }

  // If the region is not contained by any moves, then we
  // can skip individual checks, and return false here; an
  // early return optimization.
  if (not touched and not descend_regardless) {
    if (unaccounted != nullptr and unaccounted->empty()) { *unaccounted = PlaceText(region); }
    return false;
  }

  auto part = region;
  part.Steps.push_back({}); // Extra spot for temp "final" part.

  // Each part is checked on its own, under the name a
  // destructure would have recorded it by - an attribute's
  // own name, or an element's index. A copyable part was
  // never owed to anyone, so it never has to be accounted
  // for. If any part is unaccounted for, then nor is the
  // value holding it.
  for (auto const &[step, _, part_type, part_ref, part_scope] : type_members::GetAllParts(type, scope)) {
    if (part_ref.Symbol == nullptr or part_ref.Symbol->IsCopyable()) { continue; }
    part.Steps.back() = {.Name = step->NameId()};
    if (not RegionConsumed(part, part_ref, *part_scope, moves, unaccounted)) { return false; }
  }

  // Nothing left behind, so at this point we know the value
  // is empty via all its partial moves.
  return true;
}
