#pragma once


namespace spp::asts::generate::common_types_precompiled {
    inline std::shared_ptr<TypeAst> GEN;
    inline std::shared_ptr<TypeAst> GEN_OPT;
    inline std::shared_ptr<TypeAst> GEN_RES;
    inline std::shared_ptr<TypeAst> GEN_ONCE;

    inline std::shared_ptr<TypeAst> FUN_MOV;
    inline std::shared_ptr<TypeAst> FUN_MUT;
    inline std::shared_ptr<TypeAst> FUN_REF;

    inline std::shared_ptr<TypeAst> ARR;
    inline std::shared_ptr<TypeAst> TUP;
    inline std::shared_ptr<TypeAst> VAR;
    inline std::shared_ptr<TypeAst> TRY;

    inline std::shared_ptr<TypeAst> BOOL;
    inline std::shared_ptr<TypeAst> VOID;

    inline std::shared_ptr<TypeAst> NEVER;
    inline std::shared_ptr<TypeAst> COPY;

    auto initialize_types() -> void;
}
