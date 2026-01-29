module;
#include <spp/macros.hpp>

export module spp.analyse.utils.obj_utils;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct ObjectInitializerAst;
}


namespace spp::analyse::utils::obj_utils {
    SPP_EXP_FUN auto set_attribute_value(
        asts::ObjectInitializerAst *object,
        asts::Ast *attribute,
        std::unique_ptr<asts::ExpressionAst> &&value,
        scopes::ScopeManager const *sm)
        -> void;

    SPP_EXP_FUN auto get_attribute_value(
        asts::ObjectInitializerAst const *object,
        asts::IdentifierAst const *attribute)
        -> std::unique_ptr<asts::ExpressionAst>;
}
