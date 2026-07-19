module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_coros;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS struct TypeSymbol;
    SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct CoroutinePrototypeAst;
}

namespace spp::codegen {
    /**
     * The runtime state of a coroutine, stored in the env's STATE field.
     */
    SPP_EXP_CLS enum class CoroutineState : std::uint8_t {
        READY = 0,     // Constructed, not yet resumed.
        YIELDED = 1,   // Suspended at a "gen" with a value in the yield slot.
        EXHAUSTED = 2, // The body returned; no more values.
    };

    /**
     * The layout of a coroutine's env struct (its frame), which lives on the caller's stack (no heap). The
     * resume function pointer is NOT stored here - it is the first half of the { resume_fn, env } fat pointer
     * that the Gen/GenOnce/Generated types lower to.
     */
    SPP_EXP_CLS enum class GenEnvField : std::uint8_t {
        STATE = 0,
        LOCATION = 1,    // Which yield point to resume at (0 = start).
        SEND_SLOT = 2,   // Value passed in via ".res(x)".
        YIELD_SLOT = 3,  // Value produced by the current "gen".
        FRAME_START = 4, // Parameters and body locals follow, one field each.
    };

    /**
     * The layout of a Gen/GenOnce/Generated value: the { resume_fn, env } fat pointer.
     */
    SPP_EXP_CLS enum class GenValueField : std::uint8_t {
        RES_FN = 0,
        ENV = 1,
    };

    /**
     * Build the coroutine's env struct (its frame): { state, location, send_slot, yield_slot, then one field per
     * parameter and per body local }. The frame lives on the caller's stack, so there is no heap usage.
     * @param coro The coroutine prototype.
     * @param ctx The llvm context.
     * @param scope The coroutine's scope.
     * @return The new coroutine env struct type.
     */
    SPP_EXP_FUN auto CreateCoroEnvType(
        asts::CoroutinePrototypeAst *coro,
        LLvmCtx const *ctx,
        analyse::scopes::Scope const &scope)
        -> llvm::StructType*;

    /**
     * Collect a coroutine's frame variables (its parameters and every body local, one per env field starting at
     * @c GenEnvField::FRAME_START) in a stable order. Recurses into child scopes but stops at nested function/closure
     * scopes (their locals belong to their own frames). Called by env-type construction, the resume prologue and the
     * call site, so all three agree on the field index of each variable. These variables are needed because the locals
     * have to be stored somewhere in order to suspend and resume.
     * @param scope The coroutine's scope.
     * @return The frame variables, in env-field order.
     */
    SPP_EXP_FUN auto CollectCoroFrameVars(
        analyse::scopes::Scope const &scope)
        -> Vec<Shared<analyse::scopes::VariableSymbol>>;

    /**
     * Create the coroutine's resume function "(env*, send) -> void" and its prologue: the env pointer argument is
     * bound (via @c LlvmGenEnv) and every parameter/local's storage is pointed at its env field, so state persists
     * across resumes. The switch header (mapping the location field to yield continuations) and the body are built by
     * @c CoroutinePrototypeAst::Stage11_CodeGen.
     * @param coro The coroutine prototype.
     * @param ctx The llvm context.
     * @param scope The coroutine's scope.
     * @return The resume function (its entry block is left as the current insert point).
     */
    SPP_EXP_FUN auto CreateCoroResFunc(
        asts::CoroutinePrototypeAst *coro,
        LLvmCtx *ctx,
        analyse::scopes::Scope const &scope)
        -> llvm::Function*;

    SPP_EXP_FUN auto create_async_spawn_func(
        LLvmCtx *ctx,
        analyse::scopes::TypeSymbol const &fut_type_sym)
        -> llvm::Function*;
}
