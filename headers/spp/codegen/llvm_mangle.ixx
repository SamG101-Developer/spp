module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_mangle;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct CmpStatementAst;
  SPP_EXP_CLS struct FunctionPrototypeAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::codegen::mangle {
  SPP_EXP_FUN auto mangle_type_name(
    analyse::scopes::TypeSymbol const &type_sym)
    -> Str;

  SPP_EXP_FUN auto mangle_mod_name(
    analyse::scopes::Scope const &mod_scope)
    -> Str;

  SPP_EXP_FUN auto mangle_cmp_name(
    analyse::scopes::Scope const &owner_scope,
    asts::CmpStatementAst const &cmp_stmt)
    -> Str;

  /// A function's symbol: its readable name, fully qualified,
  /// like "std::result::expect[T=S32, E=Str]") and "$h" with a
  /// hash of its full signature, which is what keeps overloads
  /// (return type included) apart. Backtraces print it without
  /// the hash.
  SPP_EXP_FUN auto mangle_fun_name(
    analyse::scopes::Scope const &owner_scope,
    asts::FunctionPrototypeAst const &fun_proto)
    -> Str;

  /// The symbol of the "index"-th closure written in the
  /// function whose symbol is "enclosing": the enclosing
  /// function's readable name, then "::{closure#index}",
  /// then the enclosing function's hash.
  SPP_EXP_FUN auto mangle_closure_name(
    Str const &enclosing,
    std::size_t index)
    -> Str;
}
