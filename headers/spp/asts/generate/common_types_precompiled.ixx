module;
#include <spp/macros.hpp>

export module spp.asts.generate.common_types_precompiled;
import spp.asts.type_ast;

import std;


namespace spp::asts::generate::common_types_precompiled {
    /**
     * The fixed @code std::generator::Gen@endcode type, with no generic arguments.
     */
    export inline std::shared_ptr<TypeAst> GEN = nullptr;
    export inline std::shared_ptr<TypeAst> GEN_OPT = nullptr;
    export inline std::shared_ptr<TypeAst> GEN_RES = nullptr;
    export inline std::shared_ptr<TypeAst> GEN_ONCE = nullptr;

    export inline std::shared_ptr<TypeAst> GENERATED = nullptr;
    export inline std::shared_ptr<TypeAst> GENERATED_OPT = nullptr;
    export inline std::shared_ptr<TypeAst> GENERATED_RES = nullptr;

    export inline std::shared_ptr<TypeAst> INDEX_REF = nullptr;
    export inline std::shared_ptr<TypeAst> INDEX_MUT = nullptr;

    export inline std::shared_ptr<TypeAst> FUN_MOV = nullptr;
    export inline std::shared_ptr<TypeAst> FUN_MUT = nullptr;
    export inline std::shared_ptr<TypeAst> FUN_REF = nullptr;

    export inline std::shared_ptr<TypeAst> ARR = nullptr;
    export inline std::shared_ptr<TypeAst> TUP = nullptr;
    export inline std::shared_ptr<TypeAst> VAR = nullptr;
    export inline std::shared_ptr<TypeAst> TRY = nullptr;
    export inline std::shared_ptr<TypeAst> FUT = nullptr;

    export inline std::shared_ptr<TypeAst> BOOL = nullptr;
    export inline std::shared_ptr<TypeAst> VOID = nullptr;

    export inline std::shared_ptr<TypeAst> NEVER = nullptr;
    export inline std::shared_ptr<TypeAst> COPY = nullptr;

    export inline std::shared_ptr<TypeAst> SINGLE = nullptr;
    export inline std::shared_ptr<TypeAst> SHARED = nullptr;
    export inline std::shared_ptr<TypeAst> SHADOW = nullptr;

    export inline std::shared_ptr<TypeAst> S8 = nullptr;
    export inline std::shared_ptr<TypeAst> S16 = nullptr;
    export inline std::shared_ptr<TypeAst> S32 = nullptr;
    export inline std::shared_ptr<TypeAst> S64 = nullptr;
    export inline std::shared_ptr<TypeAst> S128 = nullptr;
    export inline std::shared_ptr<TypeAst> S256 = nullptr;
    export inline std::shared_ptr<TypeAst> SSIZE = nullptr;

    export inline std::shared_ptr<TypeAst> U8 = nullptr;
    export inline std::shared_ptr<TypeAst> U16 = nullptr;
    export inline std::shared_ptr<TypeAst> U32 = nullptr;
    export inline std::shared_ptr<TypeAst> U64 = nullptr;
    export inline std::shared_ptr<TypeAst> U128 = nullptr;
    export inline std::shared_ptr<TypeAst> U256 = nullptr;
    export inline std::shared_ptr<TypeAst> USIZE = nullptr;

    export inline std::shared_ptr<TypeAst> F8 = nullptr;
    export inline std::shared_ptr<TypeAst> F16 = nullptr;
    export inline std::shared_ptr<TypeAst> F32 = nullptr;
    export inline std::shared_ptr<TypeAst> F64 = nullptr;
    export inline std::shared_ptr<TypeAst> F128 = nullptr;

    /**
     * Initialize the precompiled common types. This must be called before using any of the precompiled types.
     */
    SPP_EXP_FUN auto initialize_types() -> void;
}
