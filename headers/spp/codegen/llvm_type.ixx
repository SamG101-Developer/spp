module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_type;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct ClassPrototypeAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct TypeAst);

namespace spp::codegen {
  /// Lower the type from the class prototype ast into the
  /// LLVM type, inside the llvm system. Todo: See if we can
  /// remove this overload and always use the beneath one.
  SPP_EXP_FUN auto RegisterLlvmTypeInfo(
    ClassPrototypeAst const *cls_proto, ScopeManager const &sm, LlvmCtx const *ctx) -> void;

  /// Lower the type owned by the "scope", filling the LLVM type
  /// information on the scopes "TySym". Taking the scope
  /// rather than a class prototype is important for a generic
  /// instantiation: the symbols "type" still names the template
  /// it instantiates, rather than targeting the actual generic
  /// instantiation.
  SPP_EXP_FUN auto RegisterLlvmTypeInfo(
    Scope const *scope, ScopeManager const &sm, LlvmCtx const *ctx) -> void;

  /// Get the LLVM type information off of a type symbol. If the
  /// type is borrowed, then return the opaque pointer type,
  /// otherwise ensure the LLVM type has been generated, and return
  /// it.
  SPP_EXP_FUN auto GetLlvmType(
    TypeSymbol const &type_sym, LlvmCtx const *ctx) -> llvm::Type*;

  /// Early pointer check for the type's convention, followed by
  /// the type symbol lookup and "GetLlvmType" lookup.
  SPP_EXP_FUN auto GetLlvmTypeOf(
    TypeAst const &type, Scope const &scope, LlvmCtx const *ctx) -> llvm::Type*;

  /// Ensure the provided type symbol has its LLVM type information
  /// registered all the way down its field types (so that the
  /// actual struct size is known and DataLayout can answer for it.
  /// This is needed when stage 10 does something that is not in
  /// scope walk order, or a generic instantiation.
  SPP_EXP_FUN auto EnsureLlvmTypeComplete(
    TypeSymbol const &type_sym, ScopeManager const &sm, LlvmCtx const *ctx) -> void;

  /// Whether a value of this lowered type has any runtime value
  /// representation at all. Currently, this just checks for a LLVM
  /// void type, but is defined in a single place so that we can
  /// expand it if needed. Types that match here means there is
  /// nothing to store, load, or hand off to a phi.
  SPP_EXP_FUN auto IsValuelessType(
    llvm::Type const *type) -> bool;

  /// Get the fat field pointers on a type. The "FunXXX" and
  /// "GenXXX" types have special fat pointer fields on, as do
  /// any types that extend from these two types. Return the
  /// "{ fn_ptr, env_ptr }" pair in a 1-item vector (for future
  /// expansion).
  SPP_EXP_FUN auto GetFatPointerFields(
    TypeAst const &type, Scope const &scope, LlvmCtx const *ctx) -> std::optional<Vec<llvm::Type*>>;
}
