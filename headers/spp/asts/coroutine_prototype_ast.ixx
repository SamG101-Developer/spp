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

public:
    /**
     * The generator environment that this coroutine yields into, using the GenExpressionAst nodes. This is only set for
     * coroutine prototypes.
     */
    llvm::Value *llvm_gen_env;

    llvm::Value *llvm_coro_yield_slot;

    llvm::StructType *llvm_coro_env_type;

    using FunctionPrototypeAst::FunctionPrototypeAst;

    ~CoroutinePrototypeAst() override;

    auto clone() const -> std::unique_ptr<Ast> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};


spp::asts::CoroutinePrototypeAst::~CoroutinePrototypeAst() = default;
