module;
#include <spp/macros.hpp>

export module spp.asts.let_statement_initialized_ast;
import spp.asts.ast_kind;
import spp.asts.let_statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(LetStatementInitializedAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::LetStatementInitializedAst final : LetStatementAst {
  SPP_AST_KEY_FUNCTIONS(LetStatementInitializedAst);

  /// The "let" token that starts this statement.
  Unique<TokenAst> TokLet;

  /// The variable being declared, naming the symbols that
  /// will be created in the scope the "let" is defined in.
  Unique<LocalVariableAst> Var;

  /// The optional type of the variable. It can always be
  /// inferred from the value, but providing it allows variant
  /// types to be used with values of an inner type.
  Shared<TypeAst> Type;

  /// The "=" token that precedes the initial value.
  Unique<TokenAst> TokAssign;

  /// The value that is evaluated and assigned to the
  /// variable.
  Unique<ExpressionAst> Val;

  struct {
    Shared<TypeAst> OriginalType;
  } Source;

  LetStatementInitializedAst(
    decltype(TokLet) &&tok_let,
    decltype(Var) &&var,
    decltype(Type) type,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) &&val);

  ~LetStatementInitializedAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;
};
