module;
#include <spp/macros.hpp>

export module spp.asts.generate.common_types_precompiled;
import spp.asts.type_ast;

import std;


namespace spp::asts::generate::common_types_precompiled {
    /**
     * The fixed @code std::generator::Gen@endcode type, with no generic arguments.
     */
    SPP_EXP_CLS std::shared_ptr<TypeAst> GEN;
    SPP_EXP_CLS std::shared_ptr<TypeAst> GEN_OPT;
    SPP_EXP_CLS std::shared_ptr<TypeAst> GEN_RES;
    SPP_EXP_CLS std::shared_ptr<TypeAst> GEN_ONCE;

    SPP_EXP_CLS std::shared_ptr<TypeAst> GENERATED;
    SPP_EXP_CLS std::shared_ptr<TypeAst> GENERATED_OPT;
    SPP_EXP_CLS std::shared_ptr<TypeAst> GENERATED_RES;

    SPP_EXP_CLS std::shared_ptr<TypeAst> INDEX_REF;
    SPP_EXP_CLS std::shared_ptr<TypeAst> INDEX_MUT;

    SPP_EXP_CLS std::shared_ptr<TypeAst> FUN_MOV;
    SPP_EXP_CLS std::shared_ptr<TypeAst> FUN_MUT;
    SPP_EXP_CLS std::shared_ptr<TypeAst> FUN_REF;

    SPP_EXP_CLS std::shared_ptr<TypeAst> ARR;
    SPP_EXP_CLS std::shared_ptr<TypeAst> TUP;
    SPP_EXP_CLS std::shared_ptr<TypeAst> VAR;
    SPP_EXP_CLS std::shared_ptr<TypeAst> TRY;
    SPP_EXP_CLS std::shared_ptr<TypeAst> FUT;

    SPP_EXP_CLS std::shared_ptr<TypeAst> BOOL;
    SPP_EXP_CLS std::shared_ptr<TypeAst> VOID;

    SPP_EXP_CLS std::shared_ptr<TypeAst> NEVER;
    SPP_EXP_CLS std::shared_ptr<TypeAst> COPY;

    SPP_EXP_CLS std::shared_ptr<TypeAst> SINGLE;
    SPP_EXP_CLS std::shared_ptr<TypeAst> SHARED;
    SPP_EXP_CLS std::shared_ptr<TypeAst> SHADOW;

    SPP_EXP_CLS std::shared_ptr<TypeAst> S8;
    SPP_EXP_CLS std::shared_ptr<TypeAst> S16;
    SPP_EXP_CLS std::shared_ptr<TypeAst> S32;
    SPP_EXP_CLS std::shared_ptr<TypeAst> S64;
    SPP_EXP_CLS std::shared_ptr<TypeAst> S128;
    SPP_EXP_CLS std::shared_ptr<TypeAst> S256;
    SPP_EXP_CLS std::shared_ptr<TypeAst> SSIZE;

    SPP_EXP_CLS std::shared_ptr<TypeAst> U8;
    SPP_EXP_CLS std::shared_ptr<TypeAst> U16;
    SPP_EXP_CLS std::shared_ptr<TypeAst> U32;
    SPP_EXP_CLS std::shared_ptr<TypeAst> U64;
    SPP_EXP_CLS std::shared_ptr<TypeAst> U128;
    SPP_EXP_CLS std::shared_ptr<TypeAst> U256;
    SPP_EXP_CLS std::shared_ptr<TypeAst> USIZE;

    SPP_EXP_CLS std::shared_ptr<TypeAst> F8;
    SPP_EXP_CLS std::shared_ptr<TypeAst> F16;
    SPP_EXP_CLS std::shared_ptr<TypeAst> F32;
    SPP_EXP_CLS std::shared_ptr<TypeAst> F64;
    SPP_EXP_CLS std::shared_ptr<TypeAst> F128;

    /**
     * Initialize the precompiled common types. This must be called before using any of the precompiled types.
     */
    SPP_EXP_FUN auto initialize_types() -> void;
}
