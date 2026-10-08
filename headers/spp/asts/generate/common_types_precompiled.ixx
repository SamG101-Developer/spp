module;
#include <spp/macros.hpp>

export module spp.asts.generate.common_types_precompiled;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.utils.types;
import std;

use(spp::analyse::scopes, struct TypeSymbol);

namespace spp::asts::generate::common_types_precompiled {
  SPP_EXP_CMP Shared<TypeAst> GEN = nullptr;
  SPP_EXP_CMP Shared<TypeAst> GEN_ONCE = nullptr;
  SPP_EXP_CMP Shared<TypeAst> INDEX_REF = nullptr;
  SPP_EXP_CMP Shared<TypeAst> INDEX_MUT = nullptr;
  SPP_EXP_CMP Shared<TypeAst> SLICE_REF = nullptr;
  SPP_EXP_CMP Shared<TypeAst> SLICE_MUT = nullptr;
  SPP_EXP_CMP Shared<TypeAst> FUN_MOV = nullptr;
  SPP_EXP_CMP Shared<TypeAst> FUN_MUT = nullptr;
  SPP_EXP_CMP Shared<TypeAst> FUN_REF = nullptr;
  SPP_EXP_CMP Shared<TypeAst> ARR = nullptr;
  SPP_EXP_CMP Shared<TypeAst> TUP = nullptr;
  SPP_EXP_CMP Shared<TypeAst> VAR = nullptr;
  SPP_EXP_CMP Shared<TypeAst> TRY = nullptr;
  SPP_EXP_CMP Shared<TypeAst> FUT = nullptr;
  SPP_EXP_CMP Shared<TypeAst> BOOL = nullptr;
  SPP_EXP_CMP Shared<TypeAst> VOID = nullptr;
  SPP_EXP_CMP Shared<TypeAst> NEVER = nullptr;
  SPP_EXP_CMP Shared<TypeAst> COPY = nullptr;
  SPP_EXP_CMP Shared<TypeAst> DROP = nullptr;
  SPP_EXP_CMP Shared<TypeAst> THREAD_SAFE = nullptr;
  SPP_EXP_CMP Shared<TypeAst> FWD_MUT = nullptr;
  SPP_EXP_CMP Shared<TypeAst> FWD_REF = nullptr;
  SPP_EXP_CMP Shared<TypeAst> NON_NULL = nullptr;
  SPP_EXP_CMP Shared<TypeAst> S8 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> S16 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> S32 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> S64 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> S128 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> S256 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> SSIZE = nullptr;
  SPP_EXP_CMP Shared<TypeAst> U8 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> U16 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> U32 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> U64 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> U128 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> U256 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> USIZE = nullptr;
  SPP_EXP_CMP Shared<TypeAst> F8 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> F16 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> F32 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> F64 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> F128 = nullptr;
  SPP_EXP_CMP Shared<TypeAst> CHAR = nullptr;
  SPP_EXP_CMP Shared<TypeAst> STR_VIEW = nullptr;
  SPP_EXP_CMP Shared<TypeAst> VIEW = nullptr;
  SPP_EXP_CMP Shared<TypeAst> SELF_TYPE = nullptr;
  SPP_EXP_CMP Shared<IdentifierAst> SELF_VAR = nullptr;

  /// The template symbol each precompiled template type names
  /// ("TypeRef::IsA"), found on first query and kept for the
  /// compile, with the type it was found for held alive so its
  /// address is not reused. Cleared with the types.
  SPP_EXP_CMP Map<TypeAst const*, Pair<Shared<TypeAst const>, TypeSymbol*>> TEMPLATE_SYMBOLS = {};

  /// Initialize the precompiled common types. This must be
  /// called before using any of the precompiled types.
  SPP_EXP_FUN auto InitTypes() -> void;

  SPP_EXP_FUN auto BoolAt(std::size_t pos) -> Shared<TypeAst>;
  SPP_EXP_FUN auto VoidAt(std::size_t pos) -> Shared<TypeAst>;
  SPP_EXP_FUN auto NeverAt(std::size_t pos) -> Shared<TypeAst>;
  SPP_EXP_FUN auto StrViewAt(std::size_t pos) -> Shared<TypeAst>;

  /// Reset all precompiled type globals to nullptr. This
  /// releases the TypeAst objects (and their CachedTypeSymbols
  /// maps) so that stale cache entries do not accumulate
  /// across compilation runs. Must be called during cleanup,
  /// before the scope tree is destroyed.
  SPP_EXP_FUN auto ClearTypes() -> void;
}
