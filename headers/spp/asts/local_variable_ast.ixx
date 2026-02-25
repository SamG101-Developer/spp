module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct LetStatementInitializedAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableAst : Ast {
    LocalVariableAst();
    ~LocalVariableAst() override;
};
