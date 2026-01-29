module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_coros;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS struct TypeSymbol;
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
        EXCEPTION, // On GenRes
    };

    SPP_EXP_ENUM enum class GenEnvField {
        RES_FN = 0,
        STATE = 1,
        LOCATION = 2,
        ARGS = 3,
        YIELD_SLOT = 4,
        SEND_SLOT = 5,
        ERROR_SLOT = 6,
    };

    SPP_EXP_FUN auto create_coro_env_type(
        asts::CoroutinePrototypeAst *coro,
        LLvmCtx *ctx,
        analyse::scopes::Scope const &scope)
        -> std::pair<llvm::StructType*, llvm::StructType*>;


    /**
     * When a coroutine is called, the generate is immediately returned, whilst the <i>resuming</i> of the coroutine is
     * what runs the actual body. This is the initial constructor for the coroutine generator, which is received
     * immediately in the caller.
     * @param coro
     * @param ctx
     * @param scope
     * @return The generated function, and the generator environment object.
     */
    SPP_EXP_FUN auto create_coro_gen_ctor(
        asts::CoroutinePrototypeAst *coro,
        LLvmCtx *ctx,
        analyse::scopes::Scope const &scope)
        -> std::tuple<llvm::Function*, llvm::Value*, llvm::Type*>;


    /**
     * When a coroutine is resumed, this function is called to actually run the body of the coroutine. This works by
     * analysing the entire body of the coroutine, at every every "gen"/yield site, inserting a jump label. Then, a
     * header block of switch/case is injected into the top of the resume function, which jumps to the correct label
     * based on the current state of the coroutine (tracked by the generator environment itself).
     * @param coro
     * @param ctx
     * @param scope
     * @return The generated resume function.
     */
    SPP_EXP_FUN auto create_coro_res_func(
        asts::CoroutinePrototypeAst const *coro,
        llvm::Type *llvm_arg_struct_type,
        LLvmCtx *ctx,
        analyse::scopes::Scope const &scope)
        -> llvm::Function*;

    SPP_EXP_FUN auto create_async_spawn_func(
        LLvmCtx *ctx,
        analyse::scopes::TypeSymbol const &fut_type_sym)
        -> llvm::Function*;
}
