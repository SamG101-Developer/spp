module;
#include <spp/macros.hpp>

export module spp.asts.generate.common_types_precompiled;
import spp.asts.type_ast;
import spp.utils.types;
import std;

namespace spp::asts::generate::common_types_precompiled {
    /**
     * The fixed @code std::generator::Gen@endcode type, with no generic arguments.
     * Todo: For some reason, `Unique` won't work here,
     */
    SPP_EXP_CMP Unique<TypeAst> GEN = nullptr;
    SPP_EXP_CMP Unique<TypeAst> GEN_ONCE = nullptr;

    SPP_EXP_CMP Unique<TypeAst> INDEX_REF = nullptr;
    SPP_EXP_CMP Unique<TypeAst> INDEX_MUT = nullptr;

    SPP_EXP_CMP Unique<TypeAst> SLICE_REF = nullptr;
    SPP_EXP_CMP Unique<TypeAst> SLICE_MUT = nullptr;

    SPP_EXP_CMP Unique<TypeAst> FUN_MOV = nullptr;
    SPP_EXP_CMP Unique<TypeAst> FUN_MUT = nullptr;
    SPP_EXP_CMP Unique<TypeAst> FUN_REF = nullptr;

    SPP_EXP_CMP Unique<TypeAst> ARR = nullptr;
    SPP_EXP_CMP Unique<TypeAst> TUP = nullptr;
    SPP_EXP_CMP Unique<TypeAst> VAR = nullptr;
    SPP_EXP_CMP Unique<TypeAst> TRY = nullptr;
    SPP_EXP_CMP Unique<TypeAst> FUT = nullptr;

    SPP_EXP_CMP Unique<TypeAst> BOOL = nullptr;
    SPP_EXP_CMP Unique<TypeAst> VOID = nullptr;

    SPP_EXP_CMP Unique<TypeAst> NEVER = nullptr;
    SPP_EXP_CMP Unique<TypeAst> COPY = nullptr;

    SPP_EXP_CMP Unique<TypeAst> SINGLE = nullptr;
    SPP_EXP_CMP Unique<TypeAst> SHARED = nullptr;
    SPP_EXP_CMP Unique<TypeAst> SHADOW = nullptr;

    SPP_EXP_CMP Unique<TypeAst> FWD_MUT = nullptr;
    SPP_EXP_CMP Unique<TypeAst> FWD_REF = nullptr;

    SPP_EXP_CMP Unique<TypeAst> S8 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> S16 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> S32 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> S64 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> S128 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> S256 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> SSIZE = nullptr;

    SPP_EXP_CMP Unique<TypeAst> U8 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> U16 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> U32 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> U64 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> U128 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> U256 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> USIZE = nullptr;

    SPP_EXP_CMP Unique<TypeAst> F8 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> F16 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> F32 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> F64 = nullptr;
    SPP_EXP_CMP Unique<TypeAst> F128 = nullptr;

    SPP_EXP_CMP Unique<TypeAst> CHAR = nullptr;
    SPP_EXP_CMP Unique<TypeAst> STR_VIEW = nullptr;
    SPP_EXP_CMP Unique<TypeAst> VIEW = nullptr;

    SPP_EXP_CMP Unique<TypeAst> SELF = nullptr;

    /**
     * Initialize the precompiled common types. This must be called before using any of the precompiled types.
     */
    SPP_EXP_FUN auto InitTypes() -> void;

    /**
     * Reset all precompiled type globals to nullptr. This releases the TypeAst objects (and their
     * CachedTypeSymbols maps) so that stale cache entries do not accumulate across compilation runs.
     * Must be called during cleanup, before the scope tree is destroyed.
     */
    SPP_EXP_FUN auto ClearTypes() -> void;
}
