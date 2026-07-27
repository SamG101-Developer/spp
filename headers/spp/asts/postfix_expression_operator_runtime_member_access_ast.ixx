module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct PostfixExpressionAst;
  SPP_EXP_CLS struct PostfixExpressionOperatorRuntimeMemberAccessAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst final : PostfixExpressionOperatorAst {
  /**
   * The @c . token that indicates a runtime member access operation in a postfix expression.
   */
  Unique<TokenAst> TokDot;

  /**
   * The identifier that represents the member being accessed. This is the name of the member in the class or struct.
   */
  Shared<IdentifierAst> Name;

  /**
   * Construct the PostfixExpressionOperatorMemberAccessAst with the arguments matching the members.
   * @param[in] tok_dot The @c . token that indicates a runtime member access operation in a postfix expression.
   * @param[in] name The identifier that represents the member being accessed.
   */
  explicit PostfixExpressionOperatorRuntimeMemberAccessAst(
    decltype(TokDot) &&tok_dot,
    decltype(Name) name);

  ~PostfixExpressionOperatorRuntimeMemberAccessAst() override;

  SPP_AST_KEY_FUNCTIONS;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto ExprParts() const
    -> Vec<Ast*> override;

  /**
   * The call that forwards the left-hand-side to the type this member was found on, that is the
   * @code x.fwd_ref()@endcode of the mapped access, or @c nullptr when the member belongs to the left-hand-side's own
   * type. A method call reached through forwarding needs it, because the forwarded-to value, and not the object that
   * forwards to it, is the @c self the method is invoked on.
   * @return The forwarding call applied to the left-hand-side, or @c nullptr if no forwarding took place.
   */
  SPP_ATTR_NODISCARD auto GetFwdReceiver() const
    -> PostfixExpressionAst*;

private:
  /**
   * The access rewritten against the forwarded-to value (@code x.fwd_ref().field@endcode), built when the member is
   * not found on the left-hand-side's own type but is reachable through a @c FwdRef / @c FwdMut superimposition. When
   * it is set, inference and code generation both defer to it.
   */
  Shared<PostfixExpressionAst> _MappedFwd;
};
