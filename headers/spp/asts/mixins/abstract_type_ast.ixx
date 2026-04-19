module;
#include <spp/macros.hpp>

export module spp.asts:abstract_type_ast;
import genex;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::asts::mixins {
    SPP_EXP_CLS struct AbstractTypeAst;
}


SPP_EXP_CLS struct spp::asts::mixins::AbstractTypeAst {
    virtual ~AbstractTypeAst();
};
