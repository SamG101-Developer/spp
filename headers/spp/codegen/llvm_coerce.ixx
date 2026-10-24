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
  /// Coerce a value into a slot of a declared type: the one entry point every site storing a value where a type was
  /// declared goes through ("ret", "let", an argument, an attribute, an assignment, a "gen", a "case" branch, a
  /// closure's body). A named function going into a function type is built into the pair for the overload analysis
  /// chose ("$f" into "FunRef[..]"). A member going into a variant is tagged and copied into the payload ("S32" into
  /// "S32 or Str"), and a variant going into a wider one is re-tagged. A tuple or an array is coerced element by
  /// element, and a "!" value stands in as poison. Anything else is returned as it is.
  SPP_EXP_FUN auto CoerceToSlot(
    llvm::Value *llvm_val,
    TypeRef const &target,
    TypeRef const &source,
    ScopeManager const &sm,
    Str const &name,
    LlvmCtx *ctx)
    -> llvm::Value*;
}
