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
}


SPP_EXP_CLS struct spp::asts::FunctionImplementationLoweredAst final : FunctionImplementationAst {
private:
    Str _ScopePtr;

public:
    static auto NewEmpty() -> Unique<FunctionImplementationLoweredAst>;

    using FunctionImplementationAst::FunctionImplementationAst;

    ~FunctionImplementationLoweredAst() override;

    SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto SetScopePtr(Str const &scope_str) -> void;
};
