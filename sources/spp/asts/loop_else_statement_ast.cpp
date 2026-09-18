module;
#include <spp/macros.hpp>

module spp.asts.loop_else_statement_ast;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
LoopElseStatementAst::LoopElseStatementAst(
  decltype(TokElse) &&tok_else,
  decltype(Body) &&body) :
  TokElse(std::move(tok_else)),
  Body(std::move(body)) {
}

LoopElseStatementAst::~LoopElseStatementAst() = default;

auto LoopElseStatementAst::PosStart() const -> std::size_t {
  // Use the "else" token.
  return TokElse->PosStart();
}

auto LoopElseStatementAst::PosEnd() const -> std::size_t {
  // Use the "else" token.
  return TokElse->PosEnd();
}

auto LoopElseStatementAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<LoopElseStatementAst>(
    AstClone(TokElse),
    AstClone(Body));
}

auto LoopElseStatementAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokElse);
  SPP_STRING_APPEND(Body);
  SPP_STRING_END;
}

auto LoopElseStatementAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Create a scope and analyse the body.
  auto scope_name = ScopeBlockName::FromParts(
    "loop-else-stmt", {}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
  Body->Stage7_AnalyseSemantics(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto LoopElseStatementAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check the body for memory issues.
  sm->MoveToNextScope();
  Body->Stage8_CheckMemory(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto LoopElseStatementAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Generate code for the body.
  sm->MoveToNextScope();
  const auto val = Body->Stage11_CodeGen(sm, meta, ctx);
  sm->MoveOutOfCurrentScope();
  return val;
}

auto LoopElseStatementAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // The type of an else statement is the type of its body.
  return Body->InferType(sm, meta);
}

auto LoopElseStatementAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  return Body->InferTypeRef(sm, meta);
}

SPP_MOD_END
