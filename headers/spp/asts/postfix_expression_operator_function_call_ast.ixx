module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_function_call_ast;
import spp.analyse.scopes.type_key;
import spp.asts.ast_kind;
import spp.asts.convention_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorFunctionCallAst);
use(spp::analyse::scopes, struct ExprSubst);
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

  SPP_ATTR_NODISCARD auto ReadExpr(
    analyse::scopes::ExprSubst const &sub) const
    -> Unique<PostfixExpressionOperatorAst> override;

  auto MarkAsAsync(Ast *async_token) -> void;

  SPP_ATTR_NODISCARD auto Target() const -> FunctionPrototypeAst*;

  /// The scope "Target" is declared in (the overload's own), which an error showing the target is formatted against.
  SPP_ATTR_NODISCARD auto TargetScope() const -> Scope const*;

  /// Whether this is a method called on a value ("obj.m(a)"): its
  /// receiver is the "self" argument, first, and the left-hand side
  /// ("obj.m") names the method rather than being a use of "obj".
  SPP_ATTR_NODISCARD auto HasReceiver() const -> bool;

  /// Whether "arg" is that receiver: this call's first argument.
  SPP_ATTR_NODISCARD auto IsReceiverArg(FunctionCallArgumentAst const &arg) const -> bool;

  /// The prototype the call resolved to, as resolved ("Target" is
  /// what is emitted, which for a lowered "GenOnce" is no coroutine).
  SPP_ATTR_NODISCARD auto ResolvedProto() const -> FunctionPrototypeAst*;

  /// Whether the call is made "async".
  SPP_ATTR_NODISCARD auto IsAsync() const -> bool;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  struct _OInfo {
    Scope const *OverloadScope;
    FunctionPrototypeAst *Proto;

    /// What "Self" stands for at this call, decided with the
    /// overload ("PassedOverload::SelfId").
    analyse::scopes::TypeId SelfId;

    /// "PassedOverload::HasReceiver".
    bool HasReceiver;
  };

  Opt<_OInfo> _OverloadInfo;

  /// The return type "InferType" built, analysed, in the scope it
  /// was spelled in ("_InferredIn"), kept so it is built once there.
  Shared<TypeAst> _Inferred;
  Scope const *_InferredIn = nullptr;

  /// A callable value being called (a closure, a "FunXxx" held in
  /// a variable), resolved once at stage 7: the function type it is
  /// called through, and how the call holds it ("FunRef" borrowed,
  /// "FunMut" borrowed mutably, "FunMov" taken).
  struct _CallableInfo {
    Shared<TypeAst const> FnType;
    ConventionTag Conv;
  };

  Opt<_CallableInfo> _Callable;

  /// The borrow of the callee a "FunRef" or "FunMut" call makes, as
  /// an argument group of its own, checked against what the body
  /// already holds.
  Unique<FunctionCallArgumentGroupAst> _CalleeArgGroup;
  Vec<Unique<PostfixExpressionOperatorFunctionCallAst>> _FoldedAsts;

  Ast *_IsAsync;
  bool _IsCoroAndAutoResume;

  auto _HandleFnFolding(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Vec<Unique<PostfixExpressionOperatorFunctionCallAst>>;
};
