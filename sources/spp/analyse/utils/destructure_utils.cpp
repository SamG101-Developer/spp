module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.destructure_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.linear_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_destructure_array_ast;
import spp.asts.local_variable_destructure_attribute_binding_ast;
import spp.asts.local_variable_destructure_object_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.local_variable_destructure_tuple_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_type;
import spp.utils.uid;
import genex;
import llvm;

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

auto spp::analyse::utils::destructure_utils::IsDestructurePlaceExpression(
  ExpressionAst const &expr) -> bool {
  // Strip the member accesses off the expression: "a.b.c"
  // becomes "a". Any other postfix operator (a function call,
  // an early return etc) means the expression produces a new
  // value rather than naming existing storage.
  auto cur = static_cast<Ast const*>(&expr);
  while (auto const *postfix = cur->To<PostfixExpressionAst>()) {
    if (postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() == nullptr) { return false; }
    cur = postfix->Lhs.get();
  }

  return cur->To<IdentifierAst>() != nullptr;
}

auto spp::analyse::utils::destructure_utils::BindDestructureTemporary(
  Ast const &owner, ExpressionAst *val, Shared<TypeAst> const &val_type,
  ScopeManager &sm) -> Shared<IdentifierAst> {
  // The "$" prefix cannot be written in user code, so the
  // temporary can never collide with a real binding.
  auto name = MakeShared<IdentifierAst>(val->PosEnd(), "$_dst_" + spp::utils::Uid(&owner));

  // Mirror the symbol an initialized single-identifier "let"
  // would create.
  const auto sym = MakeShared<VariableSymbol>(
    name, val_type, sm.CurrentScope, VariableKind::Temporary, true);
  sym->MemInfo->AstInitialization = {name.get(), sm.CurrentScope};
  sym->MemInfo->AstInitializationOrigin = {name.get(), sm.CurrentScope};
  sym->MemInfo->InitializationCounter = 1;

  // Carry the value's convention over, so that destructuring
  // a borrow yields borrowed elements.
  if (val_type->GetConvention() != nullptr) {
    sym->MemInfo->AstBorrowed = {val, sm.CurrentScope};
  }

  sm.CurrentScope->AddVarSymbol(sym);
  return name;
}

auto spp::analyse::utils::destructure_utils::ConsumeDestructureSource(
  Ast const &owner, const bool from_case_pattern, const bool any_binding_is_moving,
  ScopeManager &sm, CompilerMetaData const *meta) -> void {
  // If the destructure is not from a case pattern, then
  // nothing is consumed. The case ast must've also specified
  // that the condition is meant to be consumed.
  if (from_case_pattern and meta->CaseConsumedSubjects.IsEmpty()) { return; }

  // If this is for a let statement + local variable destructure
  // (not a case pattern), then there must be a value, and it
  // must name existing storage.
  const auto val = meta->LetStatementValue;
  if (val == nullptr or not IsDestructurePlaceExpression(*val)) { return; }

  // Get the outermost (root) symbol for the value being
  // destructured.
  const auto sym = sm.CurrentScope->GetVarSymbolOutermost(*val).first;
  if (sym == nullptr) { return; }

  // Destructuring a borrow reads through it. The value behind
  // it belongs to someone else, so it is not consumed here.
  if (spp::get<0>(sym->MemInfo->AstBorrowed) != nullptr) { return; }
  if (sym->Type != nullptr and sym->Type->GetConvention() != nullptr) { return; }

  // A destructure takes the value apart, so what it does
  // not bind is left with nothing holding it. This creates
  // leaks because there is then no way to drop those values,
  // so make sure all movables are bound.
  if (any_binding_is_moving) {
    // Get the region path of the value, and check if any parts
    // have not been considered by the destructure. These cannot
    // be left unbound, because they would silently drop.
    const auto region = mem_utils::RegionPath(*val);

    if (const auto skipped = linear_utils::FirstUnaccountedPart(*sym, region, sm); not skipped.empty()) {
      Raise<errors::SppDestructureSkipsOwnedPartError>(
        {sm.CurrentScope}, ERR_ARGS(owner, *val, StrView(skipped)));
    }
  }

  // "let Self(x) = self" takes the symbol itself, so the
  // symbol is moved. "let Self(x) = self.inner" takes one
  // region of it, which is a partial move like any other.
  if (val->To<IdentifierAst>() != nullptr) {
    if (from_case_pattern) { return; }
    sym->MemInfo->MovedBy(owner, sm.CurrentScope);
    sym->MemInfo->AstPartialMoves.Clear();
  }
  else {
    sym->MemInfo->AstPartialMoves.EmplaceBack(val);
  }
}

auto spp::analyse::utils::destructure_utils::ConsumeDestructureTemp(
  IdentifierAst const &tmp_name, ScopeManager const &sm) -> void {
  // Every part the bindings read came off the temporary, so
  // between them they took all of it.
  const auto sym = sm.CurrentScope->GetVarSymbol(&tmp_name);
  if (sym == nullptr) { return; }
  sym->MemInfo->MovedBy(tmp_name, sm.CurrentScope);
  sym->MemInfo->AstPartialMoves.Clear();
}

auto spp::analyse::utils::destructure_utils::DestructureTempStage8(
  Ast const &owner, IdentifierAst const &tmp_name,
  ScopeManager &sm, CompilerMetaData *meta) -> void {
  using mem_utils::ValidateSymbolMemory;
  // The value is moved into the temporary as a whole, so it
  // is checked (and consumed) once here, rather than once
  // per expanded "let". This traversal is also what walks
  // the scopes the value created in stage 7.
  meta->LetStatementValue->Stage8_CheckMemory(&sm, meta);
  ValidateSymbolMemory(
    *meta->LetStatementValue, owner, sm, true, true, true, true, meta);

  // Mark the temporary as initialized by the value.
  const auto sym = sm.CurrentScope->GetVarSymbol(&tmp_name);
  sym->MemInfo->InitializedBy(tmp_name, sm.CurrentScope);
}

auto spp::analyse::utils::destructure_utils::DestructureTempStage9(
  Shared<IdentifierAst> const &tmp_name, ScopeManager const &sm,
  CompilerMetaData const *meta) -> void {
  // The owning "let" statement has already resolved the
  // value, so the temporary takes a copy of that result
  // rather than resolving the value a second time (which
  // would walk the value's scopes twice).
  const auto sym = sm.CurrentScope->GetVarSymbol(tmp_name.get());
  sym->CompTimeValue = AstClone(meta->CmpResult);
}

auto spp::analyse::utils::destructure_utils::DestructureTempStage11(
  Shared<IdentifierAst> const &tmp_name, llvm::Value *llvm_subject,
  ScopeManager &sm, CompilerMetaData *meta, LlvmCtx *ctx) -> void {
  // Give the temporary its own stack slot.
  const auto uid = "." + spp::utils::Uid(tmp_name.get());
  const auto sym = sm.CurrentScope->GetVarSymbol(tmp_name.get());

  const auto no_tmp_msg = Str(
    "The hidden temporary for this destructure has no symbol in the scope being generated. Its binding was introduced "
    "during semantic analysis, so the destructure is being generated against a different scope to the one it was "
    "analysed in");
  RaiseIf<errors::SppInternalCompilerError>(
    sym == nullptr, {sm.CurrentScope},
    ERR_ARGS(*tmp_name, no_tmp_msg));

  const auto type_sym = sym->TypeRefIn(*sm.CurrentScope).Sym;
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

  // Taking every element off a value takes the value, so the
  // symbol holding it is left moved rather than partly moved.
  if (tmp_name != nullptr) {
    ConsumeDestructureTemp(*tmp_name, sm);
    return;
  }

  // A pattern that takes something apart has to account for
  // every owned part of what it took; one that only tests
  // the shape, or that binds the rest into a name of its own,
  // has nothing left over to answer for.
  const auto accounts_for_parts = destructure.BindsByMove()
    and not genex::any_of(elems, [](auto const &elem) { return elem->TakesRest(); });
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
