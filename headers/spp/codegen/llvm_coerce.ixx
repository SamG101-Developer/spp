module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_coerce;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);

namespace spp::codegen {
  /// Coerce a value into a slot of a declared type. This is the single entry
  /// point that every site storing a value (where a type is also declared) goes
  /// through (ret/let/arg/field/assignment/gen/case/closure). It handles two
  /// key coercions: function values into the correct type, and variant widening
  /// (needs retagging). Tuples and arrays have per-element coercion.
  SPP_EXP_FUN auto CoerceToSlot(
    llvm::Value *llvm_val, TypeRef const &target, TypeRef const &source,
    ScopeManager const &sm, Str const &name, LlvmCtx *ctx) -> llvm::Value*;

  /// A tuple or array of "type" holding each element at its physical index. A
  /// constant element of another shape than its slot's (a variant member built
  /// as a packed struct) makes the whole aggregate one too, laid out as "type".
  /// So "arr: Arr[Opt[Str]]" being assigned "= [Some[Str]()]".
  SPP_EXP_FUN auto BuildAggregate(
    llvm::Type *type, Vec<Pair<unsigned, llvm::Value*>> const &elems,
    Str const &name, LlvmCtx *ctx) -> llvm::Value*;
}
