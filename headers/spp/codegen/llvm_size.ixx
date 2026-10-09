module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_size;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);

namespace spp::codegen {
  /// Determine the size of a type based on byte size, as a value
  /// of it is held: a borrow is a pointer.
  SPP_EXP_FUN auto SizeOf(ScopeManager const &sm, TypeRef const &ref) -> std::size_t;

  /// Determine the alignment of a type based on bytes.
  SPP_EXP_FUN auto AlignOf(ScopeManager const &sm, TypeRef const &ref) -> std::size_t;
}
