module;
#include <spp/macros.hpp>

export module spp.asts.let_statement_uninitialized_ast;
import spp.asts.ast_kind;
import spp.asts.let_statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(LetStatementUninitializedAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::LetStatementUninitializedAst final : LetStatementAst {
  SPP_AST_KEY_FUNCTIONS(LetStatementUninitializedAst);

  /// The "let" token that starts this statement.
  Unique<TokenAst> TokLet;

  /// The variable being declared, naming the symbols that
  /// will be created in the scope the "let" is defined in.
  Unique<LocalVariableAst> Var;

  /// The ":" token that separates the variable name from its
  /// type.
  Unique<TokenAst> TokColon;

  /// The type of the uninitialized variable, used to check
  /// that values later assigned to it are the correct type.
  Shared<TypeAst> Type;

  struct {
    Shared<TypeAst> OriginalType;
  } Source;

  LetStatementUninitializedAst(
    decltype(TokLet) &&tok_let,
    decltype(Var) &&var,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type);

  ~LetStatementUninitializedAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;
};
