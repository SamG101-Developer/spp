module;
#include <spp/macros.hpp>

export module spp.asts.let_statement_ast;
import spp.asts.statement_ast;


SPP_EXP struct spp::asts::LetStatementAst : StatementAst {
    using StatementAst::StatementAst;
};
