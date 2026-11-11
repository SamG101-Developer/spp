module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_mangle;
import spp.utils.types;
import std;

use(spp::asts, struct CmpStatementAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, struct TypeSymbol);

namespace spp::codegen::mangle {
  /// Produce a stringification of a type's name for the LLVM system to identify
  /// uniquely. This will be unique per generic instantiation, per type.
  SPP_EXP_FUN auto MangleTypeName(TypeSymbol const &type_sym) -> Str;

  /// Produce a stringification of a module's name for the LLVM system to
  /// identify uniquely. This will be unique per module.
  SPP_EXP_FUN auto MangleModName(Scope const &mod_scope) -> Str;

  /// Produce a stringification of a constant's name for the LLVM system to
  /// identify uniquely. This will be unique per constant, per module.
  SPP_EXP_FUN auto MangleCmpName(
    Scope const &owner_scope, CmpStatementAst const &cmp_stmt) -> Str;

  /// Produce a stringification of a function's name for the LLVM system to
  /// identify uniquely. This will be unique per function, per overload,
  /// per module/owner-type, and include the signature (hashed).
  SPP_EXP_FUN auto MangleFnName(
    Scope const &owner_scope, FunctionPrototypeAst const &fun_proto) -> Str;

  /// Produce a stringification of a closure's name for the LLVM system to
  /// identify uniquely. This will be unique per symbol storing the closure,
  /// and includes the storage's name, and "::{closure#index}".
  SPP_EXP_FUN auto MangleClosureName(
    Str const &enclosing, std::size_t index) -> Str;
}
