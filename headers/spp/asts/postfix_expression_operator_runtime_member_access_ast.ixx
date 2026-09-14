module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorRuntimeMemberAccessAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst final : PostfixExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorRuntimeMemberAccessAst);

  /// The "." token that indicates a runtime member access.
  Unique<TokenAst> TokDot;

  /// The identifier of the member being accessed, ie the name
  /// of the member in the class.
  Shared<IdentifierAst> Name;

  struct {
    Ast *OriginalExpr; // Original asts mapped into member accesses - (try? op for example)
  } Source;

  explicit PostfixExpressionOperatorRuntimeMemberAccessAst(
    decltype(TokDot) &&tok_dot,
    decltype(Name) name);

  ~PostfixExpressionOperatorRuntimeMemberAccessAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto ExprParts() const -> Vec<IdentifierAst*> override;

  /// The call that forwards the left-hand-side to the type this
  /// member was found on, ie the "x.fwd_ref()" of the mapped
  /// access, or "nullptr" when the member belongs to the
  /// left-hand-side's own type. A method call reached through
  /// forwarding needs it, because the forwarded-to value, not
  /// the forwarding object, is the "self" the method is
  /// invoked on.
  SPP_ATTR_NODISCARD auto GetFwdReceiver() const -> PostfixExpressionAst*;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  /// The access rewritten against the forwarded-to value
  /// ("x.fwd_ref().field"), built when the member is not found
  /// on the left-hand-side's own type but is reachable through
  /// a "FwdRef" / "FwdMut" superimposition. When set, inference
  /// and code generation both defer to it.
  Shared<PostfixExpressionAst> _MappedFwd;
};
