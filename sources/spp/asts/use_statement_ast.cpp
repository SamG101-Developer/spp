module;
#include <spp/macros.hpp>

module spp.asts.use_statement_ast;
import spp.asts.annotation_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import genex;

SPP_MOD_BEGIN
UseStatementAst::UseStatementAst(
  decltype(Annotations) &&annotations,
  decltype(TokUse) &&tok_use,
  decltype(OldType) old_type) :
  Annotations(std::move(annotations)),
  TokUse(std::move(tok_use)),
  OldType(std::move(old_type)),
  _Generated(false),
  _Conversion(nullptr) {
}

UseStatementAst::~UseStatementAst() = default;

auto UseStatementAst::PosStart() const -> std::size_t {
  // Use the "use" token.
  return TokUse->PosStart();
}

auto UseStatementAst::PosEnd() const -> std::size_t {
  // Use the old type.
  return OldType->PosEnd();
}

auto UseStatementAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<UseStatementAst>(
    AstCloneVec(Annotations),
    AstClone(TokUse),
    AstCloneShared(OldType));
  ast->_Ctx = _Ctx;
  ast->_Scope = _Scope;
  for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
  return ast;
}

auto UseStatementAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_EXTEND(Annotations, "\n");
  SPP_STRING_APPEND_RAW(not Annotations.IsEmpty() ? "\n" : "");
  SPP_STRING_APPEND(TokUse).append(" ");
  SPP_STRING_APPEND(OldType);
  SPP_STRING_END;
}

auto UseStatementAst::Stage1_PreProcess(
  Ast *ctx) -> void {
  // Pre-process the annotations.
  Ast::Stage1_PreProcess(ctx);
  for (auto const &a : Annotations) { a->SetAstCtx(this); }
}

auto UseStatementAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Run the steps for the annotations.
  Ast::Stage2_GenTopLvlScopes(sm, meta);
  for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

  // Create the type statement AST conversion.
  const auto new_type = dynamic_shared_cast<TypeIdentifierAst>(
    OldType->LastTypePart()->WithoutGenerics());

  _Conversion = MakeUnique<TypeStatementAst>(
    std::move(Annotations), nullptr, new_type, nullptr, nullptr, AstCloneShared(OldType));
  _Conversion->MarkFromUseStatement();
  _Conversion->Stage2_GenTopLvlScopes(sm, meta);
  _Generated = true;
}

auto UseStatementAst::Stage3_GenTopLvlAliases(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Generate the top-level alias for the converted type statement.
  _Conversion->Stage3_GenTopLvlAliases(sm, meta);
}

auto UseStatementAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Qualify the types in the conversion AST.
  _Conversion->Stage4_ResolveDeclarations(sm, meta);
}

auto UseStatementAst::Stage5_LoadSupScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Load the super scopes for the conversion AST.
  _Conversion->Stage5_LoadSupScopes(sm, meta);
}

auto UseStatementAst::Stage6_PreAnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Skip all scopes, as this is a pre-generated AST.
  _Conversion->Stage6_PreAnalyseSemantics(sm, meta);
}

auto UseStatementAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Analyse the conversion AST.
  _Conversion->Stage7_AnalyseSemantics(sm, meta);
}

auto UseStatementAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check memory for the conversion AST.
  _Conversion->Stage8_CheckMemory(sm, meta);
}

auto UseStatementAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Comptime resolve the conversion AST.
  return _Conversion->Stage9_CompTimeResolve(sm, meta);
}

auto UseStatementAst::Stage10_PreCodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Code gen for the conversion AST.
  return _Conversion->Stage10_PreCodeGen(sm, meta, ctx);
}

auto UseStatementAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Code gen for the conversion AST.
  return _Conversion->Stage11_CodeGen(sm, meta, ctx);
}

SPP_MOD_END
