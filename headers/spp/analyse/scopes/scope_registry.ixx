module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope_registry;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeStatementAst;
}


namespace spp::analyse::scopes {
    SPP_EXP_CLS struct TypeSymbol;
}


export namespace spp::analyse::scopes {
    std::map<spp::asts::TypeStatementAst*, std::shared_ptr<spp::analyse::scopes::TypeSymbol>> *ALIAS_TO_SYM_MAP;

    std::map<spp::analyse::scopes::TypeSymbol const*, std::unique_ptr<spp::asts::TypeStatementAst>> *SYM_TO_ALIAS_MAP;
}
