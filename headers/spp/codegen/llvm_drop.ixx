module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_drop;
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

namespace spp::asts::meta {
  SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::codegen {
  /**
   * Emit the destruction of the value at @p ptr , which is a pointer to storage holding a live value of the type
   * @p type_sym describes. A type that superimposes @c Del has its own @c del run first - it may still read its
   * attributes - and then every attribute is destroyed in reverse declaration order, recursively.
   * @param[in] type_sym The symbol of the type of the value being destroyed.
   * @param[in] ptr A pointer to the value's storage.
   * @param[in] sm The scope manager, positioned anywhere the type resolves from.
   * @param[in] meta Associated metadata.
   * @param[in] ctx The LLVM context containing all codegen info.
   */
  SPP_EXP_FUN auto EmitDrop(
    analyse::scopes::TypeSymbol const &type_sym,
    llvm::Value *ptr,
    analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;

  /**
   * Whether destroying @p sym has to be decided at runtime rather than statically: the analyser found a path that
   * moves it and a path that does not, so neither answer is right for both.
   * @param[in] sym The local in question.
   * @return Whether the local needs a drop flag.
   */
  SPP_EXP_FUN auto NeedsDropFlag(
    analyse::scopes::VariableSymbol const &sym)
    -> bool;

  /**
   * Record that @p sym holds a value from here on, if it is one of the locals whose destruction is decided at
   * runtime. Emitted where the local is initialized.
   * @param[in] sym The local that was just initialized.
   * @param[in] ctx The LLVM context containing all codegen info.
   */
  SPP_EXP_FUN auto EmitDropFlagSet(
    analyse::scopes::VariableSymbol &sym,
    LlvmCtx *ctx)
    -> void;

  /**
   * Record that @p sym no longer holds a value, if it is one of the locals whose destruction is decided at runtime.
   * Emitted where the local is moved away.
   * @param[in] sym The local that was just moved out of.
   * @param[in] ctx The LLVM context containing all codegen info.
   */
  SPP_EXP_FUN auto EmitDropFlagClear(
    analyse::scopes::VariableSymbol &sym,
    LlvmCtx *ctx)
    -> void;

  /**
   * Emit the destruction of a value a statement produced and nothing took: the @c Res an ignored call handed back,
   * the generator an expression built and consumed on the spot. Such a value has no name and no owner, so no scope
   * exit covers it - it is destroyed where it was produced.
   *
   * @n
   * Nothing is emitted for a borrow (it owns nothing), for a @c Void statement, or for a type that destroys to
   * nothing.
   * @param[in] type_sym The symbol of the discarded value's type.
   * @param[in] value The value itself, as produced by the statement.
   * @param[in] sm The scope manager, positioned where the statement was generated.
   * @param[in] meta Associated metadata.
   * @param[in] ctx The LLVM context containing all codegen info.
   */
  SPP_EXP_FUN auto EmitDiscardedValueDrop(
    analyse::scopes::TypeSymbol const &type_sym,
    llvm::Value *value,
    analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;

  /**
   * Emit the destruction of everything @p scope owns, in reverse declaration order - the reverse of the order the
   * values were initialized in, so a value is never destroyed while something declared after it can still name it. A
   * local is only this scope's to destroy if it still holds a value when the scope ends. Anything borrowed, moved out,
   * comptime, or never initialized is left alone, as is @p skip.
   * @param[in] scope The scope being left.
   * @param[in] skip A local the scope yields rather than destroys, or @c nullptr .
   * @param[in] sm The scope manager, positioned in @p scope .
   * @param[in] meta Associated metadata.
   * @param[in] ctx The LLVM context containing all codegen info.
   */
  SPP_EXP_FUN auto EmitScopeDrops(
    analyse::scopes::Scope const &scope,
    analyse::scopes::VariableSymbol const *skip,
    analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;

  /**
   * Emit the destruction of everything owned by @p innermost and every scope up to and including @p outermost , for
   * control that leaves them early. A @c ret or a loop @c exit jumps over the scope ends that would have run those
   * drops, so they are run at the jump instead, innermost scope first.
   * @param[in] innermost The scope control is leaving from.
   * @param[in] boundary The scope the walk stops at, or @c nullptr to walk to the root.
   * @param[in] boundary_inclusive Whether @p boundary is itself destroyed. A @c ret destroys the function scope it
   * stops at; a loop jump stops at the scope the loop sits in, which it is not leaving.
   * @param[in] skip A local being carried out of the jump, or @c nullptr .
   * @param[in] sm The scope manager, positioned in @p innermost .
   * @param[in] meta Associated metadata.
   * @param[in] ctx The LLVM context containing all codegen info.
   */
  SPP_EXP_FUN auto EmitUnwindDrops(
    analyse::scopes::Scope const &innermost,
    analyse::scopes::Scope const *boundary,
    bool boundary_inclusive,
    analyse::scopes::VariableSymbol const *skip,
    analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;
}
