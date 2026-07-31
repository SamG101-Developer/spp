module;
#include <spp/macros.hpp>

module spp.analyse.utils.destructure_utils;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
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
  Vec<Unique<asts::LocalVariableAst>> const &elems)
  -> Vec<Shared<asts::IdentifierAst>> {
  // Recursively walk the destructure pattern to extract all identifiers.
  return elems
    | genex::views::transform(&asts::LocalVariableAst::ExtractNames)
    | genex::views::join
    | genex::to<Vec>();
}

auto spp::analyse::utils::destructure_utils::UnmatchableSingleIdentifier(
  const std::size_t pos)
  -> Shared<asts::IdentifierAst> {
  // No single identifier represents a binding destructuring.
  return MakeShared<asts::IdentifierAst>(pos, kUnmatchableTag);
}

auto spp::analyse::utils::destructure_utils::IsDestructurePlaceExpression(
  asts::ExpressionAst const &expr)
  -> bool {
  // Strip the member accesses off the expression: "a.b.c" becomes "a". Any other postfix operator (a function call,
  // an early return etc) means the expression produces a new value rather than naming existing storage.
  auto cur = static_cast<asts::Ast const*>(&expr);
  while (auto const *postfix = cur->To<asts::PostfixExpressionAst>()) {
    if (postfix->Op->To<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>() == nullptr) { return false; }
    cur = postfix->Lhs.get();
  }

  return cur->To<asts::IdentifierAst>() != nullptr;
}

auto spp::analyse::utils::destructure_utils::BindDestructureTemporary(
  asts::Ast const &owner,
  asts::ExpressionAst *const val,
  Shared<asts::TypeAst> const &val_type,
  scopes::ScopeManager &sm)
  -> Shared<asts::IdentifierAst> {
  // The "$" prefix cannot be written in user code, so the temporary can never collide with a real binding.
  auto name = MakeShared<asts::IdentifierAst>(val->PosEnd(), "$_dst_" + spp::utils::Uid(&owner));

  // Mirror the symbol an initialized single-identifier "let" would create.
  const auto sym = MakeShared<scopes::VariableSymbol>(
    name, val_type, sm.CurrentScope, true);
  sym->MemInfo->AstInitialization = {name.get(), sm.CurrentScope};
  sym->MemInfo->AstInitializationOrigin = {name.get(), sm.CurrentScope};
  sym->MemInfo->InitializationCounter = 1;

  // Carry the value's convention over, so that destructuring a borrow yields borrowed elements.
  if (val_type->GetConvention() != nullptr) {
    sym->MemInfo->AstBorrowed = {val, sm.CurrentScope};
  }

  sm.CurrentScope->AddVarSymbol(sym);
  return name;
}

auto spp::analyse::utils::destructure_utils::DestructureTempStage8(
  asts::Ast const &owner,
  asts::IdentifierAst const &tmp_name,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *const meta)
  -> void {
  // The value is moved into the temporary as a whole, so it is checked (and consumed) once here, rather than once per
  // expanded "let". This traversal is also what walks the scopes the value created in stage 7.
  meta->LetStatementValue->Stage8_CheckMemory(&sm, meta);
  mem_utils::ValidateSymbolMemory(*meta->LetStatementValue, owner, sm, true, true, true, true, meta);

  // Mark the temporary as initialized by the value.
  const auto sym = sm.CurrentScope->GetVarSymbol(&tmp_name);
  sym->MemInfo->InitializedBy(tmp_name, sm.CurrentScope);
}

auto spp::analyse::utils::destructure_utils::DestructureTempStage9(
  Shared<asts::IdentifierAst> const &tmp_name,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *const meta)
  -> void {
  // The owning "let" statement has already resolved the value, so the temporary takes a copy of that result rather
  // than resolving the value a second time (which would walk the value's scopes twice).
  const auto sym = sm.CurrentScope->GetVarSymbol(tmp_name.get());
  sym->CompTimeValue = AstClone(meta->CmpResult);
}

auto spp::analyse::utils::destructure_utils::DestructureTempStage11(
  Shared<asts::IdentifierAst> const &tmp_name,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData *const meta,
  codegen::LLvmCtx *const ctx)
  -> void {
  // Give the temporary its own stack slot.
  const auto uid = "." + spp::utils::Uid(tmp_name.get());
  const auto sym = sm.CurrentScope->GetVarSymbol(tmp_name.get());
  const auto type_sym = sm.CurrentScope->GetTypeSymbol(sym->Type.get());
  const auto llvm_type = codegen::GetLlvmType(*type_sym, ctx);
  SPP_ASSERT(llvm_type != nullptr);

  const auto alloca = codegen::LlvmEntryAlloca(llvm_type, "destructure.alloca" + uid, ctx);
  sym->LlvmInfo->Alloca = alloca;

  // Generate the value exactly once, into the temporary. The expanded "let" statements then index the temporary.
  meta->Save();
  meta->AssignmentTarget = tmp_name;
  const auto llvm_val = meta->LetStatementValue->Stage11_CodeGen(&sm, meta, ctx);
  ctx->Builder.CreateStore(llvm_val, alloca);
  meta->Restore();
}
