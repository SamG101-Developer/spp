module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.destructure_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.regions;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_type;
import spp.utils.uid;
import genex;
import llvm;

namespace spp::analyse::utils::destructure_utils {
  namespace {
    /// When we have a pattern like "let Self(fd) = self", we
    /// mark "self" as moved, because all non-copyable fields
    /// have to be moved (for linear system drop rules), and
    /// this is how in the "drop" methods, we finish consuming
    /// "self".
    auto ConsumeDestructureSource(
      Ast const &owner, const bool from_case_pattern, const bool any_binding_is_moving,
      ScopeManager &sm, CompilerMetaData const *meta) -> void {
      // A case pattern only answers for the parts of a subject its
      // "case" can take ("meta->CaseSubject"); the "case" itself takes
      // the subject, on the branch's match edge.
      if (from_case_pattern and meta->CaseSubject == nullptr) { return; }

      // If this is for a let statement + local variable destructure
      // (not a case pattern), then there must be a value, and it
      // must name existing storage.
      const auto val = meta->LetStatementValue;
      if (val == nullptr or not IsDestructurePlaceExpression(*val)) { return; }

      // Get the outermost (root) symbol for the value being
      // destructured.
      const auto sym = sm.CurrentScope->FindVarSymbolOutermost(*val).first;
      if (sym == nullptr) { return; }

      // Destructuring a borrow reads through it. The value behind
      // it belongs to someone else, so it is not consumed here.
      if (sym->MemInfo->AstBorrowed != nullptr) { return; }
      if (sym->Type != nullptr and sym->Type->GetConvention() != nullptr) { return; }

      // The value has to still be there to be taken apart. Each
      // bound part is checked as it is read, but a destructure
      // binding nothing ("let L() = l") reads no part, so a value
      // already moved away was consumed a second time unnoticed.
      if (val->To<IdentifierAst>() != nullptr) { mem_utils::RaiseIfMoved(*sym, *val, sm.CurrentScope); }

      // A destructure takes the value apart, so what it does
      // not bind is left with nothing holding it. This creates
      // leaks because there is then no way to drop those values,
      // so make sure all movables are bound.
      if (any_binding_is_moving) {
        // Get the region path of the value, and check if any parts
        // have not been considered by the destructure. These cannot
        // be left unbound, because they would silently drop.
        const auto region = regions::PlaceOf(*val, *sm.CurrentScope);

        if (const auto skipped = region.has_value() ? regions::FirstUnaccountedPart(*sym, *region, sm) : Str();
          not skipped.empty()) {
          Raise<errors::SppDestructureSkipsOwnedPartError>(
            {sm.CurrentScope}, ERR_ARGS(owner, *val, StrView(skipped)));
        }
      }

      // The "case" subject itself is taken on the branch's
      // match edge; a part nested inside it is taken here. The
      // pattern's "let" holds a copy of the condition, so the
      // two are compared by the place they name.
      if (from_case_pattern and meta->CaseCondition != nullptr
        and regions::PlaceOf(*val, *sm.CurrentScope) == regions::PlaceOf(*meta->CaseCondition, *sm.CurrentScope)) {
        return;
      }
      if (val->To<IdentifierAst>() != nullptr) {
        memory_state::StateOf(*sm.CurrentScope, *sym).MovedBy(owner);
      }
      else {
        memory_state::StateOf(*sm.CurrentScope, *sym).PartMovedBy(regions::PlaceAtOf(*val, *sm.CurrentScope));
      }
    }

    /// When we have a temporary materialization, mark it as
    /// consumed by getting the symbol and setting the memory
    /// fields on it to mark as "moved".
    auto ConsumeDestructureTemp(
      IdentifierAst const &tmp_name, ScopeManager const &sm) -> void {
      // Every part the bindings read came off the temporary, so
      // between them they took all of it.
      const auto sym = sm.CurrentScope->FindVarSymbol(&tmp_name);
      if (sym == nullptr) { return; }
      memory_state::StateOf(*sm.CurrentScope, *sym).MovedBy(tmp_name);
    }

    /// Run uniform stage 8 memory analysis on the destructure
    /// temporary materialization, should it exist (this won't
    /// be called if not).
    auto DestructureTempStage8(
      Ast const &owner, IdentifierAst const &tmp_name,
      ScopeManager &sm, CompilerMetaData *meta) -> void {
      using mem_utils::ValidateSymbolMemory;
      // The value is moved into the temporary as a whole, so it
      // is checked (and consumed) once here, rather than once
      // per expanded "let". This traversal is also what walks
      // the scopes the value created in stage 7.
      meta->LetStatementValue->Stage8_CheckMemory(&sm, meta);
      ValidateSymbolMemory(
        *meta->LetStatementValue, owner, sm, meta);

      // Mark the temporary as initialized by the value.
      const auto sym = sm.CurrentScope->FindVarSymbol(&tmp_name);
      memory_state::StateOf(*sm.CurrentScope, *sym).InitializedBy(tmp_name);
    }

    /// Run uniform stage 9 comptime resolution on the
    /// destructure temporary materialization, should it exist
    /// (this won't be called if not).
    auto DestructureTempStage9(
      Shared<IdentifierAst> const &tmp_name, ScopeManager const &sm,
      CompilerMetaData const *meta) -> void {
      // The owning "let" statement has already resolved the
      // value, so the temporary takes a copy of that result
      // rather than resolving the value a second time (which
      // would walk the value's scopes twice).
      const auto sym = sm.CurrentScope->FindVarSymbol(tmp_name.get());
      sym->CompTimeValue = AstClone(meta->CompTimeResult);
    }
  }
}

auto spp::analyse::utils::destructure_utils::GetNestedBindingIdentifiers(
  Vec<Unique<LocalVariableAst>> const &elems) -> Vec<Shared<IdentifierAst>> {
  // Recursively walk the destructure pattern to extract all
  // identifiers.
  return elems
    | genex::views::transform(&LocalVariableAst::ExtractNames)
    | genex::views::join
    | genex::to<Vec>();
}

auto spp::analyse::utils::destructure_utils::UnmatchableSingleIdentifier(
  const std::size_t pos) -> Shared<IdentifierAst> {
  // No single identifier represents a binding destructuring.
  return MakeShared<IdentifierAst>(pos, kUnmatchableTag);
}

auto spp::analyse::utils::destructure_utils::BindDestructureTemporary(
  ExpressionAst *val, Shared<TypeAst> const &val_type,
  ScopeManager &sm) -> Shared<IdentifierAst> {
  // The lexer has no "$" token, so no written name starts with
  // one, and the temporary can never collide with a real binding.
  auto name = MakeShared<IdentifierAst>(val->PosEnd(), "$_dst_" + spp::utils::Uid());

  // Mirror the symbol an initialized single-identifier "let"
  // would create.
  const auto sym = MakeShared<VariableSymbol>(
    name, val_type, sm.CurrentScope, VariableKind::Temporary, true);
  memory_state::StateOf(*sm.CurrentScope, *sym).AstInitialization = name.get();
  memory_state::StateOf(*sm.CurrentScope, *sym).AstInitializationOrigin = name.get();

  // Carry the value's convention over, so that destructuring
  // a borrow yields borrowed elements.
  if (val_type->GetConvention() != nullptr) {
    sym->MemInfo->AstBorrowed = val;
  }

  sm.CurrentScope->AddVarSymbol(sym);
  return name;
}

auto spp::analyse::utils::destructure_utils::DestructureTempStage11(
  Shared<IdentifierAst> const &tmp_name, llvm::Value *llvm_subject,
  ScopeManager &sm, CompilerMetaData *meta, LlvmCtx *ctx) -> void {
  // Give the temporary its own stack slot.
  const auto uid = "." + spp::utils::Uid();
  const auto sym = sm.CurrentScope->FindVarSymbol(tmp_name.get());

  const auto no_tmp_msg = Str(
    "The hidden temporary for this destructure has no symbol in the scope being generated. Its binding was introduced "
    "during semantic analysis, so the destructure is being generated against a different scope to the one it was "
    "analysed in");
  RaiseIf<errors::SppInternalCompilerError>(
    sym == nullptr, {sm.CurrentScope},
    ERR_ARGS(*tmp_name, no_tmp_msg));

  const auto type_sym = sym->TypeRefIn(*sm.CurrentScope).Symbol;
  const auto llvm_type = GetLlvmType(*type_sym, ctx);
  SPP_ASSERT(llvm_type != nullptr);

  const auto alloca = LlvmEntryAlloca(
    llvm_type, "destructure.alloca" + uid, ctx);
  sym->LlvmInfo->Alloca = alloca;

  // Generate the value exactly once, into the temporary. The
  // expanded "let" statements then index the temporary.
  const auto _meta_guard = MetaGuard(meta);
  meta->AssignmentTarget = tmp_name;
  const auto llvm_val = llvm_subject != nullptr
    ? llvm_subject
    : meta->LetStatementValue->Stage11_CodeGen(&sm, meta, ctx);
  ctx->Builder.CreateStore(llvm_val, alloca);
}

auto spp::analyse::utils::destructure_utils::DestructureStage8(
  LocalVariableAst const &destructure, Vec<Unique<LocalVariableAst>> const &elems,
  Vec<Unique<LetStatementInitializedAst>> const &new_asts, Shared<IdentifierAst> const &tmp_name,
  LetStatementInitializedAst *const cond_let, const bool from_case_pattern, ScopeManager &sm,
  CompilerMetaData *meta) -> void {
  // The hidden temporary holds the only analysis of the value,
  // so the value is checked (and its scopes walked) here, then
  // the flow-typing variable, if flow typing introduced one.
  if (tmp_name != nullptr) { DestructureTempStage8(destructure, *tmp_name, sm, meta); }
  if (cond_let != nullptr) { cond_let->Stage8_CheckMemory(&sm, meta); }

  // Check the memory state of the elements. Each expanded
  // binding reads one field off the value, so each records
  // a partial move of it, and the destructure marks the
  // whole value moved once they are done.
  for (auto const &x : new_asts) { x->Stage8_CheckMemory(&sm, meta); }

  // A pattern that takes something apart has to account for
  // every owned part of what it took; one that only tests
  // the shape, or that binds the rest into a name of its own,
  // has nothing left over to answer for. A case pattern binding
  // nothing by move is a test of the shape, and takes nothing;
  // a "let" always takes its value.
  const auto accounts_for_parts = destructure.BindsByMove()
    and not genex::any_of(elems, [](auto const &elem) { return elem->TakesRest(); });

  // A value made on the spot ("let P(a, ..) = mk()") is held
  // by the hidden temporary, which answers for its parts as
  // a named value does: one the bindings left behind would be
  // dropped with nothing to drop it. Otherwise taking every
  // element off it takes it, so it is left moved rather than
  // partly moved. Todo: Clean this
  if (tmp_name != nullptr) {
    if (accounts_for_parts and not (from_case_pattern and not destructure.BindsByMove())) {
      if (auto *const tmp_sym = sm.CurrentScope->FindVarSymbol(tmp_name.get()); tmp_sym != nullptr) {
        const auto region = regions::Place{.Root = tmp_sym, .Steps = {}};
        if (auto skipped = regions::FirstUnaccountedPart(*tmp_sym, region, sm); not skipped.empty()) {
          // Named after the value as written, not the hidden temporary holding it.
          if (skipped.starts_with(tmp_name->Val)) {
            skipped = meta->LetStatementValue->ToString() + skipped.substr(tmp_name->Val.size());
          }
          Raise<errors::SppDestructureSkipsOwnedPartError>(
            {sm.CurrentScope},
            ERR_ARGS(destructure, *meta->LetStatementValue, StrView(skipped)));
        }
      }
    }
    ConsumeDestructureTemp(*tmp_name, sm);
    return;
  }
  if (from_case_pattern and not destructure.BindsByMove()) { return; }
  ConsumeDestructureSource(destructure, from_case_pattern, accounts_for_parts, sm, meta);
}

auto spp::analyse::utils::destructure_utils::DestructureStage9(
  Vec<Unique<LetStatementInitializedAst>> const &new_asts, Shared<IdentifierAst> const &tmp_name,
  LetStatementInitializedAst *cond_let, ScopeManager &sm, CompilerMetaData *meta) -> void {
  // Hand the already-resolved value to the hidden temporary
  // so the elements can index it, then resolve the flow-typing
  // variable, if flow typing introduced one, and each element.
  if (tmp_name != nullptr) { DestructureTempStage9(tmp_name, sm, meta); }
  if (cond_let != nullptr) { cond_let->Stage9_CompTimeResolve(&sm, meta); }
  for (auto const &x : new_asts) { x->Stage9_CompTimeResolve(&sm, meta); }
}

auto spp::analyse::utils::destructure_utils::DestructureSequenceStage7(
  LocalVariableAst const &self,
  Vec<Unique<LocalVariableAst>> const &elems,
  SequenceShape const &shape,
  Shared<IdentifierAst> &tmp_name,
  Vec<Unique<LetStatementInitializedAst>> &new_asts,
  const bool from_case_pattern,
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  using asts::LocalVariableDestructureSkipMultipleArgumentsAst;
  using asts::LocalVariableDestructureSkipSingleArgumentAst;

  // Only 1 "multi-skip" allowed in a destructure.
  const auto multi_arg_skips = elems
    | genex::views::ptr
    | genex::views::cast_dynamic<LocalVariableDestructureSkipMultipleArgumentsAst*>()
    | genex::to<Vec>();
  RaiseIf<errors::SppMultipleRestPatternsError>(
    multi_arg_skips.size() > 1, {sm->CurrentScope},
    ERR_ARGS(self, *multi_arg_skips[0], *multi_arg_skips[1]));

  // The value is of the destructure's kind, with as many elements as are written (fewer only with a "..").
  const auto val = meta->LetStatementValue;
  const auto val_type = val->InferType(sm, meta);
  const auto num_lhs_elems = elems.size();
  const auto num_rhs_elems = shape.CheckAndCount(*val, val_type);
  if ((num_lhs_elems < num_rhs_elems and multi_arg_skips.empty()) or num_lhs_elems > num_rhs_elems) {
    shape.RaiseSizeMismatch(num_lhs_elems, *val, num_rhs_elems);
  }

  // Bind the value to a hidden temporary, and index that from every element, so the value is analysed and evaluated
  // once for the whole pattern.
  auto effective_val = static_cast<ExpressionAst const*>(val);
  if (not IsDestructurePlaceExpression(*val) and not meta->LetStatementFromUninitialized) {
    tmp_name = BindDestructureTemporary(val, val_type, *sm);
    effective_val = tmp_name.get();
  }
  else {
    tmp_name = nullptr; // Clear from clone.
  }
  const auto index_of = [&](const std::size_t i) -> Unique<ExpressionAst> {
    auto index = MakeUnique<IdentifierAst>(val->PosEnd(), std::to_string(i));
    auto field = MakeUnique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(index));
    return MakeUnique<asts::PostfixExpressionAst>(AstClone(effective_val), std::move(field));
  };

  // Elements before the skip keep their own position; elements after it are counted back from the end of the value
  // (there are "num_lhs_elems - skip_index - 1" of them), not forward from "num_lhs_elems", which would over-count by
  // the (unindexed) skip slot itself.
  const auto skip_index = not multi_arg_skips.empty()
    ? genex::position(elems | genex::views::ptr, [&](auto const &x) { return x == multi_arg_skips[0]; }).value()
    : elems.size() - 1;

  // A bound ".." ("let [a, ..b, c] = t") collects what it skips.
  auto bound_multi_skip = Unique<ExpressionAst>(nullptr);
  if (not multi_arg_skips.empty() and multi_arg_skips[0]->Binding != nullptr) {
    auto skipped = Vec<Unique<ExpressionAst>>();
    for (auto i = skip_index; i < skip_index + num_rhs_elems - num_lhs_elems + 1; ++i) {
      skipped.emplace_back(index_of(i));
    }
    bound_multi_skip = shape.MakeRest(std::move(skipped));
  }

  auto indexes = genex::views::iota(0uz, skip_index + 1uz) | genex::to<Vec>();
  indexes.append_range(
    genex::views::iota(num_rhs_elems - num_lhs_elems + skip_index + 1uz, num_rhs_elems) | genex::to<Vec>());

  // One "let" per element; a skip ("_", or an unbound "..") converts to nothing.
  const auto add_let = [&](Unique<LocalVariableAst> &&var, Unique<ExpressionAst> &&value) {
    auto new_ast = MakeUnique<LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, std::move(value));
    if (from_case_pattern) { new_ast->Var->MarkFromCasePattern(); }
    new_ast->Stage7_AnalyseSemantics(sm, meta);
    new_asts.emplace_back(std::move(new_ast));
  };
  for (auto const &[i, elem] : genex::views::zip(indexes, elems | genex::views::ptr)) {
    if (const auto rest = elem->To<LocalVariableDestructureSkipMultipleArgumentsAst>(); rest != nullptr) {
      if (rest->Binding != nullptr) { add_let(AstClone(rest->Binding), std::move(bound_multi_skip)); }
    }
    else if (elem->To<LocalVariableDestructureSkipSingleArgumentAst>() == nullptr) {
      add_let(AstClone(elem), index_of(i));
    }
  }
}

auto spp::analyse::utils::destructure_utils::IsDestructurePlaceExpression(
  ExpressionAst const &expr) -> bool {
  // Strip the member accesses off the expression: "a.b.c"
  // becomes "a". Any other postfix operator (a function
  // call, an early return etc) means the expression produces
  // a new value rather than naming existing storage.
  auto cur = static_cast<Ast const*>(&expr);
  while (const auto postfix = cur->To<PostfixExpressionAst>()) {
    if (postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() == nullptr) { return false; }
    cur = postfix->Lhs.get();
  }

  return cur->To<IdentifierAst>() != nullptr;
}
