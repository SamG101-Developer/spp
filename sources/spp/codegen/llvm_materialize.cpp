module;
#include <spp/macros.hpp>

module spp.codegen.llvm_materialize;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.uid;
import spp.utils.types;
import llvm;
import std;

auto spp::codegen::llvm_materialize(
  asts::ExpressionAst &ast,
  analyse::scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta,
  LLvmCtx *ctx)
  -> asts::IdentifierAst* {
  // Materialise an expression by assigning it to a temporary variable.
  const auto uid = "." + spp::utils::Uid(&ast);
  auto var_name = MakeShared<asts::IdentifierAst>(ast.PosStart(), "$temp" + std::move(uid));
  const auto var = MakeUnique<asts::LocalVariableSingleIdentifierAst>(nullptr, std::move(var_name), nullptr);

  // Analyse semantics and generate code for the let statement.
  meta->Save();
  meta->LetStatementFromUninitialized = true; // Prevent double analysis of the expression.
  meta->LetStatementExplicitType = ast.InferType(sm, meta);
  var->Stage7_AnalyseSemantics(sm, meta);

  // Set the lhs to the variable name.
  meta->LetStatementFromUninitialized = false; // Need to generate the expression now.
  meta->LetStatementValue = &ast;
  var->Stage11_CodeGen(sm, meta, ctx);
  meta->Restore();
  const auto materialized_val = var->To<asts::LocalVariableSingleIdentifierAst>()->Name.get();
  return materialized_val;
}

auto spp::codegen::llvm_addr_of(
  asts::ExpressionAst &ast,
  analyse::scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta,
  LLvmCtx *ctx)
  -> llvm::Value* {
  // An expression that is already a borrow evaluates to the address of what it borrows, so it is its own address:
  // this covers re-borrowing a borrowed variable, and the forwarding calls ("x.fwd_ref()") that yield one. Note: we
  // don't enforce the borrow on the llvm type, because Gen[&XXX] is valid, but not a borrow.
  if (const auto type = ast.InferType(sm, meta); type != nullptr and type->GetConvention() != nullptr) {
    const auto borrow_val = ast.Stage11_CodeGen(sm, meta, ctx);
    return borrow_val;
  }

  // A member access generates the address of its own field, which is the object a borrow of it points at. The symbol
  // lookup below cannot be used for one, because it resolves to the head of the chain ("a" in "a.b").
  if (asts::IsRuntimeMemberAccess(&ast)) {
    meta->Save();
    meta->LlvmWantAddress = true;
    const auto field_ptr = ast.Stage11_CodeGen(sm, meta, ctx);
    meta->Restore();
    SPP_ASSERT(field_ptr->getType()->isPointerTy());
    return field_ptr;
  }

  // A symbolic expression (a variable, or a static member of a type or namespace) is already allocated somewhere.
  if (const auto sym = sm->CurrentScope->GetVarSymbolOutermost(ast).First; sym != nullptr) {
    const auto llvm_alloca = sym->LlvmInfo->Alloca;
    SPP_ASSERT(llvm_alloca != nullptr and llvm_alloca->getType()->isPointerTy());
    return llvm_alloca;
  }

  // Anything else has no storage of its own, so give it some by binding it to a temporary.
  const auto materialized_val = llvm_materialize(ast, sm, meta, ctx);
  const auto materialized_sym = sm->CurrentScope->GetVarSymbol(materialized_val);
  SPP_ASSERT(materialized_sym->LlvmInfo->Alloca->getType()->isPointerTy());
  return materialized_sym->LlvmInfo->Alloca;
}
