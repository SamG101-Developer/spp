module;
#include <spp/macros.hpp>

export module spp.asts.let_statement_ast;
import spp.asts.statement_ast;

namespace spp::asts {
    SPP_EXP_CLS struct LetStatementAst;
}


SPP_EXP_CLS struct spp::asts::LetStatementAst : StatementAst {
    using StatementAst::StatementAst;
};
