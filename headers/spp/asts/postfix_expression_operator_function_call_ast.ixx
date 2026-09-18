module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorFunctionCallAst);
use(spp::analyse::scopes, class Scope);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FunctionCallArgumentAst);
use(spp::asts, struct FunctionCallArgumentGroupAst);
use(spp::asts, struct FunctionCallArgumentPositionalAst);
use(spp::asts, struct FunctionParameterGroupAst);
use(spp::asts, struct FoldExpressionAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);
use(spp::asts, struct UnaryExpressionOperatorAsyncAst);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorFunctionCallAst final : PostfixExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorFunctionCallAst);

  /// The generic arguments for the function call.
  Unique<GenericArgumentGroupAst> GnArgGroup;

  /// The function arguments for the function call.
  Unique<FunctionCallArgumentGroupAst> FnArgGroup;

  /// The optional ".." fold token. This folds all tuples in the
  /// argument group, calling the function multiple times with
  /// each tuple element as the argument for the non-tuple
  /// parameters it has mapped to.
  Unique<FoldExpressionAst> Fold;

  struct {
    Ast *OriginalExpr;
  } Source;

  explicit PostfixExpressionOperatorFunctionCallAst(
    decltype(GnArgGroup) &&generic_arg_group,
    decltype(FnArgGroup) &&arg_group,
    decltype(Fold) &&fold);

  ~PostfixExpressionOperatorFunctionCallAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Unique<PostfixExpressionOperatorAst> override;

  auto MarkAsAsync(Ast *async_token) -> void;

  SPP_ATTR_NODISCARD auto Target() const -> FunctionPrototypeAst*;

  auto SetClosureDummyProto(Unique<FunctionPrototypeAst> &&proto) -> void;

  auto SetTransformedAst(Unique<PostfixExpressionAst> &&ast) -> void;

  SPP_ATTR_NODISCARD auto GetTransformedAst() const -> PostfixExpressionAst*;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  struct _OInfo {
    Scope const *OverloadScope;
    FunctionPrototypeAst *Proto;
  };

  std::optional<_OInfo> _OverloadInfo;
  Unique<PostfixExpressionAst> _TransformedAst;
  Unique<FunctionCallArgumentGroupAst> _ClosureDummyArgGroup;
  Unique<FunctionCallArgumentPositionalAst> _ClosureDummyArg;
  Unique<FunctionPrototypeAst> _ClosureDummyProto;
  Vec<Unique<PostfixExpressionOperatorFunctionCallAst>> _FoldedAsts;
  Ast *_IsAsync;
  bool _IsCoroAndAutoResume;

  auto _HandleFunctionFolding(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Vec<Unique<PostfixExpressionOperatorFunctionCallAst>>;
};
