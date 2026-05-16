module;
#include <spp/macros.hpp>

export module spp.asts.coroutine_prototype_ast;
import spp.asts.function_prototype_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::asts {
    SPP_EXP_CLS struct CoroutinePrototypeAst;
}


SPP_EXP_CLS struct spp::asts::CoroutinePrototypeAst final : FunctionPrototypeAst {
    /**
     * The generator environment that this coroutine yields into, using the GenExpressionAst nodes. This is only set for
     * coroutine prototypes.
     */
    llvm::Value *LlvmGenEnv;

    llvm::Value *LlvmCoroYieldSlot;

    llvm::StructType *LlvmCoroEnvType;

    using FunctionPrototypeAst::FunctionPrototypeAst;

    ~CoroutinePrototypeAst() override;

    SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto IsCoroutine() const -> bool override;

private:
    llvm::Function *_LlvmResumeFunc;
};
