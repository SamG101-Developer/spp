module;
#include <spp/macros.hpp>

export module spp.asts.function_implementation_lowered_ast;
import spp.asts.function_implementation_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionImplementationLoweredAst;
}


SPP_EXP_CLS struct spp::asts::FunctionImplementationLoweredAst final :
    FunctionImplementationAst {
private:
    std::string m_scope_str;

public:
    using FunctionImplementationAst::FunctionImplementationAst;

    ~FunctionImplementationLoweredAst() override;

    static auto new_empty() -> std::unique_ptr<FunctionImplementationLoweredAst>;

    auto clone() const -> std::unique_ptr<Ast> override;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto set_scope_str(std::string const &scope_str) -> void;
};
