module;
#include <spp/macros.hpp>

export module spp.asts:let_statement_ast;
import :statement_ast;

namespace spp::asts {
    SPP_EXP_CLS struct LetStatementAst;
}


SPP_EXP_CLS struct spp::asts::LetStatementAst : StatementAst {
    LetStatementAst();

    ~LetStatementAst() override;
};
