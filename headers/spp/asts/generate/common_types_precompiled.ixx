module;
#include <spp/macros.hpp>

export module spp.asts.generate.common_types_precompiled;
import spp.asts.type_ast;
import spp.utils.types;
import std;


namespace spp::asts::generate::common_types_precompiled {
    /**
     * The fixed @code std::generator::Gen@endcode type, with no generic arguments.
     */
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

    SPP_EXP_CMP Shared<TypeAst> SINGLE = nullptr;
    SPP_EXP_CMP Shared<TypeAst> SHARED = nullptr;
    SPP_EXP_CMP Shared<TypeAst> SHADOW = nullptr;

    SPP_EXP_CMP Shared<TypeAst> FWD_MUT = nullptr;
    SPP_EXP_CMP Shared<TypeAst> FWD_REF = nullptr;

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
