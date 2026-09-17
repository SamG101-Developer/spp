module;
#include <spp/macros.hpp>

export module spp.asts.loop_else_statement_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.mixins.type_inferrable_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(LoopElseStatementAst);
use(spp::asts, struct InnerScopeExpressionAst);
use(spp::asts, struct StatementAst);
use(spp::asts, struct TokenAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::LoopElseStatementAst final : Ast, mixins::TypeInferrableAst {
  SPP_AST_KEY_FUNCTIONS(LoopElseStatementAst);

  /// The "else" keyword starting the loop's else statement.
  Unique<TokenAst> TokElse;

  /// The body of the else statement, executed if the loop
  /// condition is immediately false, or the iterable is
  /// already exhausted (no loops take place).
  Unique<InnerScopeExpressionAst> Body;

  LoopElseStatementAst(
    decltype(TokElse) &&tok_else,
    decltype(Body) &&body);

  ~LoopElseStatementAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;
};
