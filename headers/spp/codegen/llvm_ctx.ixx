module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::codegen, struct DeferredReached);
use(spp::codegen, struct LlvmCtx);
use(spp::codegen, struct LlvmGenerator);

/// The LLVM context information used throughout stage 10 and 11 of the
/// compiler's code generation. Like the compiler's meta context but specific to
/// LLVM and with no save/restore mechanic.
SPP_EXP_CLS struct spp::codegen::LlvmCtx {
  /// The LLVM context object, shared by every module of one compile (so they
  /// can be linked) and owned by the compilation.
  llvm::LLVMContext *Context;

  /// The scope manager currently in use. This is used to ensure types have been
  /// lowered, and is needed when moving between type aliases.
  ScopeManager const *Sm = nullptr;

  /// The LLVM module being emitted into. Needed for generating all LLVM items
  /// like functions, etc, into.
  Unique<llvm::Module> Module;

  /// The LLVM builder currently in use. Used to build all LLVM items, into the
  /// "Module" field.
  llvm::IRBuilder<> Builder;

  /// The module's debug info, when "[binary.debug] info" asks for any; its
  /// builder, the one compile unit the module is, a file per source path a
  /// location names, and the project root those paths are recorded relative
  /// to. With full debug info ("DiVariables"), variables are described too,
  /// with a type made once per S++ type, and the lexical blocks of the blocks
  /// being generated, innermost last, each beside its function (a closure's
  /// body is generated inside its parent's) and the location current where it
  /// opened, restored as it closes. All empty without.
  Unique<llvm::DIBuilder> DiBuilder;
  llvm::DICompileUnit *DiUnit = nullptr;
  Map<Str, llvm::DIFile*> DiFiles;
  Path DiRoot;
  bool DiVariables = false;
  Map<TypeSymbol const*, llvm::DIType*> DiTypes;
  Vec<Tup<llvm::Function const*, llvm::DILexicalBlock*, llvm::DebugLoc>> DiBlocks;

  /// Whether we are generating into compile-time context or not - some literals
  /// have better generation.
  bool InConstantContext = false;

  /// A list of coroutine handles and their associated generators. Keyed on
  /// assignment target storage.
  Map<llvm::Value*, Unique<LlvmGenerator>> LlvmGenerators;

  /// Track the current closure type. Todo: Does this work with nested closures?
  /// I doubt it.
  llvm::Type *CurrentClosureType = nullptr;

  /// The current closure scope. Todo: Does this work with nested closures? I
  /// doubt it.
  Scope *CurrentClosureScope = nullptr;

  /// The "defer" statements that the walk has reached in each scope, kept here
  /// rather than on the scope, which a generic template and its instantiations
  /// share.
  Unique<DeferredReached> Deferred;

  /// Build a new context from the module name that is about to run stage 10/11,
  /// emitting into "context".
  static auto NewCtx(
    Str const &module_name, llvm::LLVMContext &context) -> Unique<LlvmCtx>;

  explicit LlvmCtx(llvm::LLVMContext &context);
  LlvmCtx(LlvmCtx const &) = delete;
  LlvmCtx(LlvmCtx &&) noexcept = delete;
  auto operator=(LlvmCtx const &) -> LlvmCtx& = delete;
  auto operator=(LlvmCtx &&) noexcept -> LlvmCtx& = delete;
  ~LlvmCtx();
};
