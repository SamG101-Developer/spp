module;
#include <spp/macros.hpp>

export module spp.asts.function_implementation_lowered_ast;
import spp.asts.ast_kind;
import spp.asts.function_implementation_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionImplementationLoweredAst) {
  SPP_EXP_CLS struct FunctionPrototypeAst;
}

SPP_EXP_CLS struct spp::asts::FunctionImplementationLoweredAst final : FunctionImplementationAst {
  SPP_AST_KIND(FunctionImplementationLoweredAst)

private:
  Str _ScopePtr;

  FunctionPrototypeAst *_ProtoPtr = nullptr;

  auto _ValidateZeroDivision(
    Vec<Unique<ExpressionAst>> const &args,
    analyse::scopes::ScopeManager const *sm) const
    -> void;

  auto _ValidateShiftAmount(
    Vec<Unique<ExpressionAst>> const &args,
    analyse::scopes::ScopeManager const *sm) const
    -> void;

public:
  static auto NewEmpty() -> Unique<FunctionImplementationLoweredAst>;

  using FunctionImplementationAst::FunctionImplementationAst;

  ~FunctionImplementationLoweredAst() override;

  SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;

  auto Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  auto SetScopePtr(Str const &scope_str) -> void;

  auto SetProtoPtr(FunctionPrototypeAst *proto) -> void;
};
