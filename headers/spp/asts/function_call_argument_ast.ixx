module;
#include <spp/macros.hpp>

export module spp.asts.function_call_argument_ast;
import spp.asts.ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.asts.utils.orderable;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionCallArgumentAst);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct PostfixExpressionOperatorFunctionCallAst);
use(spp::asts, struct TypeAst);

/// The base class for an argument in a function call, inherited
/// into the "positional" and "keyword" variants.
SPP_EXP_CLS struct spp::asts::FunctionCallArgumentAst : Ast, mixins::OrderableAst, mixins::TypeInferrableAst {
  using Ast::Ast;

  /// The convention on the argument being passed into the
  /// function call. Applies to positional and keyword arguments.
  Unique<ConventionAst> Conv;

  /// The expression being passed as the argument. Both
  /// positional and keyword arguments have a value.
  Unique<ExpressionAst> Val;

  FunctionCallArgumentAst(
    decltype(Conv) &&conv,
    decltype(Val) &&val,
    utils::OrderableTag order_tag);

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto SetSelfType(Shared<TypeAst> self_type) -> void;

  SPP_ATTR_NODISCARD auto GetSelfType() const -> Shared<TypeAst>;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  /// When the compiler generates an argument to inject the
  /// "self" type into a method call, the type is passed
  /// explicitly to speed up inference.
  Shared<TypeAst> _InjectedSelfType;
};
