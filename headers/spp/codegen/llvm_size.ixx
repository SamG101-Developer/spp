module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_size;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);

namespace spp::codegen {
  /// Determine the size of a type based on byte size. The ref
  /// version is for a type as a value of it is held, which may
  /// be a borrow.
  SPP_EXP_FUN auto SizeOf(ScopeManager const &sm, TypeRef const &ref) -> std::size_t;
  SPP_EXP_FUN auto SizeOf(ScopeManager const &sm, TypeSymbol const &sym) -> std::size_t;

  /// Determine the alignment of a type based on bytes.
  SPP_EXP_FUN auto AlignOf(ScopeManager const &sm, TypeRef const &ref) -> std::size_t;
  SPP_EXP_FUN auto AlignOf(ScopeManager const &sm, TypeSymbol const &sym) -> std::size_t;
}
