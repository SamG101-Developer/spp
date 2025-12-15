module;
#include <spp/macros.hpp>

export module spp.asts.coroutine_prototype_ast;
import spp.asts.function_prototype_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::asts {
    SPP_EXP_CLS struct CoroutinePrototypeAst;
}


SPP_EXP_CLS struct spp::asts::CoroutinePrototypeAst final : FunctionPrototypeAst {
private:
    llvm::Function *m_llvm_resume_fn;

protected:
    auto m_generate_llvm_declaration(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Function* override;

public:
    using FunctionPrototypeAst::FunctionPrototypeAst;

    llvm::Value *llvm_coro_yield_slot;

    ~CoroutinePrototypeAst() override;

    auto clone() const -> std::unique_ptr<Ast> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};


spp::asts::CoroutinePrototypeAst::~CoroutinePrototypeAst() = default;
