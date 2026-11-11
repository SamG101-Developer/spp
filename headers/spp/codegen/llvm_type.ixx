module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_type;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct TypeAst);

namespace spp::codegen {
  /// Lower the type owned by the "scope", filling the LLVM type information on
  /// the scopes "LinkedTypeSymbol". Taking the scope rather than a class
  /// prototype is needed for generic instantiations; the symbols "type" still
  /// names the template it instantiates, rather than targeting the actual
  /// generic instantiation.
  SPP_EXP_FUN auto RegisterLlvmTypeInfo(
    Scope const *scope, ScopeManager const &sm, LlvmCtx const *ctx) -> void;

  /// The LLVM type representation, of a resolved S++ type: a borrow is the
  /// opaque pointer type (whatever it borrows, as in modern LLVM convention),
  /// and anything else its symbol's type.
  SPP_EXP_FUN auto GetLlvmTypeOf(
    TypeRef const &ref, LlvmCtx const *ctx) -> llvm::Type*;

  /// Ensure the provided type symbol has its LLVM type information registered
  /// all the way down its field types (so that the actual struct size is known
  /// and DataLayout is valid. This is needed when stage 10 does something that
  /// is not in scope walk order, or a generic instantiation.
  SPP_EXP_FUN auto EnsureLlvmTypeComplete(
    TypeSymbol const &type_sym, ScopeManager const &sm,
    LlvmCtx const *ctx) -> void;

  /// Whether a value of this lowered type has any runtime value representation
  /// at all. Currently, this just checks for a LLVM void type, but is defined
  /// in a single place so that it can be expanded if needed. Types that match
  /// here means there is nothing to store, load, or hand off to a phi.
  SPP_EXP_FUN auto IsValuelessType(llvm::Type const *type) -> bool;
}
