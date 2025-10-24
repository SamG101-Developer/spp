#pragma once
#include <spp/asts/function_prototype_ast.hpp>

namespace spp::codegen {
    struct LlvmCoroFrame;
}


struct spp::asts::CoroutinePrototypeAst final : FunctionPrototypeAst {
    using FunctionPrototypeAst::FunctionPrototypeAst;

private:
    std::shared_ptr<codegen::LlvmCoroFrame> m_coro_frame;

public:
    ~CoroutinePrototypeAst() override;

    auto clone() const -> std::unique_ptr<Ast> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, mixins::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
