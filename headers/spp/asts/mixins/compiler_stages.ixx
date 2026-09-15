module;
#include <spp/macros.hpp>

export module spp.asts.mixins.compiler_stages;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct Ast);
use(spp::asts::mixins, struct CompilerStages);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::codegen, struct LlvmCtx);

/// The compiler stages are the root abstract ast that controls
/// the whole pipeline structure. Every inheriting AST can
/// choose to implement these functions, as they all have a
/// default no-op behaviour.
SPP_EXP_CLS struct spp::asts::mixins::CompilerStages {
  CompilerStages();

  virtual ~CompilerStages();

  /// The pre-processor re-organises certain asts into uniform
  /// patterns for future analysis. They key one is to convert
  /// functions to functional superimpositions over types.
  virtual auto Stage1_PreProcess(Ast *ctx) -> void;

  /// Generate top level scopes for "cls" classes, functions,
  /// and superimpositions. This is [top-level ast]-exclusive.
  /// Type aliases cannot be handled here, as at their root,
  /// must lie a pre-existing class type. Order agnostic.
  virtual auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void;

  /// Generate the type scopes for alias types, typically
  /// using the core alias resolver. All root class types
  /// will be existing at this point, so it is safe to
  /// determine the true mapped type. Order agnostic.
  virtual auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// Qualify types in key positions that have been written
  /// as not fully-qualified. This allows inter-module analysis,
  /// such as optional generic type parameters' default values.
  virtual auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// Log the super scopes being attached to their respect type
  /// targets. These aren't actually attached here, but are
  /// recorded as "going to be attached", because we have to do
  /// it at a fixed point. Relies on all aliases and types to
  /// be generated, and qualified.
  virtual auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// There are some checks that have to be done across the
  /// entire tree that need to happen before the standard semantic
  /// analysis begins. Critically, these typically prevent weird,
  /// non-sensical downstream errors.
  virtual auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// General core analysis of all asts, except memory-oriented
  /// checks. All identifier checks, type checks, overload
  /// resolution etc is handled here.
  virtual auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// All memory oriented checks, such as ownership checking,
  /// law of exclusivity, drop semantics, linear enforcement etc.
  virtual auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// Full compile time resolution for cmp statements, allowing
  /// cmp methods, conditional, literals, etc. Includes support
  /// for complex operations like the 10th Fibonacci number at
  /// compile time.
  virtual auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// The pre-code-gen step is needed for order agnostic behaviour
  /// - write out the declarations (types, functions), with no
  /// definitions. This IR is then built upon, for full definition
  /// implementation, in stage 11. Typically this stage just
  /// touches some top level asts.
  virtual auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> llvm::Value*;

  /// Do the codegen for all asts, full implementations,
  /// producing the complete per-module IR, that will be linked
  /// together with LTO, for a functioning executable.
  virtual auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> llvm::Value*;
};
