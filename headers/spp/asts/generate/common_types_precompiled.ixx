module;
#include <spp/macros.hpp>

export module spp.asts.generate.common_types_precompiled;
import spp.asts.type_ast;

import std;


namespace spp::asts::generate::common_types_precompiled {
    /**
     * The fixed @code std::generator::Gen@endcode type, with no generic arguments.
     */
    SPP_EXP_CMP std::shared_ptr<TypeAst> GEN = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> GEN_ONCE = nullptr;

    SPP_EXP_CMP std::shared_ptr<TypeAst> INDEX_REF = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> INDEX_MUT = nullptr;

    SPP_EXP_CMP std::shared_ptr<TypeAst> FUN_MOV = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> FUN_MUT = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> FUN_REF = nullptr;

    SPP_EXP_CMP std::shared_ptr<TypeAst> ARR = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> TUP = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> VAR = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> TRY = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> FUT = nullptr;

    SPP_EXP_CMP std::shared_ptr<TypeAst> BOOL = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> VOID = nullptr;

    SPP_EXP_CMP std::shared_ptr<TypeAst> NEVER = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> COPY = nullptr;

    SPP_EXP_CMP std::shared_ptr<TypeAst> SINGLE = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> SHARED = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> SHADOW = nullptr;

    SPP_EXP_CMP std::shared_ptr<TypeAst> FWD_MUT = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> FWD_REF = nullptr;

    SPP_EXP_CMP std::shared_ptr<TypeAst> S8 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> S16 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> S32 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> S64 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> S128 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> S256 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> SSIZE = nullptr;

    SPP_EXP_CMP std::shared_ptr<TypeAst> U8 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> U16 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> U32 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> U64 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> U128 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> U256 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> USIZE = nullptr;

    SPP_EXP_CMP std::shared_ptr<TypeAst> F8 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> F16 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> F32 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> F64 = nullptr;
    SPP_EXP_CMP std::shared_ptr<TypeAst> F128 = nullptr;

    /**
     * Initialize the precompiled common types. This must be called before using any of the precompiled types.
     */
    SPP_EXP_FUN auto initialize_types() -> void;
}
