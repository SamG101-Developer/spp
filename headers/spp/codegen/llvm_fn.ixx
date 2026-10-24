module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_fn;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);
use(spp::codegen, struct LlvmFnWrapper);
use(spp::codegen, struct LlvmVarSymbolInfo);

/// This is used as a shared pointer, wrapping the internal
/// LLVM function, allowing it to be shared between cloned
/// functions, and a one-place update mechanism too.
SPP_EXP_CLS struct spp::codegen::LlvmFnWrapper {
  llvm::Function *Target;
};

namespace spp::codegen {
  /// To fix cross-module declaration errors, functions declared
  /// in other modules need their declarations brought into
  /// the current LLVM module, so they're accessible in the
  /// individual TU. This function either adds the declaration
  /// into this module, or it is already here so no work needs
  /// to be done. Either way, the declaration is returned.
  SPP_EXP_FUN auto GetOrAddTargetIntoCurrentModule(
    llvm::Function const &target,
    llvm::Module &current_module) -> llvm::Function*;

  /// The global constant equivalent of the above function;
  /// "cmp" statements can be defined in one module, and accessed
  /// in another, so again we need to expose the declaration into
  /// the current TU. Defined with external linkage, so the
  /// definition still remains in one place.
  SPP_EXP_FUN auto GetOrAddGlobalIntoCurrentModule(
    llvm::GlobalVariable const &target,
    llvm::Module &current_module) -> llvm::GlobalVariable*;

  /// Read the global constant a "cmp" symbol is held in, from the
  /// module being emitted into, as the type it is read as.
  SPP_EXP_FUN auto LoadGlobalConstant(
    LlvmVarSymbolInfo const &info, Str const &name,
    LlvmCtx *ctx) -> llvm::Value*;

  /// Get the module that code is currently being emitted into.
  /// This is the module owning the function that the builder
  /// is inserting into, which is not always "ctx->Module"; a
  /// generic substitution is declared by whichever module's
  /// walk reached it first, but the body is generated into the
  /// module that declared the base version of it. For example,
  /// all "Vec[U8]" functions, whilst initially used in the "Str"
  /// module, are defined in the "Vec" module.
  SPP_EXP_FUN auto GetEmissionModule(
    LlvmCtx const &ctx) -> llvm::Module*;

  /// Build the value that a function genuinely is, when used
  /// as one: the same "{ fn_ptr, env_ptr }" that a closure is.
  /// The pointer is to a thunk rather than to "target", as a
  /// call through the pair passes the environment first, and
  /// a plain function doesn't have a parameter for it. The
  /// thunk is emitted once per module and function type
  /// ("fn_sup", which its control-flow type comes from), and
  /// reused after that.
  SPP_EXP_FUN auto BuildFnValue(
    llvm::Function const &target, TypeRef const &fn_sup,
    LlvmCtx const *ctx) -> llvm::Constant*;

  /// A closure's environment: a header of an "i8" saying whether
  /// the environment owns its storage (heap, 1) or not (stack,
  /// 0) and a pointer to the function dropping its captures
  /// ("void(ptr env)", null when none needs dropping), then one
  /// field per capture. Both are recorded here once, when it is
  /// made, because nothing later can work them out: every closure
  /// of one signature shares its type, and a "FunRef" passed as a
  /// "FunMov" is freed as one. sppc reads the same header
  /// ("sppc_closure_env", "sppc_closure_discard").
  SPP_EXP_FUN auto ClosureEnvType(
    Vec<llvm::Type*> const &capture_tys, StrView name,
    LlvmCtx const *ctx) -> llvm::StructType*;

  /// Make a closure's environment of type "env_ty" - on the heap
  /// when it may outlive the frame making it, else on the stack -
  /// and record which in its header, with "drop_captures" (null
  /// when nothing it captures needs dropping).
  SPP_EXP_FUN auto EmitNewClosureEnv(
    llvm::StructType *env_ty, bool on_heap, llvm::Function *drop_captures, StrView name,
    LlvmCtx *ctx) -> llvm::Value*;

  /// Mark "fn" as a closure's capture destructor, the type the
  /// call in "EmitDropClosureEnv" checks it against.
  SPP_EXP_FUN auto StampCfiClosureEnvDrop(
    llvm::Function &fn) -> void;

  /// Which of a generator's two functions a call through its
  /// frame reaches: the one resuming it, or the one destroying
  /// it. Each has its own control-flow type.
  SPP_EXP_CLS enum class CoroEdge { Resume, Destroy };

  /// Record that "fn" is called indirectly as a value whose
  /// function type is "fn_sup" ("FindFnSup"), which "ApplyCfi"
  /// turns into the type a call checks for. Recorded as
  /// "metadata" on the function. The type is a hash of the
  /// function type's "Args" and "Out", the s++ signature, not
  /// of which "FunXXX" it is, as a "FunRef" can be passed as a
  /// "FunMov" (constrained generic).
  SPP_EXP_FUN auto StampCfiFn(
    llvm::Function &fn, TypeRef const &fn_sup) -> void;

  /// Record a coroutine's generator. The lowering splits it into
  /// the functions resuming and destroying it, which keep this,
  /// and "ApplyCfi" types each of them by which it is.
  SPP_EXP_FUN auto StampCfiCoroutine(
    llvm::Function &ramp, TypeRef const &gen_sup) -> void;

  /// The operand bundle an indirect call through a value whose
  /// function type is "fn_sup" carries, so the backend checks
  /// that what it reaches was stamped with the same type (KCFI).
  SPP_EXP_FUN auto CfiFnCallBundles(
    TypeRef const &fn_sup, LlvmCtx const *ctx) -> Vec<llvm::OperandBundleDef>;

  /// The same for resuming or destroying a generator whose
  /// generator type is "gen_sup" ("FindGenSup"), which the
  /// coroutine lowering turns into a call through its frame.
  SPP_EXP_FUN auto CfiGenCallBundles(
    TypeRef const &gen_sup, CoroEdge edge, LlvmCtx const *ctx) -> Vec<llvm::OperandBundleDef>;

  /// The field of a closure's environment holding capture "i".
  SPP_EXP_FUN auto ClosureEnvCaptureIndex(
    std::size_t i) -> unsigned;

  /// Free a closure's environment, if it has one and it owns its
  /// storage: what consuming a "FunMov" does, as its body has
  /// consumed the captures.
  SPP_EXP_FUN auto EmitReleaseClosureEnv(
    llvm::Value *env, LlvmCtx *ctx) -> void;

  /// Drop a closure's captures through its header, then free its
  /// environment as "EmitReleaseClosureEnv" does: what dropping a
  /// closure that was never consumed does.
  SPP_EXP_FUN auto EmitDropClosureEnv(
    llvm::Value *env, LlvmCtx *ctx) -> void;
}
