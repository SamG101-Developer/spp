module;
#include <spp/macros.hpp>

export module spp.asts.coroutine_prototype_ast;
import spp.asts._fwd;
import spp.asts.function_prototype_ast;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CoroutinePrototypeAst;
}


SPP_EXP_CLS struct spp::asts::CoroutinePrototypeAst final : FunctionPrototypeAst {
    friend struct spp::asts::GenExpressionAst;
    using FunctionPrototypeAst::FunctionPrototypeAst;

private:
    std::shared_ptr<codegen::LlvmCoroFrame> m_llvm_coro_frame;

    llvm::Value *m_llvm_coro_yield_slot;

public:
    ~CoroutinePrototypeAst() override;

    auto clone() const -> std::unique_ptr<Ast> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
