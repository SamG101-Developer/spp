#pragma once

namespace spp::asts {
    struct TypeAst;
}


/// @cond
namespace spp::asts::generate::common_types_precompiled {
    /**
     * The fixed @code std::generator::Gen@endcode type, with no generic arguments.
     */
    inline std::shared_ptr<TypeAst> GEN;
    inline std::shared_ptr<TypeAst> GEN_OPT;
    inline std::shared_ptr<TypeAst> GEN_RES;
    inline std::shared_ptr<TypeAst> GEN_ONCE;

    inline std::shared_ptr<TypeAst> GENERATED;
    inline std::shared_ptr<TypeAst> GENERATED_OPT;
    inline std::shared_ptr<TypeAst> GENERATED_RES;

    inline std::shared_ptr<TypeAst> INDEX_REF;
    inline std::shared_ptr<TypeAst> INDEX_MUT;

    inline std::shared_ptr<TypeAst> FUN_MOV;
    inline std::shared_ptr<TypeAst> FUN_MUT;
    inline std::shared_ptr<TypeAst> FUN_REF;

    inline std::shared_ptr<TypeAst> ARR;
    inline std::shared_ptr<TypeAst> TUP;
    inline std::shared_ptr<TypeAst> VAR;
    inline std::shared_ptr<TypeAst> TRY;
    inline std::shared_ptr<TypeAst> FUT;

    inline std::shared_ptr<TypeAst> BOOL;
    inline std::shared_ptr<TypeAst> VOID;

    inline std::shared_ptr<TypeAst> NEVER;
    inline std::shared_ptr<TypeAst> COPY;

    inline std::shared_ptr<TypeAst> SINGLE;
    inline std::shared_ptr<TypeAst> SHARED;
    inline std::shared_ptr<TypeAst> SHADOW;

    inline std::shared_ptr<TypeAst> S8;
    inline std::shared_ptr<TypeAst> S16;
    inline std::shared_ptr<TypeAst> S32;
    inline std::shared_ptr<TypeAst> S64;
    inline std::shared_ptr<TypeAst> S128;
    inline std::shared_ptr<TypeAst> S256;
    inline std::shared_ptr<TypeAst> SSIZE;

    inline std::shared_ptr<TypeAst> U8;
    inline std::shared_ptr<TypeAst> U16;
    inline std::shared_ptr<TypeAst> U32;
    inline std::shared_ptr<TypeAst> U64;
    inline std::shared_ptr<TypeAst> U128;
    inline std::shared_ptr<TypeAst> U256;
    inline std::shared_ptr<TypeAst> USIZE;

    inline std::shared_ptr<TypeAst> F8;
    inline std::shared_ptr<TypeAst> F16;
    inline std::shared_ptr<TypeAst> F32;
    inline std::shared_ptr<TypeAst> F64;
    inline std::shared_ptr<TypeAst> F128;

    /**
     * Initialize the precompiled common types. This must be called before using any of the precompiled types.
     */
    auto initialize_types() -> void;
}

/// @endcond
