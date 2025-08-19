#pragma once


#include <spp/asts/_fwd.hpp>


namespace spp::asts::generate::common_types_precompiled {
    inline const std::shared_ptr<TypeAst> GEN;
    inline const std::shared_ptr<TypeAst> GEN_OPT;
    inline const std::shared_ptr<TypeAst> GEN_RES;
    inline const std::shared_ptr<TypeAst> GEN_ONCE;

    inline const std::shared_ptr<TypeAst> FUN_MOV;
    inline const std::shared_ptr<TypeAst> FUN_MUT;
    inline const std::shared_ptr<TypeAst> FUN_REF;

    inline const std::shared_ptr<TypeAst> ARR;
    inline const std::shared_ptr<TypeAst> BOOL;

    auto initialize_types() -> void;
}
