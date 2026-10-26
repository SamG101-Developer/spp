module;
#include <spp/macros.hpp>

export module spp.asts.is_expression_ast;
import spp.asts.ast_kind;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(IsExpressionAst);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct CaseExpressionAst);
use(spp::asts, struct CasePatternVariantAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);

SPP_EXP_CLS struct spp::asts::IsExpressionAst final : ExpressionAst {
  SPP_AST_KEY_FUNCTIONS(IsExpressionAst);

  /// The left-hand-side value being tested.
  Unique<ExpressionAst> Lhs;

  /// The "is" operator token.
  Unique<TokenAst> TokOp;

  /// The right-hand-side pattern being tested against.
  Unique<CasePatternVariantAst> Rhs;

  struct {
    std::size_t OriginalPosStart;
    std::size_t OriginalPosEnd;
  } Source;

  IsExpressionAst(
    decltype(Lhs) &&lhs,
    decltype(TokOp) &&tok_op,
    decltype(Rhs) &&rhs);

  ~IsExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto EvaluateCompTime(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  Shared<CaseExpressionAst> _MappedFn;

  Shared<IdentifierAst> _LhsAsId;

  /// The bindings this "is" exposes to the scope it is written
  /// in (its pattern's names, and the subject flow-typed by it),
  /// as Stage 7 made them. Every later walk declares them where
  /// the "is" stands.
  Vec<Shared<VariableSymbol>> _Exposed;

  /// Put the exposed bindings under their names in the current
  /// scope.
  auto _DeclareExposed(ScopeManager &sm) const -> void;
};
