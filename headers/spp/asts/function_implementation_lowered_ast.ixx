module;
#include <spp/macros.hpp>

export module spp.asts.function_implementation_lowered_ast;
import spp.asts.function_implementation_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct FunctionImplementationLoweredAst;
  SPP_EXP_CLS struct FunctionPrototypeAst;
}

SPP_EXP_CLS struct spp::asts::FunctionImplementationLoweredAst final : FunctionImplementationAst {
private:
  Str _ScopePtr;

  FunctionPrototypeAst *_ProtoPtr = nullptr;

  auto _ValidateZeroDivision(
    Vec<Unique<ExpressionAst>> const &args,
    ScopeManager const *sm) const
    -> void;

  auto _ValidateShiftAmount(
    Vec<Unique<ExpressionAst>> const &args,
    ScopeManager const *sm) const
    -> void;

public:
  static auto NewEmpty() -> Unique<FunctionImplementationLoweredAst>;

  using FunctionImplementationAst::FunctionImplementationAst;

  ~FunctionImplementationLoweredAst() override;

  SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto SetScopePtr(Str const &scope_str) -> void;

  auto SetProtoPtr(FunctionPrototypeAst *proto) -> void;
};
