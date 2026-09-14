module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_size;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct TypeAst);

namespace spp::codegen {
  /// Determine the size of a type based on byte size.
  SPP_EXP_FUN auto SizeOf(ScopeManager const &sm, TypeAst const &type) -> std::size_t;

  /// Determine the alignment of a type based on bytes.
  SPP_EXP_FUN auto AlignOf(ScopeManager const &sm, TypeAst const &type) -> std::size_t;
}
