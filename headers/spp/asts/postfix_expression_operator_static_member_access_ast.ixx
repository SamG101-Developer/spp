module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS struct TypeSymbol;
}

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorStaticMemberAccessAst) {
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorStaticMemberAccessAst final : PostfixExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorStaticMemberAccessAst);

  /**
   * The @c :: token that indicates a static member access operation in a postfix expression.
   */
  Unique<TokenAst> TokDblColon;

  /**
   * The identifier that represents the member being accessed. This is the name of the member in the class or struct.
   */
  Shared<IdentifierAst> Name;

  /**
   * Construct the PostfixExpressionOperatorMemberAccessAst with the arguments matching the members.
   * @param[in] tok_dbl_colon The @c :: token that indicates a static member access operation in a postfix expression.
   * @param[in] name The identifier that represents the member being accessed.
   */
  explicit PostfixExpressionOperatorStaticMemberAccessAst(
    decltype(TokDblColon) &&tok_dbl_colon,
    decltype(Name) &&name);

  ~PostfixExpressionOperatorStaticMemberAccessAst() override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  auto InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto ExprParts() const -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;

private:
  analyse::scopes::TypeSymbol *_LhsTypeSym;
};
