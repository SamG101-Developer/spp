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
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
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

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto ExprParts() const -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  /// The type a type left-hand-side names, or the type it forwards
  /// to when the member is reached that way ("_Resolve").
  TypeSymbol *_LhsTypeSymbol;

  /// The namespace a namespace left-hand-side resolved to, kept so a
  /// copy of this access analysed elsewhere (a parameter default at
  /// a call site) still reaches it, as the lhs may be relative.
  Scope const *_LhsNsScope;

  /// The member the access names ("_Resolve"): nullptr for a
  /// namespace inside a namespace, or a name not found.
  VariableSymbol *_Member = nullptr;

  /// The scope "_Member" is read in (the type's, the type it is
  /// forwarded to, or the namespace's); nullptr until resolved.
  Scope const *_MemberScope = nullptr;

  auto LhsNsScope(ScopeManager const *sm) -> Scope const*;

  /// Look the name up once, from "sm": a type's member as "::"
  /// reaches it ("member_lookup::MemberOf"), or failing that, the
  /// forwarded-to type's; a namespace's own. Stage 7 resolves it
  /// again in its own scope; every later read reuses it.
  auto _Resolve(ScopeManager const *sm) -> VariableSymbol*;
};
