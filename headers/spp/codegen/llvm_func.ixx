module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_func;
import spp.codegen.llvm_ctx;
import llvm;

namespace spp::codegen {
  SPP_EXP_CLS struct LlvmFuncWrapper;

  /**
   * To fix cross-module errors, functions defined in other modules need declarations in the current module, so they are
   * accessible in the individual TU. This function either generated the declaration in the module, or gets the
   * declaration if it has already been declared in this module.
   * @param[in] target The target function definition.
   * @param[in] current_module The module to generate/retrieve the declaration into/from.
   * @return The function declaration local to the current module.
   */
  SPP_EXP_FUN auto GetOrAddTargetIntoCurrentModule(
    llvm::Function const &target,
    llvm::Module &current_module)
    -> llvm::Function*;

  /**
   * The global-variable counterpart of @c GetOrAddTargetIntoCurrentModule . A @c cmp constant is defined once, in the
   * module that declares it, but is read from every module that names it, and a global cannot be referenced across a
   * module boundary - so each reader gets its own external declaration of the same symbol to load through.
   * @param[in] target The global as it was defined in its owning module.
   * @param[in] current_module The module the load is being emitted into.
   * @return The global of that name belonging to @p current_module .
   */
  SPP_EXP_FUN auto GetOrAddGlobalIntoCurrentModule(
    llvm::GlobalVariable const &target,
    llvm::Module &current_module)
    -> llvm::GlobalVariable*;

  /**
   * The module that code is currently being emitted into. This is the module owning the function the builder is
   * inserting into, which is not always @c ctx->Module : a generic substitution is declared by whichever module's
   * walk reached it first, so its body can be generated while a different module is the current one. Falls back to
   * @c ctx->Module when there is no insertion point (a constant context, such as a @c cmp initializer).
   * @param[in] ctx The LLVM context containing all codegen info.
   * @return The module to attach declarations to.
   */
  SPP_EXP_FUN auto GetEmissionModule(
    LlvmCtx const &ctx)
    -> llvm::Module*;
}

/**
 * This is used as a shared pointer, wrapping the internal llvm::Function* pointer, allowing it to be shared between
 * cloned functions.
 */
SPP_EXP_CLS struct spp::codegen::LlvmFuncWrapper {
  llvm::Function *Target;
};
