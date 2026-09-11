module;
#include <spp/macros.hpp>

export module spp.asts.let_statement_ast;
import spp.asts.statement_ast;

SPP_AST_COMMON_FWD_DECL(LetStatementAst) {
}

SPP_EXP_CLS struct spp::asts::LetStatementAst : StatementAst {
  LetStatementAst();

  ~LetStatementAst() override;
};
