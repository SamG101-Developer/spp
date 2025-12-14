module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_coros;
import spp.codegen.llvm_ctx;
import llvm;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::asts {
    SPP_EXP_CLS struct CoroutinePrototypeAst;
}


namespace spp::codegen {
    SPP_EXP_ENUM enum class CoroutineState {
        READY, // Impossible in user code
        VARIABLE,
        EXHAUSTED,
        NO_VALUE, // On GenOpt
        ERROR, // On GenRes
    };

    SPP_EXP_FUN auto create_coro_environment(
        asts::CoroutinePrototypeAst const *coro,
        LLvmCtx *ctx,
        analyse::scopes::Scope const &scope) -> llvm::StructType*;


    /**
     * When a coroutine is called, the generate is immediately returned, whilst the <i>resuming</i> of the coroutine is
     * what runs the actual body. This is the initial constructor for the coroutine generator, which is received
     * immediately in the caller.
     * @param coro
     * @param ctx
     * @param scope
     * @return
     */
    SPP_EXP_FUN auto create_coro_gen_ctor(
        asts::CoroutinePrototypeAst const *coro,
        LLvmCtx *ctx,
        analyse::scopes::Scope const &scope) -> llvm::Function*;
}
