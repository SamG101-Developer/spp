module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorStaticMemberAccessAst);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorStaticMemberAccessAst final : PostfixExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorStaticMemberAccessAst);

  /// The "::" token that indicates a static member access.
  Unique<TokenAst> TokDblColon;

  /// The identifier of the member being accessed, ie the name
  /// of the member in the class.
  Shared<IdentifierAst> Name;

  explicit PostfixExpressionOperatorStaticMemberAccessAst(
    decltype(TokDblColon) &&tok_dbl_colon,
    decltype(Name) &&name);

  ~PostfixExpressionOperatorStaticMemberAccessAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto ExprParts() const -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  TypeSymbol *_LhsTypeSym;
};
