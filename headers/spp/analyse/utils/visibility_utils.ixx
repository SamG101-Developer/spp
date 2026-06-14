module;
#include <spp/macros.hpp>

export module spp.analyse.utils.visibility_utils;
import spp.asts.utils.visibility;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct IdentifierAst;
}

namespace spp::asts::meta {
    SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::analyse::utils::visibility_utils {
    SPP_EXP_FUN auto VisibilityName(asts::utils::Visibility vis) -> Str;

    SPP_EXP_FUN auto CheckTypeMemberVisibility(
        scopes::VariableSymbol const &sym,
        asts::Ast const &access_ast,
        scopes::Scope const &type_scope,
        scopes::ScopeManager const &sm,
        asts::meta::CompilerMetaData const &meta)
        -> void;

    SPP_EXP_FUN auto CheckModuleMemberVisibility(
        scopes::VariableSymbol const &sym,
        asts::Ast const &access_ast,
        scopes::Scope const &definition_scope,
        scopes::ScopeManager const &sm,
        asts::meta::CompilerMetaData const &meta)
        -> void;
}
