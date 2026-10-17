module;
#include <spp/macros.hpp>

export module spp.asts.function_implementation_lowered_ast;
import spp.asts.ast_kind;
import spp.asts.function_implementation_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionImplementationLoweredAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::codegen::builtins, struct LoweredFnImpl);

SPP_EXP_CLS struct spp::asts::FunctionImplementationLoweredAst final : FunctionImplementationAst {
  SPP_AST_KIND(FunctionImplementationLoweredAst)

private:
  /// The builtin this lowers to, resolved once from the prototype's
  /// "!compiler_builtin" name: an entry of "kBuiltinFuncs", built
  /// once and never changed, so the pointer is stable.
  codegen::builtins::LoweredFnImpl const *_Builtin = nullptr;

  FunctionPrototypeAst *_ProtoPtr = nullptr;

  auto _ValidateZeroDivision(Vec<Unique<ExpressionAst>> const &args, meta::CompilerMetaData const *meta) const -> void;

  auto _ValidateShiftAmount(Vec<Unique<ExpressionAst>> const &args, meta::CompilerMetaData const *meta) const -> void;

public:
  static auto NewEmpty() -> Unique<FunctionImplementationLoweredAst>;

  using FunctionImplementationAst::FunctionImplementationAst;

  ~FunctionImplementationLoweredAst() override;

  SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto SetBuiltin(codegen::builtins::LoweredFnImpl const *builtin) -> void;

  SPP_ATTR_NODISCARD auto Builtin() const -> codegen::builtins::LoweredFnImpl const*;

  auto SetProtoPtr(FunctionPrototypeAst *proto) -> void;
};
